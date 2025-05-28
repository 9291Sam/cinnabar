#include "frame_generator.hpp"
#include "core/vulkan/frame_manager.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/frame_manager.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/imgui/imgui_renderer.hpp"
#include "gfx/generators/skybox/skybox_renderer.hpp"
#include "gfx/generators/triangle/triangle_renderer.hpp"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include "gfx/shader_common/bindings.slang"
#include "util/events.hpp"
#include "util/logger.hpp"
#include <ranges>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx
{
    FrameGenerator::FrameGenerator(const core::Renderer* renderer_)
        : renderer {renderer_}
        , has_resize_ocurred {true}
        , frame_descriptors {this->createFrameDescriptors()}
        , global_gpu_data {
              this->renderer,
              vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Global Data",
              UBO_GLOBAL_DATA}
    {}

    bool FrameGenerator::renderFrame(FrameGenerateArgs generators, gfx::Camera camera)
    {
        const vk::Extent2D framebufferSize = this->renderer->getWindow()->getFramebufferSize();

        if (std::optional<bool> enableReflections = util::receive<bool>("SETTING_ENABLE_REFLECTIONS"))
        {
            this->are_reflections_enabled = *enableReflections;
        }

        const GlobalGpuData thisFrameGlobalGpuData {
            .view_matrix {camera.getViewMatrix()},
            .projection_matrix {camera.getProjectionMatrix()},
            .view_projection_matrix {camera.getProjectionMatrix() * camera.getViewMatrix()},
            .camera_forward_vector {glm::vec4 {camera.getForwardVector(), 0.0}},
            .camera_right_vector {glm::vec4 {camera.getRightVector(), 0.0}},
            .camera_up_vector {glm::vec4 {camera.getUpVector(), 0.0}},
            .camera_position {glm::vec4 {camera.getPosition(), 0.0}},
            .fov_y {camera.getFovYRadians()},
            .tan_half_fov_y {std::tan(0.5f * camera.getFovYRadians())},
            .aspect_ratio {camera.getAspectRatio()},
            .time_alive {this->renderer->getTimeAlive()},
            .framebuffer_size {glm::uvec2 {framebufferSize.width, framebufferSize.height}},
            .bool_enable_reflections {static_cast<u32>(this->are_reflections_enabled)},
            .bool_moved_this_frame {
                static_cast<u32>(camera.getPosition() != this->maybe_previous_frame_camera_position)}};

        this->maybe_previous_frame_camera_position = camera.getPosition();

        this->renderer->getStager().enqueueTransfer(this->global_gpu_data, 0, {&thisFrameGlobalGpuData, 1});

        if (generators.maybe_voxel_renderer != nullptr)
        {
            generators.maybe_voxel_renderer->preFrameUpdate();
        }

        this->has_resize_ocurred = this->renderer->recordOnThread(
            [&](vk::CommandBuffer             commandBuffer,
                vk::QueryPool                 queryPool,
                u32                           swapchainImageIdx,
                gfx::core::vulkan::Swapchain& swapchain,
                std::size_t,
                std::function<void()> bufferFlushCallback)
            {
                if (queryPool != nullptr)
                {
                    std::vector<u64> queryResultData {};
                    queryResultData.resize(core::vulkan::MaxQueriesPerFrame);

                    this->renderer->getDevice()->getDevice().getQueryPoolResults(
                        queryPool,
                        0,
                        core::vulkan::MaxQueriesPerFrame,
                        std::span {queryResultData}.size_bytes(),
                        queryResultData.data(),
                        sizeof(u64),
                        vk::QueryResultFlagBits::e64);

                    // std::string out {};

                    usize lastElementIdx = 0;

                    for (const auto [idx, i] : std::ranges::enumerate_view {queryResultData})
                    {
                        // HACK: get proper things from the array of string views
                        if (i == 0)
                        {
                            continue;
                        }

                        lastElementIdx = idx;
                    }

                    std::array<u64, core::vulkan::MaxQueriesPerFrame> timeStampDeltas {};
                    std::ranges::fill(timeStampDeltas, 0);

                    for (usize i = 1; i <= lastElementIdx; ++i)
                    {
                        const u64 prev = queryResultData[i - 1];
                        const u64 curr = queryResultData[i];

                        timeStampDeltas[i] = (curr - prev);

                        // out += std::format("{} {}, ", this->active_timestamp_names[i], curr - prev);
                    }

                    util::send<std::array<u64, core::vulkan::MaxQueriesPerFrame>>(
                        "GPU_PER_FRAME_TIMESTAMPS", timeStampDeltas);

                    // if (!out.empty())
                    // {
                    //     out.pop_back();
                    //     out.pop_back();
                    // }

                    commandBuffer.resetQueryPool(queryPool, 0, gfx::core::vulkan::MaxQueriesPerFrame);
                }

                u32 nextTimeStampIndex = 0;

                auto writeTimeStamp = [&](std::string_view name)
                {
                    // We need the assert in both cases but this sucks whatever
                    if (this->renderer->getFrameNumber() == 0 || queryPool != nullptr)
                    {
                        assert::critical(
                            nextTimeStampIndex < core::vulkan::MaxQueriesPerFrame,
                            "Tried to write too many timestamps with {}",
                            name);
                    }

                    if (this->renderer->getFrameNumber() == 0)
                    {
                        this->active_timestamp_names[nextTimeStampIndex] = name;
                    }

                    if (queryPool != nullptr)
                    {
                        commandBuffer.writeTimestamp(
                            vk::PipelineStageFlagBits::eBottomOfPipe, queryPool, nextTimeStampIndex);
                    }

                    nextTimeStampIndex += 1;
                };

                writeTimeStamp("frame base");

                bufferFlushCallback();

                writeTimeStamp("buffer flushes");

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

                if (generators.maybe_voxel_renderer != nullptr)
                {
                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eCompute,
                        this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
                        0,
                        {this->renderer->getDescriptorManager()->getGlobalDescriptorSet()},
                        {});

                    generators.maybe_voxel_renderer->recordCopyCommands(commandBuffer);
                }

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eAllCommands,
                    vk::DependencyFlags {},
                    {vk::MemoryBarrier {
                        .sType {vk::StructureType::eMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eTransferWrite},
                        .dstAccessMask {vk::AccessFlagBits::eMemoryRead},
                    }},
                    {},
                    {});

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eAllCommands,
                    vk::DependencyFlags {},
                    {vk::MemoryBarrier {
                        .sType {vk::StructureType::eMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eShaderWrite},
                        .dstAccessMask {vk::AccessFlagBits::eMemoryRead},
                    }},
                    {},
                    {});

                writeTimeStamp("voxel copy commands");

                if (this->has_resize_ocurred)
                {
                    this->renderer->getWindow()->attachCursor();

                    const std::span<const vk::Image> swapchainImages = swapchain.getImages();

                    std::vector<vk::ImageMemoryBarrier> swapchainMemoryBarriers {};
                    swapchainMemoryBarriers.reserve(swapchainImages.size());

                    for (const vk::Image i : swapchainImages)
                    {
                        swapchainMemoryBarriers.push_back(
                            vk::ImageMemoryBarrier {
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
                            .image {*this->frame_descriptors->depth_buffer},
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
                            .image {*this->frame_descriptors->imgui_render_target},
                            .subresourceRange {vk::ImageSubresourceRange {
                                .aspectMask {vk::ImageAspectFlagBits::eColor},
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
                            .image {*this->frame_descriptors->voxel_render_target},
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
                        .image {*this->frame_descriptors->depth_buffer},
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
                        .image {*this->frame_descriptors->voxel_render_target},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
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
                        .image {*this->frame_descriptors->imgui_render_target},
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
                        .imageView {this->frame_descriptors->imgui_render_target.getView()},
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
                        generators.maybe_imgui_renderer->renderIntoCommandBuffer(commandBuffer, camera, swapchain);

                        commandBuffer.setViewport(0, {renderViewport});
                        commandBuffer.setScissor(0, {scissor});
                    }

                    commandBuffer.endRendering();
                }

                writeTimeStamp("imgui");

                // Voxel Prepass
                {
                    const vk::RenderingAttachmentInfo colorAttachmentInfo {
                        .sType {vk::StructureType::eRenderingAttachmentInfo},
                        .pNext {nullptr},
                        .imageView {this->frame_descriptors->voxel_render_target.getView()},
                        .imageLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .resolveMode {vk::ResolveModeFlagBits::eNone},
                        .resolveImageView {nullptr},
                        .resolveImageLayout {},
                        .loadOp {vk::AttachmentLoadOp::eClear},
                        .storeOp {vk::AttachmentStoreOp::eStore},
                        .clearValue {
                            vk::ClearColorValue {.uint32 {{~0u, ~0u, ~0u, ~0u}}},
                        },
                    };

                    const vk::RenderingAttachmentInfo depthAttachmentInfo {
                        .sType {vk::StructureType::eRenderingAttachmentInfo},
                        .pNext {nullptr},
                        .imageView {this->frame_descriptors->depth_buffer.getView()},
                        .imageLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                        .resolveMode {vk::ResolveModeFlagBits::eNone},
                        .resolveImageView {nullptr},
                        .resolveImageLayout {},
                        .loadOp {vk::AttachmentLoadOp::eClear},
                        .storeOp {vk::AttachmentStoreOp::eStore},
                        .clearValue {
                            .depthStencil {vk::ClearDepthStencilValue {.depth {0.0}, .stencil {0}}},
                        }};

                    const vk::RenderingInfo voxelRenderPassInfo {
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

                    commandBuffer.beginRendering(voxelRenderPassInfo);

                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
                        0,
                        {this->renderer->getDescriptorManager()->getGlobalDescriptorSet()},
                        {});

                    if (generators.maybe_voxel_renderer)
                    {
                        generators.maybe_voxel_renderer->recordPrepass(commandBuffer, camera);
                    }

                    commandBuffer.endRendering();
                }

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags {},
                    {},
                    {},
                    {vk::ImageMemoryBarrier {
                        .sType {vk::StructureType::eImageMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eColorAttachmentWrite},
                        .dstAccessMask {vk::AccessFlagBits::eShaderRead},
                        .oldLayout {vk::ImageLayout::eColorAttachmentOptimal},
                        .newLayout {vk::ImageLayout::eGeneral},
                        .srcQueueFamilyIndex {graphicsQueueIndex},
                        .dstQueueFamilyIndex {graphicsQueueIndex},
                        .image {*this->frame_descriptors->voxel_render_target},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eLateFragmentTests,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
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
                        .image {*this->frame_descriptors->depth_buffer},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eDepth},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                writeTimeStamp("voxel prepass");

                // voxel color calculation
                {
                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eCompute,
                        this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
                        0,
                        {this->renderer->getDescriptorManager()->getGlobalDescriptorSet()},
                        {});

                    if (generators.maybe_voxel_renderer)
                    {
                        generators.maybe_voxel_renderer->recordColorCalculation(commandBuffer);
                    }
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
                        .image {*this->frame_descriptors->imgui_render_target},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eColor},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags {},
                    {vk::MemoryBarrier {
                        .sType {vk::StructureType::eMemoryBarrier},
                        .pNext {nullptr},
                        .srcAccessMask {vk::AccessFlagBits::eShaderWrite},
                        .dstAccessMask {vk::AccessFlagBits::eShaderRead},
                    }},
                    {},
                    {});

                writeTimeStamp("voxel color calculation");

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
                        .imageView {this->frame_descriptors->depth_buffer.getView()},
                        .imageLayout {vk::ImageLayout::eDepthAttachmentOptimal},
                        .resolveMode {vk::ResolveModeFlagBits::eNone},
                        .resolveImageView {nullptr},
                        .resolveImageLayout {},
                        .loadOp {vk::AttachmentLoadOp::eLoad},
                        .storeOp {vk::AttachmentStoreOp::eStore},
                        .clearValue {},
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

                    if (generators.maybe_voxel_renderer)
                    {
                        generators.maybe_voxel_renderer->recordColorTransfer(commandBuffer);
                    }

                    writeTimeStamp("voxel color transfer");

                    if (generators.maybe_triangle_renderer)
                    {
                        generators.maybe_triangle_renderer->renderIntoCommandBuffer(commandBuffer, camera);
                    }

                    writeTimeStamp("triangles color transfer");

                    if (generators.maybe_skybox_renderer)
                    {
                        generators.maybe_skybox_renderer->renderIntoCommandBuffer(commandBuffer, camera);
                    }

                    writeTimeStamp("skybox render");

                    if (generators.maybe_imgui_renderer)
                    {
                        generators.maybe_imgui_renderer->renderImageCopyIntoCommandBuffer(commandBuffer);
                    }

                    writeTimeStamp("imgui copy");

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
                        .image {*this->frame_descriptors->depth_buffer},
                        .subresourceRange {vk::ImageSubresourceRange {
                            .aspectMask {vk::ImageAspectFlagBits::eDepth},
                            .baseMipLevel {0},
                            .levelCount {1},
                            .baseArrayLayer {0},
                            .layerCount {1}}},
                    }});

                if (this->renderer->getFrameNumber() == 0)
                {
                    util::send<std::array<std::string_view, core::vulkan::MaxQueriesPerFrame>>(
                        "GPU_TIMESTAMP_NAMES", this->active_timestamp_names);
                }
            });

        if (this->has_resize_ocurred)
        {
            this->frame_descriptors.reset();

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
                vk::ImageLayout::eDepthReadOnlyOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::ImageAspectFlagBits::eDepth,
                vk::ImageTiling::eOptimal,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                "Global Depth Buffer",
                std::nullopt},
            .imgui_render_target {
                renderer,
                renderer->getWindow()->getFramebufferSize(),
                vk::Format::eB8G8R8A8Unorm,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage,
                vk::ImageAspectFlagBits::eColor,
                vk::ImageTiling::eOptimal,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                "Imgui Render Target",
                SIO_IMGUI_RENDER_TARGET},
            .voxel_render_target {
                renderer,
                renderer->getWindow()->getFramebufferSize(),
                vk::Format::eR32G32B32A32Sfloat,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage,
                vk::ImageAspectFlagBits::eColor,
                vk::ImageTiling::eOptimal,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                "Voxel Render Target",
                SIO_VOXEL_RENDER_TARGET}};
    }
} // namespace gfx