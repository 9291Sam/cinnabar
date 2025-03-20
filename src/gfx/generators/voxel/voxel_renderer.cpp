#include "voxel_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/window.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "util/gif.hpp"
#include "util/util.hpp"
#include <bit>
#include <cstddef>
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
        std::vector<std::byte> data =
            util::loadEntireFileFromPath(util::getCanonicalPathOfShaderFile("res/badapple6464.gif"));

        this->bad_apple = util::Gif {std::span {data}};
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

        if (this->time_since_color_change > TimeBetweenFrames)
        {
            auto sensibleData = this->bad_apple.getFrameAtTime(this->time_in_video);

            this->time_since_color_change = 0.0f;
            std::vector<BooleanBrick> newVisbleBricks {};

            std::vector<MaterialBrick> newMaterialBricks {};
            ChunkBrickStorage          newChunk {};

            u16 nextBrickIndex = 0;

            for (u8 x = 0; x < ChunkSizeVoxels; ++x)
            {
                for (u8 y = 0; y < ChunkSizeVoxels; ++y)
                {
                    for (u8 z = 0; z < ChunkSizeVoxels; ++z)
                    {
                        const ChunkLocalPosition cP {x, y, z};
                        const auto [bC, bP] = cP.split();

                        MaybeBrickOffsetOrMaterialId& maybeThisBrickOffset = newChunk.modify(bC);

                        auto hash = [](u32 foo)
                        {
                            foo ^= foo >> 17;
                            foo *= 0xed5ad4bbU;
                            foo ^= foo >> 11;
                            foo *= 0xac4c1b51U;
                            foo ^= foo >> 15;
                            foo *= 0x31848babU;
                            foo ^= foo >> 14;

                            return foo;
                        };

                        const util::Gif::Color thisColor = sensibleData[(63UZ - x), z];
                        const f32              length    = glm::length(glm::vec3 {thisColor.color}) / 8.0f;

                        const bool shouldBeSolid = static_cast<f32>(y) < length; // (x + z) / 2 > y; //

                        if (maybeThisBrickOffset.data == static_cast<u16>(~0u) && shouldBeSolid)
                        {
                            maybeThisBrickOffset.data = nextBrickIndex;

                            newVisbleBricks.push_back(BooleanBrick {});
                            newMaterialBricks.push_back(MaterialBrick {});

                            nextBrickIndex += 1;
                        }

                        const Voxel v = static_cast<Voxel>((hash(y) % 17) + 1);

                        if (shouldBeSolid)
                        {
                            BooleanBrick&  thisVisiblityBrick = newVisbleBricks.at(maybeThisBrickOffset.data);
                            MaterialBrick& thisMaterialBrick  = newMaterialBricks.at(maybeThisBrickOffset.data);

                            // log::trace("{:#10x}", (*sensibleData)[x][z]);
                            // const Voxel v = (*sensibleData)[63 - x][z] == 0xFF000000 ? Voxel::Basalt : Voxel::Marble;

                            thisVisiblityBrick.write(bP, shouldBeSolid);
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