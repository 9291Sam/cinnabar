#pragma once

#include "boost/container/small_vector.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/generator.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include "util/allocators/opaque_integer_handle_allocator.hpp"

namespace gfx::generators::voxel
{
    class VoxelWorld
    {
    public:
        using VoxelEntity = util::OpaqueHandle<"Voxel Entity", u16>;
    public:

        VoxelWorld();
        ~VoxelWorld();

        VoxelWorld(const VoxelWorld&)             = delete;
        VoxelWorld(VoxelWorld&&)                  = delete;
        VoxelWorld& operator= (const VoxelWorld&) = delete;
        VoxelWorld& operator= (VoxelWorld&&)      = delete;

        void destroyVoxelEntity(VoxelEntity);

        using UniqueVoxelEntity = util::UniqueOpaqueHandle<VoxelEntity, &VoxelWorld::destroyVoxelEntity>;

        VoxelEntity       createVoxelEntity(glm::u8vec3 dimension);
        UniqueVoxelEntity createUniqueVoxelEntity(glm::u8vec3 dimension);

        void setVoxelEntityContent(const VoxelEntity&, std::vector<std::pair<Voxel, ChunkLocalPosition>>);
        void setPosition(const VoxelEntity&, WorldPosition);

        // Returns the VoxelRenderer needed for rendering
        VoxelRenderer* onFrame();

    private:
        util::OpaqueHandleAllocator<VoxelEntity> voxel_entity_allocator;

        struct VoxelEntityData
        {
            WorldPosition                                     position;
            glm::u8vec3                                       dimension;
            std::vector<std::pair<Voxel, ChunkLocalPosition>> voxels;
            boost::container::
                flat_set<AlignedChunkCoordinate, std::less<>, boost::container::small_vector<AlignedChunkCoordinate, 8>>
                    near_chunks;
        };

        WorldGenerator                                                        generator;
        std::unordered_map<AlignedChunkCoordinate, VoxelRenderer::VoxelChunk> chunks;
        VoxelRenderer                                                         voxel_renderer;
    };
} // namespace gfx::generators::voxel