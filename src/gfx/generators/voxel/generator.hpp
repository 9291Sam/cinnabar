#pragma once

#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "shared_data_structures.slang"
#include "util/util.hpp"
#include <FastNoise/FastNoise.h>

namespace gfx::generators::voxel
{

    class WorldGenerator
    {
    public:
        explicit WorldGenerator(u64 seed);

        [[nodiscard]] std::pair<BrickMap, std::vector<CombinedBrick>> generateChunkPreDense(ChunkLocation) const;

    private:

        FastNoise::SmartNode<FastNoise::Simplex>    simplex;
        FastNoise::SmartNode<FastNoise::FractalFBm> fractal;
        std::size_t                                 seed;
        i32                                         offset_x;
        i32                                         offset_z;
    };
} // namespace gfx::generators::voxel