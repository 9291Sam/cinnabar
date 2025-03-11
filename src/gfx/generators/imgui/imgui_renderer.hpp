#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/generators/generator.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

struct ImFont;

namespace gfx::core
{
    class Renderer;
} // namespace gfx::core

namespace gfx::generators::imgui
{
    class ImguiRenderer : Generator
    {
    public:
        explicit ImguiRenderer(const core::Renderer*);
        ~ImguiRenderer() override;

        ImguiRenderer(const ImguiRenderer&)             = delete;
        ImguiRenderer(ImguiRenderer&&)                  = delete;
        ImguiRenderer& operator= (const ImguiRenderer&) = delete;
        ImguiRenderer& operator= (ImguiRenderer&&)      = delete;

        // this interface is useless lmao
        void renderIntoCommandBuffer(
            vk::CommandBuffer,
            const Camera&,
            core::vulkan::DescriptorHandle<vk::DescriptorType::eUniformBuffer> globalDescriptorInfo) override;
        void renderImageCopyIntoCommandBuffer(
            vk::CommandBuffer, core::vulkan::DescriptorHandle<vk::DescriptorType::eStorageImage>);
    private:
        const core::Renderer* renderer;

        vk::UniqueDescriptorPool imgui_descriptor_pool;

        ImFont*                                         font;
        core::vulkan::PipelineManager::GraphicsPipeline menu_transfer_pipeline;
    };
} // namespace gfx::generators::imgui