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
#include <span>
#include <variant>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::core::vulkan
{
    static constexpr u32 MaxPipelines = 512;

    PipelineManager::PipelineManager(const Device& device_, vk::PipelineLayout bindlessPipelineLayout)
        : device {device_.getDevice()}
        , pipeline_cache {this->device.createPipelineCacheUnique(
              vk::PipelineCacheCreateInfo {
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

    PipelineManager::UniquePipeline PipelineManager::createPipelineUnique(ComputePipelineDescriptor d) const
    {
        return UniquePipeline {this->createPipeline(std::move(d)), this};
    }

    PipelineManager::UniquePipeline PipelineManager::createPipelineUnique(GraphicsPipelineDescriptor d) const
    {
        return UniquePipeline {this->createPipeline(std::move(d)), this};
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

                            pipelineStorage = std::move(newInternalStorage);

                            std::string_view pipelineName {};

                            if (GraphicsPipelineDescriptor* g =
                                    std::get_if<GraphicsPipelineDescriptor>(&pipelineStorage.descriptor))
                            {
                                pipelineName = g->name;
                            }
                            else if (
                                ComputePipelineDescriptor* c =
                                    std::get_if<ComputePipelineDescriptor>(&pipelineStorage.descriptor))
                            {
                                pipelineName = c->name;
                            }
                            else
                            {
                                panic("unhandled variant");
                            }

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

    std::expected<PipelineManager::PipelineInternalStorage, std::string>
    PipelineManager::tryCompilePipeline(GraphicsPipelineDescriptor& descriptor) const
    {
        vk::UniqueShaderModule                                                         vertexShader;
        vk::UniqueShaderModule                                                         fragmentShader;
        std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> allDependentFiles {};

        assert::critical(descriptor.shader_path.ends_with("slang"), "Tried to compile a non slang file");

        std::expected<cfi::SaneSlangCompiler::CompileResult, std::string> maybeCompiledCode =
            this->sane_slang_compiler.lock(
                [&](cfi::SaneSlangCompiler& c)
                {
                    return c.compile(util::getCanonicalPathOfShaderFile(descriptor.shader_path));
                });

        if (!maybeCompiledCode.has_value())
        {
            return std::unexpected(std::move(maybeCompiledCode.error()));
        }

        // Vertex
        {
            std::span<const u32> compiledSPV {
                maybeCompiledCode->maybe_vertex_data.cbegin(), maybeCompiledCode->maybe_vertex_data.cend()};

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
            std::span<const u32> compiledSPV {
                maybeCompiledCode->maybe_fragment_data.cbegin(), maybeCompiledCode->maybe_fragment_data.cend()};

            const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
                .sType {vk::StructureType::eShaderModuleCreateInfo},
                .pNext {nullptr},
                .flags {},
                .codeSize {compiledSPV.size_bytes()},
                .pCode {compiledSPV.data()},
            };

            fragmentShader = this->device.createShaderModuleUnique(shaderModuleCreateInfo);
        }

        for (std::filesystem::path& p : maybeCompiledCode->dependent_files)
        {
            std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(p);

            allDependentFiles.push_back({std::move(p), writeTime});
        }

        if (!maybeCompiledCode->maybe_warnings.empty())
        {
            // HACK: this shouldn't be here
            log::warn("Slang Compilation Warning:\n{}", maybeCompiledCode->maybe_warnings);
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
        vk::UniqueShaderModule                                                         computeShader;
        std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> allDependentFiles {};

        assert::critical(descriptor.compute_shader_path.ends_with("slang"), "Tried to compile a non slang file");

        std::expected<cfi::SaneSlangCompiler::CompileResult, std::string> maybeCompiledCode =
            this->sane_slang_compiler.lock(
                [&](cfi::SaneSlangCompiler& c)
                {
                    return c.compile(util::getCanonicalPathOfShaderFile(descriptor.compute_shader_path));
                });

        if (!maybeCompiledCode.has_value())
        {
            return std::unexpected(std::move(maybeCompiledCode.error()));
        }

        // Compute
        {
            std::span<const u32> compiledSPV {
                maybeCompiledCode->maybe_compute_data.cbegin(), maybeCompiledCode->maybe_compute_data.cend()};

            const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
                .sType {vk::StructureType::eShaderModuleCreateInfo},
                .pNext {nullptr},
                .flags {},
                .codeSize {compiledSPV.size_bytes()},
                .pCode {compiledSPV.data()},
            };

            computeShader = this->device.createShaderModuleUnique(shaderModuleCreateInfo);
        }

        for (std::filesystem::path& p : maybeCompiledCode->dependent_files)
        {
            std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(p);

            allDependentFiles.push_back({std::move(p), writeTime});
        }

        if (!maybeCompiledCode->maybe_warnings.empty())
        {
            // HACK: this shouldn't be here
            log::warn("Slang Compilation Warning:\n{}", maybeCompiledCode->maybe_warnings);
        }

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