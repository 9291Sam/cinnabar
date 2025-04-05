#include "voxel_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/model.hpp"
#include "util/events.hpp"
#include "util/gif.hpp"
#include "util/util.hpp"
#include <atomic>
#include <cstddef>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/string_cast.hpp>
#include <span>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

static constexpr u32 MaxFaceHashMapNodes = 1u << 20u;

namespace gfx::generators::voxel
{
    VoxelRenderer::VoxelRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , prepass_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/voxel/voxel_prepass.slang"},
                  .fragment_shader_path {"src/gfx/generators/voxel/voxel_prepass.frag"},
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
        , color_calculation_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::ComputePipelineDescriptor {
                  .compute_shader_path {"src/gfx/generators/voxel/voxel_color_calculation.slang"},
                  .name {"Voxel Color Calculation"}})}
        , color_transfer_pipeline {this->renderer->getPipelineManager()->createPipeline(
              core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/voxel/voxel_color_transfer.slang"},
                  .fragment_shader_path {"src/gfx/generators/voxel/voxel_color_transfer.frag"},
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
        , chunk_data(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Chunk Bricks",
              0)
        , lights(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Voxel Lights",
              1)
        , combined_bricks(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              512,
              "Combined Bricks",
              2)
        , materials {generateMaterialBuffer(this->renderer)}
        , face_hash_map(
              this->renderer,
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              MaxFaceHashMapNodes,
              "Face Hash Map",
              5)
    {
        std::vector<std::byte> badAppleData =
            util::loadEntireFileFromPath(util::getCanonicalPathOfShaderFile("res/badapple6464.gif"));

        std::vector<std::byte> goodDragonData =
            util::loadEntireFileFromPath(util::getCanonicalPathOfShaderFile("res/dragon.vox"));

        this->demos.push_back(Demo {
            .model {AnimatedVoxelModel::fromGif(util::Gif {std::span {badAppleData}})},
            .sampler {[](glm::u32vec3 c, const AnimatedVoxelModel&)
                      {
                          return glm::u32vec3 {c.z, c.y, c.x};
                      }}});
        this->names.push_back("Bad Apple");

        this->demos.push_back(Demo {
            .model {AnimatedVoxelModel {StaticVoxelModel::fromVoxFile(std::span {goodDragonData})}},
            .sampler {[](glm::u32vec3 c, const AnimatedVoxelModel& m)
                      {
                          return glm::u32vec3 {c.x, m.getExtent().y - 64 + c.y, m.getExtent().z - 64 + c.z};
                          //   return c;
                      }}});
        this->names.push_back("Good Dragon");

        this->demos.push_back(Demo {
            .model {AnimatedVoxelModel {StaticVoxelModel::createCornelBox()}},
            .sampler {[](glm::u32vec3 c, const AnimatedVoxelModel&)
                      {
                          return c;
                      }}});
        this->names.push_back("Cornel Box");

        util::send("AllAnimationNames", std::vector {this->names});
    }

    VoxelRenderer::~VoxelRenderer()
    {
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->prepass_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->color_calculation_pipeline));
        this->renderer->getPipelineManager()->destroyPipeline(std::move(this->color_transfer_pipeline));
    }

    void VoxelRenderer::onFrameUpdate()
    {
        if (std::optional t = util::receive<f32>("SetAnimationTime"))
        {
            this->setAnimationTime(*t);
        }
        if (std::optional n = util::receive<u32>("SetAnimationNumber"))
        {
            this->setAnimationNumber(*n);
        }
        if (std::optional l = util::receive<voxel::GpuRaytracedLight>("UpdateLight"))
        {
            this->setLightInformation(*l);
        }
        const f32 deltaTime     = this->renderer->getWindow()->getDeltaTimeSeconds();
        const f32 lastFrameTime = this->last_frame_time.load(std::memory_order_acquire);
        const f32 thisFrameTime = lastFrameTime + deltaTime;

        auto& thisDemo = this->demos.at(this->demo_index.load(std::memory_order_acquire));

        const u32 lastFrameAnimationNumber =
            thisDemo.model.getFrameNumberAtTime(lastFrameTime, AnimatedVoxelModel::Looping {});
        const u32 thisFrameAnimationNumber =
            thisDemo.model.getFrameNumberAtTime(thisFrameTime, AnimatedVoxelModel::Looping {});

        this->last_frame_time.fetch_add(deltaTime, std::memory_order_acq_rel);

        if (thisFrameAnimationNumber != lastFrameAnimationNumber
            || (lastFrameAnimationNumber == 0 && thisFrameAnimationNumber == 0))
        {
            auto sensibleData = thisDemo.model.getFrame(thisFrameTime);

            std::vector<CombinedBrick> newCombinedBricks {};

            ChunkData newChunk {};

            u16 nextBrickIndex = 0;

            const u32 xExtent = std::min({64U, thisDemo.model.getExtent().x});
            const u32 yExtent = std::min({64U, thisDemo.model.getExtent().y});
            const u32 zExtent = std::min({64U, thisDemo.model.getExtent().z});

            for (u32 x = 0; x < xExtent; ++x)
            {
                for (u32 y = 0; y < yExtent; ++y)
                {
                    for (u32 z = 0; z < zExtent; ++z)
                    {
                        const ChunkLocalPosition cP {x, y, z};
                        const auto [bC, bP] = cP.split();

                        MaybeBrickOffsetOrMaterialId& maybeThisBrickOffset = newChunk.brick_map[bC.x][bC.y][bC.z];

                        const glm::u32vec3 sample = thisDemo.sampler(glm::u32vec3 {x, y, z}, thisDemo.model);

                        const Voxel& v = sensibleData[sample.x, sample.y, sample.z];

                        if (maybeThisBrickOffset.data == static_cast<u16>(~0u) && v != Voxel::NullAirEmpty)
                        {
                            maybeThisBrickOffset.data = nextBrickIndex;

                            newCombinedBricks.push_back(CombinedBrick {});

                            nextBrickIndex += 1;
                        }

                        if (v != Voxel::NullAirEmpty)
                        {
                            newCombinedBricks.at(maybeThisBrickOffset.data).write(bP, static_cast<u16>(v));
                        }
                    }
                }
            }

            if (!newCombinedBricks.empty())
            {
                this->renderer->getStager().enqueueTransfer(
                    this->combined_bricks, 0, {newCombinedBricks.data(), newCombinedBricks.size()});
                this->renderer->getStager().enqueueTransfer(this->chunk_data, 0, {&newChunk, 1});
            }
        }
    }

    void VoxelRenderer::recordCopyCommands(vk::CommandBuffer commandBuffer)
    {
        commandBuffer.fillBuffer(*this->face_hash_map, 0, vk::WholeSize, ~0u);
    }

    void VoxelRenderer::recordPrepass(vk::CommandBuffer commandBuffer, const Camera&)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            this->renderer->getPipelineManager()->getPipeline(this->prepass_pipeline));

        commandBuffer.draw(36, 1, 0, 0);
    }

    void VoxelRenderer::recordColorCalculation(vk::CommandBuffer commandBuffer)
    {
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            this->renderer->getPipelineManager()->getPipeline(this->color_calculation_pipeline));

        const vk::Extent2D framebufferSize = this->renderer->getWindow()->getFramebufferSize();

        commandBuffer.dispatch(
            util::divideEuclidean<u32>(framebufferSize.width, 32) + 1,
            util::divideEuclidean<u32>(framebufferSize.height, 32) + 1,
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

    void VoxelRenderer::setAnimationTime(f32 time) const
    {
        this->last_frame_time.store(time, std::memory_order_release);
    }

    void VoxelRenderer::setAnimationNumber(u32 animationIndex) const
    {
        this->demo_index.store(animationIndex, std::memory_order_release);
    }

    std::span<const std::string> VoxelRenderer::getAnimationNames() const
    {
        return this->names;
    }
} // namespace gfx::generators::voxel