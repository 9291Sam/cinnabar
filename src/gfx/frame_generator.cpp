#include "frame_generator.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/imgui/imgui_renderer.hpp"
#include "gfx/generators/skybox/skybox_renderer.hpp"
#include "gfx/generators/triangle/triangle_renderer.hpp"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace gfx
{
    FrameGenerator::FrameGenerator(const core::Renderer* renderer_)
        : renderer {renderer_}
        , has_resize_ocurred {true}
        , frame_descriptors {this->createFrameDescriptors()}
    {}

    bool FrameGenerator::renderFrame(FrameGenerateArgs generators, gfx::Camera camera)
    {
        this->has_resize_ocurred = this->renderer->recordOnThread(
            [&](vk::CommandBuffer             commandBuffer,
                u32                           swapchainImageIdx,
                gfx::core::vulkan::Swapchain& swapchain,
                std::size_t)
            {
                const u32 graphicsQueueIndex =
                    this->renderer->getDevice()
                        ->getFamilyOfQueueType(gfx::core::vulkan::Device::QueueType::Graphics)
                        .value();

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

                commandBuffer.setViewport(0, {renderViewport});
                commandBuffer.setScissor(0, {scissor});

                if (this->has_resize_ocurred)
                {
                    this->renderer->getWindow()->attachCursor();

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

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
                        {},
                        {},
                        {},
                        {vk::ImageMemoryBarrier {
                            .sType {vk::StructureType::eImageMemoryBarrier},
                            .pNext {nullptr},
                            .srcAccessMask {vk::AccessFlagBits::eNone},
                            .dstAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentRead},
                            .oldLayout {vk::ImageLayout::eUndefined},
                            .newLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                            .srcQueueFamilyIndex {graphicsQueueIndex},
                            .dstQueueFamilyIndex {graphicsQueueIndex},
                            .image {*this->frame_descriptors.depth_buffer},
                            .subresourceRange {vk::ImageSubresourceRange {
                                .aspectMask {vk::ImageAspectFlagBits::eDepth},
                                .baseMipLevel {0},
                                .levelCount {1},
                                .baseArrayLayer {0},
                                .layerCount {1}}},
                        }});

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {},
                        {},
                        {},
                        {vk::ImageMemoryBarrier {
                            .sType {vk::StructureType::eImageMemoryBarrier},
                            .pNext {nullptr},
                            .srcAccessMask {vk::AccessFlagBits::eNone},
                            .dstAccessMask {vk::AccessFlagBits::eShaderRead},
                            .oldLayout {vk::ImageLayout::eUndefined},
                            .newLayout {vk::ImageLayout::eGeneral},
                            .srcQueueFamilyIndex {graphicsQueueIndex},
                            .dstQueueFamilyIndex {graphicsQueueIndex},
                            .image {*this->frame_descriptors.imgui_render_target},
                            .subresourceRange {vk::ImageSubresourceRange {
                                .aspectMask {vk::ImageAspectFlagBits::eColor},
                                .baseMipLevel {0},
                                .levelCount {1},
                                .baseArrayLayer {0},
                                .layerCount {1}}},
                        }});
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

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentRead},
                        .dstAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentWrite},
                        .oldLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                        .newLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {*this->frame_descriptors.depth_buffer},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eDepth},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eShaderRead},
                        .dstAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                        .oldLayout {vk::ImageLayout::eGeneral},
                        .newLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {*this->frame_descriptors.imgui_render_target},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                // ImGui
                {
                    const vk::RenderingAttachmentInfo colorAttachmentInfo {
                        .sType {vk::StructureType::eRenderingAttachmentInfo},
                        .pNext {nullptr},
                        .imageView {this->frame_descriptors.imgui_render_target.getView()},
                        .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .resolveMode {vk::ResolveModeFlagBits::eNone},
                        .resolveImageView {nullptr},
                        .resolveImageLayout {},
                        .loadOp {vk::AttachmentLoadOp::eClear},
                        .storeOp {vk::AttachmentStoreOp::eStore},
                        .clearValue {
                            vk::ClearColorValue {.float32 {{0.0f, 0.0f, 0.0f, 0.0f}}},
                        },
                    };

                    const vk::RenderingInfo imguiRenderingInfo {
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

                    commandBuffer.beginRendering(imguiRenderingInfo);

                    if (generators.maybe_imgui_renderer)
                    {
                        generators.maybe_imgui_renderer->renderIntoCommandBuffer(commandBuffer, camera);

                        commandBuffer.setViewport(0, {renderViewport});
                        commandBuffer.setScissor(0, {scissor});
                    }

                    commandBuffer.endRendering();
                }

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eShaderWrite},
                        .dstAccessMask {vk::AccessFlagBits::eShaderRead},
                        .oldLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .newLayout {vk::ImageLayout::eGeneral},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {*this->frame_descriptors.imgui_render_target},
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

                    const vk::RenderingAttachmentInfo depthAttachmentInfo {
                        .sType {vk::StructureType::eRenderingAttachmentInfo},
                        .pNext {nullptr},
                        .imageView {this->frame_descriptors.depth_buffer.getView()},
                        .imageLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                        .resolveMode {vk::ResolveModeFlagBits::eNone},
                        .resolveImageView {nullptr},
                        .resolveImageLayout {},
                        .loadOp {vk::AttachmentLoadOp::eClear},
                        .storeOp {vk::AttachmentStoreOp::eStore},
                        .clearValue {
                            .depthStencil {vk::ClearDepthStencilValue {.depth {0.0}, .stencil {0}}},
                        }};

                    const vk::RenderingInfo simpleColorRenderingInfo {
                        .sType {vk::StructureType::eRenderingInfo},
                        .pNext {nullptr},
                        .flags {},
                        .renderArea {scissor},
                        .layerCount {1},
                        .viewMask {0},
                        .colorAttachmentCount {1},
                        .pColorAttachments {&colorAttachmentInfo},
                        .pDepthAttachment {&depthAttachmentInfo},
                        .pStencilAttachment {nullptr},
                    };

                    commandBuffer.beginRendering(simpleColorRenderingInfo);

                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
                        0,
                        {this->renderer->getDescriptorManager()->getGlobalDescriptorSet()},
                        {});

                    if (generators.maybe_triangle_renderer)
                    {
                        generators.maybe_triangle_renderer->renderIntoCommandBuffer(commandBuffer, camera);
                    }

                    if (generators.maybe_skybox_renderer)
                    {
                        generators.maybe_skybox_renderer->renderIntoCommandBuffer(commandBuffer, camera);
                    }

                    if (generators.maybe_voxel_renderer)
                    {
                        generators.maybe_voxel_renderer->renderIntoCommandBuffer(commandBuffer, camera);
                    }

                    if (generators.maybe_imgui_renderer)
                    {
                        generators.maybe_imgui_renderer->renderImageCopyIntoCommandBuffer(
                            commandBuffer,
                            this->frame_descriptors.imgui_render_target.getStorageDescriptor(
                                vk::ImageLayout::eGeneral));
                    }

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

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eLateFragmentTests,
                    vk::PipelineStageFlagBits::eLateFragmentTests,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentWrite},
                        .dstAccessMask {vk::AccessFlagBits::eDepthStencilAttachmentRead},
                        .oldLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                        .newLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {*this->frame_descriptors.depth_buffer},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eDepth},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});
            });

        if (this->has_resize_ocurred)
        {
            this->frame_descriptors = this->createFrameDescriptors();
        }

        if (this->renderer->getPipelineManager()->couldAnyShadersReload())
        {
            this->renderer->getDevice()->getDevice().waitIdle();

            this->renderer->getPipelineManager()->reloadShaders();
        }

        return this->has_resize_ocurred;
    }

    FrameGenerator::FrameDescriptors FrameGenerator::createFrameDescriptors()
    {
        return FrameDescriptors {
            .depth_buffer {
                renderer,
                renderer->getWindow()->getFramebufferSize(),
                vk::Format::eD32Sfloat,
                vk::ImageLayout::eUndefined,
                vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                vk::ImageAspectFlagBits::eDepth,
                vk::ImageTiling::eOptimal,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                "Global Depth Buffer"},
            .imgui_render_target {
                renderer,
                renderer->getWindow()->getFramebufferSize(),
                vk::Format::eB8G8R8A8Unorm,
                vk::ImageLayout::eUndefined,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage,
                vk::ImageAspectFlagBits::eColor,
                vk::ImageTiling::eOptimal,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                "Imgui Render Target"}};
    }
} // namespace gfx