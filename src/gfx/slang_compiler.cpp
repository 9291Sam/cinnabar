#include "slang_compiler.hpp"
#include "util/logger.hpp"
#include "util/timer.hpp"
#include "util/util.hpp"
#include <array>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>
#undef max
#undef MemoryBarrier
#undef min
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace gfx
{
    std::atomic<usize> SaneSlangCompiler::monotonic_counter {0};

    SaneSlangCompiler::SaneSlangCompiler()
    {
        assert::critical(
            SaneSlangCompiler::isSlangAvailable(), "Slang was not found on your system. Is this a production build?");

        this->temporary_dir =
            std::filesystem::temp_directory_path() / std::format("cinnabar_slang_compiler_{}", std::random_device {}());

        std::filesystem::create_directories(this->temporary_dir);
    }

    SaneSlangCompiler::~SaneSlangCompiler()
    {
        std::error_code ec;
        std::filesystem::remove_all(this->temporary_dir, ec);

        assert::warn(!ec, "Failed to cleanup SaneSlangCompiler temporary directory");
    }

    SaneSlangCompiler::CompileResult SaneSlangCompiler::compile(std::filesystem::path path)
    {
        assert::critical(
            std::filesystem::exists(path),
            "Sane Slang Compiler was requested to compile |{}| but that path does not exist!",
            path.generic_string());

        bool         hasAnyCompileSucceeded = false;
        SpirvShaders shaders {};
        std::string  compileMessages {};

        auto tryCompileEntry = [&](const std::string& entryName, const std::string& stage, std::vector<u32>& outputData)
        {
            if (!SaneSlangCompiler::hasEntryPoint(path, entryName))
            {
                return;
            }

            auto [maybeOutputData, outputMessage] = this->compileEntryPoint(path, entryName, stage);

            if (maybeOutputData.has_value())
            {
                outputData             = *std::move(maybeOutputData);
                hasAnyCompileSucceeded = true;
            }

            compileMessages += outputMessage;

            if (!outputMessage.empty())
            {
                compileMessages += "\n";
            }
        };

        tryCompileEntry("vertexMain", "vertex", shaders.maybe_vertex_data);
        tryCompileEntry("fragmentMain", "fragment", shaders.maybe_fragment_data);
        tryCompileEntry("computeMain", "compute", shaders.maybe_compute_data);

        std::set<std::filesystem::path>    visited {};
        std::vector<std::filesystem::path> dependentFiles {};
        this->collectDependencies(path, visited, dependentFiles);

        dependentFiles.push_back(std::move(path));

        return CompileResult {
            .shaders {hasAnyCompileSucceeded ? std::optional {shaders} : std::nullopt},
            .dependent_files {std::move(dependentFiles)},
            .warnings_and_errors {std::move(compileMessages)}};
    }

    std::pair<std::optional<std::vector<u32>>, std::string> SaneSlangCompiler::compileEntryPoint(
        const std::filesystem::path& sourcePath, const std::string& entryPoint, const std::string& stage)
    {
        const std::filesystem::path outputPath = createTempFile(".spv");

        std::vector<std::string> args {
            "-target",
            "spirv",
            "-stage",
            stage,
            "-entry",
            entryPoint,
            "-o",
            outputPath.string(),
            "-O3",
            "-fvk-use-gl-layout",
            "-Wno-39001", // overlapping bindings
        };

        for (const auto& searchPath : this->search_paths)
        {
            args.push_back("-I");
            args.push_back(searchPath.string());
        }

        args.push_back(sourcePath.string());

        util::Defer _ {[&]
                       {
                           std::filesystem::remove(outputPath);
                       }};

        auto [succeeded, output] = SaneSlangCompiler::executeSlang(args);

        return std::pair<std::optional<std::vector<u32>>, std::string> {
            succeeded ? std::optional {SaneSlangCompiler::loadSpirvFromFile(outputPath)} : std::nullopt, output};
    }

    std::filesystem::path SaneSlangCompiler::createTempFile(std::string_view suffix)
    {
        return this->temporary_dir / std::format("temp_{}{}", SaneSlangCompiler::monotonic_counter++, suffix);
    }

    bool SaneSlangCompiler::hasEntryPoint(const std::filesystem::path& sourcePath, const std::string& entryPoint)
    {
        std::ifstream file(sourcePath);
        if (!file)
        {
            return false;
        }

        std::string content {(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()};

        std::string pattern1 = "void " + entryPoint + "(";
        std::string pattern2 = entryPoint + "(";

        return content.find(pattern1) != std::string::npos || content.find(pattern2) != std::string::npos;
    }

    std::vector<u32> SaneSlangCompiler::loadSpirvFromFile(const std::filesystem::path& filePath)
    {
        std::vector<std::byte> rawFileData = util::loadEntireFileFromPath(filePath);

        std::vector<u32> spirvData {};
        spirvData.resize(rawFileData.size() / 4);

        assert::critical(
            rawFileData.size() > 0 && rawFileData.size() % 4 == 0,
            "Apparent Spirv file data is of an incorrect length of {} bytes",
            rawFileData.size());

        std::memcpy(spirvData.data(), rawFileData.data(), rawFileData.size());

        assert::critical(spirvData.at(0) == 0x07230203, "Spirv magic constant was {}", spirvData.at(0));

        return spirvData;
    }

    std::pair<bool, std::string> SaneSlangCompiler::executeSlang(const std::vector<std::string>& args)
    {
        std::string command = "slangc";
        for (const auto& arg : args)
        {
            command += " " + escapeArg(arg);
        }

        command += " 2>&1"; // redirect stderr to stdout so we can capture it

        std::array<char, 4096> buffer {};
        std::string            result;

#ifdef _WIN32
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif

        assert::critical(pipe != nullptr, "Failed to execute slang command |{}|", command);

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }

#ifdef _WIN32
        int exitCode = _pclose(pipe.release());
#else
        int exitCode = pclose(pipe.release());
        exitCode     = WEXITSTATUS(exitCode);
#endif

        return std::pair<bool, std::string> {exitCode == 0, std::move(result)};
    }

    std::string SaneSlangCompiler::escapeArg(const std::string& arg)
    {
        bool needsQuotes = arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos
                        || arg.find('"') != std::string::npos;

        if (!needsQuotes)
        {
            return arg;
        }

#ifdef _WIN32
        // Windows-style escaping
        std::string escaped = "\"";
        for (char c : arg)
        {
            if (c == '"')
            {
                escaped += "\\\"";
            }
            else if (c == '\\')
            {
                escaped += "\\\\";
            }
            else
            {
                escaped += c;
            }
        }
        escaped += "\"";
        return escaped;
#else
        std::string escaped = "'";
        for (char c : arg)
        {
            if (c == '\'')
            {
                escaped += "'\"'\"'";
            }
            else
            {
                escaped += c;
            }
        }
        escaped += "'";
        return escaped;
#endif
    }

    bool SaneSlangCompiler::isSlangAvailable()
    {
#ifdef _WIN32
        FILE* pipe = _popen("slangc -v 2>NUL", "r");
        if (pipe != nullptr)
        {
            return _pclose(pipe) == 0;
        }
#else
        FILE* pipe = popen("slangc -v 2>/dev/null", "r");
        if (pipe != nullptr)
        {
            return WEXITSTATUS(pclose(pipe)) == 0;
        }
#endif
        return false;
    }

    void SaneSlangCompiler::collectDependencies(
        const std::filesystem::path&        filePath,
        std::set<std::filesystem::path>&    visited,
        std::vector<std::filesystem::path>& dependencies)
    {
        std::filesystem::path canonicalPath = std::filesystem::canonical(filePath);

        if (visited.contains(canonicalPath))
        {
            return;
        }

        visited.insert(canonicalPath);
        dependencies.push_back(canonicalPath);

        auto                  includes   = extractIncludes(filePath);
        std::filesystem::path currentDir = filePath.parent_path();

        for (const auto& includePath : includes)
        {
            auto resolvedPath = resolveIncludePath(includePath, currentDir);
            if (!resolvedPath.empty() && std::filesystem::exists(resolvedPath))
            {
                collectDependencies(resolvedPath, visited, dependencies);
            }
        }
    }

    std::vector<std::string> SaneSlangCompiler::extractIncludes(const std::filesystem::path& filePath)
    {
        std::vector<std::string> includes;
        std::ifstream            file(filePath);

        if (!file)
        {
            return includes;
        }

        std::string line;
        while (std::getline(file, line))
        {
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            if (line.starts_with("#include"))
            {
                size_t quoteStart = line.find('"');
                size_t angleStart = line.find('<');

                if (quoteStart != std::string::npos)
                {
                    size_t quoteEnd = line.find('"', quoteStart + 1);
                    if (quoteEnd != std::string::npos)
                    {
                        includes.push_back(line.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
                    }
                }
                else if (angleStart != std::string::npos)
                {
                    size_t angleEnd = line.find('>', angleStart + 1);
                    if (angleEnd != std::string::npos)
                    {
                        includes.push_back(line.substr(angleStart + 1, angleEnd - angleStart - 1));
                    }
                }
            }
        }

        return includes;
    }

    std::filesystem::path
    SaneSlangCompiler::resolveIncludePath(const std::string& includePath, const std::filesystem::path& currentDir)
    {
        std::filesystem::path relativePath = currentDir / includePath;
        if (std::filesystem::exists(relativePath))
        {
            return relativePath;
        }

        for (const auto& searchPath : this->search_paths)
        {
            std::filesystem::path candidatePath = searchPath / includePath;
            if (std::filesystem::exists(candidatePath))
            {
                return candidatePath;
            }
        }

        return {}; // Not found
    }
} // namespace gfx