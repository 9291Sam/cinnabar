#pragma once

#include "gfx/core/vulkan/buffer.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/generators/generator.hpp"
#include <random>

namespace gfx::core
{
    class Renderer;
} // namespace gfx::core

namespace gfx::generators::voxel
{
    struct BooleanBrick
    {
        std::array<u32, 16> data;
    };

    class VoxelRenderer : Generator
    {
    public:

        explicit VoxelRenderer(const core::Renderer*);
        ~VoxelRenderer() override;

        VoxelRenderer(const VoxelRenderer&)             = delete;
        VoxelRenderer(VoxelRenderer&&)                  = delete;
        VoxelRenderer& operator= (const VoxelRenderer&) = delete;
        VoxelRenderer& operator= (VoxelRenderer&&)      = delete;

        void renderIntoCommandBuffer(
            vk::CommandBuffer,
            const Camera&,
            core::vulkan::DescriptorHandle<vk::DescriptorType::eStorageBuffer> globalDescriptorInfo) override;

    private:
        const core::Renderer*                                renderer;
        gfx::core::vulkan::PipelineManager::GraphicsPipeline pipeline;

        gfx::core::vulkan::GpuOnlyBuffer<BooleanBrick> bricks;
        std::mt19937                                   generator;
    };

} // namespace gfx::generators::voxel