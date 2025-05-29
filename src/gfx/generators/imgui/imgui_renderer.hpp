#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/vulkan/frame_manager.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

struct ImFont;

namespace gfx::core
{
    class Renderer;
} // namespace gfx::core

namespace gfx::generators::imgui
{
    class ImguiRenderer
    {
    public:
        explicit ImguiRenderer(const core::Renderer*);
        ~ImguiRenderer();

        ImguiRenderer(const ImguiRenderer&)             = delete;
        ImguiRenderer(ImguiRenderer&&)                  = delete;
        ImguiRenderer& operator= (const ImguiRenderer&) = delete;
        ImguiRenderer& operator= (ImguiRenderer&&)      = delete;

        void renderIntoCommandBuffer(vk::CommandBuffer, const Camera&, const core::vulkan::Swapchain&);
        void renderImageCopyIntoCommandBuffer(vk::CommandBuffer);
    private:
        const core::Renderer* renderer;

        vk::UniqueDescriptorPool imgui_descriptor_pool;

        ImFont*                                       font;
        core::vulkan::PipelineManager::UniquePipeline menu_transfer_pipeline;

        std::vector<std::string>                                          owned_present_mode_strings;
        std::vector<const char*>                                          raw_present_mode_strings;
        std::vector<std::string>                                          owned_animation_name_strings;
        std::vector<const char*>                                          raw_animation_name_strings;
        std::array<std::string, core::vulkan::MaxQueriesPerFrame>         owned_gpu_timestamp_names {};
        std::array<const char*, core::vulkan::MaxQueriesPerFrame>         raw_gpu_timestamp_names {};
        std::array<std::array<f32, core::vulkan::MaxQueriesPerFrame>, 32> frame_moving_average_data {};
        std::array<f32, core::vulkan::MaxQueriesPerFrame>                 most_recent_timestamp_average {};
        usize                                                             next_average_to_place_values_in = 0;

        std::vector<const char*>         raw_cpu_profile_names;
        usize                            next_average_cpu_timestamp = 0;
        std::array<std::vector<f32>, 32> cpu_timestamp_moving_average;
        std::vector<f32>                 cpu_averaged_timestamp;

        voxel::GpuRaytracedLight light;

        int  present_mode_combo_box_value   = 0;
        bool are_reflections_enabled        = true;
        bool is_global_illumination_enabled = false;
    };
} // namespace gfx::generators::imgui