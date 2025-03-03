
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/device.hpp"
#include "gfx/core/vulkan/instance.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/core/window.hpp"
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

// std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> loadShaders()
// {
//     shaderc::CompileOptions options {};
//     options.SetSourceLanguage(shaderc_source_language_glsl);
//     options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
//     options.SetTargetSpirv(shaderc_spirv_version_1_5);
//     options.SetOptimizationLevel(shaderc_optimization_level_performance);
//     options.SetGenerateDebugInfo();

//     shaderc::Compiler             compiler {};
//     shaderc::SpvCompilationResult vertexShaderResult =
//         compiler.CompileGlslToSpv(vertexShader, shaderc_vertex_shader, "", options);
//     shaderc::SpvCompilationResult fragmentShaderResult =
//         compiler.CompileGlslToSpv(fragmentShader, shaderc_fragment_shader, "", options);

//     assert::critical(vertexShaderResult.GetCompilationStatus() == shaderc_compilation_status_success, "oops v");
//     if (fragmentShaderResult.GetCompilationStatus() != shaderc_compilation_status_success)
//     {
//         assert::critical(false, "{}", fragmentShaderResult.GetErrorMessage());
//     }

//     std::vector<u32> vertexData {std::from_range, vertexShaderResult};
//     std::vector<u32> fragmentData {std::from_range, fragmentShaderResult};

//     assert::critical(!vertexData.empty(), "Vertex Size: {}", vertexData.size());
//     assert::critical(!fragmentData.empty(), "Fragment Size: {}", fragmentData.size());

//     std::unordered_map<vk::ShaderStageFlagBits, std::vector<u32>> out {};

//     out[vk::ShaderStageFlagBits::eVertex]   = std::move(vertexData);
//     out[vk::ShaderStageFlagBits::eFragment] = std::move(fragmentData);

//     return out;

//     // if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
//     // {
//     //     log::error(
//     //         "compile failed: {} {}",
//     //         static_cast<int>(compileResult.GetCompilationStatus()),
//     //         compileResult.GetErrorMessage());
//     // }
//     // else
//     // {
//     //     std::span<const char> data {compileResult.cbegin(), compileResult.cend()};

//     //     std::array<char, 16> hexChars {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
//     //     'F'}; std::string          result {}; result.reserve(data.size() * 6);

//     //     for (char d : data)
//     //     {
//     //         const u32 c = static_cast<u8>(d);

//     //         result.push_back('0');
//     //         result.push_back('x');
//     //         result += hexChars[static_cast<u32>(c & 0xF0u) >> 4u];
//     //         result += hexChars[static_cast<u32>(c & 0x0Fu) >> 0u];
//     //         result.push_back(',');
//     //         result.push_back(' ');
//     //     }

//     //     if (!result.empty())
//     //     {
//     //         result.pop_back();
//     //         result.pop_back();
//     //     }

//     //     log::info("{}", result);
//     // }
// }

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

        gfx::core::Renderer renderer {};
        bool                hasResizeOccurred = true;
        const u32           graphicsQueueIndex =
            renderer.getDevice()->getFamilyOfQueueType(gfx::core::vulkan::Device::QueueType::Graphics).value();

        gfx::core::vulkan::PipelineManager::GraphicsPipeline trianglePipeline =
            renderer.getPipelineManager()->createGraphicsPipeline(gfx::core::vulkan::GraphicsPipelineDescriptor {
                .vertex_shader_path {"triangle.vert"},
                .fragment_shader_path {"triangle.frag"},
                .topology {vk::PrimitiveTopology::eTriangleList}, // remove
                .polygon_mode {vk::PolygonMode::eFill},           // replace with dynamic state
                .cull_mode {vk::CullModeFlagBits::eNone},
                .front_face {vk::FrontFace::eClockwise}, // remove
                .depth_test_enable {vk::False},
                .depth_write_enable {vk::False},
                .depth_compare_op {}, // remove
                .color_format {gfx::core::Renderer::ColorFormat.format},
                .depth_format {}, // remove lmao?
                .blend_enable {vk::True},
                .name {"hacky triangle pipeline"},
            });

        while (!renderer.shouldWindowClose())
        {
            auto rec = [&](vk::CommandBuffer             commandBuffer,
                           u32                           swapchainImageIdx,
                           gfx::core::vulkan::Swapchain& swapchain,
                           std::size_t                   frameIndex)
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

                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        renderer.getDescriptorManager()->getGlobalPipelineLayout(),
                        0,
                        {renderer.getDescriptorManager()->getGlobalDescriptorSet()},
                        {});
                    commandBuffer.bindPipeline(
                        vk::PipelineBindPoint::eGraphics, renderer.getPipelineManager()->getPipeline(trianglePipeline));
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

            if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::ToggleCursorAttachment))
            {
                renderer.getWindow()->toggleCursor();
            }

            if (renderer.getWindow()->isActionActive(gfx::core::Window::Action::CloseWindow))
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
