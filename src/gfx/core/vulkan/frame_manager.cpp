#include "frame_manager.hpp"
#include "device.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include <expected>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

namespace gfx::core::vulkan
{
    static constexpr vk::SemaphoreCreateInfo SemaphoreCreateInfo {
        .sType {vk::StructureType::eSemaphoreCreateInfo}, .pNext {nullptr}, .flags {}};

    Frame::Frame(const Device& device_, vk::SwapchainKHR swapchain_, std::size_t number)
        : image_available {device_.getDevice().createSemaphoreUnique(SemaphoreCreateInfo)}
        , render_finished {device_.getDevice().createSemaphoreUnique(SemaphoreCreateInfo)}
        , frame_in_flight {std::make_shared<vk::UniqueFence>(device_.getDevice().createFenceUnique(
              vk::FenceCreateInfo {
                  .sType {vk::StructureType::eFenceCreateInfo},
                  .pNext {nullptr},
                  .flags {vk::FenceCreateFlagBits::eSignaled},
              }))}
        , command_pool {device_.getDevice().createCommandPoolUnique(
              vk::CommandPoolCreateInfo {
                  .sType {vk::StructureType::eCommandPoolCreateInfo},
                  .pNext {nullptr},
                  .flags {
                      vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer},
                  .queueFamilyIndex {device_ // NOLINT
                                         .getFamilyOfQueueType(Device::QueueType::Graphics)
                                         .value()},
              })}
        , should_profiling_query_pool_reset {true}
        , profiling_query_pool {device_.getDevice().createQueryPoolUnique(
              vk::QueryPoolCreateInfo {
                  .sType {vk::StructureType::eQueryPoolCreateInfo},
                  .pNext {nullptr},
                  .flags {},
                  .queryType {vk::QueryType::eTimestamp},
                  .queryCount {MaxQueriesPerFrame},
                  .pipelineStatistics {},
              })}
        , device {&device_}
        , swapchain {swapchain_}
    {
        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            const std::string queryPoolName      = std::format("Frame #{} Query Pool", number);
            const std::string commandPoolName    = std::format("Frame #{} Command Pool", number);
            const std::string imageAvailableName = std::format("Frame #{} Image Available Semaphore", number);
            const std::string renderFinishedName = std::format("Frame #{} Render Finished Semaphore", number);
            const std::string frameInFlightName  = std::format("Frame #{} Frame In Flight Fence", number);

            device->getDevice().setDebugUtilsObjectNameEXT(
                vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eSemaphore},
                    .objectHandle {std::bit_cast<u64>(*this->image_available)},
                    .pObjectName {imageAvailableName.c_str()},
                });

            device->getDevice().setDebugUtilsObjectNameEXT(
                vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eSemaphore},
                    .objectHandle {std::bit_cast<u64>(*this->render_finished)},
                    .pObjectName {renderFinishedName.c_str()},
                });

            device->getDevice().setDebugUtilsObjectNameEXT(
                vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eFence},
                    .objectHandle {std::bit_cast<u64>(**this->frame_in_flight)},
                    .pObjectName {frameInFlightName.c_str()},
                });

            device->getDevice().setDebugUtilsObjectNameEXT(
                vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eCommandPool},
                    .objectHandle {std::bit_cast<u64>(*this->command_pool)},
                    .pObjectName {commandPoolName.c_str()},
                });

            device->getDevice().setDebugUtilsObjectNameEXT(
                vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eQueryPool},
                    .objectHandle {std::bit_cast<u64>(*this->profiling_query_pool)},
                    .pObjectName {queryPoolName.c_str()},
                });
        }
    }

    Frame::~Frame() noexcept
    {
        this->device->getDevice().waitIdle();
    }

    std::expected<void, Frame::ResizeNeeded> Frame::recordAndDisplay(
        util::TimestampStamper*                                                           maybeRenderThreadProfiler,
        std::function<void(vk::CommandBuffer, vk::QueryPool, u32, std::function<void()>)> withCommandBuffer,
        const BufferStager&                                                               stager)
    {
        // Wait for this frame's fence to ensure we don't overwrite resources still in use
        vk::Result waitResult = this->device->getDevice().waitForFences(**this->frame_in_flight, vk::True, TimeoutNs);
        if (waitResult != vk::Result::eSuccess)
        {
            panic("Failed to wait for fence: {}", vk::to_string(waitResult));
        }

        if (maybeRenderThreadProfiler != nullptr)
        {
            maybeRenderThreadProfiler->stamp("fence wait");
        }

        // Acquire next swapchain image
        const auto [acquireImageResult, maybeNextImageIdx] =
            this->device->getDevice().acquireNextImageKHR(this->swapchain, TimeoutNs, *this->image_available, nullptr);

        switch (acquireImageResult)
        {
        case vk::Result::eSuccess:
            break;
        case vk::Result::eSuboptimalKHR:
            [[fallthrough]];
        case vk::Result::eErrorOutOfDateKHR:
            return std::unexpected(Frame::ResizeNeeded {});
        default:
            panic("acquireNextImage returned {}", vk::to_string(acquireImageResult));
        }

        if (maybeRenderThreadProfiler != nullptr)
        {
            maybeRenderThreadProfiler->stamp("acquire image");
        }

        // Reset command pool
        this->device->getDevice().resetCommandPool(*this->command_pool);

        // Allocate command buffer if needed or reset existing one
        if (!this->command_buffer)
        {
            const vk::CommandBufferAllocateInfo commandBufferAllocateInfo {
                .sType {vk::StructureType::eCommandBufferAllocateInfo},
                .pNext {nullptr},
                .commandPool {*this->command_pool},
                .level {vk::CommandBufferLevel::ePrimary},
                .commandBufferCount {1},
            };

            this->command_buffer =
                std::move(this->device->getDevice().allocateCommandBuffersUnique(commandBufferAllocateInfo).at(0));
        }

        // Begin command buffer recording
        const vk::CommandBufferBeginInfo commandBufferBeginInfo {
            .sType {vk::StructureType::eCommandBufferBeginInfo},
            .pNext {nullptr},
            .flags {vk::CommandBufferUsageFlagBits::eOneTimeSubmit},
            .pInheritanceInfo {nullptr},
        };

        this->command_buffer->begin(commandBufferBeginInfo);

        // Handle query pool reset
        vk::QueryPool activeQueryPool = *this->profiling_query_pool;
        if (this->should_profiling_query_pool_reset)
        {
            this->command_buffer->resetQueryPool(*this->profiling_query_pool, 0, MaxQueriesPerFrame);
            this->should_profiling_query_pool_reset = false;
            // Don't use query pool on the same frame we reset it
            activeQueryPool                         = vk::QueryPool {};
        }

        // Buffer flush callback
        std::function<void()> callback = [&]
        {
            stager.flushTransfers(*this->command_buffer, this->frame_in_flight);
        };

        // Record user commands
        withCommandBuffer(*this->command_buffer, activeQueryPool, maybeNextImageIdx, std::move(callback));

        // Reset fence for this frame, ensure its visible to stager.flushTransfesr()
        this->device->getDevice().resetFences(**this->frame_in_flight);

        this->command_buffer->end();

        if (maybeRenderThreadProfiler != nullptr)
        {
            maybeRenderThreadProfiler->stamp("record command buffer");
        }

        // Submit command buffer to graphics queue
        std::expected<void, Frame::ResizeNeeded> result;

        this->device->acquireQueue(
            Device::QueueType::Graphics,
            [&](vk::Queue queue)
            {
                const vk::PipelineStageFlags dstStageWaitFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;

                const vk::SubmitInfo queueSubmitInfo {
                    .sType {vk::StructureType::eSubmitInfo},
                    .pNext {nullptr},
                    .waitSemaphoreCount {1},
                    .pWaitSemaphores {&*this->image_available},
                    .pWaitDstStageMask {&dstStageWaitFlags},
                    .commandBufferCount {1},
                    .pCommandBuffers {&*this->command_buffer},
                    .signalSemaphoreCount {1},
                    .pSignalSemaphores {&*this->render_finished},
                };

                queue.submit(queueSubmitInfo, **this->frame_in_flight);

                if (maybeRenderThreadProfiler != nullptr)
                {
                    maybeRenderThreadProfiler->stamp("queue submit");
                }

                // Present the image
                const vk::PresentInfoKHR presentInfo {
                    .sType {vk::StructureType::ePresentInfoKHR},
                    .pNext {nullptr},
                    .waitSemaphoreCount {1},
                    .pWaitSemaphores {&*this->render_finished},
                    .swapchainCount {1},
                    .pSwapchains {&this->swapchain},
                    .pImageIndices {&maybeNextImageIdx},
                    .pResults {nullptr},
                };

                const vk::Result presentResult = queue.presentKHR(presentInfo);

                if (maybeRenderThreadProfiler != nullptr)
                {
                    maybeRenderThreadProfiler->stamp("present");
                }

                switch (presentResult)
                {
                case vk::Result::eSuccess:
                    result = {};
                    return;
                case vk::Result::eSuboptimalKHR:
                    [[fallthrough]];
                case vk::Result::eErrorOutOfDateKHR:
                    result = std::unexpected(Frame::ResizeNeeded {});
                    return;
                default:
                    panic("presentKHR returned {}", vk::to_string(presentResult));
                }
            });

        if (!result)
        {
            this->should_profiling_query_pool_reset = true;
            return result;
        }

        return {};
    }

    vk::Fence Frame::getFrameInFlightFence() const noexcept
    {
        return **this->frame_in_flight;
    }

    FrameManager::FrameManager(const Device& device_, vk::SwapchainKHR swapchain)
        : device {device_.getDevice()}
        , flying_frames {Frame {device_, swapchain, 0}, Frame {device_, swapchain, 1}, Frame {device_, swapchain, 2}}
        , current_frame_index {0}
    {
        log::debug("Created Frame Manager");
    }

    FrameManager::~FrameManager() noexcept = default;

    std::expected<void, Frame::ResizeNeeded> FrameManager::recordAndDisplay(
        util::TimestampStamper* maybeRenderThreadProfiler,
        std::function<void(std::size_t, vk::QueryPool, vk::CommandBuffer, u32, std::function<void()>)> recordFunc,
        const BufferStager&                                                                            stager)
    {
        Frame& currentFrame = this->flying_frames[this->current_frame_index];

        std::expected<void, Frame::ResizeNeeded> result = currentFrame.recordAndDisplay(
            maybeRenderThreadProfiler,
            [&](vk::CommandBuffer     commandBuffer,
                vk::QueryPool         queryPool,
                u32                   swapchainIndex,
                std::function<void()> bufferFlushCallback)
            {
                recordFunc(
                    this->current_frame_index,
                    queryPool,
                    commandBuffer,
                    swapchainIndex,
                    std::move(bufferFlushCallback));
            },
            stager);

        // Advance to next frame
        this->current_frame_index = (this->current_frame_index + 1) % FramesInFlight;

        return result;
    }
} // namespace gfx::core::vulkan