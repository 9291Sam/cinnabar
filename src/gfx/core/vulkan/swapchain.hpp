#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace gfx::core::vulkan
{
    class Device;

    class Swapchain
    {
    public:
        Swapchain(
            const Device&,
            vk::SurfaceKHR,
            vk::Extent2D,
            std::optional<vk::PresentModeKHR> maybeDesiredPresentMode = std::nullopt);
        ~Swapchain() noexcept = default;

        Swapchain(const Swapchain&)             = delete;
        Swapchain(Swapchain&&)                  = delete;
        Swapchain& operator= (const Swapchain&) = delete;
        Swapchain& operator= (Swapchain&&)      = delete;

        [[nodiscard]] std::span<const vk::ImageView>      getViews() const noexcept;
        [[nodiscard]] std::span<const vk::Image>          getImages() const noexcept;
        [[nodiscard]] vk::Extent2D                        getExtent() const noexcept;
        [[nodiscard]] vk::Format                          getFormat() const noexcept;
        [[nodiscard]] std::span<const vk::PresentModeKHR> getPresentModes() const noexcept;
        [[nodiscard]] vk::PresentModeKHR                  getActivePresentMode() const noexcept;
        [[nodiscard]] vk::SwapchainKHR                    operator* () const noexcept;

    private:
        std::vector<vk::UniqueImageView> image_views;
        std::vector<vk::ImageView>       dense_image_views;
        std::vector<vk::Image>           images;
        std::vector<vk::PresentModeKHR>  present_modes;
        vk::PresentModeKHR               active_present_mode;
        vk::UniqueSwapchainKHR           swapchain;
        vk::Extent2D                     extent;
    };
} // namespace gfx::core::vulkan
