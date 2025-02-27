#include "swapchain.hpp"
#include "device.hpp"
#include "gfx/render/renderer.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::render::vulkan
{
    Swapchain::Swapchain(const Device& device, vk::SurfaceKHR surface, vk::Extent2D extent_)
        : extent {extent_}
    {
        const std::vector<vk::SurfaceFormatKHR> availableSurfaceFormats =
            device.getPhysicalDevice().getSurfaceFormatsKHR(surface);

        assert::critical(
            std::ranges::find(availableSurfaceFormats, Renderer::ColorFormat)
                != availableSurfaceFormats.cend(),
            "Required surface format {} {} is not supported!",
            vk::to_string(Renderer::ColorFormat.format),
            vk::to_string(Renderer::ColorFormat.colorSpace));

        const std::array desiredPresentModes {
            vk::PresentModeKHR::eMailbox,
            vk::PresentModeKHR::eImmediate,
            vk::PresentModeKHR::eFifo,
        };

        const std::vector<vk::PresentModeKHR> availablePresentModes =
            device.getPhysicalDevice().getSurfacePresentModesKHR(surface);

        vk::PresentModeKHR selectedPresentMode = vk::PresentModeKHR::eFifo;

        for (vk::PresentModeKHR desiredPresentMode : desiredPresentModes)
        {
            if (std::ranges::find(availablePresentModes, desiredPresentMode)
                != availablePresentModes.cend())
            {
                selectedPresentMode = desiredPresentMode;
                break;
            }
        }

        log::info(
            "Selected {} as present mode with inital size of {}x{}",
            vk::to_string(selectedPresentMode),
            this->extent.width,
            this->extent.height);

        const vk::SurfaceCapabilitiesKHR surfaceCapabilities =
            device.getPhysicalDevice().getSurfaceCapabilitiesKHR(surface);
        const u32 numberOfSwapchainImages = std::min(
            {std::max({4U, surfaceCapabilities.minImageCount}), surfaceCapabilities.maxImageCount});

        const vk::SwapchainCreateInfoKHR swapchainCreateInfo {
            .sType {vk::StructureType::eSwapchainCreateInfoKHR},
            .pNext {nullptr},
            .flags {},
            .surface {surface},
            .minImageCount {numberOfSwapchainImages},
            .imageFormat {Renderer::ColorFormat.format},
            .imageColorSpace {Renderer::ColorFormat.colorSpace},
            .imageExtent {this->extent},
            .imageArrayLayers {1},
            .imageUsage {vk::ImageUsageFlagBits::eColorAttachment},
            .imageSharingMode {vk::SharingMode::eExclusive},
            .queueFamilyIndexCount {0},
            .pQueueFamilyIndices {nullptr},
            .preTransform {surfaceCapabilities.currentTransform},
            .compositeAlpha {vk::CompositeAlphaFlagBitsKHR::eOpaque},
            .presentMode {selectedPresentMode},
            .clipped {vk::True},
            .oldSwapchain {nullptr},
        };

        this->swapchain = device->createSwapchainKHRUnique(swapchainCreateInfo);
        this->images    = device->getSwapchainImagesKHR(*this->swapchain);

        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eSwapchainKHR},
                .objectHandle {std::bit_cast<u64>(*this->swapchain)},
                .pObjectName {"Swapchain"},
            });
        }

        this->image_views.reserve(this->images.size());
        this->dense_image_views.reserve(this->images.size());

        std::size_t idx = 0;
        for (vk::Image i : this->images)
        {
            const vk::ImageViewCreateInfo swapchainImageViewCreateInfo {
                .sType {vk::StructureType::eImageViewCreateInfo},
                .pNext {nullptr},
                .flags {},
                .image {i},
                .viewType {vk::ImageViewType::e2D},
                .format {Renderer::ColorFormat.format},
                .components {vk::ComponentMapping {
                    .r {vk::ComponentSwizzle::eIdentity},
                    .g {vk::ComponentSwizzle::eIdentity},
                    .b {vk::ComponentSwizzle::eIdentity},
                    .a {vk::ComponentSwizzle::eIdentity},
                }},
                .subresourceRange {vk::ImageSubresourceRange {
                    .aspectMask {vk::ImageAspectFlagBits::eColor},
                    .baseMipLevel {0},
                    .levelCount {1},
                    .baseArrayLayer {0},
                    .layerCount {1}}},
            };

            vk::UniqueImageView imageView =
                device->createImageViewUnique(swapchainImageViewCreateInfo);

            if constexpr (CINNABAR_DEBUG_BUILD)
            {
                std::string imageName = std::format("Swapchain Image #{}", idx);
                std::string viewName  = std::format("View #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eImage},
                    .objectHandle {std::bit_cast<u64>(i)},
                    .pObjectName {imageName.c_str()},
                });

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eImageView},
                    .objectHandle {std::bit_cast<u64>(*imageView)},
                    .pObjectName {viewName.c_str()},
                });
            }

            this->dense_image_views.push_back(*imageView);
            this->image_views.push_back(std::move(imageView));

            idx += 1;
        }

        log::trace("Created swapchain with {} images", this->images.size());
    }

    std::span<const vk::ImageView> Swapchain::getViews() const noexcept
    {
        return this->dense_image_views;
    }

    std::span<const vk::Image> Swapchain::getImages() const noexcept
    {
        return this->images;
    }

    vk::Extent2D Swapchain::getExtent() const noexcept
    {
        return this->extent;
    }

    vk::SwapchainKHR Swapchain::operator* () const noexcept
    {
        return *this->swapchain;
    }

    vk::Format Swapchain::getFormat() const noexcept
    {
        return Renderer::ColorFormat.format;
    }
} // namespace gfx::render::vulkan
