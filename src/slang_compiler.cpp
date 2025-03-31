#include "slang_compiler.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <slang.h>
#include <vector>

namespace cfi
{

    SaneSlangCompiler::SaneSlangCompiler()
        : session {nullptr}
    {
        // Initialize Global Session
        {
            const SlangGlobalSessionDesc globalSessionDescriptor {};

            slang::IGlobalSession* rawGlobalSessionPtr = nullptr;

            // Literally the dumbest API in all of existence, it requires a fucking T**. WHY
            const SlangResult result = slang::createGlobalSession(&globalSessionDescriptor, &rawGlobalSessionPtr);
            assert::critical(result == 0, "Failed to create Slang Global Session with error {}", result);
            assert::critical(rawGlobalSessionPtr != nullptr, "slang::createGlobalSession returned nullptr");
            this->global_session = {rawGlobalSessionPtr, SaneSlangCompiler::releaseSlangObject};
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

            const SlangResult result = this->global_session->createSession(slangSessionDescriptor, &this->session);
            assert::critical(result == 0, "Failed to create Slang Session with error {}", result);
            assert::critical(this->session != nullptr, "SlangGlobalSession::createSession returned nullptr!");
        }
    }

    SaneSlangCompiler::~SaneSlangCompiler() = default;

    SaneSlangCompiler::CompileResult SaneSlangCompiler::compile(const std::filesystem::path& path)
    {
        const SlangUniquePtr<slang::IModule>                    module = this->loadModule(path.generic_string());
        const std::optional<SlangUniquePtr<slang::IEntryPoint>> maybeFragmentEntry =
            this->tryFindEntryPoint(module.get(), "fragmentMain");
        const std::optional<SlangUniquePtr<slang::IEntryPoint>> maybeVertexEntry =
            this->tryFindEntryPoint(module.get(), "vertexMain");
        const std::optional<SlangUniquePtr<slang::IEntryPoint>> maybeComputeEntry =
            this->tryFindEntryPoint(module.get(), "computeMain");

        auto tryComposeEntrypoint =
            [&](const std::optional<SlangUniquePtr<slang::IEntryPoint>>& maybeEntryPoint) -> std::vector<std::byte>
        {
            if (!maybeEntryPoint.has_value())
            {
                return {};
            }

            const SlangUniquePtr<slang::IComponentType> composedProgram =
                this->composeProgram(&*module, &**maybeEntryPoint);
            const SlangUniquePtr<slang::IBlob> spirvBlob = this->compileComposedProgram(composedProgram.get());

            const usize outputSize = spirvBlob->getBufferSize();
            assert::critical(
                outputSize % 4 == 0, "Returned spirv was of size {} which is not divisble by 4", outputSize);

            std::vector<std::byte> data {};
            data.resize(outputSize);

            std::memcpy(data.data(), spirvBlob->getBufferPointer(), data.size());

            return data;
        };

        return CompileResult {
            .maybe_vertex_data {tryComposeEntrypoint(maybeVertexEntry)},
            .maybe_fragment_data {tryComposeEntrypoint(maybeFragmentEntry)},
            .maybe_compute_data {tryComposeEntrypoint(maybeComputeEntry)},
            .dependent_files {this->getDependencies(module.get())}};
    }

    SaneSlangCompiler::SlangUniquePtr<slang::IModule>
    SaneSlangCompiler::loadModule(const std::filesystem::path& modulePath)
    {
        const std::string modulePathString = modulePath.generic_string();

        slang::IBlob*                  rawModuleBlobPtr = nullptr;
        SlangUniquePtr<slang::IModule> maybeModule {
            this->session->loadModule(modulePathString.c_str(), &rawModuleBlobPtr),
            SaneSlangCompiler::releaseSlangObject};
        SlangUniquePtr<slang::IBlob> moduleBlob = {rawModuleBlobPtr, SaneSlangCompiler::releaseSlangObject};

        if (maybeModule == nullptr)
        {
            std::string_view errorMessage {
                static_cast<const char*>(moduleBlob->getBufferPointer()), moduleBlob->getBufferSize()};

            panic("Failed to load module @ {}: {}", modulePath.string(), errorMessage);
        }
        return maybeModule;
    }

    std::optional<SaneSlangCompiler::SlangUniquePtr<slang::IEntryPoint>>
    SaneSlangCompiler::tryFindEntryPoint(slang::IModule* module, const char* entryPointName)
    {
        slang::IEntryPoint* entryPoint = nullptr;
        std::ignore                    = module->findEntryPointByName(entryPointName, &entryPoint);

        if (entryPoint == nullptr)
        {
            return std::nullopt;
        }
        else
        {
            return SlangUniquePtr<slang::IEntryPoint> {entryPoint, SaneSlangCompiler::releaseSlangObject};
        }
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    std::vector<std::filesystem::path> SaneSlangCompiler::getDependencies(slang::IModule* module)
    {
        std::vector<std::filesystem::path> result {};
        result.resize(static_cast<usize>(module->getDependencyFileCount()));

        for (usize i = 0; i < result.size(); ++i)
        {
            result[i] = module->getDependencyFilePath(static_cast<i32>(i));
        }

        return result;
    }

    SaneSlangCompiler::SlangUniquePtr<slang::IComponentType>
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    SaneSlangCompiler::composeProgram(slang::IModule* module, slang::IEntryPoint* entry_point)
    {
        SlangUniquePtr<slang::IBlob> diagnosticBlob {nullptr, SaneSlangCompiler::releaseSlangObject};
        slang::IBlob*                rawDiagnosticBlobPtr = diagnosticBlob.get();

        std::vector<slang::IComponentType*> components {};
        components.push_back(module);
        components.push_back(entry_point);

        slang::IComponentType* composedProgram = nullptr;

        if (session->createCompositeComponentType(
                components.data(), static_cast<SlangInt>(components.size()), &composedProgram, &rawDiagnosticBlobPtr)
            != SLANG_OK)
        {
            const std::string_view error {
                static_cast<const char*>(diagnosticBlob->getBufferPointer()), diagnosticBlob->getBufferSize()};

            panic("Failed to compose program: \n{}", error);
        }

        return SlangUniquePtr<slang::IComponentType>(composedProgram, SaneSlangCompiler::releaseSlangObject);
    }
    SaneSlangCompiler::SlangUniquePtr<slang::IBlob>
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    SaneSlangCompiler::compileComposedProgram(slang::IComponentType* composedProgram)
    {
        SlangUniquePtr<slang::IBlob> diagnosticBlob {nullptr, SaneSlangCompiler::releaseSlangObject};
        slang::IBlob*                rawDiagnosticBlobPtr = diagnosticBlob.get();

        slang::IBlob* rawSpirvBlob = nullptr;

        if (composedProgram->getEntryPointCode(0, 0, &rawSpirvBlob, &rawDiagnosticBlobPtr) != SLANG_OK)
        {
            const std::string_view error {
                static_cast<const char*>(diagnosticBlob->getBufferPointer()), diagnosticBlob->getBufferSize()};

            panic("Failed to compile composed program: {}", error);
        }

        return SlangUniquePtr<slang::IBlob> {rawSpirvBlob, SaneSlangCompiler::releaseSlangObject};
    }

} // namespace cfi