#pragma once

#include "util/task_generator.hpp"
#include "util/util.hpp"
#include <chrono>
#include <cstddef>
#include <functional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::core::vulkan
{
    class BufferStager;
    class Device;
    class Swapchain;

    static constexpr u32         FramesInFlight = 3;
    static constexpr std::size_t TimeoutNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<std::size_t> {7}).count();

    static constexpr u32 MaxQueriesPerFrame = 32;

    class Frame
    {
    public:
        struct ResizeNeeded
        {};

    public:
        Frame(const Device&, vk::SwapchainKHR, std::size_t number);
        ~Frame() noexcept;

        Frame(const Frame&)             = delete;
        Frame(Frame&&)                  = delete;
        Frame& operator= (const Frame&) = delete;
        Frame& operator= (Frame&&)      = delete;

        [[nodiscard]] std::expected<void, Frame::ResizeNeeded> recordAndDisplay(
            util::TimestampStamper* maybeRenderThreadProfiler,
            // u32 is the swapchain image's index
            std::function<void(vk::CommandBuffer, vk::QueryPool, u32, std::function<void()>)>,
            const BufferStager&,
            const std::vector<vk::UniqueSemaphore>& renderFinishedSemaphore);

        [[nodiscard]] vk::Fence getFrameInFlightFence() const noexcept;

    private:
        vk::UniqueSemaphore              image_available;
        std::shared_ptr<vk::UniqueFence> frame_in_flight;

        vk::UniqueCommandPool   command_pool;
        vk::UniqueCommandBuffer command_buffer;

        bool                should_profiling_query_pool_reset;
        vk::UniqueQueryPool profiling_query_pool;

        const vulkan::Device* device;
        vk::SwapchainKHR      swapchain;
    };

    class FrameManager
    {
    public:
        FrameManager(const Device&, const Swapchain&);
        ~FrameManager() noexcept;

        FrameManager(const FrameManager&)             = delete;
        FrameManager(FrameManager&&)                  = delete;
        FrameManager& operator= (const FrameManager&) = delete;
        FrameManager& operator= (FrameManager&&)      = delete;

        // std::size_t is the flying frame index
        // u32 is the swapchain image's index
        // the void function is a callback for flushing buffers
        [[nodiscard]] std::expected<void, Frame::ResizeNeeded> recordAndDisplay(
            util::TimestampStamper* maybeRenderThreadProfiler,
            std::function<void(std::size_t, vk::QueryPool, vk::CommandBuffer, u32, std::function<void()>)>,
            const BufferStager&);

    private:
        vk::Device                        device;
        std::vector<vk::UniqueSemaphore>  present_ready_semaphores;
        std::array<Frame, FramesInFlight> flying_frames;
        std::size_t                       current_frame_index;
    };

} // namespace gfx::core::vulkan