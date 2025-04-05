#pragma once

#ifdef __cplusplus
    #include "util/util.hpp"
    #include <glm/vec3.hpp>
    #include <glm/vec4.hpp>
    
    #define float3 glm::vec3
    #define float4 glm::vec4

    #define INLINE inline
    #define CONST_MEMBER_FUNCTION const
    #define MUTABLE_MEMBER_FUNCTION
    #define NODISCARD [[nodiscard]]
#else
    #include "globals.slang"
    #define CONST_MEMBER_FUNCTION
    #define MUTABLE_MEMBER_FUNCTION [mutating]
    #define NODISCARD
    #define INLINE

    typedef uint3 BrickLocalPosition;

#endif // __cplusplus


INLINE u32 gpu_hashU32(u32 x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bbU;
    x ^= x >> 11;
    x *= 0xac4c1b51U;
    x ^= x >> 15;
    x *= 0x31848babU;
    x ^= x >> 14;

    return x;
}


INLINE u32 rotate_right(u32 x, u32 r)
{
    return (x >> r) | (x << (32u - r));
}

INLINE u32 gpu_hashCombineU32(u32 a, u32 h)
{
    a *= 0xcc9e2d51u;
    a = rotate_right(a, 17u);
    a *= 0x1b873593u;
    h ^= a;
    h = rotate_right(h, 19u);
    return h * 5u + 0xe6546b64u;
}

/// 0 - 511 Offset in chunk
/// 512 - 65535 Voxel
/// To get VoxelMaterial id
struct MaybeBrickOffsetOrMaterialId
{
    NODISCARD u16 getMaterial() CONST_MEMBER_FUNCTION
    {
        return ~data;
    }

    NODISCARD bool isMaterial() CONST_MEMBER_FUNCTION
    {
        return data >= u16(512);
    }

    NODISCARD bool isPointer() CONST_MEMBER_FUNCTION
    {
        return !isMaterial();
    }


    u16 data = 65535;
};

struct IdxBrickResult
{
    u32 idx;
    u32 bit;
};

struct BooleanBrick
{
    u32 data[16];

    MUTABLE_MEMBER_FUNCTION void write(BrickLocalPosition p, bool b)
    {
        const IdxBrickResult res = BooleanBrick::getIdxAndBit(p);

        if (b)
        {
            data[res.idx] |= (1u << res.bit); // NOLINT
        }
        else
        {
            data[res.idx] &= ~(1u << res.bit); // NOLINT
        }
    }

    NODISCARD bool read(BrickLocalPosition p) CONST_MEMBER_FUNCTION
    {
        const IdxBrickResult res = BooleanBrick::getIdxAndBit(p);

        return (data[res.idx] & (1u << res.bit)) != 0; // NOLINT
    }

    NODISCARD static IdxBrickResult getIdxAndBit(BrickLocalPosition p)
    {
        const u32 linearIndex = p.x + (8 * p.y) + (64 * p.z);

        return {linearIndex / 32, linearIndex % 32};
    }
};

struct MaterialBrick
{
    u16 data[8][8][8]; // voxels of type Voxel in C++


    MUTABLE_MEMBER_FUNCTION void write(BrickLocalPosition p, u16 v) 
    {
        data[p.x][p.y][p.z] = v;
    }

    NODISCARD u16 read(BrickLocalPosition p) CONST_MEMBER_FUNCTION
    {
        return data[p.x][p.y][p.z];
    }
};

struct CombinedBrickReadResult
{
    u16 voxel;
    bool solid;
};

struct CombinedBrick
{
    BooleanBrick  boolean_brick;
    MaterialBrick material_brick;

    MUTABLE_MEMBER_FUNCTION void write(BrickLocalPosition p, u16 v)
    {
        boolean_brick.write(p, v != 0);
        material_brick.write(p, v);
    }

    NODISCARD CombinedBrickReadResult read(BrickLocalPosition p) CONST_MEMBER_FUNCTION
    {
        CombinedBrickReadResult res;

        res.voxel = material_brick.read(p);
        res.solid = boolean_brick.read(p);
        
        return res;
    }
};

struct GpuRaytracedLight
{
    float4 position_and_half_intensity_distance;
    float4 color_and_power;
};

struct GpuColorHashMapNode
{
    u32 key;
    u32 r_1024;
    u32 g_1024;
    u32 b_1024;
    u32 samples;
};

struct ChunkBrickStorage
{
    MaybeBrickOffsetOrMaterialId data[8][8][8];
};