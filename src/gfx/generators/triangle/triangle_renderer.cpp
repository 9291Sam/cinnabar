#include "triangle_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/shader_common/bindings.slang"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::generators::triangle
{
    static constexpr glm::vec3 NullTrianglePosition {NAN};

    TriangleRenderer::TriangleRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , pipeline(this->renderer->getPipelineManager()->createPipeline(core::vulkan::GraphicsPipelineDescriptor {
              .shader_path {"src/gfx/generators/triangle/triangle.slang"},
              .topology {vk::PrimitiveTopology::eTriangleList},
              .polygon_mode {vk::PolygonMode::eFill},
              .cull_mode {vk::CullModeFlagBits::eNone},
              .front_face {vk::FrontFace::eClockwise},
              .depth_test_enable {vk::True},
              .depth_write_enable {vk::True},
              .depth_compare_op {vk::CompareOp::eGreater},
              .color_format {gfx::core::Renderer::ColorFormat.format},
              .depth_format {gfx::core::Renderer::DepthFormat},
              .blend_enable {vk::True},
              .name {"SRGB triangle pipeline"},
          }))
        , triangle_allocator {Triangle::MaxValidElement}
        , triangle_gpu_data {
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              Triangle::MaxValidElement,
              "SRGB Triangle Data",
              SBO_SRGB_TRIANGLE_DATA}
    {}

    TriangleRenderer::~TriangleRenderer()
    {
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->pipeline));
    }

    TriangleRenderer::Triangle TriangleRenderer::createTriangle(glm::vec3 position)
    {
        Triangle newHandle = this->triangle_allocator.allocateOrPanic();

        this->renderer->getStager().enqueueTransfer(
            this->triangle_gpu_data, this->triangle_allocator.getValueOfHandle(newHandle), {&position, 1});

        return newHandle;
    }

    void TriangleRenderer::destroyTriangle(Triangle t)
    {
        this->renderer->getStager().enqueueTransfer(
            this->triangle_gpu_data, this->triangle_allocator.getValueOfHandle(t), {&NullTrianglePosition, 1});

        this->triangle_allocator.free(std::move(t));
    }

    void TriangleRenderer::updateTriangle(const Triangle& t, glm::vec3 newPosition)
    {
        this->renderer->getStager().enqueueTransfer(
            this->triangle_gpu_data, this->triangle_allocator.getValueOfHandle(t), {&newPosition, 1});
    }

    void TriangleRenderer::renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const Camera&)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, this->renderer->getPipelineManager()->getPipeline(this->pipeline));

        const u32 maxTriangles = this->triangle_allocator.getUpperBoundOnAllocatedElements();

        commandBuffer.draw(maxTriangles * 3, 1, 0, 0);
    }

} // namespace gfx::generators::triangle