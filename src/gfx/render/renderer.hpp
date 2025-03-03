#pragma once

#include "util/util.hpp"
#include "vulkan/buffer.hpp" // TODO: get rid of this include
#include <functional>
#include <memory>
#include <util/threads.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::render
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
        // Command buffer, swapchain idx, swapchain, frame idx
        bool recordOnThread(std::function<void(vk::CommandBuffer, u32, vulkan::Swapchain&, std::size_t)>) const;
        [[nodiscard]] bool shouldWindowClose() const noexcept;

        [[nodiscard]] const vulkan::Instance*          getInstance() const noexcept;
        [[nodiscard]] const vulkan::Device*            getDevice() const noexcept;
        [[nodiscard]] const vulkan::Allocator*         getAllocator() const noexcept;
        [[nodiscard]] const vulkan::DescriptorManager* getDescriptorManager() const noexcept;
        [[nodiscard]] const vulkan::PipelineManager*   getPipelineManager() const noexcept;
        [[nodiscard]] const Window*                    getWindow() const noexcept;
        [[nodiscard]] const vulkan::BufferStager&      getStager() const noexcept;
        [[nodiscard]] u32                              getFrameNumber() const noexcept;
        [[nodiscard]] float                            getTimeAlive() const noexcept;

    private:
        struct RenderingCriticalSection
        {
            std::unique_ptr<vulkan::FrameManager> frame_manager;
            std::unique_ptr<vulkan::Swapchain>    swapchain;
        };

        static std::unique_ptr<RenderingCriticalSection>
        makeCriticalSection(const vulkan::Device&, vk::SurfaceKHR, const Window&);

        std::unique_ptr<Window> window;

        std::unique_ptr<vulkan::Instance>          instance;
        vk::UniqueSurfaceKHR                       surface;
        std::unique_ptr<vulkan::Device>            device;
        std::unique_ptr<vulkan::Allocator>         allocator;
        std::unique_ptr<vulkan::BufferStager>      stager;
        std::unique_ptr<vulkan::DescriptorManager> descriptor_manager;
        std::unique_ptr<vulkan::PipelineManager>   pipeline_manager;

        util::Mutex<std::unique_ptr<RenderingCriticalSection>> critical_section;

        mutable std::atomic<float> time_alive;
        mutable std::atomic<u32>   frame_number;
    };
} // namespace gfx::render
