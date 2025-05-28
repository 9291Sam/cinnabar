#pragma once

#include "util/util.hpp"
#include <functional>
#include <memory>
#include <util/threads.hpp>
#include <vulkan/vulkan.hpp>

namespace gfx::core
{
    class Window;

    namespace vulkan
    {
        class Instance;
        class Device;
        class Allocator;
        class FrameManager;
        class Swapchain;
        class PipelineManager;
        class DescriptorManager;
        class BufferStager;
    } // namespace vulkan

    class Renderer
    {
    public:
        static constexpr vk::SurfaceFormatKHR ColorFormat =
            vk::SurfaceFormatKHR {.format {vk::Format::eB8G8R8A8Srgb}, .colorSpace {vk::ColorSpaceKHR::eSrgbNonlinear}};
        static constexpr vk::Format DepthFormat = vk::Format::eD32Sfloat;

        struct FrameRecordArguments
        {
            vk::CommandBuffer  command_buffer;
            u32                swapchain_idx;
            vulkan::Swapchain* swapchain;
            std::size_t        global_frame_number;
        };

        struct FrameBuilder
        {
            virtual ~FrameBuilder() = 0;
            void recordFrame(FrameRecordArguments);
        };
    public:
        Renderer();
        ~Renderer() noexcept;

        Renderer(const Renderer&)             = delete;
        Renderer(Renderer&&)                  = delete;
        Renderer& operator= (const Renderer&) = delete;
        Renderer& operator= (Renderer&&)      = delete;

        // Returns true if a resize occurred
        // Command buffer, query pool,  swapchain idx, swapchain, frame idx, buffer flush callback
        bool recordOnThread(
            std::function<void(
                vk::CommandBuffer, vk::QueryPool, u32, vulkan::Swapchain&, std::size_t, std::function<void()>)>) const;
        [[nodiscard]] bool shouldWindowClose() const noexcept;

        // TODO: don't actually expose these, just give the pass through functions that are required
        [[nodiscard]] const vulkan::Instance*          getInstance() const noexcept;
        [[nodiscard]] const vulkan::Device*            getDevice() const noexcept;
        [[nodiscard]] const vulkan::Allocator*         getAllocator() const noexcept;
        [[nodiscard]] const vulkan::DescriptorManager* getDescriptorManager() const noexcept;
        [[nodiscard]] const vulkan::PipelineManager*   getPipelineManager() const noexcept;
        [[nodiscard]] const Window*                    getWindow() const noexcept;
        [[nodiscard]] const vulkan::BufferStager&      getStager() const noexcept;
        [[nodiscard]] u32                              getFrameNumber() const noexcept;
        [[nodiscard]] f32                              getTimeAlive() const noexcept;

        void setDesiredPresentMode(vk::PresentModeKHR) const noexcept;

    private:
        struct RenderingCriticalSection
        {
            std::unique_ptr<vulkan::FrameManager> frame_manager;
            std::unique_ptr<vulkan::Swapchain>    swapchain;
        };

        std::unique_ptr<RenderingCriticalSection> makeCriticalSection() const;

        std::unique_ptr<Window> window;

        std::unique_ptr<vulkan::Instance>          instance;
        vk::UniqueSurfaceKHR                       surface;
        std::unique_ptr<vulkan::Device>            device;
        std::unique_ptr<vulkan::DescriptorManager> descriptor_manager;
        std::unique_ptr<vulkan::PipelineManager>   pipeline_manager;
        std::unique_ptr<vulkan::Allocator>         allocator;
        std::unique_ptr<vulkan::BufferStager>      stager;

        util::Mutex<std::unique_ptr<RenderingCriticalSection>> critical_section;

        mutable std::atomic<f32> time_alive;
        mutable std::atomic<u32> frame_number;

        mutable std::atomic<std::optional<vk::PresentModeKHR>> desired_present_mode;
        mutable std::atomic<bool>                              should_resize_occur;
    };
} // namespace gfx::core
