#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include <vulkan/vulkan_handles.hpp>

namespace gfx::generators::skybox
{
    class SkyboxRenderer
    {
    public:
        explicit SkyboxRenderer(const core::Renderer*);
        ~SkyboxRenderer();

        SkyboxRenderer(const SkyboxRenderer&)             = delete;
        SkyboxRenderer(SkyboxRenderer&&)                  = delete;
        SkyboxRenderer& operator= (const SkyboxRenderer&) = delete;
        SkyboxRenderer& operator= (SkyboxRenderer&&)      = delete;

        void renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const Camera&);

    private:
        const core::Renderer*                   renderer;
        core::vulkan::PipelineManager::Pipeline pipeline;
        f32                                     time_alive;
    };

} // namespace gfx::generators::skybox