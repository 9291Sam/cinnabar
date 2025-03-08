#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/image.hpp"

namespace gfx
{
    namespace generators::imgui
    {
        class ImguiRenderer;
    } // namespace generators::imgui
    namespace generators::skybox
    {
        class SkyboxRenderer;
    } // namespace generators::skybox
    namespace generators::triangle
    {
        class TriangleRenderer;
    } // namespace generators::triangle

    class FrameGenerator
    {
    public:
        struct FrameGenerateArgs
        {
            generators::triangle::TriangleRenderer* maybe_triangle_renderer;
            generators::skybox::SkyboxRenderer*     maybe_skybox_renderer;
            generators::imgui::ImguiRenderer*       maybe_imgui_renderer;
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

        struct FrameDescriptors
        {
            gfx::core::vulkan::Image2D depth_buffer;
            gfx::core::vulkan::Image2D imgui_render_target;

            std::optional<core::vulkan::DescriptorHandle<vk::DescriptorType::eStorageImage>> imgui_image_descriptor;
        };

        FrameDescriptors createFrameDescriptors();

        FrameDescriptors frame_descriptors;
    };
} // namespace gfx