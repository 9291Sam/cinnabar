#pragma once

#include "data_structures.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/model.hpp"
#include "util/allocators/opaque_integer_handle_allocator.hpp"
#include "util/allocators/range_allocator.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::core
{
    class Renderer;
} // namespace gfx::core

namespace gfx::generators::voxel
{
    class VoxelRenderer
    {
    public:
        using VoxelChunk = util::OpaqueHandle<"Voxel Chunk", u32>;
    public:

        explicit VoxelRenderer(const core::Renderer*);
        ~VoxelRenderer();

        VoxelRenderer(const VoxelRenderer&)             = delete;
        VoxelRenderer(VoxelRenderer&&)                  = delete;
        VoxelRenderer& operator= (const VoxelRenderer&) = delete;
        VoxelRenderer& operator= (VoxelRenderer&&)      = delete;

        VoxelChunk createVoxelChunk(glm::vec3);
        void       destroyVoxelChunk(VoxelChunk);

        void setVoxelChunkData(const VoxelChunk&, std::span<const std::pair<ChunkLocalPosition, Voxel>>);

        void preFrameUpdate();

        void recordCopyCommands(vk::CommandBuffer);
        void recordPrepass(vk::CommandBuffer, const Camera&);
        void recordColorCalculation(vk::CommandBuffer);
        void recordColorTransfer(vk::CommandBuffer);

        void setLightInformation(GpuRaytracedLight);
        // void                         setAnimationTime(f32) const;
        // void                         setAnimationNumber(u32) const;
        // std::span<const std::string> getAnimationNames() const;


    private:
        const core::Renderer*                        renderer;
        gfx::core::vulkan::PipelineManager::Pipeline prepass_pipeline;
        gfx::core::vulkan::PipelineManager::Pipeline color_calculation_pipeline;
        gfx::core::vulkan::PipelineManager::Pipeline color_transfer_pipeline;

        util::OpaqueHandleAllocator<VoxelChunk>       chunk_allocator;
        gfx::core::vulkan::CpuCachedBuffer<ChunkData> chunk_data;

        util::RangeAllocator                            brick_allocator;
        gfx::core::vulkan::GpuOnlyBuffer<CombinedBrick> combined_bricks;

        gfx::core::vulkan::GpuOnlyBuffer<GpuColorHashMapNode> face_hash_map;
        gfx::core::vulkan::GpuOnlyBuffer<GpuRaytracedLight>   lights;
        gfx::core::vulkan::WriteOnlyBuffer<PBRVoxelMaterial>  materials;

        // mutable std::atomic<f32> last_frame_time;
        // mutable std::atomic<u32> demo_index;

        // struct Demo
        // {
        //     AnimatedVoxelModel                                              model;
        //     util::Fn<glm::u32vec3(glm::u32vec3, const AnimatedVoxelModel&)> sampler;
        // };

        // std::vector<Demo>        demos;
        // std::vector<std::string> names;
    };
} // namespace gfx::generators::voxel