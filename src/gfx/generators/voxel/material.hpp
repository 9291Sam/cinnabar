#pragma once

#include "util/util.hpp"
#include <array>
#include <glm/vec4.hpp>

namespace gfx::generators::voxel
{
    struct VoxelMaterial
    {
        glm::vec4 color;
    };

    // NOLINTNEXTLINE(performance-enum-size)
    enum Material : u16 // TODO: enum bad... make a better registration system
    {
        Air,
        Red,
        Green,
        Blue,
    };

    static constexpr std::array<VoxelMaterial, 4> Materials {
        VoxelMaterial {glm::vec4 {0.0, 0.0, 0.0, 1.0}},
        VoxelMaterial {glm::vec4 {1.0, 0.0, 0.0, 1.0}},
        VoxelMaterial {glm::vec4 {0.0, 1.0, 0.0, 1.0}},
        VoxelMaterial {glm::vec4 {0.0, 0.0, 1.0, 1.0}},
    };

} // namespace gfx::generators::voxel