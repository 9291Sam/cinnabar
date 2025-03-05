#pragma once

#include "gfx/camera.hpp"
#include <vulkan/vulkan.hpp>

namespace gfx::renderables
{
    class Renderable
    {
    public:
        virtual ~Renderable() = 0;

        virtual void renderIntoCommandBuffer(vk::CommandBuffer, const Camera&) = 0;
    };
} // namespace gfx::renderables