#pragma once

#include "data_structures.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "util/gif.hpp"

namespace gfx::core
{
    class Renderer;
} // namespace gfx::core

namespace gfx::generators::voxel
{
    class VoxelRenderer
    {
    public:

        explicit VoxelRenderer(const core::Renderer*);
        ~VoxelRenderer();

        VoxelRenderer(const VoxelRenderer&)             = delete;
        VoxelRenderer(VoxelRenderer&&)                  = delete;
        VoxelRenderer& operator= (const VoxelRenderer&) = delete;
        VoxelRenderer& operator= (VoxelRenderer&&)      = delete;

        void renderIntoCommandBuffer(vk::CommandBuffer, const Camera&);

        static f32 time_in_video;

    private:
        const core::Renderer*                                renderer;
        gfx::core::vulkan::PipelineManager::GraphicsPipeline pipeline;

        gfx::core::vulkan::GpuOnlyBuffer<ChunkBrickStorage> chunk_bricks;
        gfx::core::vulkan::GpuOnlyBuffer<BooleanBrick>      visible_bricks;
        gfx::core::vulkan::GpuOnlyBuffer<MaterialBrick>     material_bricks;
        gfx::core::vulkan::WriteOnlyBuffer<VoxelMaterial>   materials;

        float time_since_color_change;

        util::Gif bad_apple;
    };
} // namespace gfx::generators::voxel