#include "voxel_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/model.hpp"
#include "util/gif.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <atomic>
#include <bit>
#include <cstddef>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/string_cast.hpp>
#include <span>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

// HACK: replace with event system
const gfx::generators::voxel::VoxelRenderer* GlobalVoxelRendererSneak = nullptr;

namespace gfx::generators::voxel
{
    VoxelRenderer::VoxelRenderer(const core::Renderer* renderer_)
        : renderer {renderer_}
        , pipeline {this->renderer->getPipelineManager()->createGraphicsPipeline(
              core::vulkan::GraphicsPipelineDescriptor {
                  .vertex_shader_path {"src/gfx/generators/voxel/voxel.vert"},
                  .fragment_shader_path {"src/gfx/generators/voxel/voxel.frag"},
                  .topology {vk::PrimitiveTopology::eTriangleList}, // remove
                  .polygon_mode {vk::PolygonMode::eFill},           // replace with dynamic state
                  .cull_mode {vk::CullModeFlagBits::eBack},
                  .front_face {vk::FrontFace::eClockwise}, // remove
                  .depth_test_enable {vk::True},
                  .depth_write_enable {vk::True},
                  .depth_compare_op {vk::CompareOp::eGreater}, // remove
                  .color_format {gfx::core::Renderer::ColorFormat.format},
                  .depth_format {gfx::core::Renderer::DepthFormat}, // remove lmao?
                  .blend_enable {vk::True},
                  .name {"Voxel pipeline"},
              })}
        , chunk_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              1,
              "Chunk Bricks")
        , visible_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              512,
              "Visible Bricks")
        , material_bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              512,
              "Material Bricks")
        , materials {generateMaterialBuffer(this->renderer)}
    {
        GlobalVoxelRendererSneak = this;

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
    }

    VoxelRenderer::~VoxelRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->pipeline));
    }

    void VoxelRenderer::renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const Camera&)
    {
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

            std::vector<BooleanBrick> newVisbleBricks {};

            std::vector<MaterialBrick> newMaterialBricks {};
            ChunkBrickStorage          newChunk {};

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

                        MaybeBrickOffsetOrMaterialId& maybeThisBrickOffset = newChunk.modify(bC);

                        const glm::u32vec3 sample = thisDemo.sampler(glm::u32vec3 {x, y, z}, thisDemo.model);

                        const Voxel& v = sensibleData[sample.x, sample.y, sample.z];

                        if (maybeThisBrickOffset.data == static_cast<u16>(~0u) && v != Voxel::NullAirEmpty)
                        {
                            maybeThisBrickOffset.data = nextBrickIndex;

                            newVisbleBricks.push_back(BooleanBrick {});
                            newMaterialBricks.push_back(MaterialBrick {});

                            nextBrickIndex += 1;
                        }

                        if (v != Voxel::NullAirEmpty)
                        {
                            BooleanBrick&  thisVisiblityBrick = newVisbleBricks.at(maybeThisBrickOffset.data);
                            MaterialBrick& thisMaterialBrick  = newMaterialBricks.at(maybeThisBrickOffset.data);

                            // log::trace("{:#10x}", (*sensibleData)[x][z]);
                            // const Voxel v = (*sensibleData)[63 - x][z] == 0xFF000000 ? Voxel::Basalt :
                            // Voxel::Marble;

                            thisVisiblityBrick.write(bP, true);
                            thisMaterialBrick.write(bP, v);
                        }
                    }
                }
            }

            if (!newVisbleBricks.empty())
            {
                this->renderer->getStager().enqueueTransfer(
                    this->visible_bricks, 0, {newVisbleBricks.data(), newVisbleBricks.size()});
                this->renderer->getStager().enqueueTransfer(
                    this->material_bricks, 0, {newMaterialBricks.data(), newMaterialBricks.size()});
                this->renderer->getStager().enqueueTransfer(this->chunk_bricks, 0, {&newChunk, 1});
            }
        }

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, this->renderer->getPipelineManager()->getPipeline(this->pipeline));

        commandBuffer.pushConstants<std::array<u32, 4>>(
            this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
            vk::ShaderStageFlagBits::eAll,
            0,
            std::array<u32, 4> {
                this->chunk_bricks.getStorageDescriptor().getOffset(),
                this->visible_bricks.getStorageDescriptor().getOffset(),
                this->material_bricks.getStorageDescriptor().getOffset(),
                this->materials.getStorageDescriptor().getOffset()});

        commandBuffer.draw(36, 1, 0, 0);
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