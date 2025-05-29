#pragma once

#include "core/vulkan/frame_manager.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/image.hpp"
#include "util/task_generator.hpp"

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

        struct FrameGenerateReturn
        {
            bool                   has_resize_occurred {};
            util::TimestampStamper render_thread_profile;
        };

    public:
        explicit FrameGenerator(const core::Renderer*);
        ~FrameGenerator() = default;

        FrameGenerator(const FrameGenerator&)             = delete;
        FrameGenerator(FrameGenerator&&)                  = delete;
        FrameGenerator& operator= (const FrameGenerator&) = delete;
        FrameGenerator& operator= (FrameGenerator&&)      = delete;

        [[nodiscard]] FrameGenerateReturn renderFrame(FrameGenerateArgs, gfx::Camera, util::TimestampStamper);
    private:
        const core::Renderer* renderer;
        bool                  has_resize_occurred;

        struct FrameDescriptors
        {
            gfx::core::vulkan::Image2D depth_buffer;
            gfx::core::vulkan::Image2D imgui_render_target;
            gfx::core::vulkan::Image2D voxel_render_target;
        };

        FrameDescriptors createFrameDescriptors();

        // Actually always valid, its just that sometimes I need a null state for internal reasons
        std::optional<FrameDescriptors> frame_descriptors;

        struct GlobalGpuData
        {
            glm::mat4  view_matrix;
            glm::mat4  projection_matrix;
            glm::mat4  view_projection_matrix;
            glm::vec4  camera_forward_vector;
            glm::vec4  camera_right_vector;
            glm::vec4  camera_up_vector;
            glm::vec4  camera_position;
            f32        fov_y;
            f32        tan_half_fov_y;
            f32        aspect_ratio;
            f32        time_alive;
            glm::uvec2 framebuffer_size;
            u32        bool_enable_reflections;
            u32        bool_moved_this_frame;
        };

        gfx::core::vulkan::WriteOnlyBuffer<GlobalGpuData>              global_gpu_data;
        bool                                                           are_reflections_enabled = true;
        std::optional<glm::vec3>                                       maybe_previous_frame_camera_position;
        std::array<std::string_view, core::vulkan::MaxQueriesPerFrame> active_timestamp_names;
    };
} // namespace gfx