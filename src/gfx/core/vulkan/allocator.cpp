#include "allocator.hpp"
#include "device.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "instance.hpp"
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::core::vulkan
{
    Allocator::Allocator(const Instance& instance, const Device* device_, const DescriptorManager* descriptorManager)
        : device {device_}
        , descriptor_manager {descriptorManager}
        , allocator {nullptr}
    {
        VmaVulkanFunctions vulkanFunctions {};
        vulkanFunctions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr   = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

        const VmaAllocatorCreateInfo allocatorCreateInfo {
            .flags {},
            .physicalDevice {device_->getPhysicalDevice()},
            .device {this->device->getDevice()},
            .preferredLargeHeapBlockSize {0}, // chosen by VMA
            .pAllocationCallbacks {nullptr},
            .pDeviceMemoryCallbacks {nullptr},
            .pHeapSizeLimit {nullptr},
            .pVulkanFunctions {&vulkanFunctions},
            .instance {*instance},
            .vulkanApiVersion {instance.getVulkanVersion()},
            .pTypeExternalMemoryHandleTypes {nullptr},
        };

        const vk::Result result {::vmaCreateAllocator(&allocatorCreateInfo, &this->allocator)};

        assert::critical(result == vk::Result::eSuccess, "Failed to create allocator | {}", vk::to_string(result));
    }

    Allocator::~Allocator()
    {
        ::vmaDestroyAllocator(this->allocator);
    }

    VmaAllocator Allocator::operator* () const
    {
        return this->allocator;
    }

    const vulkan::Device* Allocator::getDevice() const
    {
        return this->device;
    }

    const vulkan::DescriptorManager* Allocator::getDescriptorManager() const
    {
        return this->descriptor_manager;
    }

} // namespace gfx::core::vulkan
