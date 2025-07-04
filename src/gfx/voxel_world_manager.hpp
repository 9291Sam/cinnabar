// #pragma once

// #include "boost/container/small_vector.hpp"
// #include "generators/voxel/data_structures.hpp"
// #include "generators/voxel/shared_data_structures.slang"
// #include "generators/voxel/voxel_renderer.hpp"
// #include "gfx/core/renderer.hpp"
// #include "gfx/generators/voxel/generator.hpp"
// #include "util/allocators/opaque_integer_handle_allocator.hpp"
// #include <unordered_set>
// #include <vector>

// namespace gfx
// {
//     using gfx::generators::voxel::AlignedChunkCoordinate;
//     using gfx::generators::voxel::BrickMap;
//     using gfx::generators::voxel::ChunkLocalPosition;
//     using gfx::generators::voxel::ChunkLocation;
//     using gfx::generators::voxel::GpuRaytracedLight;
//     using gfx::generators::voxel::Voxel;
//     using gfx::generators::voxel::WorldPosition;

//     class VoxelWorldManager
//     {
//     public:
//         using VoxelLight       = gfx::generators::voxel::VoxelRenderer::VoxelLight;
//         using UniqueVoxelLight = gfx::generators::voxel::VoxelRenderer::UniqueVoxelLight;
//         using VoxelEntity      = util::OpaqueHandle<"Voxel Entity", u16>; // TODO: is 65k enough? hopefully!
//     public:
//         explicit VoxelWorldManager(const core::Renderer*, u64 seed); // TODO: this should be an injectable generator
//         ~VoxelWorldManager();

//         VoxelWorldManager(const VoxelWorldManager&)             = delete;
//         VoxelWorldManager(VoxelWorldManager&&)                  = delete;
//         VoxelWorldManager& operator= (const VoxelWorldManager&) = delete;
//         VoxelWorldManager& operator= (VoxelWorldManager&&)      = delete;

//         void destroyVoxelEntity(VoxelEntity);

//         using UniqueVoxelEntity = util::UniqueOpaqueHandle<VoxelEntity, &VoxelWorldManager::destroyVoxelEntity>;

//         // TODO: add a size and allow for custom origin points
//         [[nodiscard]] UniqueVoxelEntity createVoxelEntityUnique(WorldPosition, glm::u8vec3 size);
//         [[nodiscard]] VoxelEntity       createVoxelEntity(WorldPosition, glm::u8vec3 size);
//         void updateVoxelEntityData(const VoxelEntity&, std::vector<std::pair<ChunkLocalPosition, Voxel>> newVoxels);
//         void updateVoxelEntityPosition(const VoxelEntity&, WorldPosition);

//         [[nodiscard]] UniqueVoxelLight createVoxelLightUnique(GpuRaytracedLight);
//         [[nodiscard]] VoxelLight       createVoxelLight(GpuRaytracedLight);
//         void                           updateVoxelLight(const VoxelLight&, GpuRaytracedLight);
//         void                           destroyVoxelLight(VoxelLight);

//         void onFrameUpdate(gfx::Camera);

//         generators::voxel::VoxelRenderer* getRenderer();

//     private:
//         const core::Renderer*             renderer;
//         generators::voxel::WorldGenerator world_generator;
//         generators::voxel::VoxelRenderer  voxel_renderer;

//         using UniqueVoxelChunk = generators::voxel::VoxelRenderer::UniqueVoxelChunk;
//         using VoxelChunk       = generators::voxel::VoxelRenderer::VoxelChunk;
//         using CombinedBrick    = generators::voxel::CombinedBrick;

//         std::unordered_map<ChunkLocation, UniqueVoxelChunk> chunks;

//         struct PerEntityData
//         {
//             boost::container::small_flat_set<AlignedChunkCoordinate, 8> current_chunks;
//             std::vector<std::pair<ChunkLocalPosition, Voxel>>           model;
//             glm::u8vec3                                                 size;
//             WorldPosition                                               position;
//         };

//         struct ChunkCacheEntry
//         {
//             std::pair<BrickMap, std::vector<CombinedBrick>> data;
//             u32                                             frames_alive;
//         };

//         util::OpaqueHandleAllocator<VoxelEntity> voxel_entity_allocator;
//         std::vector<PerEntityData>               voxel_entity_storage;
//         std::unordered_map<ChunkLocation, boost::container::flat_set<u16>>
//                                                            chunks_that_need_regeneration_to_ids_in_each_chunk;
//         std::unordered_map<ChunkLocation, ChunkCacheEntry> generated_chunk_data_cache;
//     };

//     /*
//      class ChunkLocalUpdate
//     {
//     public:
//         enum class ShadowUpdate : u8
//         {
//             ShadowTransparent = 0,
//             ShadowCasting     = 1,
//         };

//         enum class CameraVisibleUpdate : u8
//         {
//             CameraInvisible = 0,
//             CameraVisible   = 1,
//         };
//     public:
//         ChunkLocalUpdate(ChunkLocalPosition p, Voxel v, ShadowUpdate s, CameraVisibleUpdate c)
//             : pos_x {p.x}
//             , shadow_casting {std::to_underlying(s)}
//             , camera_visible {std::to_underlying(c)}
//             , pos_y {p.y}
//             , pos_z {p.z}
//             , material {std::bit_cast<std::array<u8, 2>>(v)}
//         {}
//         ~ChunkLocalUpdate() = default;

//         ChunkLocalUpdate(const ChunkLocalUpdate&)             = default;
//         ChunkLocalUpdate(ChunkLocalUpdate&&)                  = default;
//         ChunkLocalUpdate& operator= (const ChunkLocalUpdate&) = default;
//         ChunkLocalUpdate& operator= (ChunkLocalUpdate&&)      = default;

//         [[nodiscard]] ChunkLocalPosition getPosition() const noexcept
//         {
//             return ChunkLocalPosition {{this->pos_x, this->pos_y, this->pos_z}};
//         }
//         [[nodiscard]] Voxel getVoxel() const noexcept
//         {
//             return std::bit_cast<Voxel>(this->material);
//         }
//         [[nodiscard]] ShadowUpdate getShadowUpdate() const noexcept
//         {
//             return static_cast<ShadowUpdate>(this->shadow_casting);
//         }
//         [[nodiscard]] CameraVisibleUpdate getCameraVisibility() const noexcept
//         {
//             return static_cast<CameraVisibleUpdate>(this->camera_visible);
//         }

//     private:
//         u8 pos_x          : 6;
//         u8 shadow_casting : 1;
//         u8 camera_visible : 1;

//         u8 pos_y : 6;

//         u8 pos_z : 6;

//         std::array<u8, 2> material;
//     };
//     */
// } // namespace gfx