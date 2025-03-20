#include "pipeline_manager.hpp"
#include "gfx/core/vulkan/device.hpp"
#include "util/allocators/opaque_integer_handle_allocator.hpp"
#include "util/logger.hpp"
#include "util/threads.hpp"
#include "util/util.hpp"
#include <deque>
#include <filesystem>
#include <fstream>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>
#include <span>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

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
              .graphics_pipeline_handle_allocator {util::OpaqueHandleAllocator<GraphicsPipeline> {MaxPipelines}},
              .graphics_pipeline_storage {MaxPipelines}}}}
    {}

    PipelineManager::~PipelineManager() = default;

    PipelineManager::GraphicsPipeline
    PipelineManager::createGraphicsPipeline(GraphicsPipelineDescriptor descriptor) const
    {
        std::expected<GraphicsPipelineInternalStorage, TryCreateGraphicsPipelineFromDescriptorError>
            shouldBeInternalRepresentation = this->tryCreateGraphicsPipelineFromDescriptor(std::move(descriptor));

        if (!shouldBeInternalRepresentation.has_value())
        {
            const std::string errorString = shouldBeInternalRepresentation.error().error;
            std::string_view  errorView   = errorString;

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
                GraphicsPipeline newPipelineHandle =
                    criticalSection.graphics_pipeline_handle_allocator.allocateOrPanic();

                const u16 handleValue =
                    criticalSection.graphics_pipeline_handle_allocator.getValueOfHandle(newPipelineHandle);

                criticalSection.graphics_pipeline_storage[handleValue] = std::move(*shouldBeInternalRepresentation);

                return newPipelineHandle;
            });
    }

    void PipelineManager::destroyGraphicsPipeline(GraphicsPipeline p) const
    {
        this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                const u16 handleValue = criticalSection.graphics_pipeline_handle_allocator.getValueOfHandle(p);

                criticalSection.graphics_pipeline_storage[handleValue] = {};

                criticalSection.graphics_pipeline_handle_allocator.free(std::move(p));
            });
    }

    bool PipelineManager::couldAnyShadersReload() const
    {
        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                bool canAnyReload = false;

                criticalSection.graphics_pipeline_handle_allocator.iterateThroughAllocatedElements(
                    [&](const u16 handleValue)
                    {
                        GraphicsPipelineInternalStorage& pipelineStorage =
                            criticalSection.graphics_pipeline_storage[handleValue];

                        if (pipelineStorage.vertex_modify_time
                                != std::filesystem::last_write_time(pipelineStorage.vertex_path)
                            || pipelineStorage.fragment_modify_time
                                   != std::filesystem::last_write_time(pipelineStorage.fragment_path))
                        {
                            canAnyReload |= true;
                        }
                    });

                return canAnyReload;
            });
    }

    void PipelineManager::reloadShaders() const
    {
        usize numberOfReloadedPipelines = 0;

        this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                criticalSection.graphics_pipeline_handle_allocator.iterateThroughAllocatedElements(
                    [&](const u16 handleValue)
                    {
                        GraphicsPipelineInternalStorage& pipelineStorage =
                            criticalSection.graphics_pipeline_storage[handleValue];

                        if (pipelineStorage.vertex_modify_time
                                != std::filesystem::last_write_time(pipelineStorage.vertex_path)
                            || pipelineStorage.fragment_modify_time
                                   != std::filesystem::last_write_time(pipelineStorage.fragment_path))
                        {
                            std::expected<GraphicsPipelineInternalStorage, TryCreateGraphicsPipelineFromDescriptorError>
                                result = this->tryCreateGraphicsPipelineFromDescriptor(
                                    std::move(pipelineStorage.descriptor));

                            if (result.has_value())
                            {
                                pipelineStorage = std::move(*result);

                                log::debug("Reloaded {}", pipelineStorage.descriptor.name);

                                numberOfReloadedPipelines += 1;
                            }
                            else
                            {
                                TryCreateGraphicsPipelineFromDescriptorError err = std::move(result).error();

                                pipelineStorage.descriptor = std::move(err.recycled_descriptor);

                                pipelineStorage.vertex_modify_time   = err.vertex_modify_time;
                                pipelineStorage.fragment_modify_time = err.fragment_modify_time;

                                log::error(
                                    "Dynamic Pipeline {} recompilation failure:\n{}",
                                    pipelineStorage.descriptor.name,
                                    err.error);
                            }
                        }
                    });
            });

        if (numberOfReloadedPipelines != 0)
        {
            log::info("Reloaded {} pipelines", numberOfReloadedPipelines);
        }
    }

    vk::Pipeline PipelineManager::getPipeline(const GraphicsPipeline& p) const
    {
        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                const u16 handleValue = criticalSection.graphics_pipeline_handle_allocator.getValueOfHandle(p);

                return *criticalSection.graphics_pipeline_storage[handleValue].pipeline;
            });
    }

    PipelineManager::TryCreateShaderModuleResult
    PipelineManager::tryCreateShaderModuleFromShaderPath(const std::string& shaderString, vk::ShaderStageFlags) const
    {
        shaderc::CompileOptions options {};
        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetTargetSpirv(shaderc_spirv_version_1_5);
        options.SetIncluder(std::make_unique<ShaderCommonIncluder>());

        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetGenerateDebugInfo();

        shaderc_shader_kind kind {};
        if (shaderString.ends_with("vert"))
        {
            kind = shaderc_vertex_shader;
        }
        else if (shaderString.ends_with("frag"))
        {
            kind = shaderc_fragment_shader;
        }
        else
        {
            panic("unsupported!");
        }

        const std::filesystem::path canonicalPath = util::getCanonicalPathOfShaderFile(shaderString);

        std::vector<std::byte> source = util::loadEntireFileFromPath(canonicalPath);
        source.shrink_to_fit();
        const char* sourceChar = reinterpret_cast<const char*>(source.data());

        shaderc::SpvCompilationResult compileResult =
            this->shader_compiler.CompileGlslToSpv(sourceChar, source.size(), kind, shaderString.c_str(), options);

        if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            return TryCreateShaderModuleResult {
                .path {canonicalPath},
                .success {false},
                .maybe_shader {nullptr},
                .maybe_error_string {compileResult.GetErrorMessage()}};
        }

        std::span<const u32> compiledSPV {compileResult.cbegin(), compileResult.cend()};

        const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
            .sType {vk::StructureType::eShaderModuleCreateInfo},
            .pNext {nullptr},
            .flags {},
            .codeSize {compiledSPV.size_bytes()},
            .pCode {compiledSPV.data()},
        };

        return TryCreateShaderModuleResult {
            .path {canonicalPath},
            .success {true},
            .maybe_shader {this->device.createShaderModuleUnique(shaderModuleCreateInfo)},
            .maybe_error_string {}};
    }

    auto PipelineManager::tryCreateGraphicsPipelineFromDescriptor(GraphicsPipelineDescriptor descriptor) const
        -> std::expected<GraphicsPipelineInternalStorage, TryCreateGraphicsPipelineFromDescriptorError>
    {
        TryCreateShaderModuleResult vertexResult =
            this->tryCreateShaderModuleFromShaderPath(descriptor.vertex_shader_path, vk::ShaderStageFlagBits::eVertex);
        TryCreateShaderModuleResult fragmentResult = this->tryCreateShaderModuleFromShaderPath(
            descriptor.fragment_shader_path, vk::ShaderStageFlagBits::eFragment);

        const std::filesystem::path vertexShaderPath   = std::move(vertexResult.path);
        const std::filesystem::path fragmentShaderPath = std::move(fragmentResult.path);

        const std::filesystem::file_time_type vertexShaderModifyTime =
            std::filesystem::last_write_time(vertexShaderPath);
        const std::filesystem::file_time_type fragmentShaderModifyTime =
            std::filesystem::last_write_time(fragmentShaderPath);

        if (!vertexResult.success || !fragmentResult.success)
        {
            return std::unexpected(TryCreateGraphicsPipelineFromDescriptorError {
                .recycled_descriptor {std::move(descriptor)},
                .error {std::format("{}{}", vertexResult.maybe_error_string, fragmentResult.maybe_error_string)},
                .vertex_modify_time {vertexShaderModifyTime},
                .fragment_modify_time {fragmentShaderModifyTime}});
        }

        vk::UniqueShaderModule vertexShader   = std::move(vertexResult.maybe_shader);
        vk::UniqueShaderModule fragmentShader = std::move(fragmentResult.maybe_shader);

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
            device.createGraphicsPipelineUnique(*this->pipeline_cache, graphicsPipelineCreateInfo);

        assert::critical(
            result == vk::Result::eSuccess, "Failed to create graphics pipeline | Error: {}", vk::to_string(result));

        return GraphicsPipelineInternalStorage {
            .descriptor {std::move(descriptor)},
            .pipeline {std::move(maybeUniquePipeline)},
            .vertex_path {vertexShaderPath},
            .vertex_modify_time {vertexShaderModifyTime},
            .fragment_path {fragmentShaderPath},
            .fragment_modify_time {fragmentShaderModifyTime}};
    }

} // namespace gfx::core::vulkan