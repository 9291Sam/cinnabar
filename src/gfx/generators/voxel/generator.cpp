#include "generator.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"

namespace gfx::generators::voxel
{
    WorldGenerator::WorldGenerator(u64 seed_)
        : simplex {FastNoise::New<FastNoise::Simplex>()}
        , fractal {FastNoise::New<FastNoise::FractalFBm>()}
        , seed {seed_}
    {
        this->fractal->SetSource(this->simplex);
        this->fractal->SetOctaveCount(1);
        this->fractal->SetGain(1.024f);
        this->fractal->SetLacunarity(2.534f);
    }

    std::vector<std::pair<ChunkLocalPosition, Voxel>>
    WorldGenerator::generateChunk(AlignedChunkCoordinate chunkRoot) const
    {
        // const u32                  lod = 0;
        const voxel::WorldPosition root {voxel::WorldPosition::assemble(chunkRoot, {})};
        const i32                  lodres = 1; // gpu_calculateChunkVoxelSizeUnits(lod)

        // const i32 integerScale = static_cast<i32>(gpu_calculateChunkVoxelSizeUnits(lod));
        const i32 integerScale = lodres;

        auto gen2D = [&](float scale, std::size_t localSeed) -> std::unique_ptr<std::array<std::array<float, 64>, 64>>
        {
            std::unique_ptr<std::array<std::array<float, 64>, 64>> res {new std::array<std::array<float, 64>, 64>};

            this->fractal->GenUniformGrid2D(
                res->data()->data(),
                root.x / integerScale,
                root.z / integerScale,
                64,
                64,
                scale,
                static_cast<int>(localSeed));

            return res;
        };

        // auto gen3D =
        //     [&](float       scale,
        //         std::size_t localSeed) -> std::unique_ptr<std::array<std::array<std::array<float, 64>, 64>, 64>>
        // {
        //     std::unique_ptr<std::array<std::array<std::array<float, 64>, 64>, 64>> res {
        //         new std::array<std::array<std::array<float, 64>, 64>, 64>};

        //     this->fractal->GenUniformGrid3D(
        //         res->data()->data()->data(), root.x, root.z, root.y, 64, 64, 64, scale, static_cast<int>(localSeed));

        //     return res;
        // };

        auto height         = gen2D(static_cast<float>(integerScale) * 0.001f, this->seed + 487484);
        auto bumpHeight     = gen2D(static_cast<float>(integerScale) * 0.01f, this->seed + 7373834);
        auto mountainHeight = gen2D(static_cast<float>(integerScale) * 1.0f / 16384.0f, (this->seed * 3884) - 83483);
        // auto mainRock    = gen3D(static_cast<float>(integerScale) * 0.001f, this->seed - 747875);
        // auto pebblesRock = gen3D(static_cast<float>(integerScale) * 0.01f, this->seed -
        // 52649274); auto pebbles        = gen3D(static_cast<float>(integerScale) * 0.05f, this->seed - 948);

        std::vector<std::pair<ChunkLocalPosition, Voxel>> out {};
        out.reserve(32768);

        for (u8 j = 0; j < 64; ++j)
        {
            for (u8 i = 0; i < 64; ++i)
            {
                // const i32 unscaledWorldHeight =
                //     static_cast<i32>(std::exp2((*height)[j][i] * 12.0f));

                const i32 unscaledWorldHeight = static_cast<i32>(
                    (*height)[j][i] * 32.0f + (*bumpHeight)[j][i] * 2.0f); //+ (*mountainHeight)[j][i] * 256.0f)

                for (u8 h = 0; h < 64; ++h)
                {
                    const i32 worldHeightOfVoxel = static_cast<i32>(h * lodres) + root.y;
                    const i32 relativeDistanceToHeight =
                        (worldHeightOfVoxel - unscaledWorldHeight) + (4 * integerScale);

                    if (relativeDistanceToHeight < 0 * integerScale)
                    {
                        out.push_back({voxel::ChunkLocalPosition {{i, h, j}}, Voxel::Marble});
                    }
                    else if (relativeDistanceToHeight < 2 * integerScale)
                    {
                        out.push_back({voxel::ChunkLocalPosition {{i, h, j}}, voxel::Voxel::Dirt});
                    }
                    else if (relativeDistanceToHeight < 3 * integerScale)
                    {
                        if (rand() % 8192 * 8 == 0)
                        {
                            out.push_back(
                                {voxel::ChunkLocalPosition {{i, std::min(63, h + 6), j}}, voxel::Voxel::EmissiveWhite});
                        }
                        else
                        {
                            out.push_back({voxel::ChunkLocalPosition {{i, h, j}}, voxel::Voxel::Grass});
                        }
                    }
                }
            }
        }

        return out;
    }
} // namespace gfx::generators::voxel

// rolling hills
// a few mountains
// simple ores
// marble
