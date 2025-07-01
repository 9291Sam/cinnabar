#include "voxel_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/emissive_integer_tree.hpp"
#include "gfx/generators/voxel/light_influence_storage.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "gfx/shader_common/bindings.slang"
#include "util/allocators/range_allocator.hpp"
#include "util/events.hpp"
#include "util/logger.hpp"
#include "util/timer.hpp"
#include "util/util.hpp"
#include <cstddef>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/string_cast.hpp>
#include <span>
#include <tracy/Tracy.hpp>
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
    std::pair<BrickMap, std::vector<CombinedBrick>> appendVoxelsToDenseChunk(
        const BrickMap&                                       oldBrickMap,
        std::vector<CombinedBrick>                            oldBricks,
        std::span<const std::pair<ChunkLocalPosition, Voxel>> newVoxels)
    {
        BrickMap                   partiallyDenseBrickMap = oldBrickMap;
        std::vector<CombinedBrick> partiallyDenseBricks   = std::move(oldBricks);
        u16                        nextBrickId            = static_cast<u16>(partiallyDenseBricks.size());

        for (const auto& [cP, v] : newVoxels)
        {
            const auto [bC, bP] = cP.split();

            MaybeBrickOffsetOrMaterialId& maybeThisBrickOffset = partiallyDenseBrickMap[bC.x][bC.y][bC.z];

            if (maybeThisBrickOffset.isMaterial() && maybeThisBrickOffset.getMaterial() == static_cast<u16>(v))
            {
                // awesome, we need to insert a voxel and that brick is already a dense brick of those, do nothing!
            }
            else if (maybeThisBrickOffset.isMaterial())
            {
                // ok well its a dense brick, but not of what we need
                CombinedBrick workingBrick {};
                workingBrick.fill(maybeThisBrickOffset.getMaterial());
                workingBrick.write(bP, static_cast<u16>(v));

                const u16 newBrickPointer = nextBrickId;
                nextBrickId += 1;

                partiallyDenseBricks.push_back(workingBrick);
                maybeThisBrickOffset = MaybeBrickOffsetOrMaterialId::fromOffset(newBrickPointer);
            }
            else
            {
                const u16 brickPointer = maybeThisBrickOffset._data;

                partiallyDenseBricks[brickPointer].write(bP, static_cast<u16>(v));
            }
        }

        std::vector<CombinedBrick> compactedBricks {};
        u16                        nextCompactedBrickIndex = 0;
        BrickMap                   compactedBrickMap {};
        for (u8 bCX = 0; bCX < 8; ++bCX)
        {
            for (u8 bCY = 0; bCY < 8; ++bCY)
            {
                for (u8 bCZ = 0; bCZ < 8; ++bCZ)
                {
                    MaybeBrickOffsetOrMaterialId partiallyDenseOffset = partiallyDenseBrickMap[bCX][bCY][bCZ];

                    if (partiallyDenseOffset.isPointer())
                    {
                        const CombinedBrick& maybeCompactBrick = partiallyDenseBricks[partiallyDenseOffset._data];

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
                    else
                    {
                        // ok, we have a material brick, it's definitely dense, just copy it
                        compactedBrickMap[bCX][bCY][bCZ] = partiallyDenseOffset;
                    }
                }
            }
        }

        return std::make_pair(compactedBrickMap, std::move(compactedBricks));
    }

    std::pair<BrickMap, std::vector<CombinedBrick>>
    createDenseChunk(std::span<const std::pair<ChunkLocalPosition, Voxel>> input)
    {
        return appendVoxelsToDenseChunk({}, {}, input);
    }

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
                  .name {"Voxel Prepass pipeline"},
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
        , light_influence_storage {MaxVoxelLights}
        , chunk_allocator {MaxChunks}
        , gpu_chunk_data{
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxChunks,
              "Chunk Data",
              SBO_CHUNK_DATA}
        , cpu_chunk_data {MaxChunks}
        , chunk_hash_map{
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              chunkHashTableCapacity,
              "Chunk Hash Map",
              SBO_CHUNK_HASH_MAP}
        , brick_allocator{BricksToAllocate, MaxChunks}
        , combined_bricks{
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              BricksToAllocate,
              "Combined Bricks",
              SBO_COMBINED_BRICKS}
        , light_allocator {MaxVoxelLights}
        , lights{
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxVoxelLights,
              "Voxel Lights",
              SBO_VOXEL_LIGHTS}
        , face_hash_map{
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              faceHashTableCapacity,
              "Face Hash Map",
              SBO_FACE_HASH_MAP}
        , materials {generateMaterialBuffer(this->renderer)}
    {}

    VoxelRenderer::~VoxelRenderer()
    {
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->prepass_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->face_normalizer_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->color_calculation_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->color_transfer_pipeline));
    }

    VoxelRenderer::UniqueVoxelChunk VoxelRenderer::createVoxelChunkUnique(AlignedChunkCoordinate ac)
    {
        return UniqueVoxelChunk {this->createVoxelChunk(ac), this};
    }

    VoxelRenderer::VoxelChunk VoxelRenderer::createVoxelChunk(AlignedChunkCoordinate coordinate)
    {
        VoxelChunk newChunk = this->chunk_allocator.allocateOrPanic();
        const u32  chunkId  = this->chunk_allocator.getValueOfHandle(newChunk);

        this->gpu_chunk_data.write<&GpuChunkData::aligned_chunk_coordinate>(chunkId, coordinate);
        this->gpu_chunk_data.write<&GpuChunkData::offset>(chunkId, ~0u);
        assert::critical(this->cpu_chunk_data[chunkId].brick_allocation.isNull(), "should be empty");
        assert::critical(this->cpu_chunk_data[chunkId] == CpuChunkData {}, "should be default");

        insertUniqueChunkHashTable(this->chunk_hash_map, coordinate, {chunkId});

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
        removeUniqueChunkHashTable(this->chunk_hash_map, oldGpuChunkData.aligned_chunk_coordinate, {chunkId});
        this->chunk_allocator.free(std::move(c));

        oldGpuChunkData = {};
        oldCpuChunkData = {};
    }

    void VoxelRenderer::destroyVoxelLight(VoxelLight light)
    {
        const u16 lightId = this->light_allocator.getValueOfHandle(light);

        this->light_influence_storage.remove(lightId);

        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            this->lights.modify<&GpuRaytracedLight::color_and_power>(lightId) = {1.0, 0.5, 0.5, 100000};
        }

        this->light_allocator.free(std::move(light));
    }

    VoxelRenderer::VoxelLight VoxelRenderer::createVoxelLight(GpuRaytracedLight gpuLight)
    {
        VoxelLight newLight   = this->light_allocator.allocateOrPanic();
        const u16  newLightId = this->light_allocator.getValueOfHandle(newLight);

        this->light_influence_storage.insert(newLightId, gpuLight);
        this->lights.write(newLightId, gpuLight);

        return newLight;
    }

    VoxelRenderer::UniqueVoxelLight VoxelRenderer::createVoxelLightUnique(GpuRaytracedLight gpuLight)
    {
        return UniqueVoxelLight {this->createVoxelLight(gpuLight), this};
    }

    void VoxelRenderer::updateVoxelLight(const VoxelLight& light, GpuRaytracedLight gpuLight)
    {
        const u16 lightId = this->light_allocator.getValueOfHandle(light);

        this->light_influence_storage.update(lightId, gpuLight);
        this->lights.write(lightId, gpuLight);
    }

    void VoxelRenderer::preFrameUpdate()
    {
        ZoneScoped;

        const bool haveAnyLightsChanged = this->light_influence_storage.pack();

        if (haveAnyLightsChanged)
        {
            // util::Timer t {"generated light gpu uploads"};

            this->chunk_allocator.iterateThroughAllocatedElements(
                [this](u32 chunkId)
                {
                    std::vector<u16> polledLightIds =
                        this->light_influence_storage.poll(this->gpu_chunk_data.read(chunkId).aligned_chunk_coordinate);

                    std::ranges::sort(polledLightIds);

                    const GpuChunkData& readOnlyGpuChunkData = this->gpu_chunk_data.read(chunkId);

                    std::span<const u16> currentLightIds {
                        readOnlyGpuChunkData.nearby_light_ids.data(),
                        readOnlyGpuChunkData.nearby_light_ids.data() + readOnlyGpuChunkData.number_of_nearby_lights};

                    if (!std::ranges::equal(polledLightIds, currentLightIds))
                    {
                        GpuChunkData& partiallyCoherentGpuChunkData = this->gpu_chunk_data.modifyCoherentRangeSized(
                            chunkId,
                            offsetof(GpuChunkData, number_of_nearby_lights),
                            sizeof(GpuChunkData::number_of_nearby_lights)
                                + (polledLightIds.size()
                                   * sizeof(decltype(partiallyCoherentGpuChunkData.nearby_light_ids)::value_type)));

                        partiallyCoherentGpuChunkData.number_of_nearby_lights = static_cast<u16>(polledLightIds.size());
                        std::memcpy(
                            partiallyCoherentGpuChunkData.nearby_light_ids.data(),
                            polledLightIds.data(),
                            std::span<const u16> {polledLightIds}.size_bytes());
                    }
                });
        }

        this->gpu_chunk_data.flushViaStager(this->renderer->getStager());
        this->chunk_hash_map.flushViaStager(this->renderer->getStager());
        this->lights.flushViaStager(this->renderer->getStager());
    }

    void VoxelRenderer::setVoxelChunkData(
        const VoxelChunk& c, const BrickMap& compactBrickMap, std::span<const CombinedBrick> compactedBricks)
    {
        const u32 chunkId = this->chunk_allocator.getValueOfHandle(c);

        GpuChunkData& partiallyCoherentGpuChunkData = this->gpu_chunk_data.modifyCoherentRangeOffsets(
            chunkId, offsetof(GpuChunkData, offset), offsetof(GpuChunkData, brick_map) + sizeof(BrickMap));

        CpuChunkData& cpuChunkData = this->cpu_chunk_data[chunkId];

        if (!cpuChunkData.brick_allocation.isNull())
        {
            this->brick_allocator.free(std::move(cpuChunkData.brick_allocation));
        }

        if (!compactedBricks.empty())
        {
            cpuChunkData.brick_allocation = this->brick_allocator.allocate(static_cast<u32>(compactedBricks.size()));
        }
        else
        {
            cpuChunkData.brick_allocation = util::RangeAllocation {};
        }

        partiallyCoherentGpuChunkData.offset =
            util::RangeAllocator::getOffsetofAllocation(cpuChunkData.brick_allocation);
        partiallyCoherentGpuChunkData.brick_map = compactBrickMap;

        if (compactedBricks.empty())
        {
            assert::warn(
                partiallyCoherentGpuChunkData.offset == ~0u,
                "Empty chunk has brick allocation offset of {}",
                partiallyCoherentGpuChunkData.offset);
        }

        if (!compactedBricks.empty())
        {
            this->renderer->getStager().enqueueTransfer(
                this->combined_bricks,
                partiallyCoherentGpuChunkData.offset,
                {compactedBricks.data(), compactedBricks.size()});
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