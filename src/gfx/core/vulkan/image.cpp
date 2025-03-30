#include "image.hpp"
#include "allocator.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/device.hpp"
#include "util/logger.hpp"
#include <optional>
#include <source_location>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::core::vulkan
{
    Image2D::Image2D(
        const Renderer*         renderer_,
        vk::Extent2D            extent_,
        vk::Format              format_,
        vk::ImageLayout         initalLayout, // NOLINT
        vk::ImageLayout         usageLayout,
        vk::ImageUsageFlags     usage_,
        vk::ImageAspectFlags    aspect_,
        vk::ImageTiling         tiling,
        vk::MemoryPropertyFlags memoryPropertyFlags,
        std::string             name_,
        std::optional<u8>       maybeBindingLocation,
        std::source_location    loc)
        : renderer {renderer_}
        , extent {extent_}
        , format {format_}
        , aspect {aspect_}
        , used_layout {usageLayout}
        , usage {usage_}
        , image {nullptr}
        , memory {nullptr}
        , view {nullptr}
        , name {std::move(name_)}
        , maybe_shader_binding_location {maybeBindingLocation}
    {
        const VkImageCreateInfo imageCreateInfo {
            .sType {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO},
            .pNext {nullptr},
            .flags {},
            .imageType {VK_IMAGE_TYPE_2D},
            .format {static_cast<VkFormat>(format)},
            .extent {VkExtent3D {.width {this->extent.width}, .height {this->extent.height}, .depth {1}}},
            .mipLevels {1},
            .arrayLayers {1},
            .samples {VK_SAMPLE_COUNT_1_BIT},
            .tiling {static_cast<VkImageTiling>(tiling)},
            .usage {static_cast<VkImageUsageFlags>(this->usage)},
            .sharingMode {VK_SHARING_MODE_EXCLUSIVE},
            .queueFamilyIndexCount {0},
            .pQueueFamilyIndices {nullptr},
            .initialLayout {static_cast<VkImageLayout>(initalLayout)}};

        const VmaAllocationCreateInfo imageAllocationCreateInfo {
            .flags {},
            .usage {VMA_MEMORY_USAGE_AUTO},
            .requiredFlags {static_cast<VkMemoryPropertyFlags>(memoryPropertyFlags)},
            .preferredFlags {},
            .memoryTypeBits {},
            .pool {nullptr},
            .pUserData {nullptr},
            .priority {1.0f}};

        VkImage outputImage = nullptr;

        const vk::Result result {::vmaCreateImage(
            **this->renderer->getAllocator(),
            &imageCreateInfo,
            &imageAllocationCreateInfo,
            &outputImage,
            &this->memory,
            nullptr)};

        assert::critical(result == vk::Result::eSuccess, "Failed to allocate image memory {}", vk::to_string(result));

        assert::critical(outputImage != nullptr, "Returned image was nullptr!");

        this->image = vk::Image {outputImage};

        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            this->renderer->getDevice()->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eImage},
                .objectHandle {std::bit_cast<u64>(this->image)},
                .pObjectName {name.c_str()},
            });
        }

        vk::ImageViewCreateInfo imageViewCreateInfo {
            .sType {vk::StructureType::eImageViewCreateInfo},
            .pNext {nullptr},
            .flags {},
            .image {this->image},
            .viewType {vk::ImageViewType::e2D},
            .format {this->format},
            .components {},
            .subresourceRange {vk::ImageSubresourceRange {
                .aspectMask {this->aspect},
                .baseMipLevel {0},
                .levelCount {1},
                .baseArrayLayer {0},
                .layerCount {1},
            }},
        };

        this->view = this->renderer->getDevice()->getDevice().createImageViewUnique(imageViewCreateInfo);

        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            std::string viewName = std::format("{} View", name);

            this->renderer->getDevice()->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eImageView},
                .objectHandle {std::bit_cast<u64>(*this->view)},
                .pObjectName {viewName.c_str()},
            });
        }

        if (this->usage & vk::ImageUsageFlagBits::eSampled)
        {
            const u8 descriptorBindingLocation = *this->maybe_shader_binding_location.or_else(
                [] -> std::optional<u8>
                {
                    panic("Buffer was not given a binding location!");

                    return std::nullopt;
                });

            this->maybe_sampled_image_descriptor_handle =
                this->renderer->getDescriptorManager()->registerDescriptor<vk::DescriptorType::eSampledImage>(
                    {.view {*this->view}, .layout {this->used_layout}}, this->name, descriptorBindingLocation, loc);
        }

        if (this->usage & vk::ImageUsageFlagBits::eStorage)
        {
            const u8 descriptorBindingLocation = *this->maybe_shader_binding_location.or_else(
                [] -> std::optional<u8>
                {
                    panic("Buffer was not given a binding location!");

                    return std::nullopt;
                });

            this->maybe_storage_image_descriptor_handle =
                this->renderer->getDescriptorManager()->registerDescriptor<vk::DescriptorType::eStorageImage>(
                    {.view {*this->view}, .layout {this->used_layout}}, this->name, descriptorBindingLocation, loc);
        }
    }

    Image2D::~Image2D()
    {
        if (this->maybe_sampled_image_descriptor_handle.has_value())
        {
            this->renderer->getDescriptorManager()->deregisterDescriptor(
                *std::exchange(this->maybe_sampled_image_descriptor_handle, std::nullopt));
        }

        if (this->maybe_storage_image_descriptor_handle.has_value())
        {
            this->renderer->getDescriptorManager()->deregisterDescriptor(
                *std::exchange(this->maybe_storage_image_descriptor_handle, std::nullopt));
        }

        this->free();
    }

    Image2D::Image2D(Image2D&& other) noexcept
        : renderer {other.renderer}
        , extent {other.extent}
        , format {other.format}
        , aspect {other.aspect}
        , used_layout {other.used_layout}
        , usage {other.usage}
        , maybe_sampled_image_descriptor_handle {other.maybe_sampled_image_descriptor_handle}
        , maybe_storage_image_descriptor_handle {other.maybe_storage_image_descriptor_handle}
        , image {other.image}
        , memory {other.memory}
        , view {std::move(other.view)}
        , name {std::move(other.name)}
    {
        other.renderer                              = nullptr;
        other.extent                                = vk::Extent2D {};
        other.format                                = {};
        other.aspect                                = {};
        other.used_layout                           = {};
        other.usage                                 = {};
        other.maybe_sampled_image_descriptor_handle = std::nullopt;
        other.maybe_storage_image_descriptor_handle = std::nullopt;
        other.image                                 = nullptr;
        other.memory                                = nullptr;
        other.name                                  = {};
    }

    Image2D& Image2D::operator= (Image2D&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->~Image2D();

        new (this) Image2D {std::move(other)};

        return *this;
    }

    vk::Image Image2D::operator* () const
    {
        return this->image;
    }

    vk::ImageView Image2D::getView() const
    {
        return *this->view;
    }

    vk::Format Image2D::getFormat() const
    {
        return this->format;
    }

    vk::Extent2D Image2D::getExtent() const
    {
        return this->extent;
    }

    void Image2D::free()
    {
        if (this->renderer != nullptr)
        {
            vmaDestroyImage(**this->renderer->getAllocator(), static_cast<VkImage>(this->image), this->memory);
        }
    }

} // namespace gfx::core::vulkan
