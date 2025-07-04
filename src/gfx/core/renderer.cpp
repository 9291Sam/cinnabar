#include "renderer.hpp"
#include "util/logger.hpp"
#include "vulkan/allocator.hpp"
#include "vulkan/buffer.hpp"
#include "vulkan/descriptor_manager.hpp"
#include "vulkan/device.hpp"
#include "vulkan/frame_manager.hpp"
#include "vulkan/instance.hpp"
#include "vulkan/pipeline_manager.hpp"
#include "vulkan/swapchain.hpp"
#include "window.hpp"
#include <GLFW/glfw3.h>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::core
{

    Renderer::Renderer()
        : desired_present_mode {std::nullopt}
    {
#ifdef __APPLE__
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        ::setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1", 1);
#endif

        this->window = std::make_unique<Window>(
            std::map<Window::Action, Window::ActionInformation> {
                {Window::Action::PlayerMoveForward,
                 Window::ActionInformation {.key {GLFW_KEY_W}, .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveBackward,
                 Window::ActionInformation {.key {GLFW_KEY_S}, .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveLeft,
                 Window::ActionInformation {.key {GLFW_KEY_A}, .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveRight,
                 Window::ActionInformation {.key {GLFW_KEY_D}, .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveUp,
                 Window::ActionInformation {.key {GLFW_KEY_SPACE}, .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerMoveDown,
                 Window::ActionInformation {
                     .key {GLFW_KEY_LEFT_CONTROL}, .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::PlayerSprint,
                 Window::ActionInformation {
                     .key {GLFW_KEY_LEFT_SHIFT}, .method {Window::InteractionMethod::EveryFrame}}},

                {Window::Action::ToggleConsole,
                 Window::ActionInformation {
                     .key {GLFW_KEY_GRAVE_ACCENT}, .method {Window::InteractionMethod::SinglePress}}},

                {Window::Action::ResetPlayPosition,
                 Window::ActionInformation {.key {GLFW_KEY_K}, .method {Window::InteractionMethod::SinglePress}}},

                {Window::Action::ReloadShaders,
                 Window::ActionInformation {.key {GLFW_KEY_R}, .method {Window::InteractionMethod::SinglePress}}},

                {Window::Action::CloseWindow,
                 Window::ActionInformation {.key {GLFW_KEY_ESCAPE}, .method {Window::InteractionMethod::SinglePress}}},

                {Window::Action::ToggleCursorAttachment,
                 Window::ActionInformation {
                     .key {GLFW_KEY_BACKSLASH}, .method {Window::InteractionMethod::SinglePress}}},

            },
            vk::Extent2D {1920, 1080}, // NOLINT
            "cinnabar");
        this->instance           = std::make_unique<vulkan::Instance>();
        this->surface            = this->window->createSurface(**this->instance);
        this->device             = std::make_unique<vulkan::Device>(**this->instance, *this->surface);
        this->descriptor_manager = std::make_unique<vulkan::DescriptorManager>(*this->device);
        this->pipeline_manager   = std::make_unique<vulkan::PipelineManager>(
            *this->device, this->descriptor_manager->getGlobalPipelineLayout());
        this->allocator = std::make_unique<vulkan::Allocator>(*this->instance, *this->device);
        this->stager    = std::make_unique<vulkan::BufferStager>(this);

        this->critical_section = util::Mutex {this->makeCriticalSection()};

        log::debug("Created renderer");
    }

    Renderer::~Renderer() noexcept
    {
        this->device->getDevice().waitIdle();
    }

    bool Renderer::recordOnThread(
        util::TimestampStamper* maybeRenderThreadProfiler,
        std::function<void(
            vk::CommandBuffer, vk::QueryPool, u32, vulkan::Swapchain&, std::size_t, std::function<void()>)> recordFunc)
        const
    {
        bool resizeOcurred = false;

        this->critical_section.lock(
            [&](std::unique_ptr<RenderingCriticalSection>& lockedCriticalSection)
            {
                const std::expected<void, vulkan::Frame::ResizeNeeded> drawFrameResult =
                    lockedCriticalSection->frame_manager->recordAndDisplay(
                        maybeRenderThreadProfiler,
                        [&](std::size_t           flyingFrameIdx,
                            vk::QueryPool         queryPool,
                            vk::CommandBuffer     commandBuffer,
                            u32                   swapchainImageIdx,
                            std::function<void()> bufferFlushCallback)
                        {
                            recordFunc(
                                commandBuffer,
                                queryPool,
                                swapchainImageIdx,
                                *lockedCriticalSection->swapchain,
                                flyingFrameIdx,
                                std::move(bufferFlushCallback));
                        },
                        *this->stager);

                if (!drawFrameResult.has_value()
                    || this->should_resize_occur.exchange(false, std::memory_order_acq_rel))
                {
                    this->device->getDevice().waitIdle();

                    this->window->blockThisThreadWhileMinimized();

                    lockedCriticalSection.reset();

                    lockedCriticalSection = this->makeCriticalSection();

                    resizeOcurred = true;
                }
            });

        this->window->endFrame();

        this->time_alive += this->window->getDeltaTimeSeconds();

        this->frame_number.fetch_add(1);

        return resizeOcurred;
    }

    bool Renderer::shouldWindowClose() const noexcept
    {
        return this->window->shouldClose();
    }

    std::unique_ptr<Renderer::RenderingCriticalSection> Renderer::makeCriticalSection() const
    {
        std::unique_ptr<vulkan::Swapchain> swapchain = std::make_unique<vulkan::Swapchain>(
            *this->device,
            *this->surface,
            this->window->getFramebufferSize(),
            this->desired_present_mode.load(std::memory_order_acquire));

        std::unique_ptr<vulkan::FrameManager> frameManager =
            std::make_unique<vulkan::FrameManager>(*this->device, *swapchain);

        return std::make_unique<RenderingCriticalSection>(RenderingCriticalSection {
            .frame_manager {std::move(frameManager)},
            .swapchain {std::move(swapchain)},
        });
    }

    const vulkan::Instance* Renderer::getInstance() const noexcept
    {
        return &*this->instance;
    }

    const vulkan::Device* Renderer::getDevice() const noexcept
    {
        return &*this->device;
    }

    const vulkan::Allocator* Renderer::getAllocator() const noexcept
    {
        return &*this->allocator;
    }

    const vulkan::PipelineManager* Renderer::getPipelineManager() const noexcept
    {
        return &*this->pipeline_manager;
    }

    const vulkan::DescriptorManager* Renderer::getDescriptorManager() const noexcept
    {
        return &*this->descriptor_manager;
    }

    const vulkan::BufferStager& Renderer::getStager() const noexcept
    {
        return *this->stager;
    }

    const Window* Renderer::getWindow() const noexcept
    {
        return &*this->window;
    }

    u32 Renderer::getFrameNumber() const noexcept
    {
        return this->frame_number.load(std::memory_order_seq_cst);
    }

    f32 Renderer::getTimeAlive() const noexcept
    {
        return this->time_alive.load(std::memory_order_seq_cst);
    }

    void Renderer::setDesiredPresentMode(vk::PresentModeKHR newPresentMode) const noexcept
    {
        this->desired_present_mode.store(newPresentMode, std::memory_order_release);
        this->should_resize_occur.store(true, std::memory_order_release);
    }
} // namespace gfx::core
