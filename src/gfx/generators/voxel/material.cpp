#include "material.hpp"

namespace gfx::generators::voxel
{
    PBRVoxelMaterial getMaterialFromVoxel(Voxel v)
    {
        switch (v)
        {
        case Voxel::Aluminum:
            return PBRVoxelMaterial {
                .albedo_roughness {0.912f, 0.914f, 0.92f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Banana:
            return PBRVoxelMaterial {
                .albedo_roughness {0.634f, 0.532f, 0.111f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Blackboard:
            return PBRVoxelMaterial {
                .albedo_roughness {0.039f, 0.039f, 0.039f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Blood:
            return PBRVoxelMaterial {
                .albedo_roughness {0.64448f, 0.003f, 0.005f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Bone:
            return PBRVoxelMaterial {
                .albedo_roughness {0.793f, 0.793f, 0.664f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Brass:
            return PBRVoxelMaterial {
                .albedo_roughness {0.887f, 0.789f, 0.434f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Brick:
            return PBRVoxelMaterial {
                .albedo_roughness {0.262f, 0.095f, 0.061f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::CarPaint:
            return PBRVoxelMaterial {
                .albedo_roughness {0.1f, 0.1f, 0.1f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Carrot:
            return PBRVoxelMaterial {
                .albedo_roughness {0.713f, 0.17f, 0.026f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Charcoal:
            return PBRVoxelMaterial {
                .albedo_roughness {0.02f, 0.02f, 0.02f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Chocolate:
            return PBRVoxelMaterial {
                .albedo_roughness {0.162f, 0.091f, 0.06f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Chromium:
            return PBRVoxelMaterial {
                .albedo_roughness {0.638f, 0.651f, 0.663f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Cobalt:
            return PBRVoxelMaterial {
                .albedo_roughness {0.692f, 0.703f, 0.673f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Coffee:
            return PBRVoxelMaterial {
                .albedo_roughness {0.027f, 0.019f, 0.018f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Concrete:
            return PBRVoxelMaterial {
                .albedo_roughness {0.51f, 0.51f, 0.51f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::CookingOil:
            return PBRVoxelMaterial {
                .albedo_roughness {0.737911f, 0.687f, 0.091f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Copper:
            return PBRVoxelMaterial {
                .albedo_roughness {0.926f, 0.721f, 0.504f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Diamond:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::EggShell:
            return PBRVoxelMaterial {
                .albedo_roughness {0.61f, 0.624f, 0.631f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::EyeCornea:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::EyeLens:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::EyeSclera:
            return PBRVoxelMaterial {
                .albedo_roughness {0.68f, 0.49f, 0.37f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Gasoline:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 0.97f, 0.617f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Glass:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Gold:
            return PBRVoxelMaterial {
                .albedo_roughness {0.944f, 0.776f, 0.373f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::GrayCard:
            return PBRVoxelMaterial {
                .albedo_roughness {0.18f, 0.18f, 0.18f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Honey:
            return PBRVoxelMaterial {
                .albedo_roughness {0.83077f, 0.397f, 0.038f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Ice:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Iron:
            return PBRVoxelMaterial {
                .albedo_roughness {0.531f, 0.512f, 0.496f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Ketchup:
            return PBRVoxelMaterial {
                .albedo_roughness {0.164f, 0.006f, 0.002f, 0.1f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Lead:
            return PBRVoxelMaterial {
                .albedo_roughness {0.632f, 0.626f, 0.641f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Lemon:
            return PBRVoxelMaterial {
                .albedo_roughness {0.718f, 0.483f, 0.0f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Marble:
            return PBRVoxelMaterial {
                .albedo_roughness {0.83f, 0.791f, 0.753f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Mercury:
            return PBRVoxelMaterial {
                .albedo_roughness {0.781f, 0.779f, 0.779f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Milk:
            return PBRVoxelMaterial {
                .albedo_roughness {0.815f, 0.813f, 0.682f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::MITBlack:
            return PBRVoxelMaterial {
                .albedo_roughness {5e-05, 5e-05f, 5e-05f, 0.99f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::MusouBlack:
            return PBRVoxelMaterial {
                .albedo_roughness {0.006f, 0.006f, 0.006f, 0.9f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Nickel:
            return PBRVoxelMaterial {
                .albedo_roughness {0.649f, 0.61f, 0.541f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::OfficePaper:
            return PBRVoxelMaterial {
                .albedo_roughness {0.794f, 0.834f, 0.884f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Pearl:
            return PBRVoxelMaterial {
                .albedo_roughness {0.8f, 0.75f, 0.7f, 0.35f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Petroleum:
            return PBRVoxelMaterial {
                .albedo_roughness {0.03f, 0.027f, 0.024f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Plastic:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Platinum:
            return PBRVoxelMaterial {
                .albedo_roughness {0.679f, 0.642f, 0.588f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Polyurethane:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Salt:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.2f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Sand:
            return PBRVoxelMaterial {
                .albedo_roughness {0.44f, 0.386f, 0.23074f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Sapphire:
            return PBRVoxelMaterial {
                .albedo_roughness {0.67f, 0.764f, 0.855f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Silicon:
            return PBRVoxelMaterial {
                .albedo_roughness {0.344f, 0.367f, 0.419f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Silver:
            return PBRVoxelMaterial {
                .albedo_roughness {0.962f, 0.949f, 0.922f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::SkinI:
            return PBRVoxelMaterial {
                .albedo_roughness {0.847f, 0.638f, 0.552f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::SkinII:
            return PBRVoxelMaterial {
                .albedo_roughness {0.799f, 0.485f, 0.347f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::SkinIII:
            return PBRVoxelMaterial {
                .albedo_roughness {0.623f, 0.433f, 0.343f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::SkinIV:
            return PBRVoxelMaterial {
                .albedo_roughness {0.436f, 0.227f, 0.131f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::SkinV:
            return PBRVoxelMaterial {
                .albedo_roughness {0.283f, 0.148f, 0.079f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::SkinVI:
            return PBRVoxelMaterial {
                .albedo_roughness {0.09f, 0.05f, 0.02f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Snow:
            return PBRVoxelMaterial {
                .albedo_roughness {0.85f, 0.85f, 0.85f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::SoapBubble:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Tire:
            return PBRVoxelMaterial {
                .albedo_roughness {0.023f, 0.023f, 0.023f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Titanium:
            return PBRVoxelMaterial {
                .albedo_roughness {0.616f, 0.582f, 0.544f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::TonerBlack:
            return PBRVoxelMaterial {
                .albedo_roughness {0.05f, 0.05f, 0.05f, 0.5f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Tungsten:
            return PBRVoxelMaterial {
                .albedo_roughness {0.504f, 0.498f, 0.478f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Vanadium:
            return PBRVoxelMaterial {
                .albedo_roughness {0.52f, 0.532f, 0.541f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Water:
            return PBRVoxelMaterial {
                .albedo_roughness {1.0f, 1.0f, 1.0f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Whiteboard:
            return PBRVoxelMaterial {
                .albedo_roughness {0.869f, 0.867f, 0.771f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Zinc:
            return PBRVoxelMaterial {
                .albedo_roughness {0.802f, 0.844f, 0.863f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 1.0f},
            };
        case Voxel::Ruby:
            return PBRVoxelMaterial {
                .albedo_roughness {0.61424, 0.04136, 0.04136, 0.02f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::Jade:
            return PBRVoxelMaterial {
                .albedo_roughness {0.54f, 0.89f, 0.63f, 0.1f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        case Voxel::EmissiveWhite:
            return PBRVoxelMaterial {
                .albedo_roughness {0.0f, 0.0f, 0.0f, 0.0f}, .emission_metallic {32.0f, 32.0f, 32.0f, 0.0f}};
        case Voxel::NullAirEmpty:
            [[fallthrough]];
        default:
            return PBRVoxelMaterial {
                .albedo_roughness {0.8f, 0.4f, 0.4f, 0.0f},
                .emission_metallic {0.0f, 0.0f, 0.0f, 0.0f},
            };
        }
    }
    /*
    PBRVoxelMaterial old(Voxel v)
    {
        switch (v)
        {
        case Voxel::Emerald:
            return PBRVoxelMaterial {
                .ambient_color {0.0215, 0.1745, 0.0215, 0.55},
                .diffuse_color {0.07568, 0.61424, 0.07568, 0.55},
                .specular_color {0.633, 0.727811, 0.633, 0.55},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {76.87483f},
                .roughness {0.02f},
                .metallic {0.0},
            };
        case Voxel::Ruby:
            return PBRVoxelMaterial {
                .ambient_color {0.1745, 0.01175, 0.01175, 0.55},
                .diffuse_color {0.61424, 0.04136, 0.04136, 0.55},
                .specular_color {0.727811, 0.626959, 0.626959, 0.55},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {76.89304f},
                .roughness {0.02f},
                .metallic {0.0},
            };
        case Voxel::Pearl:
            return PBRVoxelMaterial {
                .ambient_color {0.25, 0.20725, 0.20725, 0.922},
                .diffuse_color {1.0, 0.829, 0.829, 0.922},
                .specular_color {0.296648, 0.296648, 0.296648, 0.922},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {11.264f},
                .roughness {0.1f},
                .metallic {0.0},
            };
        case Voxel::Obsidian:
            return PBRVoxelMaterial {
                .ambient_color {0.05375, 0.05, 0.06625, 0.82},
                .diffuse_color {0.18275, 0.17, 0.22525, 0.82},
                .specular_color {0.332741, 0.328634, 0.346435, 0.82},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {38.4394f},
                .roughness {0.2f},
                .metallic {0.0},
            };
        case Voxel::Brass:
            return PBRVoxelMaterial {
                .ambient_color {0.329412, 0.223529, 0.027451, 1.0},
                .diffuse_color {0.780392, 0.568627, 0.113725, 1.0},
                .specular_color {0.992157, 0.941176, 0.807843, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {27.8974f},
                .roughness {0.05f},
                .metallic {0.5},
            };
        case Voxel::Chrome:
            return PBRVoxelMaterial {
                .ambient_color {0.25, 0.25, 0.25, 1.0},
                .diffuse_color {0.4, 0.4, 0.4, 1.0},
                .specular_color {0.774597, 0.774597, 0.774597, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {76.88138f},
                .roughness {0.01f},
                .metallic {1.0},
            };
        case Voxel::Copper:
            return PBRVoxelMaterial {
                .ambient_color {0.19125, 0.0735, 0.0225, 1.0},
                .diffuse_color {0.7038, 0.27048, 0.0828, 1.0},
                .specular_color {0.256777, 0.137622, 0.086014, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {12.84395f},
                .roughness {0.05f},
                .metallic {1.0},
            };
        case Voxel::Gold:
            return PBRVoxelMaterial {
                .ambient_color {0.24725, 0.1995, 0.0745, 1.0},
                .diffuse_color {0.75164, 0.60648, 0.22648, 1.0},
                .specular_color {0.628281, 0.555802, 0.366065, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {51.28434f},
                .roughness {0.02f},
                .metallic {1.0},
            };
        case Voxel::Topaz:
            return PBRVoxelMaterial {
                .ambient_color {0.23125, 0.1425, 0.035, 1.0},
                .diffuse_color {0.904, 0.493, 0.058, 1.0},
                .specular_color {0.628, 0.556, 0.366, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.1f},
                .specular {51.2f},
                .roughness {0.05f},
                .metallic {0.0},
            };
        case Voxel::Sapphire:
            return PBRVoxelMaterial {
                .ambient_color {0.00045, 0.00045, 0.20015, 1.0},
                .diffuse_color {0.07085, 0.23568, 0.73554, 1.0},
                .specular_color {0.71396, 0.77568, 0.78563, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {51.32f},
                .roughness {0.02f},
                .metallic {0.0},
            };
        case Voxel::Amethyst:
            return PBRVoxelMaterial {
                .ambient_color {0.17258, 0.03144, 0.34571, 1.0},
                .diffuse_color {0.58347, 0.39256, 0.79752, 1.0},
                .specular_color {0.90584, 0.81946, 0.93425, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.1021f},
                .specular {68.00452f},
                .roughness {0.032f},
                .metallic {0.0},
            };
        case Voxel::Jade:
            return PBRVoxelMaterial {
                .ambient_color {0.135f, 0.2225f, 0.1575f, 1.0},
                .diffuse_color {0.54f, 0.89f, 0.63f, 1.0},
                .specular_color {0.316228f, 0.316228f, 0.316228f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.2f},
                .specular {12.8f},
                .roughness {0.1f},
                .metallic {0.0},
            };
        case Voxel::Diamond:
            return PBRVoxelMaterial {
                .ambient_color {0.25f, 0.25f, 0.25f, 1.0},
                .diffuse_color {0.9f, 0.9f, 0.9f, 1.0},
                .specular_color {0.774597f, 0.774597f, 0.774597f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.3f},
                .specular {76.8f},
                .roughness {0.028f},
                .metallic {0.0},
            };
        case Voxel::Marble:
            return PBRVoxelMaterial {
                .ambient_color {0.19125f, 0.19125f, 0.19125f, 1.0},
                .diffuse_color {0.85f, 0.85f, 0.85f, 1.0},
                .specular_color {0.332741f, 0.332741f, 0.332741f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.15f},
                .specular {11.264f},
                .roughness {0.092f},
                .metallic {0.0},
            };
        case Voxel::Granite:
            return PBRVoxelMaterial {
                .ambient_color {0.2175f, 0.2175f, 0.2175f, 1.0},
                .diffuse_color {0.78f, 0.73f, 0.71f, 1.0},
                .specular_color {0.245455f, 0.245455f, 0.245455f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {9.846f},
                .roughness {0.116f},
                .metallic {0.0},
            };
        case Voxel::Basalt:
            return PBRVoxelMaterial {
                .ambient_color {0.11f, 0.11f, 0.11f, 1.0},
                .diffuse_color {0.23f, 0.23f, 0.23f, 1.0},
                .specular_color {0.282723f, 0.282723f, 0.282723f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {8.932f},
                .roughness {0.148f},
                .metallic {0.0},
            };
        case Voxel::Limestone:
            return PBRVoxelMaterial {
                .ambient_color {0.19225f, 0.19225f, 0.17225f, 1.0},
                .diffuse_color {0.78f, 0.78f, 0.74f, 1.0},
                .specular_color {0.296648f, 0.296648f, 0.296648f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.05f},
                .specular {10.234f},
                .roughness {0.128f},
                .metallic {0.0},
            };
        case Voxel::Dirt:
            return PBRVoxelMaterial {
                .ambient_color {0.3828125f, 0.28515625, 0.1953125f, 1.0},
                .diffuse_color {0.3828125f, 0.28515625, 0.1953125f, 1.0},
                .specular_color {0.296648f, 0.296648f, 0.296648f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.00f},
                .specular {0.01f},
                .roughness {1.0f},
                .metallic {0.0},
            };

        case Voxel::Grass:
            return PBRVoxelMaterial {
                .ambient_color {0.140625f, 0.28515625f, 0.12445385f, 1.0},
                .diffuse_color {0.140625f, 0.28515625f, 0.12445385f, 1.0},
                .specular_color {0.034848f, 0.05945f, 0.1642348f, 1.0},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.00f},
                .specular {0.01f},
                .roughness {1.0f},
                .metallic {0.0},
            };
        case Voxel::NullAirEmpty:
            [[fallthrough]];
        default:
            return PBRVoxelMaterial {
                .ambient_color {0.8f, 0.4f, 0.4f, 1.0f},
                .diffuse_color {0.8f, 0.4f, 0.4f, 1.0f},
                .specular_color {0.8f, 0.4f, 0.4f, 1.0f},
                .emissive_color_power {},
                .coat_color_power {},
                .diffuse_subsurface_weight {0.0},
                .specular {0.0},
                .roughness {0.0},
                .metallic {0.0},

            };
        }
    }

    */

    core::vulkan::WriteOnlyBuffer<PBRVoxelMaterial> generateMaterialBuffer(const core::Renderer* renderer)
    {
        std::vector<PBRVoxelMaterial> materials {};
        materials.reserve(std::numeric_limits<u16>::max());

        for (u16 i = 0; i < std::numeric_limits<u16>::max(); ++i)
        {
            materials.push_back(getMaterialFromVoxel(static_cast<Voxel>(i)));
        }

        gfx::core::vulkan::WriteOnlyBuffer<PBRVoxelMaterial> buffer {
            renderer,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,
            materials.size(),
            "Voxel Material Buffer",
            4};

        buffer.uploadImmediate(0, materials);

        return buffer;
    }
} // namespace gfx::generators::voxel