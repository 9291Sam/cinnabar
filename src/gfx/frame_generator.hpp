#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/generators/triangle/triangle_renderer.hpp"

namespace gfx
{
    class FrameGenerator
    {
    public:
        struct FrameGenerateArgs
        {
            generators::triangle::TriangleRenderer* maybe_triangle_renderer;
        };
    public:
        explicit FrameGenerator(const core::Renderer*);
        ~FrameGenerator() = default;

        FrameGenerator(const FrameGenerator&)             = delete;
        FrameGenerator(FrameGenerator&&)                  = delete;
        FrameGenerator& operator= (const FrameGenerator&) = delete;
        FrameGenerator& operator= (FrameGenerator&&)      = delete;

        /// Return value is whether or not a resize ocurred
        [[nodiscard]] bool renderFrame(FrameGenerateArgs, gfx::Camera);
    private:
        const core::Renderer* renderer;
        bool                  has_resize_ocurred;
    };
} // namespace gfx