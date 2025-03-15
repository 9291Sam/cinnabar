#pragma once

#include "data_structures.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include <random>

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

    private:
        const core::Renderer*                                renderer;
        gfx::core::vulkan::PipelineManager::GraphicsPipeline pipeline;

        gfx::core::vulkan::GpuOnlyBuffer<ChunkBrickStorage> chunk_bricks;
        gfx::core::vulkan::GpuOnlyBuffer<BooleanBrick>      bricks;
        std::mt19937                                        generator;
    };

} // namespace gfx::generators::voxel