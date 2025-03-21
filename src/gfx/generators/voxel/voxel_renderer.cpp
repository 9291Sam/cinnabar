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
#include <bit>
#include <cstddef>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/string_cast.hpp>
#include <span>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

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
        std::vector<std::byte> badAppleData =
            util::loadEntireFileFromPath(util::getCanonicalPathOfShaderFile("res/badapple6464.gif"));

        std::vector<std::byte> goodDragonData =
            util::loadEntireFileFromPath(util::getCanonicalPathOfShaderFile("res/dragon.vox"));

        this->bad_apple   = AnimatedVoxelModel::fromGif(util::Gif {std::span {badAppleData}});
        this->good_dragon = StaticVoxelModel::fromVoxFile(std::span {goodDragonData});

        log::debug("Loaded model with Extents: {}", glm::to_string(this->good_dragon.getExtent()));
    }

    f32 VoxelRenderer::time_in_video = 0;

    VoxelRenderer::~VoxelRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->pipeline));
    }

    void VoxelRenderer::renderIntoCommandBuffer(vk::CommandBuffer commandBuffer, const Camera&)
    {
        this->time_since_color_change += this->renderer->getWindow()->getDeltaTimeSeconds();
        this->time_in_video += this->renderer->getWindow()->getDeltaTimeSeconds();

        static constexpr float TimeBetweenFrames = 1.0f / 30.0f;

        auto& sampler = this->bad_apple;

        if (this->time_since_color_change > TimeBetweenFrames)
        {
            auto sensibleData = sampler.getFrame(this->time_in_video);

            // auto sensibleData = sampler.getModel();

            this->time_since_color_change = 0.0f;
            std::vector<BooleanBrick> newVisbleBricks {};

            std::vector<MaterialBrick> newMaterialBricks {};
            ChunkBrickStorage          newChunk {};

            u16 nextBrickIndex = 0;

            const u32 xExtent = std::min({64U, sampler.getExtent().x});
            const u32 yExtent = std::min({64U, sampler.getExtent().y});
            const u32 zExtent = std::min({64U, sampler.getExtent().z});

            for (u32 x = 0; x < xExtent; ++x)
            {
                for (u32 y = 0; y < yExtent; ++y)
                {
                    for (u32 z = 0; z < zExtent; ++z)
                    {
                        const ChunkLocalPosition cP {x, y, z};
                        const auto [bC, bP] = cP.split();

                        MaybeBrickOffsetOrMaterialId& maybeThisBrickOffset = newChunk.modify(bC);

                        const Voxel& v =
                            sensibleData[x, sampler.getExtent().y - 64 + y, sampler.getExtent().z - 64 + z];

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
} // namespace gfx::generators::voxel