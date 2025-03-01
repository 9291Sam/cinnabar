
#include "gfx/render/renderer.hpp"
#include "gfx/render/vulkan/device.hpp"
#include "gfx/render/vulkan/instance.hpp"
#include "gfx/render/vulkan/swapchain.hpp"
#include "gfx/render/window.hpp"
#include "util/logger.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <cstddef>
#include <iterator>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

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
)";

std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> loadShaders()
{
    shaderc::CompileOptions options {};
    options.SetSourceLanguage(shaderc_source_language_glsl);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetGenerateDebugInfo();

    shaderc::Compiler             compiler {};
    shaderc::SpvCompilationResult vertexShaderResult =
        compiler.CompileGlslToSpv(vertexShader, shaderc_vertex_shader, "", options);
    shaderc::SpvCompilationResult fragmentShaderResult =
        compiler.CompileGlslToSpv(fragmentShader, shaderc_fragment_shader, "", options);

    assert::critical(vertexShaderResult.GetCompilationStatus() == shaderc_compilation_status_success, "oops v");
    assert::critical(fragmentShaderResult.GetCompilationStatus() == shaderc_compilation_status_success, "oops f");

    std::vector<u32> vertexData {std::from_range, vertexShaderResult};
    std::vector<u32> fragmentData {std::from_range, fragmentShaderResult};

    assert::critical(!vertexData.empty(), "Vertex Size: {}", vertexData.size());
    assert::critical(!fragmentData.empty(), "Fragment Size: {}", fragmentData.size());

    std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> out {};

    out[vk::ShaderStageFlagBits::eVertex]   = std::move(vertexData);
    out[vk::ShaderStageFlagBits::eFragment] = std::move(fragmentData);

    return out;

    // if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
    // {
    //     log::error(
    //         "compile failed: {} {}",
    //         static_cast<int>(compileResult.GetCompilationStatus()),
    //         compileResult.GetErrorMessage());
    // }
    // else
    // {
    //     std::span<const char> data {compileResult.cbegin(), compileResult.cend()};

    //     std::array<char, 16> hexChars {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
    //     'F'}; std::string          result {}; result.reserve(data.size() * 6);

    //     for (char d : data)
    //     {
    //         const u32 c = static_cast<u8>(d);

    //         result.push_back('0');
    //         result.push_back('x');
    //         result += hexChars[static_cast<u32>(c & 0xF0u) >> 4u];
    //         result += hexChars[static_cast<u32>(c & 0x0Fu) >> 0u];
    //         result.push_back(',');
    //         result.push_back(' ');
    //     }

    //     if (!result.empty())
    //     {
    //         result.pop_back();
    //         result.pop_back();
    //     }

    //     log::info("{}", result);
    // }
}

int main()
{
    util::GlobalLoggerContext loggerContext {};

    CPPTRACE_TRYZ
    {
        log::info(
            "Cinnabar has started v{}.{}.{}.{}{}",
            CINNABAR_VERSION_MAJOR,
            CINNABAR_VERSION_MINOR,
            CINNABAR_VERSION_PATCH,
            CINNABAR_VERSION_TWEAK,
            CINNABAR_DEBUG_BUILD ? " Debug Build" : "");

        gfx::render::Renderer renderer {};
        bool                  hasResizeOccurred = true;
        const u32             graphicsQueueIndex =
            renderer.getDevice()->getFamilyOfQueueType(gfx::render::vulkan::Device::QueueType::Graphics).value();

        std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> shadersSPirv = loadShaders();

        struct CacheablePipelineShaderStageCreateInfo
        {
            vk::ShaderStageFlagBits                 stage;
            std::shared_ptr<vk::UniqueShaderModule> shader;
            std::string                             entry_point;

            // bool operator== (const CacheablePipelineShaderStageCreateInfo&) const = default;
        };

        std::vector<CacheablePipelineShaderStageCreateInfo> stages {};

        for (auto [flags, code] : shadersSPirv)
        {
            log::trace("{} {}", vk::to_string(flags), code.size());

            stages.push_back(CacheablePipelineShaderStageCreateInfo {
                .stage {flags},
                .shader {std::make_shared<vk::UniqueShaderModule>(
                    renderer.getDevice()->getDevice().createShaderModuleUnique({
                        .sType {vk::StructureType::eShaderModuleCreateInfo},
                        .pNext {nullptr},
                        .flags {},
                        .codeSize {code.size() * sizeof(u32)},
                        .pCode {code.data()},
                    }))},
                .entry_point {"main"},
            });
        }

        struct CacheableGraphicsPipelineCreateInfo
        {
            std::vector<CacheablePipelineShaderStageCreateInfo> stages;
            std::vector<vk::VertexInputAttributeDescription>    vertex_attributes;
            std::vector<vk::VertexInputBindingDescription>      vertex_bindings;
            vk::PrimitiveTopology                               topology;
            bool                                                discard_enable;
            vk::PolygonMode                                     polygon_mode;
            vk::CullModeFlags                                   cull_mode;
            vk::FrontFace                                       front_face;
            bool                                                depth_test_enable;
            bool                                                depth_write_enable;
            vk::CompareOp                                       depth_compare_op;
            vk::Format                                          color_format;
            vk::Format                                          depth_format;
            bool                                                blend_enable;
            std::shared_ptr<vk::UniquePipelineLayout>           layout;
            std::string                                         name;

            bool operator== (const CacheableGraphicsPipelineCreateInfo&) const = default;
        };

        const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
            .sType {vk::StructureType::ePipelineLayoutCreateInfo},
            .pNext {nullptr},
            .flags {},
            .setLayoutCount {0},
            .pSetLayouts {nullptr},
            .pushConstantRangeCount {0},
            .pPushConstantRanges {nullptr},
        };

        std::shared_ptr<vk::UniquePipelineLayout> layout = std::make_shared<vk::UniquePipelineLayout>(
            renderer.getDevice()->getDevice().createPipelineLayoutUnique(pipelineLayoutCreateInfo));

        CacheableGraphicsPipelineCreateInfo info {
            .stages {std::move(stages)},
            .vertex_attributes {},
            .vertex_bindings {},
            .topology {vk::PrimitiveTopology::eTriangleList},
            .discard_enable {false},
            .polygon_mode {vk::PolygonMode::eFill},
            .cull_mode {vk::CullModeFlagBits::eNone},
            .front_face {vk::FrontFace::eClockwise},
            .depth_test_enable {false},
            .depth_write_enable {false},
            .depth_compare_op {},
            .color_format {gfx::render::Renderer::ColorFormat.format},
            .depth_format {},
            .blend_enable {true},
            .layout {std::move(layout)},
            .name {"idk bro"}};

        std::vector<vk::PipelineShaderStageCreateInfo> denseStages {};
        denseStages.reserve(info.stages.size());

        for (const CacheablePipelineShaderStageCreateInfo& s : info.stages)
        {
            denseStages.push_back(vk::PipelineShaderStageCreateInfo {

                .sType {vk::StructureType::ePipelineShaderStageCreateInfo},
                .pNext {nullptr},
                .flags {},
                .stage {static_cast<vk::ShaderStageFlagBits>(s.stage)},
                .module {**s.shader},
                .pName {s.entry_point.c_str()},
                .pSpecializationInfo {},
            });
        }

        const vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo {
            .sType {vk::StructureType::ePipelineVertexInputStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .vertexBindingDescriptionCount {static_cast<u32>(info.vertex_bindings.size())},
            .pVertexBindingDescriptions {info.vertex_bindings.data()},
            .vertexAttributeDescriptionCount {static_cast<u32>(info.vertex_attributes.size())},
            .pVertexAttributeDescriptions {info.vertex_attributes.data()},
        };

        const vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo {
            .sType {vk::StructureType::ePipelineInputAssemblyStateCreateInfo},
            .pNext {nullptr},
            .flags {},
            .topology {info.topology},
            .primitiveRestartEnable {false},
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
            .rasterizerDiscardEnable {info.discard_enable},
            .polygonMode {info.polygon_mode},
            .cullMode {info.cull_mode},
            .frontFace {info.front_face},
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
            .depthTestEnable {info.depth_test_enable},
            .depthWriteEnable {info.depth_write_enable},
            .depthCompareOp {info.depth_compare_op},
            .depthBoundsTestEnable {vk::False}, // TODO: expose?
            .stencilTestEnable {vk::False},
            .front {},
            .back {},
            .minDepthBounds {0.0}, // TODO: expose?
            .maxDepthBounds {1.0}, // TODO: expose?
        };

        const vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState {
            .blendEnable {info.blend_enable},
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
            .blendConstants {{0.0, 0.0, 0.0, 0.0}},
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
            .colorAttachmentCount {info.color_format == vk::Format::eUndefined ? 0u : 1u},
            .pColorAttachmentFormats {&info.color_format},
            .depthAttachmentFormat {info.depth_format},
            .stencilAttachmentFormat {},
        };

        const vk::GraphicsPipelineCreateInfo pipelineCreateInfo {
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
            .layout {**info.layout},
            .renderPass {nullptr},
            .subpass {0},
            .basePipelineHandle {nullptr},
            .basePipelineIndex {0},
        };

        auto [result, pipeline] =
            renderer.getDevice()->getDevice().createGraphicsPipelineUnique(nullptr, pipelineCreateInfo);

        assert::critical(result == vk::Result::eSuccess, "oops {}", vk::to_string(result));

        while (!renderer.shouldWindowClose())
        {
            auto rec = [&](vk::CommandBuffer               commandBuffer,
                           u32                             swapchainImageIdx,
                           gfx::render::vulkan::Swapchain& swapchain,
                           std::size_t                     frameIndex)
            {
                const vk::Image     thisSwapchainImage     = swapchain.getImages()[swapchainImageIdx];
                const vk::ImageView thisSwapchainImageView = swapchain.getViews()[swapchainImageIdx];

                vk::Extent2D renderExtent = swapchain.getExtent();

                const vk::Rect2D scissor {.offset {vk::Offset2D {.x {0}, .y {0}}}, .extent {renderExtent}};

                const vk::Viewport renderViewport {
                    .x {0.0},
                    .y {static_cast<f32>(renderExtent.height)},
                    .width {static_cast<f32>(renderExtent.width)},
                    .height {static_cast<f32>(renderExtent.height) * -1.0f},
                    .minDepth {0.0},
                    .maxDepth {1.0},
                };

                if (hasResizeOccurred)
                {
                    const std::span<const vk::Image> swapchainImages = swapchain.getImages();

                    std::vector<vk::ImageMemoryBarrier> swapchainMemoryBarriers {};
                    swapchainMemoryBarriers.reserve(swapchainImages.size());

                    for (const vk::Image i : swapchainImages)
                    {
                        swapchainMemoryBarriers.push_back(vk::ImageMemoryBarrier {
                            .sType {vk::StructureType::eImageMemoryBarrier},
                            .pNext {nullptr},
                            .srcAccessMask {vk::AccessFlagBits::eNone},
                            .dstAccessMask {vk::AccessFlagBits::eNone},
                            .oldLayout {vk::ImageLayout::eUndefined},
                            .newLayout {vk::ImageLayout::ePresentSrcKHR},
                            .srcQueueFamilyIndex {graphicsQueueIndex},
                            .dstQueueFamilyIndex {graphicsQueueIndex},
                            .image {i},
                            .subresourceRange {vk::ImageSubresourceRange {
                                .aspectMask {vk::ImageAspectFlagBits::eColor},
                                .baseMipLevel {0},
                                .levelCount {1},
                                .baseArrayLayer {0},
                                .layerCount {1}}},
                        });
                    }

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        {},
                        {},
                        {},
                        swapchainMemoryBarriers);
                }

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eNone},
                        .dstAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                        .oldLayout {vk::ImageLayout::ePresentSrcKHR},
                        .newLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {thisSwapchainImage},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                // Simple Color
                {
                    const vk::RenderingAttachmentInfo colorAttachmentInfo {
                        .sType {vk::StructureType::eRenderingAttachmentInfo},
                        .pNext {nullptr},
                        .imageView {thisSwapchainImageView},
                        .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .resolveMode {vk::ResolveModeFlagBits::eNone},
                        .resolveImageView {nullptr},
                        .resolveImageLayout {},
                        .loadOp {vk::AttachmentLoadOp::eClear},
                        .storeOp {vk::AttachmentStoreOp::eStore},
                        .clearValue {
                            vk::ClearColorValue {.float32 {{192.0f / 255.0f, 57.0f / 255.0f, 43.0f / 255.0f, 1.0f}}}},
                    };

                    const vk::RenderingInfo simpleColorRenderingInfo {
                        .sType {vk::StructureType::eRenderingInfo},
                        .pNext {nullptr},
                        .flags {},
                        .renderArea {scissor},
                        .layerCount {1},
                        .viewMask {0},
                        .colorAttachmentCount {1},
                        .pColorAttachments {&colorAttachmentInfo},
                        .pDepthAttachment {nullptr},
                        .pStencilAttachment {nullptr},
                    };

                    commandBuffer.setViewport(0, {renderViewport});
                    commandBuffer.setScissor(0, {scissor});

                    commandBuffer.beginRendering(simpleColorRenderingInfo);

                    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
                    commandBuffer.draw(3, 1, 0, 0);

                    commandBuffer.endRendering();
                }

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                        .dstAccessMask {vk::AccessFlagBits::eColorAttachmentRead},
                        .oldLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .newLayout {vk::ImageLayout::ePresentSrcKHR},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {thisSwapchainImage},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});
            };

            if (renderer.getWindow()->isActionActive(gfx::render::Window::Action::ToggleCursorAttachment))
            {
                renderer.getWindow()->toggleCursor();
            }

            if (renderer.getWindow()->isActionActive(gfx::render::Window::Action::CloseWindow))
            {
                break;
            }

            hasResizeOccurred = renderer.recordOnThread(rec);
        }

        renderer.getDevice()->getDevice().waitIdle();
    }
    CPPTRACE_CATCHZ(const std::exception& e)
    {
        log::critical(
            "Cinnabar has crashed! | {} {}\n{}",
            e.what(),
            typeid(e).name(),
            cpptrace::from_current_exception().to_string(true));
    }
    CPPTRACE_CATCH_ALT(...)
    {
        log::critical(
            "Cinnabar has crashed! | Unknown Exception type thrown!\n{}",
            cpptrace::from_current_exception().to_string(true));
    }

    log::info("Cinnabar has shutdown successfully!");
}
