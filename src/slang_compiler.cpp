#include "slang_compiler.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <expected>
#include <filesystem>
#include <fmt/format.h>
#include <slang-com-ptr.h>
#include <slang.h>
#include <vector>

namespace cfi
{

    SaneSlangCompiler::SaneSlangCompiler()
        : unique_filename_integer {0}
    {
        // Initialize Global Session
        {
            const SlangGlobalSessionDesc globalSessionDescriptor {};

            const SlangResult result =
                slang::createGlobalSession(&globalSessionDescriptor, this->global_session.writeRef());
            assert::critical(result == 0, "Failed to create Slang Global Session with error {}", result);
        }

        // Initialize Session
        {
            std::vector<const char*> cStringPaths {};
            for (const std::filesystem::path& path : this->search_paths)
            {
                this->lifetime_extender.push_back(path.generic_string());

                cStringPaths.push_back(this->lifetime_extender.back().c_str());
            }

            slang::TargetDesc target {
                .format {SlangCompileTarget::SLANG_SPIRV},
                .profile {this->global_session->findProfile("spirv_1_5")},
                .flags {0},
            };

            slang::SessionDesc slangSessionDescriptor {};
            slangSessionDescriptor.searchPaths     = cStringPaths.data();
            slangSessionDescriptor.searchPathCount = static_cast<int>(cStringPaths.size());
            slangSessionDescriptor.targets         = &target;
            slangSessionDescriptor.targetCount     = 1;

            std::vector<slang::CompilerOptionEntry> compileOptions {};

            compileOptions.push_back(slang::CompilerOptionEntry {
                .name {slang::CompilerOptionName::FloatingPointMode},
                .value {.intValue0 {SlangFloatingPointMode::SLANG_FLOATING_POINT_MODE_FAST}}});

            compileOptions.push_back(slang::CompilerOptionEntry {
                .name {slang::CompilerOptionName::Optimization},
                .value {.intValue0 {SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_MAXIMAL}}});

            compileOptions.push_back(slang::CompilerOptionEntry {
                .name {slang::CompilerOptionName::VulkanUseGLLayout}, .value {.intValue0 {1}}});

            compileOptions.push_back(slang::CompilerOptionEntry {
                .name {slang::CompilerOptionName::DebugInformation},
                .value {.intValue0 {SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_MAXIMAL}}});

            slangSessionDescriptor.compilerOptionEntryCount = static_cast<u32>(compileOptions.size());
            slangSessionDescriptor.compilerOptionEntries    = compileOptions.data();

            const SlangResult result =
                this->global_session->createSession(slangSessionDescriptor, this->session.writeRef());
            assert::critical(result == 0, "Failed to create Slang Session with error {}", result);
        }
    }

    SaneSlangCompiler::~SaneSlangCompiler() = default;

    std::expected<SaneSlangCompiler::CompileResult, std::string> SaneSlangCompiler::compile(std::filesystem::path path)
    {
        auto [maybeModule, maybeCompileMessage] = this->loadModule(path.generic_string());

        if (maybeModule == nullptr)
        {
            return std::unexpected(std::move(maybeCompileMessage));
        }

        const std::optional<Slang::ComPtr<slang::IEntryPoint>> maybeFragmentEntry =
            this->tryFindEntryPoint(maybeModule, "fragmentMain");
        const std::optional<Slang::ComPtr<slang::IEntryPoint>> maybeVertexEntry =
            this->tryFindEntryPoint(maybeModule, "vertexMain");
        const std::optional<Slang::ComPtr<slang::IEntryPoint>> maybeComputeEntry =
            this->tryFindEntryPoint(maybeModule, "computeMain");

        auto tryComposeEntrypoint =
            [&](const std::optional<Slang::ComPtr<slang::IEntryPoint>>& maybeEntryPoint) -> std::vector<u32>
        {
            if (!maybeEntryPoint.has_value())
            {
                return {};
            }

            const Slang::ComPtr<slang::IComponentType> composedProgram =
                this->composeProgram(maybeModule, &**maybeEntryPoint);
            const Slang::ComPtr<slang::IBlob> spirvBlob = this->compileComposedProgram(composedProgram.get());

            const usize outputSize = spirvBlob->getBufferSize();

            assert::critical(
                outputSize % 4 == 0, "Returned spirv was of size {} which is not divisble by 4", outputSize);

            std::vector<u32> data {};
            data.resize(outputSize / 4);

            std::memcpy(data.data(), spirvBlob->getBufferPointer(), spirvBlob->getBufferSize());

            return data;
        };

        return CompileResult {
            .maybe_vertex_data {tryComposeEntrypoint(maybeVertexEntry)},
            .maybe_fragment_data {tryComposeEntrypoint(maybeFragmentEntry)},
            .maybe_compute_data {tryComposeEntrypoint(maybeComputeEntry)},
            .dependent_files {this->getDependencies(maybeModule, std::move(path))},
            .maybe_warnings {std::move(maybeCompileMessage)}};
    }

    std::pair<slang::IModule*, std::string> SaneSlangCompiler::loadModule(const std::filesystem::path& modulePath)
    {
        const std::string modulePathString = modulePath.generic_string();

        Slang::ComPtr<slang::IBlob> moduleBlob;
        // slang::IModule* maybeModule = this->session->loadModule(modulePathString.c_str(), moduleBlob.writeRef());

        std::vector<std::byte> entireFile = util::loadEntireFileFromPath(modulePath);

        std::string str {};
        str.resize(entireFile.size());

        std::memcpy(str.data(), entireFile.data(), str.size());

        const std::string uniqueFileName =
            std::format("UNIQUE_FILE_LOADED_IDENTIFIER{} {}", this->unique_filename_integer++, modulePathString);

        slang::IModule* maybeModule = this->session->loadModuleFromSourceString(
            uniqueFileName.c_str(), uniqueFileName.c_str(), str.data(), moduleBlob.writeRef());

        // this->session->loadModuleFromSource(const char *moduleName, const char *path, slang::IBlob *source)
        // this->session->loadModuleFromSourceString(const char* moduleName, const char* path, const char* string)

        std::string compilerOutput {};

        if (moduleBlob != nullptr)
        {
            std::string_view errorMessage {
                static_cast<const char*>(moduleBlob->getBufferPointer()), moduleBlob->getBufferSize()};

            compilerOutput = errorMessage;
        }

        return {maybeModule, std::move(compilerOutput)};
    }

    std::optional<Slang::ComPtr<slang::IEntryPoint>>
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    SaneSlangCompiler::tryFindEntryPoint(slang::IModule* module, const char* entryPointName) const
    {
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        std::ignore = module->findEntryPointByName(entryPointName, entryPoint.writeRef());

        if (entryPoint == nullptr)
        {
            return std::nullopt;
        }
        else
        {
            return entryPoint;
        }
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    std::vector<std::filesystem::path>
    SaneSlangCompiler::getDependencies(slang::IModule* module, std::filesystem::path selfPath) const
    {
        std::vector<std::filesystem::path> result {};

        result.push_back(std::move(selfPath));

        const i32 deps = module->getDependencyFileCount();

        for (i32 i = 0; i < deps; ++i)
        {
            std::string_view p = module->getDependencyFilePath(i);

            if (p.contains("UNIQUE_FILE_LOADED_IDENTIFIER"))
            {
                continue;
            }

            result.push_back(std::filesystem::path {p});
        }

        return result;
    }

    Slang::ComPtr<slang::IComponentType>
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    SaneSlangCompiler::composeProgram(slang::IModule* module, slang::IEntryPoint* entryPoint) const
    {
        Slang::ComPtr<slang::IBlob> diagnosticBlob;

        std::vector<slang::IComponentType*> components {};
        components.push_back(module);
        components.push_back(entryPoint);

        Slang::ComPtr<slang::IComponentType> composedProgram;

        if (session->createCompositeComponentType(
                components.data(),
                static_cast<SlangInt>(components.size()),
                composedProgram.writeRef(),
                diagnosticBlob.writeRef())
            != SLANG_OK)
        {
            const std::string_view error {
                static_cast<const char*>(diagnosticBlob->getBufferPointer()), diagnosticBlob->getBufferSize()};

            panic("Failed to compose program: \n{}", error);
        }

        return composedProgram;
    }
    Slang::ComPtr<slang::IBlob>
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    SaneSlangCompiler::compileComposedProgram(slang::IComponentType* composedProgram) const
    {
        Slang::ComPtr<slang::IBlob> spirvBlob      = nullptr;
        Slang::ComPtr<slang::IBlob> diagnosticBlob = nullptr;

        const SlangResult result =
            composedProgram->getEntryPointCode(0, 0, spirvBlob.writeRef(), diagnosticBlob.writeRef());

        if (result != SLANG_OK)
        {
            log::error(
                "Create Composed Program failed! | Spirv Blob: {} | Diagnostic Blob: {}",
                static_cast<const void*>(spirvBlob.get()),
                static_cast<const void*>(diagnosticBlob.get()));

            if (diagnosticBlob != nullptr)
            {
                const std::string_view error {
                    static_cast<const char*>(diagnosticBlob->getBufferPointer()), diagnosticBlob->getBufferSize()};

#warning make recoverable
                panic("Failed to compile composed program: {}", error);
            }
        }

        return spirvBlob;
    }

} // namespace cfi