#include "triangle_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/transform.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::generators::triangle
{
    TriangleRenderer::TriangleRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , pipeline(
              this->renderer->getPipelineManager()->createGraphicsPipeline(core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/triangle/triangle.vert"},
                  .fragment_shader_path {"src/gfx/generators/triangle/triangle.frag"},
                  .topology {vk::PrimitiveTopology::eTriangleList}, // remove
                  .polygon_mode {vk::PolygonMode::eFill},           // replace with dynamic state
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eClockwise}, // remove
                  .depth_test_enable {vk::True},
                  .depth_write_enable {vk::True},
                  .depth_compare_op {vk::CompareOp::eGreater}, // remove
                  .color_format {gfx::core::Renderer::ColorFormat.format},
                  .depth_format {gfx::core::Renderer::DepthFormat}, // remove lmao?
                  .blend_enable {vk::True},
                  .name {"hacky triangle pipeline"},
              }))
        , triangle_allocator {Triangle::MaxValidElement}
        , triangle_data {Triangle::MaxValidElement, Transform {}}
    {}

    TriangleRenderer::~TriangleRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->pipeline));
    }

    TriangleRenderer::Triangle TriangleRenderer::createTriangle(const Transform& transform)
    {
        Triangle newHandle = this->triangle_allocator.allocateOrPanic();

        const u32 handleOffset = this->triangle_allocator.getValueOfHandle(newHandle);

        this->triangle_data[handleOffset] = transform;

        return newHandle;
    }

    void TriangleRenderer::destroyTriangle(Triangle t)
    {
        this->triangle_allocator.free(std::move(t));
    }

    void TriangleRenderer::updateTriangle(const Triangle& t, const Transform& newTransform)
    {
        this->triangle_data[this->triangle_allocator.getValueOfHandle(t)] = newTransform;
    }

    void TriangleRenderer::renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const Camera& camera)
    {
        struct PushConstants
        {
            glm::mat4 mvp;
        };

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, this->renderer->getPipelineManager()->getPipeline(this->pipeline));

        this->triangle_allocator.iterateThroughAllocatedElements(
            [&](const u32 id)
            {
                const Transform     thisTransform = this->triangle_data[id];
                const PushConstants pushConstants {.mvp {camera.getPerspectiveMatrix(thisTransform)}};

                commandBuffer.pushConstants<PushConstants>(
                    this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
                    vk::ShaderStageFlagBits::eAll,
                    0,
                    pushConstants);

                commandBuffer.draw(3, 1, 0, 0);
            });

        // log::trace("Rendering {} triangles!", this->triangle_allocator.getNumberAllocated());
    }

} // namespace gfx::generators::triangle