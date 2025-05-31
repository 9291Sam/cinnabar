#include "pipeline_manager.hpp"
#include "device.hpp"
#include "slang_compiler.hpp"
#include "util/logger.hpp"
#include "util/threads.hpp"
#include "util/timer.hpp"
#include "util/util.hpp"
#include <algorithm>
#include <expected>
#include <filesystem>
#include <future>
#include <map>
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
        util::MaybeAsynchronousObject<PipelineCompilationResult> pipelineCompilationHandle =
            this->startPipelineCompilation(graphicsDescriptor);

        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                Pipeline newPipelineHandle = criticalSection.pipeline_handle_allocator.allocateOrPanic();

                const u16 newHandleValue =
                    criticalSection.pipeline_handle_allocator.getValueOfHandle(newPipelineHandle);

                criticalSection.pipeline_storage[newHandleValue] = PipelineInternalStorage {
                    .descriptor {std::move(graphicsDescriptor)},
                    .dependent_files {},
                    .current_pipeline {},
                    .maybe_new_pipeline {std::move(pipelineCompilationHandle)}};

                return newPipelineHandle;
            });
    }

    PipelineManager::Pipeline PipelineManager::createPipeline(ComputePipelineDescriptor computeDescriptor) const
    {
        util::MaybeAsynchronousObject<PipelineCompilationResult> pipelineCompilationHandle =
            this->startPipelineCompilation(computeDescriptor);

        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                Pipeline newPipelineHandle = criticalSection.pipeline_handle_allocator.allocateOrPanic();

                const u16 newHandleValue =
                    criticalSection.pipeline_handle_allocator.getValueOfHandle(newPipelineHandle);

                criticalSection.pipeline_storage[newHandleValue] = PipelineInternalStorage {
                    .descriptor {std::move(computeDescriptor)},
                    .dependent_files {},
                    .current_pipeline {},
                    .maybe_new_pipeline {std::move(pipelineCompilationHandle)}};

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

                PipelineInternalStorage& pipelineStorage = criticalSection.pipeline_storage[handleValue];

                if (!pipelineStorage.current_pipeline.has_value())
                {
                    // we have a first compile of a pipeline, let's wait for it
                    util::Timer t {"ff"};

                    pipelineStorage.maybe_new_pipeline->get();

                    log::trace("waited {}us for pipeline compilation", t.end(false));
                }

                // we've completed compilation of a new pipeline, let's try and update current_pipeline
                if (pipelineStorage.maybe_new_pipeline.has_value() && pipelineStorage.maybe_new_pipeline->isReady())
                {
                    util::MaybeAsynchronousObject<PipelineCompilationResult> justCompiledPipeline =
                        *std::exchange(pipelineStorage.maybe_new_pipeline, std::nullopt);

                    // ensure the new pipeline is actually valid
                    PipelineCompilationResult& newPipelineCompilationResult = justCompiledPipeline.get();

                    pipelineStorage.dependent_files = std::move(newPipelineCompilationResult.dependent_files);

                    if (newPipelineCompilationResult.maybe_new_pipeline.has_value())
                    {
                        // excellent, the new pipeline is real!

                        // slightly hacky, but just slam it in there, it's fine
                        pipelineStorage.current_pipeline = std::move(newPipelineCompilationResult.maybe_new_pipeline);

                        std::string_view name {};

                        if (GraphicsPipelineDescriptor* g =
                                std::get_if<GraphicsPipelineDescriptor>(&pipelineStorage.descriptor))
                        {
                            name = g->name;
                        }
                        else if (
                            ComputePipelineDescriptor* c =
                                std::get_if<ComputePipelineDescriptor>(&pipelineStorage.descriptor))
                        {
                            name = c->name;
                        }
                        else
                        {
                            panic("unhandled variant");
                        }

                        if (newPipelineCompilationResult.warnings_and_errors.empty())
                        {
                            log::debug("Dynamically Reloaded Pipeline: {}", name);
                        }
                        else
                        {
                            log::warn(
                                "Dynamically Reloaded Pipeline: {} with warnings {}",
                                name,
                                newPipelineCompilationResult.warnings_and_errors);
                        }
                    }
                    else
                    {
                        // welp compilation failed, lets just keep the old one

                        if (pipelineStorage.current_pipeline.has_value())
                        {
                            log::error(
                                "Dynamic Pipeline Recompilation Failure: {}",
                                newPipelineCompilationResult.warnings_and_errors);
                        }
                        else
                        {
                            // shit there is no old pipeline to keep, all we can do it panic

                            panic(
                                "Failed to compile pipeline on first attempt! {}",
                                newPipelineCompilationResult.warnings_and_errors);
                        }
                    }
                }

                return *pipelineStorage.current_pipeline.value();
            });
    }

    bool PipelineManager::couldAnyShadersReload() const
    {
        // OPTIMIZATION: each call to std::filesystem::last_write_time takes like 20-50us, this becomes significant fast
        std::map<std::filesystem::path, std::filesystem::file_time_type> writeTimeCache {};

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
                            decltype(writeTimeCache)::const_iterator maybeIt = writeTimeCache.find(path);

                            if (maybeIt == writeTimeCache.cend())
                            {
                                auto [it, inserted] =
                                    writeTimeCache.insert({path, std::filesystem::last_write_time(path)});

                                maybeIt = it;
                                assert::critical(inserted, "oops");
                            }

                            if (time != maybeIt->second)
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

                        bool canThisPipelineReload = std::ranges::any_of(
                            pipelineStorage.dependent_files,
                            [](const auto& pair)
                            {
                                const auto& [path, time] = pair;

                                return time != std::filesystem::last_write_time(path);
                            });

                        if (canThisPipelineReload && !pipelineStorage.maybe_new_pipeline.has_value())
                        {
                            if (GraphicsPipelineDescriptor* g =
                                    std::get_if<GraphicsPipelineDescriptor>(&pipelineStorage.descriptor))
                            {
                                pipelineStorage.maybe_new_pipeline = this->startPipelineCompilation(*g);
                            }
                            else if (
                                ComputePipelineDescriptor* c =
                                    std::get_if<ComputePipelineDescriptor>(&pipelineStorage.descriptor))
                            {
                                pipelineStorage.maybe_new_pipeline = this->startPipelineCompilation(*c);
                            }
                            else
                            {
                                panic("unhandled variant");
                            }
                        }
                    });
            });
    }

    util::MaybeAsynchronousObject<PipelineManager::PipelineCompilationResult>
    PipelineManager::startPipelineCompilation(GraphicsPipelineDescriptor descriptor) const
    {
        std::future<PipelineCompilationResult> resultFuture = std::async(
            std::launch::async,
            [descriptor, this]() -> PipelineCompilationResult
            {
                vk::UniqueShaderModule vertexShader;
                vk::UniqueShaderModule fragmentShader;

                assert::critical(descriptor.shader_path.ends_with("slang"), "Tried to compile a non slang file");

                cfi::SaneSlangCompiler::CompileResult slangCompilationResult = this->sane_slang_compiler.lock(
                    [&](cfi::SaneSlangCompiler& c)
                    {
                        return c.compile(util::getCanonicalPathOfShaderFile(descriptor.shader_path));
                    });

                DependentFileStorage dependentFiles {};
                dependentFiles.reserve(slangCompilationResult.dependent_files.size());

                for (std::filesystem::path& p : slangCompilationResult.dependent_files)
                {
                    const std::filesystem::file_time_type time = std::filesystem::last_write_time(p);

                    dependentFiles.push_back(std::pair {std::move(p), time});
                }

                if (!slangCompilationResult.shaders.has_value())
                {
                    // well shit slang failed to compile, bail

                    return PipelineCompilationResult {
                        .maybe_new_pipeline {std::nullopt},
                        .warnings_and_errors {std::move(slangCompilationResult.warnings_and_errors)},
                        .dependent_files {std::move(dependentFiles)}};
                }

                // Vertex
                {
                    std::span<const u32> compiledSPV {
                        slangCompilationResult.shaders->maybe_vertex_data.cbegin(),
                        slangCompilationResult.shaders->maybe_vertex_data.cend()};

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
                        slangCompilationResult.shaders->maybe_fragment_data.cbegin(),
                        slangCompilationResult.shaders->maybe_fragment_data.cend()};

                    const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
                        .sType {vk::StructureType::eShaderModuleCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .codeSize {compiledSPV.size_bytes()},
                        .pCode {compiledSPV.data()},
                    };

                    fragmentShader = this->device.createShaderModuleUnique(shaderModuleCreateInfo);
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
                    result == vk::Result::eSuccess,
                    "Failed to create graphics pipeline | Error: {}",
                    vk::to_string(result));

                return PipelineCompilationResult {
                    .maybe_new_pipeline {std::move(maybeUniquePipeline)},
                    .warnings_and_errors {std::move(slangCompilationResult.warnings_and_errors)},
                    .dependent_files {std::move(dependentFiles)}};
            });

        return util::MaybeAsynchronousObject {std::move(resultFuture)};
    }

    util::MaybeAsynchronousObject<PipelineManager::PipelineCompilationResult>
    PipelineManager::startPipelineCompilation(ComputePipelineDescriptor descriptor) const
    {
        std::future<PipelineCompilationResult> resultFuture = std::async(
            std::launch::async,
            [descriptor, this]() -> PipelineCompilationResult
            {
                vk::UniqueShaderModule computeShader;

                assert::critical(
                    descriptor.compute_shader_path.ends_with("slang"), "Tried to compile a non slang file");

                cfi::SaneSlangCompiler::CompileResult slangCompilationResult = this->sane_slang_compiler.lock(
                    [&](cfi::SaneSlangCompiler& c)
                    {
                        return c.compile(util::getCanonicalPathOfShaderFile(descriptor.compute_shader_path));
                    });

                DependentFileStorage dependentFiles {};
                dependentFiles.reserve(slangCompilationResult.dependent_files.size());

                for (std::filesystem::path& p : slangCompilationResult.dependent_files)
                {
                    const std::filesystem::file_time_type time = std::filesystem::last_write_time(p);

                    dependentFiles.push_back(std::pair {std::move(p), time});
                }

                if (!slangCompilationResult.shaders.has_value())
                {
                    // well shit slang failed to compile, bail

                    return PipelineCompilationResult {
                        .maybe_new_pipeline {std::nullopt},
                        .warnings_and_errors {std::move(slangCompilationResult.warnings_and_errors)},
                        .dependent_files {std::move(dependentFiles)}};
                }

                // Compute
                {
                    std::span<const u32> compiledSPV {
                        slangCompilationResult.shaders->maybe_compute_data.cbegin(),
                        slangCompilationResult.shaders->maybe_compute_data.cend()};

                    const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
                        .sType {vk::StructureType::eShaderModuleCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .codeSize {compiledSPV.size_bytes()},
                        .pCode {compiledSPV.data()},
                    };

                    computeShader = this->device.createShaderModuleUnique(shaderModuleCreateInfo);
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

                return PipelineCompilationResult {
                    .maybe_new_pipeline {std::move(maybePipeline)},
                    .warnings_and_errors {std::move(slangCompilationResult.warnings_and_errors)},
                    .dependent_files {std::move(dependentFiles)}};
            });

        return util::MaybeAsynchronousObject {std::move(resultFuture)};
    }

} // namespace gfx::core::vulkan