#include "voxel_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/emissive_integer_tree.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "gfx/shader_common/bindings.slang"
#include "util/allocators/range_allocator.hpp"
#include "util/events.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <cstddef>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/string_cast.hpp>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

static constexpr u32 MaxChunks                          = 1u << 14u; // this can be extended
static constexpr u32 AverageNonHomogenousBricksPerChunk = 192;
static constexpr u32 BricksToAllocate                   = MaxChunks * AverageNonHomogenousBricksPerChunk;

namespace gfx::generators::voxel
{
    VoxelRenderer::VoxelRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , prepass_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::GraphicsPipelineDescriptor {
                  .shader_path {"src/gfx/generators/voxel/voxel_prepass.slang"},
                  .topology {vk::PrimitiveTopology::eTriangleList},
                  .polygon_mode {vk::PolygonMode::eFill},
                  .cull_mode {vk::CullModeFlagBits::eBack},
                  .front_face {vk::FrontFace::eClockwise},
                  .depth_test_enable {vk::True},
                  .depth_write_enable {vk::True},
                  .depth_compare_op {vk::CompareOp::eGreater},
                  .color_format {vk::Format::eR32G32B32A32Sfloat},
                  .depth_format {gfx::core::Renderer::DepthFormat},
                  .blend_enable {vk::False},
                  .name {"Voxel prepass pipeline"},
              })}
        , face_normalizer_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::ComputePipelineDescriptor {
                  .compute_shader_path {"src/gfx/generators/voxel/voxel_hash_map_normalizer.slang"},
                  .name {"Voxel Face Normalizer"}})}
        , color_calculation_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::ComputePipelineDescriptor {
                  .compute_shader_path {"src/gfx/generators/voxel/voxel_color_calculation.slang"},
                  .name {"Voxel Color Calculation"}})}
        , color_transfer_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::GraphicsPipelineDescriptor {
                  .shader_path {"src/gfx/generators/voxel/voxel_color_transfer.slang"},
                  .topology {vk::PrimitiveTopology::eTriangleList},
                  .polygon_mode {vk::PolygonMode::eFill},
                  .cull_mode {vk::CullModeFlagBits::eNone},
                  .front_face {vk::FrontFace::eClockwise},
                  .depth_test_enable {vk::False},
                  .depth_write_enable {vk::False},
                  .depth_compare_op {},
                  .color_format {gfx::core::Renderer::ColorFormat.format},
                  .depth_format {gfx::core::Renderer::DepthFormat},
                  .blend_enable {vk::True},
                  .name {"Color Transfer Pipeline"},
              })}
        , chunk_allocator {MaxChunks}
        , gpu_chunk_data(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxChunks,
              "Chunk Data",
              SBO_CHUNK_DATA)
        , cpu_chunk_data {MaxChunks}
        , chunk_hash_map(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              chunkHashTableCapacity,
              "Chunk Hash Map",
              SBO_CHUNK_HASH_MAP)
        , brick_allocator(BricksToAllocate, MaxChunks)
        , combined_bricks(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              BricksToAllocate,
              "Combined Bricks",
              SBO_COMBINED_BRICKS)
        , face_hash_map(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              faceHashTableCapacity,
              "Face Hash Map",
              SBO_FACE_HASH_MAP)
        , lights(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Voxel Lights",
              SBO_VOXEL_LIGHTS)
        , materials {generateMaterialBuffer(this->renderer)}
    {}

    VoxelRenderer::~VoxelRenderer()
    {
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->prepass_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->face_normalizer_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->color_calculation_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->color_transfer_pipeline));
    }

    VoxelRenderer::VoxelChunk VoxelRenderer::createVoxelChunk(AlignedChunkCoordinate coordinate)
    {
        VoxelChunk newChunk = this->chunk_allocator.allocateOrPanic();
        const u32  chunkId  = this->chunk_allocator.getValueOfHandle(newChunk);

        this->gpu_chunk_data.write<&GpuChunkData::aligned_chunk_coordinate>(chunkId, coordinate);
        this->gpu_chunk_data.write<&GpuChunkData::offset>(chunkId, ~0u);
        assert::critical(this->cpu_chunk_data[chunkId].brick_allocation.isNull(), "should be empty");
        assert::critical(this->cpu_chunk_data[chunkId] == CpuChunkData {}, "should be default");

        insertUniqueChunkHashTable(this->chunk_hash_map, coordinate.asVector(), chunkId);

        return newChunk;
    }

    void VoxelRenderer::destroyVoxelChunk(VoxelChunk c)
    {
        const u32     chunkId         = this->chunk_allocator.getValueOfHandle(c);
        GpuChunkData& oldGpuChunkData = this->gpu_chunk_data.modify(chunkId);
        CpuChunkData& oldCpuChunkData = this->cpu_chunk_data[chunkId];

        if (!oldCpuChunkData.brick_allocation.isNull())
        {
            this->brick_allocator.free(std::move(oldCpuChunkData.brick_allocation));
        }
        removeUniqueChunkHashTable(this->chunk_hash_map, oldGpuChunkData.aligned_chunk_coordinate.asVector(), chunkId);
        this->chunk_allocator.free(std::move(c));

        for (ChunkLocalPosition p : oldCpuChunkData.current_chunk_local_emissive_voxels)
        {
            this->emissives_in_world.remove(WorldPosition::assemble(oldGpuChunkData.aligned_chunk_coordinate, p));
        }

        oldGpuChunkData = {};
        oldCpuChunkData = {};
    }

    void VoxelRenderer::preFrameUpdate()
    {
        bool anyUpdates = false;
        // check for chunks with dirty emissives, if so populate the changes
        this->chunk_allocator.iterateThroughAllocatedElements(
            [&](u32 chunkId)
            {
                const AlignedChunkCoordinate thisChunkCoordinate =
                    this->gpu_chunk_data.read(chunkId).aligned_chunk_coordinate;
                // TODO: this is totally going to be a data race in the future
                CpuChunkData& cpuChunkData = this->cpu_chunk_data[chunkId];

                if (!cpuChunkData.emissive_updates.empty())
                {
                    anyUpdates = true;

                    for (EmissiveVoxelUpdateChange emissiveUpdate : cpuChunkData.emissive_updates)
                    {
                        const WorldPosition thisWorldPosition =
                            WorldPosition::assemble(thisChunkCoordinate, emissiveUpdate.position);

                        if (emissiveUpdate.change_type == EmissiveVoxelUpdateChangeType::Insert)
                        {
                            this->emissives_in_world.insert(thisWorldPosition);
                            const auto [_, wasInserted] =
                                cpuChunkData.current_chunk_local_emissive_voxels.insert(emissiveUpdate.position);

                            if (!wasInserted)
                            {
                                assert::warn(
                                    wasInserted,
                                    "Duplicate insertion of Voxel @ {} in chunk {} @ {}",
                                    glm::to_string(emissiveUpdate.position.asVector()),
                                    chunkId,
                                    glm::to_string(thisChunkCoordinate.asVector()));
                            }
                        }
                        else if (emissiveUpdate.change_type == EmissiveVoxelUpdateChangeType::Removal)
                        {
                            this->emissives_in_world.remove(thisWorldPosition);
                            const std::size_t elementsRemoved =
                                cpuChunkData.current_chunk_local_emissive_voxels.erase(emissiveUpdate.position);

                            assert::warn(
                                elementsRemoved == 1,
                                "Erroneous removal of emissive voxel {} @ {}",
                                elementsRemoved,
                                chunkId);
                        }
                        else
                        {
                            panic(
                                "unexpected EmissiveVoxelUpdateChangeType of {}",
                                std::to_underlying(emissiveUpdate.change_type));
                        }
                    }

                    cpuChunkData.emissive_updates.clear();
                }
            });

        if (anyUpdates)
        {
            this->emissives_in_world.optimize();

            this->chunk_allocator.iterateThroughAllocatedElements(
                [&](u32 chunkId)
                {
                    const i32 searchRadius = 512;

                    const WorldPosition chunkCornerPosition =
                        WorldPosition::assemble(this->gpu_chunk_data.read(chunkId).aligned_chunk_coordinate, {});

                    const std::vector<WorldPosition> nearEmissiviesPositions =
                        this->emissives_in_world.getNearestElements(chunkCornerPosition, searchRadius);

                    // TODO: sort by importance

                    std::vector<ChunkLocalEmissiveOffset> thisChunkPossibleEmissivies;
                    thisChunkPossibleEmissivies.reserve(nearEmissiviesPositions.size());

                    for (WorldPosition nearEmissiveWorldPosition : nearEmissiviesPositions)
                    {
                        const ChunkLocalEmissiveOffset thisEmissiveChunkLocalOffset {
                            .x {nearEmissiveWorldPosition.x - chunkCornerPosition.x},
                            .y {nearEmissiveWorldPosition.y - chunkCornerPosition.y},
                            .z {nearEmissiveWorldPosition.z - chunkCornerPosition.z},
                        };

#warning better vertifiation and extend the range to truly be 1024x512x1024
                        // TODO: fix face hash table info to prevent colissions

                        thisChunkPossibleEmissivies.push_back(thisEmissiveChunkLocalOffset);
                    }

                    const u32 emissiveDataPacketBytes =
                        static_cast<u32>(thisChunkPossibleEmissivies.size() * sizeof(ChunkLocalEmissiveOffset));

                    GpuChunkData& partData = this->gpu_chunk_data.modifyCoherentRange(
                        chunkId,
                        offsetof(GpuChunkData, number_of_emissives),
                        sizeof(GpuChunkData::number_of_emissives) + emissiveDataPacketBytes);

                    partData.number_of_emissives = static_cast<u32>(thisChunkPossibleEmissivies.size());
                    std::memcpy(&partData.emissive_data, thisChunkPossibleEmissivies.data(), emissiveDataPacketBytes);
                    static_assert(std::is_trivially_copyable_v<ChunkLocalEmissiveOffset>);
                });
        }

        this->gpu_chunk_data.flushViaStager(this->renderer->getStager());
        this->chunk_hash_map.flushViaStager(this->renderer->getStager());
        // if (std::optional t = util::receive<f32>("SetAnimationTime"))
        // {
        //     this->setAnimationTime(*t);
        // }
        // if (std::optional n = util::receive<u32>("SetAnimationNumber"))
        // {
        //     this->setAnimationNumber(*n);
        // }
        if (std::optional l = util::receive<voxel::GpuRaytracedLight>("UpdateLight"))
        {
            this->setLightInformation(*l);
        }
        // const f32 deltaTime     = this->renderer->getWindow()->getDeltaTimeSeconds();
        // const f32 lastFrameTime = this->last_frame_time.load(std::memory_order_acquire);
        // const f32 thisFrameTime = lastFrameTime + deltaTime;

        // auto& thisDemo = this->demos.at(this->demo_index.load(std::memory_order_acquire));

        // const u32 lastFrameAnimationNumber =
        //     thisDemo.model.getFrameNumberAtTime(lastFrameTime, AnimatedVoxelModel::Looping {});
        // const u32 thisFrameAnimationNumber =
        //     thisDemo.model.getFrameNumberAtTime(thisFrameTime, AnimatedVoxelModel::Looping {});

        // this->last_frame_time.fetch_add(deltaTime, std::memory_order_acq_rel);

        // if (thisFrameAnimationNumber != lastFrameAnimationNumber
        //     || (lastFrameAnimationNumber == 0 && thisFrameAnimationNumber == 0))
        // {
        //     auto sensibleData = thisDemo.model.getFrame(thisFrameTime);

        //     std::vector<CombinedBrick> newCombinedBricks {};

        //     ChunkData newChunk {.getWorldChunkCorner() {-16.0f, -16.0f, -16.0f}, .offset {0}, .brick_map {}};

        //     u16 nextBrickIndex = 0;

        //     const u32 xExtent = std::min({64U, thisDemo.model.getExtent().x});
        //     const u32 yExtent = std::min({64U, thisDemo.model.getExtent().y});
        //     const u32 zExtent = std::min({64U, thisDemo.model.getExtent().z});

        //     for (u32 x = 0; x < xExtent; ++x)
        //     {
        //         for (u32 y = 0; y < yExtent; ++y)
        //         {
        //             for (u32 z = 0; z < zExtent; ++z)
        //             {
        //                 const ChunkLocalPosition cP {x, y, z};
        //                 const auto [bC, bP] = cP.split();

        //                 MaybeBrickOffsetOrMaterialId& maybeThisBrickOffset = newChunk.brick_map[bC.x][bC.y][bC.z];

        //                 const glm::u32vec3 sample = thisDemo.sampler(glm::u32vec3 {x, y, z}, thisDemo.model);

        //                 const Voxel& v = sensibleData[sample.x, sample.y, sample.z];

        //                 if (maybeThisBrickOffset.data == static_cast<u16>(~0u) && v != Voxel::NullAirEmpty)
        //                 {
        //                     maybeThisBrickOffset.data = nextBrickIndex;

        //                     newCombinedBricks.push_back(CombinedBrick {});

        //                     nextBrickIndex += 1;
        //                 }

        //                 if (v != Voxel::NullAirEmpty)
        //                 {
        //                     newCombinedBricks.at(maybeThisBrickOffset.data).write(bP, static_cast<u16>(v));
        //                 }
        //             }
        //         }
        //     }

        //     if (!newCombinedBricks.empty())
        //     {
        //         this->renderer->getStager().enqueueTransfer(
        //             this->combined_bricks, 0, {newCombinedBricks.data(), newCombinedBricks.size()});
        //         this->renderer->getStager().enqueueTransfer(this->chunk_data, 0, {&newChunk, 1});
        //     }
        // }
    }

    void
    VoxelRenderer::setVoxelChunkData(const VoxelChunk& c, std::span<const std::pair<ChunkLocalPosition, Voxel>> input)
    {
        const u32 chunkId = this->chunk_allocator.getValueOfHandle(c);

        GpuChunkData& gpuChunkData = this->gpu_chunk_data.modify(chunkId);
        CpuChunkData& cpuChunkData = this->cpu_chunk_data[chunkId];

        // Since we're going to nuke whatever is there, might as well reserve the space for it
        cpuChunkData.emissive_updates.reserve(
            cpuChunkData.emissive_updates.size() + cpuChunkData.current_chunk_local_emissive_voxels.size());

        for (ChunkLocalPosition currentEmissivePosition : cpuChunkData.current_chunk_local_emissive_voxels)
        {
            cpuChunkData.emissive_updates.push_back(EmissiveVoxelUpdateChange {
                .position {currentEmissivePosition}, .change_type {EmissiveVoxelUpdateChangeType::Removal}});
        }

        if (input.empty())
        {
            log::warn("empty setVoxelChunkData");
            return;
        }
        u16                               nextNonCompactedBrickIndex = 0;
        std::vector<CombinedBrick>        nonCompactedBricks {};
        decltype(GpuChunkData::brick_map) nonCompactedBrickMap {};

        for (const auto& [cP, v] : input)
        {
            if (getMaterialFromVoxel(v).emission_metallic.xyz() != glm::vec3(0.0))
            {
                // we have an emissive voxel
                cpuChunkData.emissive_updates.push_back(
                    EmissiveVoxelUpdateChange {.position {cP}, .change_type {EmissiveVoxelUpdateChangeType::Insert}});
            }

            if (v == Voxel::NullAirEmpty)
            {
                continue;
            }

            const auto [bC, bP] = cP.split();

            MaybeBrickOffsetOrMaterialId& maybeThisBrickOffset = nonCompactedBrickMap[bC.x][bC.y][bC.z];

            if (maybeThisBrickOffset.isMaterial()
                && maybeThisBrickOffset.getMaterial() == static_cast<u16>(Voxel::NullAirEmpty))
            {
                maybeThisBrickOffset._data = nextNonCompactedBrickIndex;

                nonCompactedBricks.push_back(CombinedBrick {});

                nextNonCompactedBrickIndex += 1;
            }

            nonCompactedBricks[maybeThisBrickOffset._data].write(bP, static_cast<u16>(v));
        }

        std::vector<CombinedBrick>        compactedBricks {};
        u16                               nextCompactedBrickIndex = 0;
        decltype(GpuChunkData::brick_map) compactedBrickMap {};
        for (u8 bCX = 0; bCX < 8; ++bCX)
        {
            for (u8 bCY = 0; bCY < 8; ++bCY)
            {
                for (u8 bCZ = 0; bCZ < 8; ++bCZ)
                {
                    MaybeBrickOffsetOrMaterialId nonCompactedMaybeOffset = nonCompactedBrickMap[bCX][bCY][bCZ];

                    if (nonCompactedMaybeOffset.isPointer())
                    {
                        const CombinedBrick& maybeCompactBrick = nonCompactedBricks[nonCompactedMaybeOffset._data];

                        const CombinedBrickReadResult compactionResult = maybeCompactBrick.isCompact();

                        if (compactionResult.solid)
                        {
                            compactedBrickMap[bCX][bCY][bCZ] =
                                MaybeBrickOffsetOrMaterialId::fromMaterial(compactionResult.voxel);
                        }
                        else
                        {
                            compactedBrickMap[bCX][bCY][bCZ] =
                                MaybeBrickOffsetOrMaterialId::fromOffset(nextCompactedBrickIndex);

                            compactedBricks.push_back(maybeCompactBrick);

                            nextCompactedBrickIndex += 1;
                        }
                    }
                    else if (nonCompactedMaybeOffset.getMaterial() != static_cast<u16>(Voxel::NullAirEmpty))
                    {
                        log::warn("erm what");

                        compactedBrickMap[bCX][bCY][bCZ] = nonCompactedMaybeOffset;
                    }
                }
            }
        }

        if (!cpuChunkData.brick_allocation.isNull())
        {
            this->brick_allocator.free(std::move(cpuChunkData.brick_allocation));
        }

        cpuChunkData.brick_allocation = this->brick_allocator.allocate(static_cast<u32>(compactedBricks.size()));

        gpuChunkData = {
            .aligned_chunk_coordinate {gpuChunkData.aligned_chunk_coordinate},
            .offset {util::RangeAllocator::getOffsetofAllocation(cpuChunkData.brick_allocation)}};
        std::memcpy(&gpuChunkData.brick_map[0][0][0], &compactedBrickMap[0][0][0], sizeof(GpuChunkData::brick_map));

        // log::trace("Compaction {} -> {}", nonCompactedBricks.size(), compactedBricks.size());
        if (!compactedBricks.empty())
        {
            this->renderer->getStager().enqueueTransfer(
                this->combined_bricks, gpuChunkData.offset, {compactedBricks.data(), compactedBricks.size()});
        }
    }

    void VoxelRenderer::recordCopyCommands(vk::CommandBuffer commandBuffer)
    {
        if (this->renderer->getFrameNumber() == 0 || util::receive<bool>("CLEAR_FACE_HASH_MAP").value_or(false))
        {
            commandBuffer.fillBuffer(*this->face_hash_map, 0, vk::WholeSize, ~0u);
        }
        else
        {
            commandBuffer.bindPipeline(
                vk::PipelineBindPoint::eCompute,
                this->renderer->getPipelineManager()->getPipeline(this->face_normalizer_pipeline));

            static_assert(faceHashTableCapacity % 1024 == 0);
            commandBuffer.dispatch(faceHashTableCapacity / 1024, 1, 1);
        }
    }

    void VoxelRenderer::recordPrepass(vk::CommandBuffer commandBuffer, const Camera&)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            this->renderer->getPipelineManager()->getPipeline(this->prepass_pipeline));

        commandBuffer.draw(36 * this->chunk_allocator.getUpperBoundOnAllocatedElements(), 1, 0, 0);
    }

    void VoxelRenderer::recordColorCalculation(vk::CommandBuffer commandBuffer)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            this->renderer->getPipelineManager()->getPipeline(this->color_calculation_pipeline));

        commandBuffer.pushConstants<u32>(
            this->renderer->getDescriptorManager()->getGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, {1});

        const vk::Extent2D framebufferSize = this->renderer->getWindow()->getFramebufferSize();

        commandBuffer.dispatch(
            static_cast<u32>(util::divideEuclideani32(static_cast<i32>(framebufferSize.width), 32) + 1),
            static_cast<u32>(util::divideEuclideani32(static_cast<i32>(framebufferSize.height), 32) + 1),
            1);
    }

    void VoxelRenderer::recordColorTransfer(vk::CommandBuffer commandBuffer)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            this->renderer->getPipelineManager()->getPipeline(this->color_transfer_pipeline));

        commandBuffer.draw(3, 1, 0, 0);
    }

    void VoxelRenderer::setLightInformation(GpuRaytracedLight light)
    {
        this->renderer->getStager().enqueueTransfer(this->lights, 0, {&light, 1});
    }

    // void VoxelRenderer::setAnimationTime(f32 time) const
    // {
    //     this->last_frame_time.store(time, std::memory_order_release);
    // }

    // void VoxelRenderer::setAnimationNumber(u32 animationIndex) const
    // {
    //     this->demo_index.store(animationIndex, std::memory_order_release);
    // }

    // std::span<const std::string> VoxelRenderer::getAnimationNames() const
    // {
    //     return this->names;
    // }
} // namespace gfx::generators::voxel