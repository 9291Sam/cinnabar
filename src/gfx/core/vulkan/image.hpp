#pragma once

#include "gfx/core/vulkan/descriptor_manager.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VmaAllocator)

namespace gfx::core::vulkan
{
    class Allocator;
    class Device;

    class Image2D
    {
    public:

        Image2D() = default;
        Image2D(
            const Allocator*,
            vk::Device,
            vk::Extent2D,
            vk::Format,
            vk::ImageLayout,
            vk::ImageUsageFlags,
            vk::ImageAspectFlags,
            vk::ImageTiling,
            vk::MemoryPropertyFlags,
            std::string name);
        ~Image2D();

        Image2D(const Image2D&) = delete;
        Image2D(Image2D&&) noexcept;
        Image2D& operator= (const Image2D&) = delete;
        Image2D& operator= (Image2D&&) noexcept;

        [[nodiscard]] vk::Image    operator* () const;
        [[nodiscard]] vk::Format   getFormat() const;
        [[nodiscard]] vk::Extent2D getExtent() const;

        /// Returns an image view over the entire image in the type it was created with
        [[nodiscard]] vk::ImageView getView() const;

    private:
        void free();

        VmaAllocator allocator = nullptr;

        vk::Extent2D         extent;
        vk::Format           format {};
        vk::ImageAspectFlags aspect;

        vk::Image           image;
        VmaAllocation       memory {};
        vk::UniqueImageView view;
    }; // class Image2D

} // namespace gfx::core::vulkan
