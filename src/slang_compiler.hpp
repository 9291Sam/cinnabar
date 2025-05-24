#pragma once

#include "util/util.hpp"
#include <expected>
#include <filesystem>
#include <optional>
#include <slang-com-ptr.h>
#include <slang.h>
#include <string_view>

/// Thanks to wakamatsu / Caio / Legend this lovely file
/// Tweaked to my style. but the majority is his code.

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
        ~SaneSlangCompiler();

        SaneSlangCompiler(const SaneSlangCompiler&)             = delete;
        SaneSlangCompiler(SaneSlangCompiler&&)                  = delete;
        SaneSlangCompiler& operator= (const SaneSlangCompiler&) = delete;
        SaneSlangCompiler& operator= (SaneSlangCompiler&&)      = delete;

        // Compile result or error string
        [[nodiscard]] std::expected<CompileResult, std::string> compile(std::filesystem::path);

    private:

        std::vector<std::string> lifetime_extender;

        Slang::ComPtr<slang::IGlobalSession> global_session;
        Slang::ComPtr<slang::ISession>       session;

        void loadNewSession();

        std::vector<std::filesystem::path> search_paths = {util::getCanonicalPathOfShaderFile("src/gfx/shader_common")};

        // Module or an error string
        [[nodiscard]] std::pair<slang::IModule*, std::string> loadModule(const std::filesystem::path&);
        [[nodiscard]] std::optional<Slang::ComPtr<slang::IEntryPoint>>
        tryFindEntryPoint(slang::IModule*, const char*) const;
        [[nodiscard]] std::vector<std::filesystem::path>
        getDependencies(slang::IModule*, std::filesystem::path selfPath) const;
        [[nodiscard]] Slang::ComPtr<slang::IComponentType> composeProgram(slang::IModule*, slang::IEntryPoint*) const;
        [[nodiscard]] std::pair<Slang::ComPtr<slang::IBlob>, std::string>
        compileComposedProgram(slang::IComponentType*) const;
    };

} // namespace cfi
