#pragma once

#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "util/util.hpp"
#include <FastNoise/FastNoise.h>

namespace gfx::generators::voxel
{

    class WorldGenerator
    {
    public:
        explicit WorldGenerator(u64 seed);

        [[nodiscard]] std::vector<std::pair<ChunkLocalPosition, Voxel>> generateChunk(AlignedChunkCoordinate) const;
        [[nodiscard]] std::tuple<BrickMap, std::vector<CombinedBrick>>
            generateChunkPreDense(AlignedChunkCoordinate) const;

    private:

        FastNoise::SmartNode<FastNoise::Simplex>    simplex;
        FastNoise::SmartNode<FastNoise::FractalFBm> fractal;
        std::size_t                                 seed;
    };
} // namespace gfx::generators::voxel