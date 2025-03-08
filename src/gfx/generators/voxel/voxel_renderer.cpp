#include "voxel_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::generators::voxel
{
    VoxelRenderer::VoxelRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , pipeline {
              this->renderer->getPipelineManager()->createGraphicsPipeline(core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/voxel/voxel.vert"},
                  .fragment_shader_path {"src/gfx/generators/voxel/voxel.frag"},
                  .topology {vk::PrimitiveTopology::eTriangleStrip}, // remove
                  .polygon_mode {vk::PolygonMode::eFill},            // replace with dynamic state
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eClockwise}, // remove
                  .depth_test_enable {vk::True},
                  .depth_write_enable {vk::True},
                  .depth_compare_op {vk::CompareOp::eGreater}, // remove
                  .color_format {gfx::core::Renderer::ColorFormat.format},
                  .depth_format {gfx::core::Renderer::DepthFormat}, // remove lmao?
                  .blend_enable {vk::True},
                  .name {"Voxel pipeline"},
              })}
    {}

    VoxelRenderer::~VoxelRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->pipeline));
    }

    void VoxelRenderer::renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const Camera& camera)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, this->renderer->getPipelineManager()->getPipeline(this->pipeline));

        commandBuffer.pushConstants<glm::mat4>(
            this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
            vk::ShaderStageFlagBits::eAll,
            0,
            camera.getPerspectiveMatrix({}));

        commandBuffer.draw(14, 1, 0, 0);
    }
} // namespace gfx::generators::voxel