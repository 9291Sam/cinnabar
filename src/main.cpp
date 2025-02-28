
#include "gfx/render/renderer.hpp"
#include "gfx/render/vulkan/device.hpp"
#include "gfx/render/vulkan/instance.hpp"
#include "gfx/render/vulkan/swapchain.hpp"
#include "gfx/render/window.hpp"
#include "util/logger.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <vulkan/vulkan_structs.hpp>

// void comp()
// {
//     shaderc::CompileOptions options {};
//     options.SetSourceLanguage(shaderc_source_language_glsl);
//     options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
//     options.SetTargetSpirv(shaderc_spirv_version_1_6);
//     options.SetOptimizationLevel(shaderc_optimization_level_performance);
//     options.SetGenerateDebugInfo();

//     std::string string = R"(
// #version 460

// #extension GL_EXT_shader_explicit_arithmetic_types : require
// #extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
// #extension GL_EXT_nonuniform_qualifier : require
// #extension GL_KHR_shader_subgroup_basic : require
// #extension GL_KHR_shader_subgroup_ballot : require
// #extension GL_KHR_shader_subgroup_quad : require

// layout(local_size_x = 256) in;

// layout(std430, binding = 0) buffer DataBuffer {
//     uint data[];
// };

// void main() {
//     uint idx = gl_GlobalInvocationID.x;
//     data[idx] += 1;
// }

//         )";

//     shaderc::Compiler                compiler {};
//     shaderc::CompilationResult<char> compileResult =
//         compiler.CompileGlslToSpvAssembly(string, shaderc_compute_shader, "", options);

//     if (compileResult.GetCompilationStatus() != shaderc_compilation_status_success)
//     {
//         log::error(
//             "compile failed: {} {}",
//             static_cast<int>(compileResult.GetCompilationStatus()),
//             compileResult.GetErrorMessage());
//     }
//     else
//     {
//         std::span<const char> data {compileResult.cbegin(), compileResult.cend()};

//         std::array<char, 16> hexChars {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
//         'F'}; std::string          result {}; result.reserve(data.size() * 6);

//         for (char d : data)
//         {
//             const u32 c = static_cast<u8>(d);

//             result.push_back('0');
//             result.push_back('x');
//             result += hexChars[static_cast<u32>(c & 0xF0u) >> 4u];
//             result += hexChars[static_cast<u32>(c & 0x0Fu) >> 0u];
//             result.push_back(',');
//             result.push_back(' ');
//         }

//         if (!result.empty())
//         {
//             result.pop_back();
//             result.pop_back();
//         }

//         log::info("{}", result);
//     }
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

        gfx::render::Renderer renderer {};
        bool                  hasResizeOccurred = false;
        const u32             graphicsQueueIndex =
            renderer.getDevice()->getFamilyOfQueueType(gfx::render::vulkan::Device::QueueType::Graphics).value();

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
                    commandBuffer.beginRendering(simpleColorRenderingInfo);

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
