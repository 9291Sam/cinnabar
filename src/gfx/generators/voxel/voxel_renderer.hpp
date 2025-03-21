#pragma once

#include "data_structures.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/model.hpp"
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

        void                         setAnimationTime(f32) const;
        void                         setAnimationNumber(u32) const;
        std::span<const std::string> getAnimationNames() const;

    private:
        const core::Renderer*                                renderer;
        gfx::core::vulkan::PipelineManager::GraphicsPipeline pipeline;

        gfx::core::vulkan::GpuOnlyBuffer<ChunkBrickStorage> chunk_bricks;
        gfx::core::vulkan::GpuOnlyBuffer<BooleanBrick>      visible_bricks;
        gfx::core::vulkan::GpuOnlyBuffer<MaterialBrick>     material_bricks;
        gfx::core::vulkan::WriteOnlyBuffer<VoxelMaterial>   materials;

        mutable std::atomic<f32> last_frame_time;
        mutable std::atomic<u32> demo_index;

        struct Demo
        {
            AnimatedVoxelModel                                              model;
            util::Fn<glm::u32vec3(glm::u32vec3, const AnimatedVoxelModel&)> sampler;
        };

        std::vector<Demo>        demos;
        std::vector<std::string> names;
    };
} // namespace gfx::generators::voxel