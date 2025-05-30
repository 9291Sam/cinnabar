#pragma once

#include "util/util.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define popen  _popen
#define pclose _pclose
#undef max
#undef MemoryBarrier
#undef min
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace cfi
{
    class SaneSlangCompiler
    {
    public:
        struct CompileResult
        {
            std::vector<u32>                   maybe_vertex_data;
            std::vector<u32>                   maybe_fragment_data;
            std::vector<u32>                   maybe_compute_data;
            std::vector<std::filesystem::path> dependent_files;
            std::string                        maybe_warnings;
        };

    public:
        explicit SaneSlangCompiler();
        explicit SaneSlangCompiler(const std::vector<std::filesystem::path>& search_paths);
        ~SaneSlangCompiler();

        SaneSlangCompiler(const SaneSlangCompiler&)             = delete;
        SaneSlangCompiler(SaneSlangCompiler&&)                  = default;
        SaneSlangCompiler& operator= (const SaneSlangCompiler&) = delete;
        SaneSlangCompiler& operator= (SaneSlangCompiler&&)      = default;

        // Main API - maintains compatibility
        [[nodiscard]] std::expected<CompileResult, std::string> compile(std::filesystem::path path);

        // Search path management
        void addSearchPath(const std::filesystem::path& path);
        void setSearchPaths(const std::vector<std::filesystem::path>& paths);
        [[nodiscard]] const std::vector<std::filesystem::path>& getSearchPaths() const;

    private:
        std::filesystem::path              temp_dir_;
        std::vector<std::filesystem::path> search_paths_;
        static std::atomic<int>            temp_counter_;

        // Core compilation
        [[nodiscard]] std::expected<std::vector<u32>, std::string> compileEntryPoint(
            const std::filesystem::path& source_path, const std::string& entry_point, const std::string& stage);

        // Utility functions
        [[nodiscard]] std::filesystem::path createTempFile(const std::string& suffix);
        [[nodiscard]] bool hasEntryPoint(const std::filesystem::path& source_path, const std::string& entry_point);
        [[nodiscard]] std::expected<std::vector<u32>, std::string>
                                                              loadSpirvFromFile(const std::filesystem::path& file_path);
        [[nodiscard]] std::expected<std::string, std::string> executeSlangc(const std::vector<std::string>& args);

        static std::string escapeArg(const std::string& arg);
        static bool        checkSlangcAvailable();
    };

    // Static member definition
    inline std::atomic<int> SaneSlangCompiler::temp_counter_ {0};

    // Implementation
    inline SaneSlangCompiler::SaneSlangCompiler()
        : SaneSlangCompiler({})
    {}

    inline SaneSlangCompiler::SaneSlangCompiler(const std::vector<std::filesystem::path>& search_paths)
        : search_paths_(search_paths)
    {
        // Check if slangc is available during construction
        if (!checkSlangcAvailable())
        {
            throw std::runtime_error("slangc executable not found in PATH. Please install the Slang shader compiler.");
        }

        // Create unique temporary directory
        temp_dir_ = std::filesystem::temp_directory_path()
                  / ("slang_compiler_" + std::to_string(reinterpret_cast<uintptr_t>(this)));

        std::error_code ec;
        std::filesystem::create_directories(temp_dir_, ec);
        if (ec)
        {
            throw std::runtime_error("Failed to create temporary directory: " + ec.message());
        }
    }

    inline SaneSlangCompiler::~SaneSlangCompiler()
    {
        // Clean up temporary directory
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
        // Don't throw from destructor, just ignore cleanup errors
    }

    inline void SaneSlangCompiler::addSearchPath(const std::filesystem::path& path)
    {
        search_paths_.push_back(path);
    }

    inline void SaneSlangCompiler::setSearchPaths(const std::vector<std::filesystem::path>& paths)
    {
        search_paths_ = paths;
    }

    inline const std::vector<std::filesystem::path>& SaneSlangCompiler::getSearchPaths() const
    {
        return search_paths_;
    }

    inline std::expected<SaneSlangCompiler::CompileResult, std::string>
    SaneSlangCompiler::compile(std::filesystem::path path)
    {
        if (!std::filesystem::exists(path))
        {
            return std::unexpected("Source file does not exist: " + path.string());
        }

        CompileResult result {};
        std::string   all_warnings;

        // Helper lambda to attempt compilation of an entry point
        auto tryCompileEntry = [&](const std::string& entry_name,
                                   const std::string& stage,
                                   std::vector<u32>&  output_data) -> std::expected<void, std::string>
        {
            if (!hasEntryPoint(path, entry_name))
            {
                return {}; // Entry point doesn't exist, that's okay
            }

            auto spirv_result = compileEntryPoint(path, entry_name, stage);
            if (!spirv_result)
            {
                return std::unexpected(spirv_result.error());
            }

            output_data = std::move(*spirv_result);
            return {};
        };

        // Try to compile each shader stage
        if (auto vertex_result = tryCompileEntry("vertexMain", "vertex", result.maybe_vertex_data); !vertex_result)
        {
            return std::unexpected("Vertex shader error: " + vertex_result.error());
        }

        if (auto fragment_result = tryCompileEntry("fragmentMain", "fragment", result.maybe_fragment_data);
            !fragment_result)
        {
            return std::unexpected("Fragment shader error: " + fragment_result.error());
        }

        if (auto compute_result = tryCompileEntry("computeMain", "compute", result.maybe_compute_data); !compute_result)
        {
            return std::unexpected("Compute shader error: " + compute_result.error());
        }

        // Set dependencies (just the source file for now)
        result.dependent_files.push_back(std::move(path));
        result.maybe_warnings = std::move(all_warnings);

        return result;
    }

    inline std::expected<std::vector<u32>, std::string> SaneSlangCompiler::compileEntryPoint(
        const std::filesystem::path& source_path, const std::string& entry_point, const std::string& stage)
    {
        // Create temporary output file
        auto output_path = createTempFile(".spv");

        // Build slangc arguments
        std::vector<std::string> args {
            "-target",
            "spirv",
            "-stage",
            stage,
            "-entry",
            entry_point,
            "-o",
            output_path.string(),
            "-O3",                // Maximum optimization
            "-fvk-use-gl-layout", // Vulkan GL layout
        };

        // Add search paths using -I flags
        for (const auto& search_path : search_paths_)
        {
            args.push_back("-I");
            args.push_back(search_path.string());
        }

        // Add source file at the end
        args.push_back(source_path.string());

        // Execute slangc
        auto exec_result = executeSlangc(args);
        if (!exec_result)
        {
            // Clean up on failure
            std::error_code ec;
            std::filesystem::remove(output_path, ec);
            return std::unexpected(exec_result.error());
        }

        // Load the compiled SPIRV
        auto spirv_result = loadSpirvFromFile(output_path);

        // Always clean up temporary file
        std::error_code ec;
        std::filesystem::remove(output_path, ec);

        return spirv_result;
    }

    inline std::filesystem::path SaneSlangCompiler::createTempFile(const std::string& suffix)
    {
        auto filename = "temp_" + std::to_string(temp_counter_++) + suffix;
        return temp_dir_ / filename;
    }

    inline bool
    SaneSlangCompiler::hasEntryPoint(const std::filesystem::path& source_path, const std::string& entry_point)
    {
        std::ifstream file(source_path);
        if (!file)
        {
            return false;
        }

        std::string content {(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()};

        // Look for function definition pattern: "void entryPointName(" or similar
        std::string pattern1 = "void " + entry_point + "(";
        std::string pattern2 = entry_point + "(";

        return content.find(pattern1) != std::string::npos || content.find(pattern2) != std::string::npos;
    }

    inline std::expected<std::vector<u32>, std::string>
    SaneSlangCompiler::loadSpirvFromFile(const std::filesystem::path& file_path)
    {
        std::ifstream file(file_path, std::ios::binary);
        if (!file)
        {
            return std::unexpected("Failed to open compiled SPIRV file: " + file_path.string());
        }

        // Get file size
        file.seekg(0, std::ios::end);
        auto file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (file_size <= 0)
        {
            return std::unexpected("Empty or invalid SPIRV file: " + file_path.string());
        }

        if (file_size % 4 != 0)
        {
            return std::unexpected("SPIRV file size (" + std::to_string(file_size) + ") is not divisible by 4");
        }

        // Read SPIRV data as 32-bit words
        std::vector<u32> spirv_data(file_size / 4);
        file.read(reinterpret_cast<char*>(spirv_data.data()), file_size);

        if (!file)
        {
            return std::unexpected("Failed to read SPIRV data from: " + file_path.string());
        }

        // Basic SPIRV validation - check magic number
        if (!spirv_data.empty() && spirv_data[0] != 0x07230203)
        {
            return std::unexpected("Invalid SPIRV magic number in: " + file_path.string());
        }

        return spirv_data;
    }

    inline std::expected<std::string, std::string>
    SaneSlangCompiler::executeSlangc(const std::vector<std::string>& args)
    {
        // Build command string
        std::string command = "slangc";
        for (const auto& arg : args)
        {
            command += " " + escapeArg(arg);
        }

        // Execute command and capture output
        std::array<char, 256> buffer {};
        std::string           result;

#ifdef _WIN32
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif

        if (!pipe)
        {
            return std::unexpected("Failed to execute slangc command");
        }

        // Read command output
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }

        // Get exit code
#ifdef _WIN32
        int exit_code = _pclose(pipe.release());
#else
        int exit_code = pclose(pipe.release());
        exit_code     = WEXITSTATUS(exit_code);
#endif

        if (exit_code != 0)
        {
            std::string error_msg = "slangc failed (exit code " + std::to_string(exit_code) + ")";
            if (!result.empty())
            {
                error_msg += ":\n" + result;
            }
            return std::unexpected(error_msg);
        }

        return result;
    }

    inline std::string SaneSlangCompiler::escapeArg(const std::string& arg)
    {
        // If argument contains spaces or special characters, quote it
        bool needs_quotes = arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos
                         || arg.find('"') != std::string::npos;

        if (!needs_quotes)
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
        // Unix-style escaping with single quotes
        std::string escaped = "'";
        for (char c : arg)
        {
            if (c == '\'')
            {
                escaped += "'\"'\"'"; // End quote, escaped quote, start quote
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

    inline bool SaneSlangCompiler::checkSlangcAvailable()
    {
        // Try to run "slangc --version" to check if it's available
#ifdef _WIN32
        FILE* pipe = _popen("slangc -v 2>NUL", "r");
        if (pipe)
        {
            int exit_code = _pclose(pipe);
            return exit_code == 0;
        }
#else
        FILE* pipe = popen("slangc -v 2>/dev/null", "r");
        if (pipe)
        {
            int exit_code = pclose(pipe);
            return WEXITSTATUS(exit_code) == 0;
        }
#endif
        return false;
    }

} // namespace cfi