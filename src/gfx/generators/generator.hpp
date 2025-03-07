#pragma once

#include "gfx/camera.hpp"
#include <vulkan/vulkan.hpp>

namespace gfx::generators
{
    class Generator
    {
    public:
        virtual ~Generator() = 0;

        virtual void renderIntoCommandBuffer(vk::CommandBuffer, const Camera&) = 0;
    };
} // namespace gfx::generators