#pragma once

#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "util/util.hpp"
#include <array>
#include <glm/vec4.hpp>

namespace gfx::generators::voxel
{
    // TODO: enum bad... make a better registration system
    enum class Voxel : u16 // NOLINT(performance-enum-size)
    {
        NullAirEmpty = 0,
        Emerald,
        Ruby,
        Pearl,
        Obsidian,
        Brass,
        Chrome,
        Copper,
        Gold,
        Topaz,
        Sapphire,
        Amethyst,
        Jade,
        Diamond,
        Marble,
        Granite,
        Basalt,
        Limestone,
        Dirt,
        Grass,
    };

    struct VoxelMaterial
    {
        glm::vec4 ambient_color;
        glm::vec4 diffuse_color;
        glm::vec4 specular_color;
        glm::vec4 emissive_color_power;
        glm::vec4 coat_color_power;
        float     diffuse_subsurface_weight;
        float     specular;
        float     roughness;
        float     metallic;
    };

    VoxelMaterial getMaterialFromVoxel(Voxel v);

    core::vulkan::WriteOnlyBuffer<VoxelMaterial> generateMaterialBuffer(const core::Renderer*);
} // namespace gfx::generators::voxel