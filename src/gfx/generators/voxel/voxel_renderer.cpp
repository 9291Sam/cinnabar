#include "voxel_renderer.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/generators/generator.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include <glm/gtx/string_cast.hpp>
#include <random>
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
        , bricks(
              this->renderer->getAllocator(),
              vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
              vk::MemoryPropertyFlagBits::eDeviceLocal,
              512,
              "Temporary Boolean Bricks")
        , generator {std::random_device {}()}
    {
        std::vector<BooleanBrick> newBricks {};
        ChunkBrickStorage         newChunk {};

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

                    const bool shouldBeSolid = (z + x) / 2 > y;

                    if (maybeThisBrickOffset.data == static_cast<u16>(~0u) && shouldBeSolid)
                    {
                        maybeThisBrickOffset.data = nextBrickIndex;

                        log::trace("inserted new brick {}", maybeThisBrickOffset.data);

                        newBricks.push_back(BooleanBrick {});

                        nextBrickIndex += 1;
                    }

                    if (shouldBeSolid)
                    {
                        BooleanBrick& thisBrick = newBricks.at(maybeThisBrickOffset.data);

                        thisBrick.write(bP, shouldBeSolid);
                    }
                }
            }
        }

        this->renderer->getStager().enqueueTransfer(this->bricks, 0, {newBricks.data(), 512});
        this->renderer->getStager().enqueueTransfer(this->chunk_bricks, 0, {&newChunk, 1});
    }

    VoxelRenderer::~VoxelRenderer()
    {
        this->renderer->getPipelineManager()->destroyGraphicsPipeline(std::move(this->pipeline));
    }

    void VoxelRenderer::renderIntoCommandBuffer(
        vk::CommandBuffer commandBuffer,
        const Camera&,
        core::vulkan::DescriptorHandle<vk::DescriptorType::eUniformBuffer> globalDescriptorInfo)
    {
        // BooleanBrick brick {};

        // for (u32& d : brick.data)
        // {
        //     d = this->generator();
        // }

        // this->renderer->getStager().enqueueTransfer(this->bricks, 0, {&brick, 1});

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, this->renderer->getPipelineManager()->getPipeline(this->pipeline));

        commandBuffer.pushConstants<std::array<u32, 3>>(
            this->renderer->getDescriptorManager()->getGlobalPipelineLayout(),
            vk::ShaderStageFlagBits::eAll,
            0,
            std::array<u32, 3> {
                globalDescriptorInfo.getOffset(),
                this->bricks.getStorageDescriptor().getOffset(),
                this->chunk_bricks.getStorageDescriptor().getOffset()});

        commandBuffer.draw(36, 1, 0, 0);
    }
} // namespace gfx::generators::voxel