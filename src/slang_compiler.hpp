#pragma once

#include "util/util.hpp"
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
        };
    public:
        explicit SaneSlangCompiler();
        ~SaneSlangCompiler();

        [[nodiscard]] CompileResult compile(const std::filesystem::path&) const;

    private:

        std::vector<std::string> lifetime_extender;

        slang::IGlobalSession* global_session;
        slang::ISession*       session;

        std::vector<std::filesystem::path> search_paths = {util::getCanonicalPathOfShaderFile("src/gfx/shader_common")};

        [[nodiscard]] slang::IModule* loadModule(const std::filesystem::path&) const;
        [[nodiscard]] std::optional<Slang::ComPtr<slang::IEntryPoint>>
                                                           tryFindEntryPoint(slang::IModule*, const char*) const;
        [[nodiscard]] std::vector<std::filesystem::path>   getDependencies(slang::IModule*) const;
        [[nodiscard]] Slang::ComPtr<slang::IComponentType> composeProgram(slang::IModule*, slang::IEntryPoint*) const;
        [[nodiscard]] Slang::ComPtr<slang::IBlob>          compileComposedProgram(slang::IComponentType*) const;
    };

} // namespace cfi
