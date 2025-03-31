#pragma once

#include "util/util.hpp"
#include <filesystem>
#include <optional>
#include <slang.h>
#include <string_view>

/// Thanks to wakamatsu / Caio / Legend this lovely file
/// Tweaked to my style. but the majority is his code.

namespace cfi
{
    // TODO: get separate modules out

    class SaneSlangCompiler
    {
    public:
        struct CompileResult
        {
            std::vector<std::byte>             maybe_vertex_data;
            std::vector<std::byte>             maybe_fragment_data;
            std::vector<std::byte>             maybe_compute_data;
            std::vector<std::filesystem::path> dependent_files;
        };
    public:
        explicit SaneSlangCompiler();
        ~SaneSlangCompiler();

        [[nodiscard]] CompileResult compile(const std::filesystem::path&);

    private:
        static void releaseSlangObject(ISlangUnknown* object)
        {
            object->release();
        }
        template<typename T>
        using SlangUniquePtr = std::unique_ptr<T, decltype(&releaseSlangObject)>;

        std::vector<std::string> lifetime_extender;

        SlangUniquePtr<slang::IGlobalSession> global_session {nullptr, releaseSlangObject};
        slang::ISession*                      session;

        std::vector<std::filesystem::path> search_paths = {util::getCanonicalPathOfShaderFile("src/gfx/shader_common")};

        [[nodiscard]] SlangUniquePtr<slang::IModule>                    loadModule(const std::filesystem::path&);
        [[nodiscard]] std::optional<SlangUniquePtr<slang::IEntryPoint>> tryFindEntryPoint(slang::IModule*, const char*);
        [[nodiscard]] std::vector<std::filesystem::path>                getDependencies(slang::IModule*);
        [[nodiscard]] SlangUniquePtr<slang::IComponentType> composeProgram(slang::IModule*, slang::IEntryPoint*);
        [[nodiscard]] SlangUniquePtr<slang::IBlob>          compileComposedProgram(slang::IComponentType*);
    };

} // namespace cfi
