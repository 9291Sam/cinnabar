#include "triangle_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/transform.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::generators::triangle
{
    static constexpr glm::vec3 NullTrianglePosition {NAN};

    TriangleRenderer::TriangleRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , pipeline(
              this->renderer->getPipelineManager()->createGraphicsPipeline(core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/triangle/triangle.vert"},
                  .fragment_shader_path {"src/gfx/generators/triangle/triangle.frag"},
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
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
              Triangle::MaxValidElement,
              "SRGB Triangle Data"}
    {}

    TriangleRenderer::~TriangleRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->pipeline));
    }

    TriangleRenderer::Triangle TriangleRenderer::createTriangle(glm::vec3 position)
    {
        Triangle newHandle = this->triangle_allocator.allocateOrPanic();

        this->triangle_gpu_data.uploadImmediate(this->triangle_allocator.getValueOfHandle(newHandle), {&position, 1});

        return newHandle;
    }

    void TriangleRenderer::destroyTriangle(Triangle t)
    {
        this->triangle_gpu_data.uploadImmediate(
            this->triangle_allocator.getValueOfHandle(t), {&NullTrianglePosition, 1});

        this->triangle_allocator.free(std::move(t));
    }

    void TriangleRenderer::updateTriangle(const Triangle& t, glm::vec3 newPosition)
    {
        this->triangle_gpu_data.uploadImmediate(this->triangle_allocator.getValueOfHandle(t), {&newPosition, 1});
    }

    void TriangleRenderer::renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const Camera&)
    {
        struct PushConstants
        {
            u32 position_buffer;
        };

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, this->renderer->getPipelineManager()->getPipeline(this->pipeline));

        const u32 maxTriangles = this->triangle_allocator.getUpperBoundOnAllocatedElements();

        const PushConstants pushConstants {
            .position_buffer {this->triangle_gpu_data.getStorageDescriptor().getOffset()},
        };

        commandBuffer.pushConstants<PushConstants>(
            this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
            vk::ShaderStageFlagBits::eAll,
            0,
            pushConstants);

        commandBuffer.draw(maxTriangles * 3, 1, 0, 0);
    }

} // namespace gfx::generators::triangle