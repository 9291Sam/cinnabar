#include "generator.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "model.hpp"
#include "util/logger.hpp"
#include "voxel_renderer.hpp"
#include <tuple>
#include <utility>

namespace gfx::generators::voxel
{
    namespace
    {
        void test_material_brick_writes()
        {
            MaterialBrick brick;

            // Case 1: Set all to same value (e.g., 7)
            for (i32 x = 0; x < 8; ++x)
            {
                for (i32 y = 0; y < 8; ++y)
                {
                    for (i32 z = 0; z < 8; ++z)
                    {
                        brick.write(BrickLocalPosition {x, y, z}, 7);
                    }
                }
            }

            {
                auto res = brick.isCompact();
                assert::critical(res.is_compact, "MaterialBrick (uniform 7) should be compact");
                assert::critical(res.voxel == 7, "Expected voxel value 7, got {}", res.voxel);
            }

            // Case 2: Change a single voxel
            brick.write(BrickLocalPosition {3, 3, 3}, 9); // different value

            {
                auto res = brick.isCompact();
                assert::critical(!res.is_compact, "MaterialBrick (1 diff at 3,3,3) should not be compact");
            }

            // Reset all to 5
            for (i32 x = 0; x < 8; ++x)
            {
                for (i32 y = 0; y < 8; ++y)
                {
                    for (i32 z = 0; z < 8; ++z)
                    {
                        brick.write(BrickLocalPosition {x, y, z}, 5);
                    }
                }
            }

            // Case 3: Checkerboard pattern
            for (i32 x = 0; x < 8; ++x)
            {
                for (i32 y = 0; y < 8; ++y)
                {
                    for (i32 z = 0; z < 8; ++z)
                    {
                        u16 val = ((x + y + z) % 2 == 0) ? 1 : 2;
                        brick.write(BrickLocalPosition {x, y, z}, val);
                    }
                }
            }

            {
                auto res = brick.isCompact();
                assert::critical(!res.is_compact, "MaterialBrick (checkerboard) should not be compact");
            }

            // Case 4: Write a full Z-slice (e.g., z = 0) to 99, rest are 0
            for (i32 x = 0; x < 8; ++x)
            {
                for (i32 y = 0; y < 8; ++y)
                {
                    for (i32 z = 0; z < 8; ++z)
                    {
                        brick.write(BrickLocalPosition {x, y, z}, 0);
                    }
                }
            }

            for (i32 x = 0; x < 8; ++x)
            {
                for (i32 y = 0; y < 8; ++y)
                {
                    brick.write(BrickLocalPosition {x, y, 0}, 99);
                }
            }

            {
                auto res = brick.isCompact();
                assert::critical(!res.is_compact, "MaterialBrick (plane z=0 set to 99) should not be compact");
            }

            // Case 5: Write corners to different values
            brick.write(BrickLocalPosition {0, 0, 0}, 42);
            brick.write(BrickLocalPosition {7, 7, 7}, 43);

            {
                auto v1 = brick.read(BrickLocalPosition {0, 0, 0});
                auto v2 = brick.read(BrickLocalPosition {7, 7, 7});
                assert::critical(v1 == 42, "Expected corner voxel 0,0,0 to be 42, got {}", v1);
                assert::critical(v2 == 43, "Expected corner voxel 7,7,7 to be 43, got {}", v2);
            }
        }

        void test_boolean_brick_isCompact()
        {
            BooleanBrick b;

            // Case 1: All false (empty)
            for (int i = 0; i < 8; ++i)
            {
                for (int j = 0; j < 8; ++j)
                {
                    for (int k = 0; k < 8; ++k)
                    {
                        b.write(BrickLocalPosition {i, j, k}, false);
                    }
                }
            }

            auto res1 = b.isCompact();
            assert::critical(res1.is_compact, "BooleanBrick (empty) should be compact");
            assert::critical(!res1.solid_or_empty, "BooleanBrick (empty) should report false value");

            // Case 2: All true (solid)
            for (int i = 0; i < 8; ++i)
            {
                for (int j = 0; j < 8; ++j)
                {
                    for (int k = 0; k < 8; ++k)
                    {
                        b.write(BrickLocalPosition {i, j, k}, true);
                    }
                }
            }

            auto res2 = b.isCompact();
            assert::critical(res2.is_compact, "BooleanBrick (solid) should be compact");
            assert::critical(res2.solid_or_empty, "BooleanBrick (solid) should report true value");

            // Case 3: Mixed values
            b.write(BrickLocalPosition {0, 0, 0}, false);
            b.write(BrickLocalPosition {1, 0, 0}, true);

            auto res3 = b.isCompact();
            assert::critical(!res3.is_compact, "BooleanBrick (mixed) should not be compact");
        }

        void test_combined_brick_isCompact()
        {
            CombinedBrick brick;

            // Case 1: Empty BooleanBrick + compact MaterialBrick
            for (int x = 0; x < 8; ++x)
            {
                for (int y = 0; y < 8; ++y)
                {
                    for (int z = 0; z < 8; ++z)
                    {
                        brick.write(BrickLocalPosition {x, y, z}, 0);
                    }
                }
            }

            auto result1 = brick.isCompact();
            assert::critical(result1.solid, "CombinedBrick (empty) should be considered compact");
            assert::critical(
                result1.voxel == 0, "CombinedBrick (empty) voxel value should be 0, got {}", result1.voxel);

            // Case 2: Solid BooleanBrick + compact MaterialBrick
            for (int x = 0; x < 8; ++x)
            {
                for (int y = 0; y < 8; ++y)
                {
                    for (int z = 0; z < 8; ++z)
                    {
                        brick.write(BrickLocalPosition {x, y, z}, 42);
                    }
                }
            }

            auto result2 = brick.isCompact();
            assert::critical(result2.solid, "CombinedBrick (solid) should be compact");
            assert::critical(
                result2.voxel == 42, "CombinedBrick (solid) voxel value should be 42, got {}", result2.voxel);

            // Case 3: Solid BooleanBrick + non-compact MaterialBrick
            brick.write(BrickLocalPosition {0, 0, 0}, 42);
            brick.write(BrickLocalPosition {1, 0, 0}, 43); // mixed material

            auto result3 = brick.isCompact();
            assert::critical(!result3.solid, "CombinedBrick (mixed material) should not be compact");
        }

    } // namespace
    WorldGenerator::WorldGenerator(u64 seed_)
        : simplex {FastNoise::New<FastNoise::Simplex>()}
        , fractal {FastNoise::New<FastNoise::FractalFBm>()}
        , seed {seed_}
    {
        test_material_brick_writes();
        test_boolean_brick_isCompact();
        test_combined_brick_isCompact();

        this->fractal->SetSource(this->simplex);
        this->fractal->SetOctaveCount(1);
        this->fractal->SetGain(1.024f);
        this->fractal->SetLacunarity(2.534f);
    }

    std::pair<BrickMap, std::vector<CombinedBrick>>
    WorldGenerator::generateChunkPreDense(AlignedChunkCoordinate chunkRoot) const
    {
        if (chunkRoot == AlignedChunkCoordinate {0, 0, 1})
        {
            std::vector<std::pair<gfx::generators::voxel::ChunkLocalPosition, gfx::generators::voxel::Voxel>>
                newVoxels {};

            gfx::generators::voxel::StaticVoxelModel cornelBox =
                gfx::generators::voxel::StaticVoxelModel::createCornelBox();

            const glm::u32vec3 extents = cornelBox.getExtent();
            const auto         voxels  = cornelBox.getModel();

            assert::critical(extents == glm::u32vec3 {64, 64, 64}, "no");

            for (u32 x = 0; x < extents.x; ++x)
            {
                for (u32 y = 0; y < extents.y; ++y)
                {
                    for (u32 z = 0; z < extents.z; ++z)
                    {
                        const gfx::generators::voxel::Voxel v = voxels[x, y, z];

                        if (v != gfx::generators::voxel::Voxel::NullAirEmpty)
                        {
                            newVoxels.push_back({gfx::generators::voxel::ChunkLocalPosition {63 - x, y, z}, v});
                        }
                    }
                }
            }

            return createDenseChunk(newVoxels);
        }

        // const u32                  lod = 0;
        const voxel::WorldPosition root {voxel::WorldPosition::assemble(chunkRoot, {})};
        const i32                  lodres = 1; // gpu_calculateChunkVoxelSizeUnits(lod)

        // const i32 integerScale = static_cast<i32>(gpu_calculateChunkVoxelSizeUnits(lod));
        const i32 integerScale = lodres;

        auto gen2D = [&](f32 scale, std::size_t localSeed) -> std::unique_ptr<std::array<std::array<f32, 64>, 64>>
        {
            std::unique_ptr<std::array<std::array<f32, 64>, 64>> res {new std::array<std::array<f32, 64>, 64>};

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
        //     [&](f32       scale,
        //         std::size_t localSeed) -> std::unique_ptr<std::array<std::array<std::array<f32, 64>, 64>, 64>>
        // {
        //     std::unique_ptr<std::array<std::array<std::array<f32, 64>, 64>, 64>> res {
        //         new std::array<std::array<std::array<f32, 64>, 64>, 64>};

        //     this->fractal->GenUniformGrid3D(
        //         res->data()->data()->data(), root.x, root.z, root.y, 64, 64, 64, scale, static_cast<int>(localSeed));

        //     return res;
        // };

        auto height     = gen2D(static_cast<f32>(integerScale) * 0.001f, this->seed + 487484);
        auto bumpHeight = gen2D(static_cast<f32>(integerScale) * 0.01f, this->seed + 7373834);
        // auto mountainHeight = gen2D(static_cast<f32>(integerScale) * 1.0f / 16384.0f, (this->seed * 3884) - 83483);
        // auto mainRock    = gen3D(static_cast<f32>(integerScale) * 0.001f, this->seed - 747875);
        // auto pebblesRock = gen3D(static_cast<f32>(integerScale) * 0.01f, this->seed -
        // 52649274); auto pebbles        = gen3D(static_cast<f32>(integerScale) * 0.05f, this->seed - 948);

        BrickMap                   brickMap {};
        std::vector<CombinedBrick> combinedBricks;
        u16                        nextFreeBrickIndex = 0;

        for (u8 bCX = 0; bCX < 8; ++bCX)
        {
            for (u8 bCY = 0; bCY < 8; ++bCY)
            {
                for (u8 bCZ = 0; bCZ < 8; ++bCZ)
                {
                    CombinedBrick workingBrick {};

                    for (u8 bPX = 0; bPX < 8; ++bPX)
                    {
                        for (u8 bPZ = 0; bPZ < 8; ++bPZ)
                        {
                            const std::size_t i = static_cast<std::size_t>((bCX * 8) + bPX);
                            const std::size_t j = static_cast<std::size_t>((bCZ * 8) + bPZ);

                            const i32 unscaledWorldHeight =
                                static_cast<i32>(((*height)[j][i] * 32.0f) + ((*bumpHeight)[j][i] * 2.0f));

                            for (u8 bPY = 0; bPY < 8; ++bPY)
                            {
                                const i32 h = (bCY * 8) + bPY;

                                const i32 worldHeightOfVoxel = static_cast<i32>(h * lodres) + root.y;
                                const i32 relativeDistanceToHeight =
                                    (worldHeightOfVoxel - unscaledWorldHeight) + (4 * integerScale);

                                const BrickLocalPosition pos {
                                    BrickLocalPosition::VectorType {bPX, bPY, bPZ}, UncheckedInDebugTag {}};

                                if (relativeDistanceToHeight < 0 * integerScale)
                                {
                                    workingBrick.write(pos, std::to_underlying(Voxel::Marble));
                                }
                                else if (relativeDistanceToHeight < 2 * integerScale)
                                {
                                    workingBrick.write(pos, std::to_underlying(Voxel::Dirt));
                                }
                                else if (relativeDistanceToHeight < 3 * integerScale)
                                {
                                    if (rand() % 1024 == 0)
                                    {
                                        const BrickLocalPosition pos2 {
                                            BrickLocalPosition::VectorType {bPX, std::min({7, bPY + 6}), bPZ},
                                            UncheckedInDebugTag {}};

                                        workingBrick.write(pos2, std::to_underlying(Voxel::EmissiveWhite));
                                    }
                                    else
                                    {
                                        workingBrick.write(pos, std::to_underlying(Voxel::Grass));
                                    }
                                }
                            }
                        }
                    }

                    MaybeBrickOffsetOrMaterialId& brickPointer = brickMap[bCX][bCY][bCZ];

                    const CombinedBrickReadResult brickCompactResult = workingBrick.isCompact();

                    if (brickCompactResult.solid)
                    {
                        brickPointer = MaybeBrickOffsetOrMaterialId::fromMaterial(brickCompactResult.voxel);
                    }
                    else
                    {
                        brickPointer = MaybeBrickOffsetOrMaterialId::fromOffset(nextFreeBrickIndex);
                        combinedBricks.push_back(workingBrick);
                        nextFreeBrickIndex += 1;
                    }
                }
            }
        }

        return std::make_tuple(brickMap, std::move(combinedBricks));
    }

} // namespace gfx::generators::voxel

// rolling hills
// a few mountains
// simple ores
// marble
