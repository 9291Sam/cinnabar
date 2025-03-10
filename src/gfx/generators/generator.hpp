#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include <vulkan/vulkan.hpp>

namespace gfx::generators
{
    struct Generator
    {
        virtual ~Generator() = 0;

        virtual void renderIntoCommandBuffer(
            vk::CommandBuffer,
            const Camera&,
            core::vulkan::DescriptorHandle<vk::DescriptorType::eStorageBuffer> globalDescriptorInfo) = 0;
    };
} // namespace gfx::generators