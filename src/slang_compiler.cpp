#include "slang_compiler.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <slang-com-ptr.h>
#include <slang.h>
#include <vector>

namespace cfi
{

    SaneSlangCompiler::SaneSlangCompiler()
    {
        // Initialize Global Session
        {
            const SlangGlobalSessionDesc globalSessionDescriptor {};

            const SlangResult result = slang::createGlobalSession(&globalSessionDescriptor, &this->global_session);
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

            const SlangResult result = this->global_session->createSession(slangSessionDescriptor, &this->session);
            assert::critical(result == 0, "Failed to create Slang Session with error {}", result);
        }
    }

    SaneSlangCompiler::~SaneSlangCompiler() = default;

    SaneSlangCompiler::CompileResult SaneSlangCompiler::compile(const std::filesystem::path& path) const
    {
        slang::IModule*                                        module = this->loadModule(path.generic_string());
        const std::optional<Slang::ComPtr<slang::IEntryPoint>> maybeFragmentEntry =
            this->tryFindEntryPoint(module, "fragmentMain");
        const std::optional<Slang::ComPtr<slang::IEntryPoint>> maybeVertexEntry =
            this->tryFindEntryPoint(module, "vertexMain");
        const std::optional<Slang::ComPtr<slang::IEntryPoint>> maybeComputeEntry =
            this->tryFindEntryPoint(module, "computeMain");

        auto tryComposeEntrypoint =
            [&](const std::optional<Slang::ComPtr<slang::IEntryPoint>>& maybeEntryPoint) -> std::vector<u32>
        {
            if (!maybeEntryPoint.has_value())
            {
                return {};
            }

            const Slang::ComPtr<slang::IComponentType> composedProgram =
                this->composeProgram(&*module, &**maybeEntryPoint);
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
            .dependent_files {this->getDependencies(module)}};
    }

    slang::IModule* SaneSlangCompiler::loadModule(const std::filesystem::path& modulePath) const
    {
        const std::string modulePathString = modulePath.generic_string();

        Slang::ComPtr<slang::IBlob> moduleBlob;
        log::trace("loading modile |{}|", modulePathString);
        slang::IModule* maybeModule = this->session->loadModule(modulePathString.c_str(), moduleBlob.writeRef());

        if (moduleBlob != nullptr)
        {
            std::string_view errorMessage {
                static_cast<const char*>(moduleBlob->getBufferPointer()), moduleBlob->getBufferSize()};

            if (!errorMessage.empty())
            {
                log::warn("Slang Compilation Output: \n{}", errorMessage);
            }
        }

        if (maybeModule == nullptr)
        {
            panic("Failed to load module @ {}", modulePath.string());
        }

        return maybeModule;
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
    std::vector<std::filesystem::path> SaneSlangCompiler::getDependencies(slang::IModule* module) const
    {
        std::vector<std::filesystem::path> result {};
        result.resize(static_cast<usize>(module->getDependencyFileCount()));

        for (usize i = 0; i < result.size(); ++i)
        {
            result[i] = module->getDependencyFilePath(static_cast<i32>(i));
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
                "Create Composed Program failed! Blob: {}", static_cast<const void*>(spirvBlob->getBufferPointer()));

            if (diagnosticBlob != nullptr)
            {
                const std::string_view error {
                    static_cast<const char*>(diagnosticBlob->getBufferPointer()), diagnosticBlob->getBufferSize()};

                panic("Failed to compile composed program: {}", error);
            }
        }

        return spirvBlob;
    }

} // namespace cfi