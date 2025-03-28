#pragma once

#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/buffer.hpp"
#include "util/util.hpp"
#include <array>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace gfx::generators::voxel
{
    // TODO: enum bad... make a better registration system
    enum class Voxel : u16 // NOLINT(performance-enum-size)
    {
        NullAirEmpty = 0,
        Aluminum,
        Banana,
        Blackboard,
        Blood,
        Bone,
        Brass,
        Brick,
        CarPaint,
        Carrot,
        Charcoal,
        Chocolate,
        Chromium,
        Cobalt,
        Coffee,
        Concrete,
        CookingOil,
        Copper,
        Diamond,
        EggShell,
        EyeCornea,
        EyeLens,
        EyeSclera,
        Gasoline,
        Glass,
        Gold,
        GrayCard,
        Honey,
        Ice,
        Iron,
        Ketchup,
        Lead,
        Lemon,
        Marble,
        Mercury,
        Milk,
        MITBlack,
        MusouBlack,
        Nickel,
        OfficePaper,
        Pearl,
        Petroleum,
        Plastic,
        Platinum,
        Polyurethane,
        Salt,
        Sand,
        Sapphire,
        Silicon,
        Silver,
        SkinI,
        SkinII,
        SkinIII,
        SkinIV,
        SkinV,
        SkinVI,
        Snow,
        SoapBubble,
        Tire,
        Titanium,
        TonerBlack,
        Tungsten,
        Vanadium,
        Water,
        Whiteboard,
        Zinc,
        Ruby,
        Jade,
        EmissiveWhite,
        // Emerald,
        // Ruby,
        // Pearl,
        // Obsidian,
        // Brass,
        // Chrome,
        // Copper,
        // Gold,
        // Topaz,
        // Sapphire,
        // Amethyst,
        // Jade,
        // Diamond,
        // Marble,
        // Granite,
        // Basalt,
        // Limestone,
        // Dirt,
        // Grass,
    };

    struct PBRVoxelMaterial
    {
        // xyz - linear rgb color
        // w - roughness
        glm::vec4 albedo_roughness;
        // xyz - emissive color (values over 1 indicate more than 1 unit of brightness)
        // w - metallic
        glm::vec4 emission_metallic;
    };

    // TODO: importance sampling, subsurface, clear coat, anisotropic, transparency

    PBRVoxelMaterial getMaterialFromVoxel(Voxel v);

    core::vulkan::WriteOnlyBuffer<PBRVoxelMaterial> generateMaterialBuffer(const core::Renderer*);
} // namespace gfx::generators::voxel