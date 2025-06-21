#include "voxel_world_manager.hpp"
#include "boost/range/algorithm_ext/erase.hpp"
#include "generators/voxel/data_structures.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include "tracy/Tracy.hpp"
#include "util/logger.hpp"
#include "util/timer.hpp"
#include <boost/range/algorithm/remove_if.hpp>

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

        for (AlignedChunkCoordinate aC : entityData.current_chunks)
        {
            this->chunks_that_need_regeneration_to_ids_in_each_chunk[aC];
        }

        entityData = {};

        this->voxel_entity_allocator.free(std::move(e));
    }

    VoxelWorldManager::UniqueVoxelEntity
    VoxelWorldManager::createVoxelEntityUnique(WorldPosition position, glm::u8vec3 size)
    {
        return {this->createVoxelEntity(position, size), this};
    }

    VoxelWorldManager::VoxelEntity VoxelWorldManager::createVoxelEntity(WorldPosition position, glm::u8vec3 size)
    {
        VoxelEntity e = this->voxel_entity_allocator.allocateOrPanic();

        this->voxel_entity_storage[this->voxel_entity_allocator.getValueOfHandle(e)] =
            PerEntityData {.current_chunks {}, .model {}, .size {size}, .position {position}};

        return e;
    }

    void VoxelWorldManager::updateVoxelEntityData(
        const VoxelEntity& e, std::vector<std::pair<ChunkLocalPosition, Voxel>> newVoxels)
    {
        const u16      entityId   = this->voxel_entity_allocator.getValueOfHandle(e);
        PerEntityData& entityData = this->voxel_entity_storage[entityId];

        for (const auto& [cP, v] : newVoxels)
        {
            assert::critical(
                glm::all(glm::lessThan(cP.asVector(), entityData.size)),
                "Voxel @ {} {} {} is out of bounds for VoxelEntity of size {} {} {}",
                cP.x,
                cP.y,
                cP.z,
                entityData.size.x,
                entityData.size.y,
                entityData.size.z);
        }

        for (AlignedChunkCoordinate aC : entityData.current_chunks)
        {
            this->chunks_that_need_regeneration_to_ids_in_each_chunk[aC].insert(entityId);
        }

        entityData.model = std::move(newVoxels);
    }

    void VoxelWorldManager::updateVoxelEntityPosition(const VoxelEntity& e, WorldPosition newPosition)
    {
        const u16      entityId   = this->voxel_entity_allocator.getValueOfHandle(e);
        PerEntityData& entityData = this->voxel_entity_storage[entityId];

        for (AlignedChunkCoordinate aC : entityData.current_chunks)
        {
            this->chunks_that_need_regeneration_to_ids_in_each_chunk[aC];
        }

        entityData.current_chunks = {};
        entityData.position       = newPosition;

        for (i32 x : {0, i32 {entityData.size.x}})
        {
            for (i32 y : {0, i32 {entityData.size.y}})
            {
                for (i32 z : {0, i32 {entityData.size.z}})
                {
                    entityData.current_chunks.insert(
                        WorldPosition {entityData.position + WorldPosition {x, y, z}}.splitIntoAlignedChunk().first);
                }
            }
        }

        for (AlignedChunkCoordinate aC : entityData.current_chunks)
        {
            this->chunks_that_need_regeneration_to_ids_in_each_chunk[aC].insert(entityId);
        }
    }

    VoxelWorldManager::UniqueVoxelLight VoxelWorldManager::createVoxelLightUnique(GpuRaytracedLight light)
    {
        return this->voxel_renderer.createVoxelLightUnique(light);
    }

    VoxelWorldManager::VoxelLight VoxelWorldManager::createVoxelLight(GpuRaytracedLight light)
    {
        return this->voxel_renderer.createVoxelLight(light);
    }

    void VoxelWorldManager::updateVoxelLight(const VoxelLight& l, GpuRaytracedLight light)
    {
        this->voxel_renderer.updateVoxelLight(l, light);
    }

    void VoxelWorldManager::destroyVoxelLight(VoxelLight l)
    {
        this->voxel_renderer.destroyVoxelLight(std::move(l));
    }

    void VoxelWorldManager::onFrameUpdate(gfx::Camera)
    {
        ZoneScoped;

        usize            totalVoxelsUpdatedThisFrame = 0;
        util::MultiTimer t {};
        this->voxel_entity_allocator.iterateThroughAllocatedElements(
            [this](const u16 entityId)
            {
                PerEntityData& entityData = this->voxel_entity_storage[entityId];

                for (AlignedChunkCoordinate aC : entityData.current_chunks)
                {
                    if (this->chunks_that_need_regeneration_to_ids_in_each_chunk.contains(aC))
                    {
                        this->chunks_that_need_regeneration_to_ids_in_each_chunk[aC].insert(entityId);
                    }
                }
            });

        std::vector<AlignedChunkCoordinate> chunksToCullFromCache {};

        for (const auto& data : this->generated_chunk_data_cache)
        {
            if (data.second.frames_alive > 120)
            {
                chunksToCullFromCache.push_back(data.first);
            }
        }

        for (AlignedChunkCoordinate aC : chunksToCullFromCache)
        {
            this->generated_chunk_data_cache.erase(aC);
        }
        t.stamp("cull first");

        usize chunksThatGotRegenerated = 0;
        for (auto& [aC, entities] : this->chunks_that_need_regeneration_to_ids_in_each_chunk)
        {
            util::MultiTimer    t {};
            const WorldPosition chunkBase = WorldPosition::assemble(aC, {});
            const VoxelChunk&   chunk     = this->chunks.at(aC);

            decltype(this->generated_chunk_data_cache)::iterator maybeCacheEntry =
                this->generated_chunk_data_cache.find(aC);

            if (maybeCacheEntry == this->generated_chunk_data_cache.cend())
            {
                maybeCacheEntry =
                    this->generated_chunk_data_cache
                        .insert(
                            {aC,
                             ChunkCacheEntry {
                                 .data {this->world_generator.generateChunkPreDense(aC)}, .frames_alive {0}}})
                        .first;

                chunksThatGotRegenerated += 1;
            }
            else
            {
                maybeCacheEntry->second.frames_alive += 1;
            }

            std::pair<BrickMap, std::vector<CombinedBrick>> data = maybeCacheEntry->second.data;

            t.stamp("generate chunk pre dense");

            usize voxelEntitiesTotalVoxelsNeededForThisChunk = 0;
            // Not a bug! remember if a handle dies and is replaced we need to clear it from the old chunk and insert it
            // into the new one
            boost::range::remove_erase_if(
                entities,
                [&, this](const u16 entityId)
                {
                    if (this->voxel_entity_allocator.isHandleAlive(entityId))
                    {
                        voxelEntitiesTotalVoxelsNeededForThisChunk += this->voxel_entity_storage[entityId].model.size();
                        return false;
                    }
                    else
                    {
                        return true;
                    }
                });

            totalVoxelsUpdatedThisFrame += voxelEntitiesTotalVoxelsNeededForThisChunk;

            std::vector<std::pair<ChunkLocalPosition, Voxel>> collectedVoxelUpdateList {};
            collectedVoxelUpdateList.reserve(voxelEntitiesTotalVoxelsNeededForThisChunk);

            for (u16 entityId : entities)
            {
                const PerEntityData& entityData = this->voxel_entity_storage[entityId];

                // offset by world position
                // ensure all variables are within the correct bound of the chunk
                const glm::i32vec3 entityOffsetFromChunk = entityData.position - chunkBase;

                for (const auto& [cP, v] : entityData.model)
                {
                    const glm::i32vec3 thisVoxelPlaceInChunk =
                        static_cast<glm::i32vec3>(cP.asVector()) + entityOffsetFromChunk;

                    if (std::optional<ChunkLocalPosition> correctlyOffsetEntityPosition =
                            ChunkLocalPosition::tryCreate(thisVoxelPlaceInChunk))
                    {
                        // excellent its in the chunk
                        collectedVoxelUpdateList.push_back({*correctlyOffsetEntityPosition, v});
                    }
                    else
                    {
                        // welp nothing happens then, it's outside
                    }
                }
            }

            t.stamp("entities pp and culling");

            auto [brickMap, bricks] = generators::voxel::appendVoxelsToDenseChunk(
                data.first, std::move(data.second), collectedVoxelUpdateList);

            t.stamp("append dense");

            this->voxel_renderer.setVoxelChunkData(chunk, brickMap, std::move(bricks));

            t.stamp("upload dense");

            std::ignore = t.finish();
        }

        t.stamp("generate chunk");

        std::ignore = t.finish();

        // log::trace(
        //     "Uploaded {} voxels in {} chunks this frame. {} chunks got regenerated",
        //     totalVoxelsUpdatedThisFrame,
        //     this->chunks_that_need_regeneration_to_ids_in_each_chunk.size(),
        //     chunksThatGotRegenerated);

        this->chunks_that_need_regeneration_to_ids_in_each_chunk.clear();
    }

    gfx::generators::voxel::VoxelRenderer* VoxelWorldManager::getRenderer()
    {
        return &this->voxel_renderer;
    }
} // namespace gfx