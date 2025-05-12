#pragma once

#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
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

        void setVoxelEntityContent(const VoxelEntity&, std::span<const std::pair<Voxel, ChunkLocalPosition>>);
        void setPosition(const VoxelEntity&, WorldPosition);

        void onFrameUpdate();

    private:
    };
} // namespace gfx::generators::voxel