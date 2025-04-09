#pragma once

#include "data_structures.hpp"
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
        MaxVoxel,
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

    // TODO: importance sampling, subsurface, clear coat, anisotropic, transparency

    PBRVoxelMaterial getMaterialFromVoxel(Voxel v);
    Voxel            getRandomVoxel(u32);

    core::vulkan::WriteOnlyBuffer<PBRVoxelMaterial> generateMaterialBuffer(const core::Renderer*);
} // namespace gfx::generators::voxel