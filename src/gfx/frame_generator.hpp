#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/buffer.hpp"
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

    namespace generators::voxel
    {
        class VoxelRenderer;
    } // namespace generators::voxel

    class FrameGenerator
    {
    public:
        struct FrameGenerateArgs
        {
            generators::triangle::TriangleRenderer* maybe_triangle_renderer;
            generators::skybox::SkyboxRenderer*     maybe_skybox_renderer;
            generators::imgui::ImguiRenderer*       maybe_imgui_renderer;
            generators::voxel::VoxelRenderer*       maybe_voxel_renderer;
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
        };

        FrameDescriptors createFrameDescriptors();

        FrameDescriptors frame_descriptors;

        struct GlobalGpuData
        {
            glm::mat4 view_matrix;
            glm::mat4 projection_matrix;
            glm::mat4 view_projection_matrix;
            glm::vec4 camera_forward_vector;
            glm::vec4 camera_right_vector;
            glm::vec4 camera_up_vector;
            glm::vec4 camera_position;
            float     fov_y;
            float     tan_half_fov_y;
            float     aspect_ratio;
            float     time_alive;
        };

        gfx::core::vulkan::WriteOnlyBuffer<GlobalGpuData> global_gpu_data;
    };
} // namespace gfx