#pragma once

#include "data_structures.hpp"
#include "emissive_integer_tree.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/generators/voxel/light_influence_storage.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
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
    static constexpr u16 MaxVoxelLights = 8192;

    std::pair<BrickMap, std::vector<CombinedBrick>>
        createDenseChunk(std::span<const std::pair<ChunkLocalPosition, Voxel>>);

    std::pair<BrickMap, std::vector<CombinedBrick>> appendVoxelsToDenseChunk(
        const BrickMap&, std::vector<CombinedBrick>, std::span<const std::pair<ChunkLocalPosition, Voxel>>);

    class VoxelRenderer
    {
    public:
        using VoxelChunk = util::OpaqueHandle<"Voxel Chunk", u32>;
        using VoxelLight = util::OpaqueHandle<"Voxel Light", u16>;
        static_assert(VoxelLight::MaxValidElement > MaxVoxelLights);

    public:

        explicit VoxelRenderer(const core::Renderer*);
        ~VoxelRenderer();

        VoxelRenderer(const VoxelRenderer&)             = delete;
        VoxelRenderer(VoxelRenderer&&)                  = delete;
        VoxelRenderer& operator= (const VoxelRenderer&) = delete;
        VoxelRenderer& operator= (VoxelRenderer&&)      = delete;

        void destroyVoxelChunk(VoxelChunk);

        using UniqueVoxelChunk = util::UniqueOpaqueHandle<VoxelChunk, &VoxelRenderer::destroyVoxelChunk>;

        [[nodiscard]] UniqueVoxelChunk createVoxelChunkUnique(AlignedChunkCoordinate);
        [[nodiscard]] VoxelChunk       createVoxelChunk(AlignedChunkCoordinate);
        void setVoxelChunkData(const VoxelChunk&, const BrickMap&, std::span<const CombinedBrick>);

        void destroyVoxelLight(VoxelLight);

        using UniqueVoxelLight = util::UniqueOpaqueHandle<VoxelLight, &VoxelRenderer::destroyVoxelLight>;
        [[nodiscard]] UniqueVoxelLight createVoxelLightUnique(GpuRaytracedLight);
        [[nodiscard]] VoxelLight       createVoxelLight(GpuRaytracedLight);
        void                           updateVoxelLight(const VoxelLight&, GpuRaytracedLight);

        void preFrameUpdate();

        void recordCopyCommands(vk::CommandBuffer);
        void recordPrepass(vk::CommandBuffer, const Camera&);
        void recordColorCalculation(vk::CommandBuffer);
        void recordColorTransfer(vk::CommandBuffer);

    private:
        const core::Renderer*                        renderer;
        gfx::core::vulkan::PipelineManager::Pipeline prepass_pipeline;
        gfx::core::vulkan::PipelineManager::Pipeline face_normalizer_pipeline;
        gfx::core::vulkan::PipelineManager::Pipeline color_calculation_pipeline;
        gfx::core::vulkan::PipelineManager::Pipeline color_transfer_pipeline;

        LightInfluenceStorage light_influence_storage;

        util::OpaqueHandleAllocator<VoxelChunk>              chunk_allocator;
        gfx::core::vulkan::CpuCachedBuffer<GpuChunkData>     gpu_chunk_data;
        std::vector<CpuChunkData>                            cpu_chunk_data;
        gfx::core::vulkan::CpuCachedBuffer<ChunkHashMapNode> chunk_hash_map;

        util::RangeAllocator                            brick_allocator;
        gfx::core::vulkan::GpuOnlyBuffer<CombinedBrick> combined_bricks;

        util::OpaqueHandleAllocator<VoxelLight>               light_allocator;
        gfx::core::vulkan::CpuCachedBuffer<GpuRaytracedLight> lights;

        gfx::core::vulkan::GpuOnlyBuffer<GpuColorHashMapNode> face_hash_map;
        gfx::core::vulkan::WriteOnlyBuffer<PBRVoxelMaterial>  materials;
    };
} // namespace gfx::generators::voxel