#pragma once

#include <vulkan/vulkan_core.h>

VK_DEFINE_HANDLE(VmaAllocator)

namespace gfx::core::vulkan
{
    class Instance;
    class Device;
    class DescriptorPool;
    class DescriptorManager;

    class Allocator
    {
    public:

        Allocator(const Instance&, const Device*, const DescriptorManager*);
        ~Allocator();

        Allocator(const Allocator&)             = delete;
        Allocator(Allocator&&)                  = delete;
        Allocator& operator= (const Allocator&) = delete;
        Allocator& operator= (Allocator&&)      = delete;

        [[nodiscard]] VmaAllocator                     operator* () const;
        [[nodiscard]] const vulkan::Device*            getDevice() const;
        [[nodiscard]] const vulkan::DescriptorManager* getDescriptorManager() const;

    private:
        // HACK: move these out of here
        const vulkan::Device*            device;
        const vulkan::DescriptorManager* descriptor_manager;
        VmaAllocator                     allocator;
    };

} // namespace gfx::core::vulkan
