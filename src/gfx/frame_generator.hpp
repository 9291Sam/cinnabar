#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/renderables/triangle/triangle_renderer.hpp"

namespace gfx
{
    class FrameGenerator
    {
    public:
        struct FrameGenerateArgs
        {
            renderables::triangle::TriangleRenderer* maybe_triangle_renderer;
        };
    public:
        explicit FrameGenerator(const core::Renderer*);
        ~FrameGenerator() = default;

        FrameGenerator(const FrameGenerator&)             = delete;
        FrameGenerator(FrameGenerator&&)                  = delete;
        FrameGenerator& operator= (const FrameGenerator&) = delete;
        FrameGenerator& operator= (FrameGenerator&&)      = delete;

        [[nodiscard]] bool hasResizeOcurred() const noexcept;

        /// Return value is whether or not a resize ocurred
        void renderFrame(FrameGenerateArgs, gfx::Camera);
    private:
        const core::Renderer* renderer;
        bool                  has_resize_ocurred;
    };
} // namespace gfx