#pragma once

#include "util/util.hpp"
#include <atomic>
#include <expected>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

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
        };

    public:
        explicit SaneSlangCompiler();
        ~SaneSlangCompiler();

        SaneSlangCompiler(const SaneSlangCompiler&)             = delete;
        SaneSlangCompiler(SaneSlangCompiler&&)                  = default;
        SaneSlangCompiler& operator= (const SaneSlangCompiler&) = delete;
        SaneSlangCompiler& operator= (SaneSlangCompiler&&)      = default;

        [[nodiscard]]
        std::pair<std::optional<SaneSlangCompiler::CompileResult>, std::string> compile(std::filesystem::path path);


    private:
        std::filesystem::path              temporary_dir;
        std::vector<std::filesystem::path> search_paths = {util::getCanonicalPathOfShaderFile("src/gfx/shader_common")};
        static std::atomic<usize>          monotonic_counter;

        [[nodiscard]] std::pair<std::optional<std::vector<u32>>, std::string>
        compileEntryPoint(const std::filesystem::path&, const std::string&, const std::string&);

        [[nodiscard]] std::filesystem::path createTempFile(std::string_view);

        [[nodiscard]] static bool             isSlangAvailable();
        [[nodiscard]] static std::string      escapeArg(const std::string& arg);
        [[nodiscard]] static std::vector<u32> loadSpirvFromFile(const std::filesystem::path&);
        [[nodiscard]] static bool             hasEntryPoint(const std::filesystem::path&, const std::string&);
        /// returns false on failure
        [[nodiscard]] static std::pair<bool, std::string> executeSlang(const std::vector<std::string>&);

        void collectDependencies(
            const std::filesystem::path&        filePath,
            std::set<std::filesystem::path>&    visited,
            std::vector<std::filesystem::path>& dependencies);
        [[nodiscard]] static std::vector<std::string> extractIncludes(const std::filesystem::path& filePath);
        [[nodiscard]] std::filesystem::path
        resolveIncludePath(const std::string& includePath, const std::filesystem::path& currentDir);
    };

} // namespace cfi