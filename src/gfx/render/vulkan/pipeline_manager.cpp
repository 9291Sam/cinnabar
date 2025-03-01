#include "pipeline_manager.hpp"
#include "gfx/render/vulkan/device.hpp"
#include "util/allocators/opaque_integer_handle_allocator.hpp"
#include "util/logger.hpp"
#include "util/threads.hpp"
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>
#include <span>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::render::vulkan
{

    static constexpr u32 MaxPipelines = 512;

    PipelineManager::PipelineManager(const Device& device_, vk::PipelineLayout bindlessPipelineLayout)
        : device {device_.getDevice()}
        , pipeline_cache {this->device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo {
              .sType {vk::StructureType::ePipelineCacheCreateInfo},
              .pNext {nullptr},
              .flags {},
              .initialDataSize {0},
              .pInitialData {nullptr}})}
        , bindless_pipeline_layout {bindlessPipelineLayout}
        , critical_section {util::RwLock {CriticalSection {
              .graphics_pipeline_handle_allocator {util::OpaqueHandleAllocator<GraphicsPipeline> {MaxPipelines}},
              .graphics_pipeline_storage {MaxPipelines}}}}
    {}

    PipelineManager::~PipelineManager() = default;

    PipelineManager::GraphicsPipeline
    PipelineManager::createGraphicsPipeline(GraphicsPipelineDescriptor descriptor) const
    {
        vk::UniquePipeline pipeline = this->createGraphicsPipelineFromDescriptor(descriptor);

        return this->critical_section.writeLock(
            [&](CriticalSection& criticalSection)
            {
                GraphicsPipeline newPipelineHandle =
                    criticalSection.graphics_pipeline_handle_allocator.allocateOrPanic();

                const u16 handleValue =
                    criticalSection.graphics_pipeline_handle_allocator.getValueOfHandle(newPipelineHandle);

                criticalSection.graphics_pipeline_storage[handleValue] = GraphicsPipelineInternalStorage {
                    .descriptor {std::move(descriptor)}, .pipeline {std::move(pipeline)}};

                return newPipelineHandle;
            });
    }

    void PipelineManager::destroyGraphicsPipeline(GraphicsPipeline p) const
    {
        this->critical_section.writeLock(
            [&](CriticalSection& criticalSection)
            {
                const u16 handleValue = criticalSection.graphics_pipeline_handle_allocator.getValueOfHandle(p);

                criticalSection.graphics_pipeline_storage[handleValue] = {};

                criticalSection.graphics_pipeline_handle_allocator.free(std::move(p));
            });
    }

    vk::Pipeline PipelineManager::getPipeline(const GraphicsPipeline& p) const
    {
        return this->critical_section.writeLock(
            [&](CriticalSection& criticalSection)
            {
                const u16 handleValue = criticalSection.graphics_pipeline_handle_allocator.getValueOfHandle(p);

                return *criticalSection.graphics_pipeline_storage[handleValue].pipeline;
            });
    }

    vk::UniqueShaderModule
    PipelineManager::createShaderModuleFromShaderPath(std::string_view path, vk::ShaderStageFlags) const
    {
        shaderc::CompileOptions options {};
        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
        options.SetTargetSpirv(shaderc_spirv_version_1_5);

        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            options.SetGenerateDebugInfo();
            options.SetOptimizationLevel(shaderc_optimization_level_zero);
        }
        else
        {
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
        }

        static const char* vertexShader = R"(
            #version 460
            
            layout(location = 0) out vec3 fragColor;
            
            vec3 positions[3] = vec3[](
                vec3( 0.0,  0.5, 0.0),  // Top
                vec3(-0.5, -0.5, 0.0),  // Bottom left
                vec3( 0.5, -0.5, 0.0)   // Bottom right
            );
            
            vec3 colors[3] = vec3[](
                vec3(1.0, 0.0, 0.0), // Red
                vec3(0.0, 1.0, 0.0), // Green
                vec3(0.0, 0.0, 1.0)  // Blue
            );
            
            void main() {
                gl_Position = vec4(positions[gl_VertexIndex], 1.0);
                fragColor = colors[gl_VertexIndex];
            })";

        static const char* fragmentShader = R"(
            #version 460
            
            layout(location = 0) in vec3 fragColor;
            layout(location = 0) out vec4 outColor;
            
            void main() {
                outColor = vec4(fragColor, 1.0);
            }
            // #version 460
            
            // #extension GL_EXT_nonuniform_qualifier : enable
            
            // layout(location = 0) in vec3 fragColor; // Use fragColor as UV coordinates
            // layout(location = 0) out vec4 outColor;
            
            // // Descriptor set 0, binding 0: an array of sampled textures (bindless)
            // layout(set = 0, binding = 0) uniform sampler2D textures[];
            
            // void main() {
            //     vec2 uv = fragColor.xy; // Use xy components as UV coordinates
            
            //     // Select textures dynamically
            //     int textureIndex1 = 0; 
            //     int textureIndex2 = 1;
            
            //     // Fetch colors from the two textures using non-uniform indexing
            //     vec4 color1 = texture(textures[textureIndex1], uv);
            //     vec4 color2 = texture(textures[textureIndex2], uv);
            
            //     // Blend the two textures (you can modify the blending logic)
            //     outColor = mix(color1, color2, 0.5);
            // }
            
            // #version 460
            
            // #extension GL_EXT_nonuniform_qualifier : enable
            
            // layout(location = 0) in vec3 fragColor;
            // layout(location = 0) out vec4 outColor;
            
            // // Bindless storage buffer arrays (same binding for different types)
            // layout(set = 0, binding = 0) readonly buffer BufferA {
            //     float data[];
            // } buffersA[]; // Array of descriptors at the same binding
            
            // layout(set = 0, binding = 0) readonly buffer BufferB {
            //     int data[];
            // } buffersB[]; // Array of descriptors at the same binding
            
            // void main() {
            //     int index = int(fragColor.x * 255) % 256; // Generate an index from UVs
            
            //     // Select buffers dynamically
            //     int bufferIndexA = 0; // Choose a specific buffer from buffersA[]
            //     int bufferIndexB = 1; // Choose a specific buffer from buffersB[]
            
            //     // Read values from storage buffers using non-uniform indexing
            //     float valueA = buffersA[bufferIndexA].data[index];
            //     int valueB = buffersB[bufferIndexB].data[index];
            
            //     // Normalize valueB for color usage
            //     float normalizedB = clamp(valueB / 255.0, 0.0, 1.0);
            
            //     // Compute final output color
            //     outColor = vec4(valueA, normalizedB, 1.0 - normalizedB, 1.0);
            // }
            
            
            
            )";

        std::string         source {};
        shaderc_shader_kind kind {};

        // TODO: properly handle filesystem things / includes
        log::warn("properly implement shader file system things!");
        if (path == "triangle.vert")
        {
            source = vertexShader;
            kind   = shaderc_vertex_shader;
        }
        else if (path == "triangle.frag")
        {
            source = fragmentShader;

            kind = shaderc_fragment_shader;
        }
        else
        {
            panic("unsupported!");
        }

        shaderc::SpvCompilationResult compileResult = this->shader_compiler.CompileGlslToSpv(source, kind, "");

        assert::critical(
            compileResult.GetCompilationStatus() == shaderc_compilation_status_success,
            "{}",
            compileResult.GetErrorMessage());

        std::span<const u32> compiledSPV {compileResult.cbegin(), compileResult.cend()};

        const vk::ShaderModuleCreateInfo shaderModuleCreateInfo {
            .sType {vk::StructureType::eShaderModuleCreateInfo},
            .pNext {nullptr},
            .flags {},
            .codeSize {compiledSPV.size_bytes()},
            .pCode {compiledSPV.data()},
        };

        return this->device.createShaderModuleUnique(shaderModuleCreateInfo);
    }

    vk::UniquePipeline
    PipelineManager::createGraphicsPipelineFromDescriptor(GraphicsPipelineDescriptor descriptor) const
    {
        vk::UniqueShaderModule vertexShader =
            this->createShaderModuleFromShaderPath(descriptor.vertex_shader_path, vk::ShaderStageFlagBits::eVertex);
        vk::UniqueShaderModule fragmentShader =
            this->createShaderModuleFromShaderPath(descriptor.fragment_shader_path, vk::ShaderStageFlagBits::eFragment);

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
            // not even supported by moltenvk https://github.com/KhronosGroup/MoltenVK/issues/1521
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

        return std::move(maybeUniquePipeline);
    }

} // namespace gfx::render::vulkan