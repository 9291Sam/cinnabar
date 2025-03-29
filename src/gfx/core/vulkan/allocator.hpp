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

        Allocator(const Instance&, const Device&);
        ~Allocator();

        Allocator(const Allocator&)             = delete;
        Allocator(Allocator&&)                  = delete;
        Allocator& operator= (const Allocator&) = delete;
        Allocator& operator= (Allocator&&)      = delete;

        [[nodiscard]] VmaAllocator operator* () const;

    private:
        VmaAllocator allocator;
    };

} // namespace gfx::core::vulkan
