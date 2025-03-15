#include "skybox_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/window.hpp"
#include <vulkan/vulkan_handles.hpp>

namespace gfx::generators::skybox
{
    SkyboxRenderer::SkyboxRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , pipeline {this->renderer->getPipelineManager()->createGraphicsPipeline(
              core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/skybox/skybox.vert"},
                  .fragment_shader_path {"src/gfx/generators/skybox/skybox.frag"},
                  .topology {vk::PrimitiveTopology::eTriangleList},
                  .polygon_mode {vk::PolygonMode::eFill},
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eClockwise},
                  .depth_test_enable {vk::True},
                  .depth_write_enable {vk::False},
                  .depth_compare_op {vk::CompareOp::eGreater},
                  .color_format {gfx::core::Renderer::ColorFormat.format},
                  .depth_format {gfx::core::Renderer::DepthFormat},
                  .blend_enable {vk::True},
                  .name {"Skybox Pipeline"}})}
        , time_alive {0.0f}
    {}

    SkyboxRenderer::~SkyboxRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->pipeline));
    }

    void SkyboxRenderer::renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const gfx::Camera&)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, this->renderer->getPipelineManager()->getPipeline(this->pipeline));

        commandBuffer.draw(3, 1, 0, 0);

        this->time_alive += this->renderer->getWindow()->getDeltaTimeSeconds();
    }
} // namespace gfx::generators::skybox