#include "voxel_world_manager.hpp"
#include "generators/voxel/data_structures.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "util/timer.hpp"

namespace gfx
{
    VoxelWorldManager::VoxelWorldManager(const core::Renderer* renderer_, u64 seed)
        : renderer {renderer_}
        , world_generator {seed}
        , voxel_renderer {this->renderer}
        , voxel_entity_allocator {VoxelEntity::MaxValidElement}
        , voxel_entity_storage {VoxelEntity::MaxValidElement, PerEntityData {}}
    {
        util::Timer worldGenerationTimer {"worldGenerationTimer"};

        const i32 dim = 8;
        const i32 him = 2;

        for (i32 cX = -dim; cX <= dim; ++cX)
        {
            for (i32 cY = -dim; cY <= him; ++cY)
            {
                for (i32 cZ = -dim; cZ <= dim; ++cZ)
                {
                    const AlignedChunkCoordinate aC {cX, cY, cZ};

                    UniqueVoxelChunk chunk = this->voxel_renderer.createVoxelChunkUnique(aC);

                    const auto [brickMap, bricks] = this->world_generator.generateChunkPreDense(aC);

                    this->voxel_renderer.setVoxelChunkData(chunk, brickMap, bricks);

                    this->chunks.insert({aC, std::move(chunk)});
                }
            }
        }
    }

    VoxelWorldManager::~VoxelWorldManager() = default;

    void VoxelWorldManager::destroyVoxelEntity(VoxelEntity e)
    {
        const u16 entityId = this->voxel_entity_allocator.getValueOfHandle(e);

        PerEntityData& entityData = this->voxel_entity_storage[entityId];
        entityData.new_data       = {};

        this->chunks_that_need_regeneration.insert(entityData.chunks.cbegin(), entityData.chunks.cend());

        this->voxel_entity_allocator.free(std::move(e));
    }

    void VoxelWorldManager::onFrameUpdate(gfx::Camera c)
    {
        // iter over alive entities and move them to chunks as needed

        // iter over dead entities and mark those chunks as needing regeneration

        //
    }
} // namespace gfx