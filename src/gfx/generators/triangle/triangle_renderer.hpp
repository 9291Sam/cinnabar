#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "util/allocators/opaque_integer_handle_allocator.hpp"

namespace gfx::generators::triangle
{
    class TriangleRenderer
    {
    public:
        using Triangle = util::OpaqueHandle<"TriangleRenderer::Triangle", u8>;
    public:

        explicit TriangleRenderer(const core::Renderer*);
        ~TriangleRenderer();

        TriangleRenderer(const TriangleRenderer&)             = delete;
        TriangleRenderer(TriangleRenderer&&)                  = delete;
        TriangleRenderer& operator= (const TriangleRenderer&) = delete;
        TriangleRenderer& operator= (TriangleRenderer&&)      = delete;

        Triangle createTriangle(glm::vec3);
        void     destroyTriangle(Triangle);

        void updateTriangle(const Triangle&, glm::vec3);

        void renderIntoCommandBuffer(vk::CommandBuffer, const Camera&);


    private:
        const core::Renderer*                   renderer;
        core::vulkan::PipelineManager::Pipeline pipeline;

        util::OpaqueHandleAllocator<Triangle>         triangle_allocator;
        gfx::core::vulkan::WriteOnlyBuffer<glm::vec3> triangle_gpu_data;
    };
} // namespace gfx::generators::triangle