#include "pipeline_manager.hpp"
#include "device.hpp"
#include "slang_compiler.hpp"
#include "util/logger.hpp"
#include "util/threads.hpp"
#include "util/util.hpp"
#include <algorithm>
#include <deque>
#include <expected>
#include <filesystem>
#include <ranges>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <span>
#include <variant>
#include <vulkan/vulkan.hpp>

namespace gfx::core::vulkan
{
    static constexpr u32 MaxPipelines = 512;

    namespace
    {
        struct ShaderCommonIncluder : shaderc::CompileOptions::IncluderInterface
        {
            shaderc_include_result* GetInclude(
                const char*          requestedSource,
                shaderc_include_type type,
                const char*          requestingSource,
                size_t               includeDepth) override
            {
                assert::critical(
                    includeDepth < 1000,
                    "Include Infinite loop detected. |{}| is requesting |{}|",
                    requestingSource,
                    requestedSource);

                std::ignore = type;

                const std::filesystem::path searchPath = util::getCanonicalPathOfShaderFile("src/gfx/shader_common");

                assert::critical(std::filesystem::is_directory(searchPath), "oops");

                std::filesystem::path requestedPath = searchPath / requestedSource;
                requestedPath.make_preferred();

                this->dependent_files.push_back({requestedPath, std::filesystem::last_write_time(requestedPath)});

                const std::string foundSourceName = requestedPath.string();

                assert::warn(
                    std::filesystem::exists(requestedPath),
                    "Compilation of |{}| has requested |{}| which does not exist!",
                    requestingSource,
                    foundSourceName);

                assert::warn(
                    std::filesystem::is_regular_file(requestedPath),
                    "Compilation of |{}| has requested |{}| which is not a regular file!",
                    requestingSource,
                    foundSourceName);

                const std::vector<std::byte> foundSourceContent = util::loadEntireFileFromPath(requestedPath);

                std::string sourceNameOwned {foundSourceName};
                std::string sourceContentOwned {};

                sourceContentOwned.resize(foundSourceContent.size());
                std::memcpy(sourceContentOwned.data(), foundSourceContent.data(), foundSourceContent.size());

                shaderc_include_result localNewResult {
                    .source_name {sourceNameOwned.data()},
                    .source_name_length {sourceNameOwned.size()},
                    .content {sourceContentOwned.data()},
                    .content_length {sourceContentOwned.size()},
                    .user_data {this}};

                this->lifetime_extension_storage.push_back(std::move(sourceNameOwned));
                this->lifetime_extension_storage.push_back(std::move(sourceContentOwned));

                // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                return new shaderc_include_result {localNewResult};
            }

            void ReleaseInclude(shaderc_include_result* oldResult) override
            {
                // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                delete oldResult;
            }

            ~ShaderCommonIncluder() override = default;

            std::deque<std::string> lifetime_extension_storage;

            std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> dependent_files;
        };

    } // namespace

    PipelineManager::PipelineManager(const Device& device_, vk::PipelineLayout bindlessPipelineLayout)
        : device {device_.getDevice()}
        , pipeline_cache {this->device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo {
              .sType {vk::StructureType::ePipelineCacheCreateInfo},
              .pNext {nullptr},
              .flags {},
              .initialDataSize {0},
              .pInitialData {nullptr}})}
        , bindless_pipeline_layout {bindlessPipelineLayout}
        , critical_section {util::Mutex {CriticalSection {
              .pipeline_handle_allocator {util::OpaqueHandleAllocator<Pipeline> {MaxPipelines}},
              .pipeline_storage {MaxPipelines}}}}
    {}

    PipelineManager::~PipelineManager() = default;

    PipelineManager::Pipeline PipelineManager::createPipeline(GraphicsPipelineDescriptor graphicsDescriptor) const
    {
        std::expected<PipelineManager::PipelineInternalStorage, std::string> shouldBeInternalRepresentation =
            this->tryCompilePipeline(graphicsDescriptor);

        if (!shouldBeInternalRepresentation.has_value())
        {
            const std::string& errorString = shouldBeInternalRepresentation.error();
            std::string_view   errorView   = errorString;

            while (errorView.starts_with('\n'))
            {
                errorView.remove_prefix(1);
            }

            while (errorView.ends_with('\n'))
            {
                errorView.remove_suffix(1);
            }

            panic("Failed to compile pipeline on startup!\n{}", errorView);
        }

        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                Pipeline newPipelineHandle = criticalSection.pipeline_handle_allocator.allocateOrPanic();

                const u16 newHandleValue =
                    criticalSection.pipeline_handle_allocator.getValueOfHandle(newPipelineHandle);

                criticalSection.pipeline_storage[newHandleValue] = std::move(*shouldBeInternalRepresentation);

                return newPipelineHandle;
            });
    }

    PipelineManager::Pipeline PipelineManager::createPipeline(ComputePipelineDescriptor computeDescriptor) const
    {
        std::expected<PipelineManager::PipelineInternalStorage, std::string> shouldBeInternalRepresentation =
            this->tryCompilePipeline(computeDescriptor);

        if (!shouldBeInternalRepresentation.has_value())
        {
            const std::string& errorString = shouldBeInternalRepresentation.error();
            std::string_view   errorView   = errorString;

            while (errorView.starts_with('\n'))
            {
                errorView.remove_prefix(1);
            }

            while (errorView.ends_with('\n'))
            {
                errorView.remove_suffix(1);
            }

            panic("Failed to compile pipeline on startup!\n{}", errorView);
        }

        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                Pipeline newPipelineHandle = criticalSection.pipeline_handle_allocator.allocateOrPanic();

                const u16 newHandleValue =
                    criticalSection.pipeline_handle_allocator.getValueOfHandle(newPipelineHandle);

                criticalSection.pipeline_storage[newHandleValue] = std::move(*shouldBeInternalRepresentation);

                return newPipelineHandle;
            });
    }

    void PipelineManager::destroyPipeline(Pipeline p) const
    {
        this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                const u16 handleValue = criticalSection.pipeline_handle_allocator.getValueOfHandle(p);

                criticalSection.pipeline_storage[handleValue] = {};

                criticalSection.pipeline_handle_allocator.free(std::move(p));
            });
    }

    vk::Pipeline PipelineManager::getPipeline(const Pipeline& p) const
    {
        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                const u16 handleValue = criticalSection.pipeline_handle_allocator.getValueOfHandle(p);

                return *criticalSection.pipeline_storage[handleValue].pipeline;
            });
    }

    bool PipelineManager::couldAnyShadersReload() const
    {
        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                bool canAnyReload = false;

                criticalSection.pipeline_handle_allocator.iterateThroughAllocatedElements(
                    [&](const u16 handleValue)
                    {
                        const PipelineInternalStorage& pipelineStorage = criticalSection.pipeline_storage[handleValue];

                        for (const auto& [path, time] : pipelineStorage.dependent_files)
                        {
                            if (time != std::filesystem::last_write_time(path))
                            {
                                canAnyReload |= true;
                            }
                        }
                    });

                return canAnyReload;
            });
    }

    void PipelineManager::reloadShaders() const
    {
        this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                criticalSection.pipeline_handle_allocator.iterateThroughAllocatedElements(
                    [&](const u16 handleValue)
                    {
                        PipelineInternalStorage& pipelineStorage = criticalSection.pipeline_storage[handleValue];

                        bool shouldThisPipelineReload = std::ranges::any_of(
                            pipelineStorage.dependent_files,
                            [](const auto& pair)
                            {
                                const auto& [path, time] = pair;

                                return time != std::filesystem::last_write_time(path);
                            });

                        if (shouldThisPipelineReload)
                        {
                            auto [newInternalStorage, maybeErrorString] =
                                this->tryRecompilePipeline(std::move(pipelineStorage));

                            std::string_view pipelineName {};

                            if (GraphicsPipelineDescriptor* g =
                                    std::get_if<GraphicsPipelineDescriptor>(&newInternalStorage.descriptor))
                            {
                                pipelineName = g->name;
                            }
                            else if (
                                ComputePipelineDescriptor* c =
                                    std::get_if<ComputePipelineDescriptor>(&newInternalStorage.descriptor))
                            {
                                pipelineName = c->name;
                            }
                            else
                            {
                                panic("unhandled variant");
                            }

                            pipelineStorage = std::move(newInternalStorage);

                            if (maybeErrorString.has_value())
                            {
                                // This is a race, but I don't give a shit, I want my console empty
                                for (auto& [path, time] : pipelineStorage.dependent_files)
                                {
                                    time = std::filesystem::last_write_time(path);
                                }

                                log::error(
                                    "Dynamic Pipeline {} recompilation failure:\n{}", pipelineName, *maybeErrorString);
                            }
                            else
                            {
                                log::debug("Reloaded {}", pipelineName);
                            }
                        }
                    });
            });
    }

    namespace
    {
        std::pair<std::vector<char>, std::pair<std::filesystem::path, std::filesystem::file_time_type>>
        loadShaderFile(std::string_view requestedFile)
        {
            const std::filesystem::path canonicalPath = util::getCanonicalPathOfShaderFile(requestedFile);

            const std::vector<std::byte> rawSource = util::loadEntireFileFromPath(canonicalPath);
            std::vector<char>            result {};
            result.resize(rawSource.size());

            std::memcpy(result.data(), rawSource.data(), rawSource.size());

            return {result, std::make_pair(canonicalPath, std::filesystem::last_write_time(canonicalPath))};
        }
    } // namespace

    std::expected<PipelineManager::FormShaderModuleFromShaderSourceResult, std::string>
    PipelineManager::formShaderModuleFromShaderSource(
        std::span<const char> glslShaderSource, const std::string& filename, shaderc_shader_kind kind) const
    {
        assert::critical(
            !glslShaderSource.empty(),
            "Invalid GLSL Source of length {} with a back character of {}",
            glslShaderSource.size(),
            static_cast<u32>(glslShaderSource.back()));

        std::unique_ptr<ShaderCommonIncluder> includer {new ShaderCommonIncluder {}};
        ShaderCommonIncluder*                 rawIncluder = includer.get();

        shaderc::CompileOptions options {};
        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetTargetSpirv(shaderc_spirv_version_1_5);
        options.SetIncluder(std::move(includer));

        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetGenerateDebugInfo();

        shaderc::SpvCompilationResult compileResult = this->shader_compiler.CompileGlslToSpv(
            glslShaderSource.data(), glslShaderSource.size(), kind, filename.c_str(), options);

        if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            return std::unexpected(compileResult.GetErrorMessage());
        }

        std::span<const u32> compiledSPV {compileResult.cbegin(), compileResult.cend()};

        const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
            .sType {vk::StructureType::eShaderModuleCreateInfo},
            .pNext {nullptr},
            .flags {},
            .codeSize {compiledSPV.size_bytes()},
            .pCode {compiledSPV.data()},
        };

        return FormShaderModuleFromShaderSourceResult {
            .module {this->device.createShaderModuleUnique(shaderModuleCreateInfo)},
            .dependent_files {std::move(rawIncluder->dependent_files)}};
    }

    std::expected<PipelineManager::PipelineInternalStorage, std::string>
    PipelineManager::tryCompilePipeline(GraphicsPipelineDescriptor& descriptor) const
    {
        vk::UniqueShaderModule                                                         vertexShader;
        vk::UniqueShaderModule                                                         fragmentShader;
        std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> allDependentFiles {};

        if (!descriptor.vertex_shader_path.ends_with("slang"))
        {
            log::warn("loading legacy glsl file!");

            const auto [vertexSourceGLSL, vertexFileDependency]     = loadShaderFile(descriptor.vertex_shader_path);
            const auto [fragmentSourceGLSL, fragmentFileDependency] = loadShaderFile(descriptor.fragment_shader_path);

            std::expected<FormShaderModuleFromShaderSourceResult, std::string> vertexResult =
                this->formShaderModuleFromShaderSource(
                    vertexSourceGLSL, descriptor.vertex_shader_path, shaderc_vertex_shader);

            if (!vertexResult.has_value())
            {
                return std::unexpected(std::move(vertexResult.error()));
            }

            std::expected<FormShaderModuleFromShaderSourceResult, std::string> fragmentResult =
                this->formShaderModuleFromShaderSource(
                    fragmentSourceGLSL, descriptor.fragment_shader_path, shaderc_fragment_shader);

            if (!fragmentResult.has_value())
            {
                return std::unexpected(std::move(fragmentResult.error()));
            }

            allDependentFiles.reserve(
                vertexResult->dependent_files.size() + fragmentResult->dependent_files.size() + 2);

            for (const auto& d : vertexResult->dependent_files)
            {
                allDependentFiles.push_back(d);
            }

            for (const auto& d : fragmentResult->dependent_files)
            {
                allDependentFiles.push_back(d);
            }

            allDependentFiles.push_back(vertexFileDependency);
            allDependentFiles.push_back(fragmentFileDependency);

            vertexShader   = std::move(vertexResult->module);
            fragmentShader = std::move(fragmentResult->module);
        }
        else
        {
            assert::critical(descriptor.vertex_shader_path.ends_with("slang"), "Tried to compile a non slang file");

            auto foo =
                this->sane_slang_compiler.compile(util::getCanonicalPathOfShaderFile(descriptor.vertex_shader_path));

            // Vertex
            {
                std::span<const u32> compiledSPV {foo.maybe_vertex_data.cbegin(), foo.maybe_vertex_data.cend()};

                const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
                    .sType {vk::StructureType::eShaderModuleCreateInfo},
                    .pNext {nullptr},
                    .flags {},
                    .codeSize {compiledSPV.size_bytes()},
                    .pCode {compiledSPV.data()},
                };

                vertexShader = this->device.createShaderModuleUnique(shaderModuleCreateInfo);
            }

            // Fragment
            {
                std::span<const u32> compiledSPV {foo.maybe_fragment_data.cbegin(), foo.maybe_fragment_data.cend()};

                const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
                    .sType {vk::StructureType::eShaderModuleCreateInfo},
                    .pNext {nullptr},
                    .flags {},
                    .codeSize {compiledSPV.size_bytes()},
                    .pCode {compiledSPV.data()},
                };

                fragmentShader = this->device.createShaderModuleUnique(shaderModuleCreateInfo);
            }

            for (std::filesystem::path& p : foo.dependent_files)
            {
                std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(p);

                allDependentFiles.push_back({std::move(p), writeTime});
            }
        }

        const std::array<vk::PipelineShaderStageCreateInfo, 2> denseStages {
            vk::PipelineShaderStageCreateInfo {
                .sType {vk::StructureType::ePipelineShaderStageCreateInfo},
                .pNext {nullptr},
                .flags {},
                .stage {vk::ShaderStageFlagBits::eVertex},
                .module {*vertexShader},
                .pName {"main"},
                .pSpecializationInfo {},
            },
            vk::PipelineShaderStageCreateInfo {
                .sType {vk::StructureType::ePipelineShaderStageCreateInfo},
                .pNext {nullptr},
                .flags {},
                .stage {vk::ShaderStageFlagBits::eFragment},
                .module {*fragmentShader},
                .pName {"main"},
                .pSpecializationInfo {},
            }};

        const vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo {
            .sType {vk::StructureType::ePipelineVertexInputStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .vertexBindingDescriptionCount {0},
            .pVertexBindingDescriptions {nullptr},
            .vertexAttributeDescriptionCount {0},
            .pVertexAttributeDescriptions {nullptr},
        };

        const vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo {
            .sType {vk::StructureType::ePipelineInputAssemblyStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .topology {descriptor.topology},
            // Moltenvk doesn't support disabling this = https://github.com/KhronosGroup/MoltenVK/issues/1521
            .primitiveRestartEnable {vk::False},
        };

        const vk::Viewport nullDynamicViewport {};
        const vk::Rect2D   nullDynamicScissor {};

        const vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo {
            .sType {vk::StructureType::ePipelineViewportStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .viewportCount {1},
            .pViewports {&nullDynamicViewport},
            .scissorCount {1},
            .pScissors {&nullDynamicScissor},
        };

        const vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo {
            .sType {vk::StructureType::ePipelineRasterizationStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .depthClampEnable {vk::False},
            .rasterizerDiscardEnable {vk::False},
            .polygonMode {descriptor.polygon_mode},
            .cullMode {descriptor.cull_mode},
            .frontFace {descriptor.front_face},
            .depthBiasEnable {vk::False},
            .depthBiasConstantFactor {0.0},
            .depthBiasClamp {0.0},
            .depthBiasSlopeFactor {0.0},
            .lineWidth {1.0},
        };

        const vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo {
            .sType {vk::StructureType::ePipelineMultisampleStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .rasterizationSamples {vk::SampleCountFlagBits::e1},
            .sampleShadingEnable {vk::False},
            .minSampleShading {1.0},
            .pSampleMask {nullptr},
            .alphaToCoverageEnable {vk::False},
            .alphaToOneEnable {vk::False},
        };

        const vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo {
            .sType {vk::StructureType::ePipelineDepthStencilStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .depthTestEnable {descriptor.depth_test_enable},
            .depthWriteEnable {descriptor.depth_write_enable},
            .depthCompareOp {descriptor.depth_compare_op},
            .depthBoundsTestEnable {vk::False}, // TODO: expose?
            .stencilTestEnable {vk::False},
            .front {},
            .back {},
            .minDepthBounds {0.0}, // TODO: expose?
            .maxDepthBounds {1.0}, // TODO: expose?
        };

        const vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState {
            .blendEnable {descriptor.blend_enable},
            .srcColorBlendFactor {vk::BlendFactor::eSrcAlpha},
            .dstColorBlendFactor {vk::BlendFactor::eOneMinusSrcAlpha},
            .colorBlendOp {vk::BlendOp::eAdd},
            .srcAlphaBlendFactor {vk::BlendFactor::eOne},
            .dstAlphaBlendFactor {vk::BlendFactor::eZero},
            .alphaBlendOp {vk::BlendOp::eAdd},
            .colorWriteMask {
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA},
        };

        const vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo {
            .sType {vk::StructureType::ePipelineColorBlendStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .logicOpEnable {vk::False},
            .logicOp {vk::LogicOp::eCopy},
            .attachmentCount {1},
            .pAttachments {&pipelineColorBlendAttachmentState},
            .blendConstants {{0.0, 0.0, 0.0, 0.0}}, // TODO: expose?
        };

        const std::array pipelineDynamicStates {
            vk::DynamicState::eScissor,
            vk::DynamicState::eViewport,
        };

        const vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo {
            .sType {vk::StructureType::ePipelineDynamicStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .dynamicStateCount {static_cast<u32>(pipelineDynamicStates.size())},
            .pDynamicStates {pipelineDynamicStates.data()},
        };

        const vk::PipelineRenderingCreateInfo renderingInfo {
            .sType {vk::StructureType::ePipelineRenderingCreateInfo},
            .pNext {nullptr},
            .viewMask {0},
            .colorAttachmentCount {descriptor.color_format == vk::Format::eUndefined ? 0u : 1u},
            .pColorAttachmentFormats {&descriptor.color_format},
            .depthAttachmentFormat {descriptor.depth_format},
            .stencilAttachmentFormat {},
        };

        const vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo {
            .sType {vk::StructureType::eGraphicsPipelineCreateInfo},
            .pNext {&renderingInfo},
            .flags {},
            .stageCount {static_cast<u32>(denseStages.size())},
            .pStages {denseStages.data()},
            .pVertexInputState {&pipelineVertexInputStateCreateInfo},
            .pInputAssemblyState {&pipelineInputAssemblyStateCreateInfo},
            .pTessellationState {nullptr},
            .pViewportState {&pipelineViewportStateCreateInfo},
            .pRasterizationState {&pipelineRasterizationStateCreateInfo},
            .pMultisampleState {&pipelineMultisampleStateCreateInfo},
            .pDepthStencilState {&pipelineDepthStencilStateCreateInfo},
            .pColorBlendState {&pipelineColorBlendStateCreateInfo},
            .pDynamicState {&pipelineDynamicStateCreateInfo},
            .layout {this->bindless_pipeline_layout},
            .renderPass {nullptr},
            .subpass {0},
            .basePipelineHandle {nullptr},
            .basePipelineIndex {0},
        };

        auto [result, maybeUniquePipeline] =
            this->device.createGraphicsPipelineUnique(*this->pipeline_cache, graphicsPipelineCreateInfo);

        assert::critical(
            result == vk::Result::eSuccess, "Failed to create graphics pipeline | Error: {}", vk::to_string(result));

        return PipelineInternalStorage {
            .descriptor {std::move(descriptor)},
            .pipeline {std::move(maybeUniquePipeline)},
            .dependent_files {std::move(allDependentFiles)}};
    }

    std::expected<PipelineManager::PipelineInternalStorage, std::string>
    PipelineManager::tryCompilePipeline(ComputePipelineDescriptor& descriptor) const
    {
        const auto [computeSourceGLSL, computeFileDependency] = loadShaderFile(descriptor.compute_shader_path);

        std::expected<FormShaderModuleFromShaderSourceResult, std::string> computeResult =
            this->formShaderModuleFromShaderSource(
                computeSourceGLSL, descriptor.compute_shader_path, shaderc_compute_shader);

        if (!computeResult.has_value())
        {
            return std::unexpected(std::move(computeResult.error()));
        }

        std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> allDependentFiles {};
        allDependentFiles.reserve(computeResult->dependent_files.size() + 1);

        for (const auto& d : computeResult->dependent_files)
        {
            allDependentFiles.push_back(d);
        }

        allDependentFiles.push_back(computeFileDependency);

        vk::UniqueShaderModule computeShader = std::move(computeResult->module);

        const vk::PipelineShaderStageCreateInfo shaderCreateInfo {
            .sType {vk::StructureType::ePipelineShaderStageCreateInfo},
            .pNext {nullptr},
            .flags {},
            .stage {vk::ShaderStageFlagBits::eCompute},
            .module {*computeShader},
            .pName {"main"},
            .pSpecializationInfo {},
        };

        const vk::ComputePipelineCreateInfo computePipelineCreateInfo {
            .sType {vk::StructureType::eComputePipelineCreateInfo},
            .pNext {nullptr},
            .flags {},
            .stage {shaderCreateInfo},
            .layout {this->bindless_pipeline_layout},
            .basePipelineHandle {},
            .basePipelineIndex {},
        };

        auto [result, maybePipeline] =
            this->device.createComputePipelineUnique(*this->pipeline_cache, computePipelineCreateInfo);

        return PipelineInternalStorage {
            .descriptor {std::move(descriptor)},
            .pipeline {std::move(maybePipeline)},
            .dependent_files {std::move(allDependentFiles)}};
    }

    std::pair<PipelineManager::PipelineInternalStorage, std::optional<std::string>>
    PipelineManager::tryRecompilePipeline(PipelineInternalStorage oldStorage) const
    {
        std::expected<PipelineInternalStorage, std::string> result {};

        if (GraphicsPipelineDescriptor* g = std::get_if<GraphicsPipelineDescriptor>(&oldStorage.descriptor))
        {
            result = this->tryCompilePipeline(*g);
        }
        else if (ComputePipelineDescriptor* c = std::get_if<ComputePipelineDescriptor>(&oldStorage.descriptor))
        {
            result = this->tryCompilePipeline(*c);
        }
        else
        {
            panic("unhandled variant");
        }

        if (result.has_value())
        {
            return {std::move(*result), std::nullopt};
        }
        else
        {
            return {std::move(oldStorage), std::move(result.error())};
        }
    }

} // namespace gfx::core::vulkan