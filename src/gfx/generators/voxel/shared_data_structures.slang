#pragma once

#ifdef __cplusplus
    #include "util/util.hpp"
    #include <glm/vec3.hpp>
    #include <glm/vec4.hpp>
    // #include "util/allocators/range_allocator.hpp"
    
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

    namespace util
    {
        struct RangeAllocation
        {
            u32 offset   = ~0u;
            u32 metadata = ~0u;
        };
    }


[[vk::binding(4)]] RWStructuredBuffer<CombinedBrick> in_combined_bricks[];

[[vk::binding(4)]] RWStructuredBuffer<ChunkData> in_global_chunk_bricks[];
[[vk::binding(4)]] StructuredBuffer<ChunkData> in_const_global_chunk_bricks[];

[[vk::binding(4)]] RWStructuredBuffer<PBRVoxelMaterial> in_voxel_materials[];
[[vk::binding(4)]] RWStructuredBuffer<GpuRaytracedLight> in_raytraced_lights[];

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

struct BrickPointer
{
    u32 brick_pointer;
};

/// 0 - 511 Offset in chunk
/// 512 - 65535 Voxel
/// To get VoxelMaterial id
struct MaybeBrickOffsetOrMaterialId
{
    NODISCARD static MaybeBrickOffsetOrMaterialId fromMaterial(u16 material)
    {
        return MaybeBrickOffsetOrMaterialId(~material);
    }


    NODISCARD static MaybeBrickOffsetOrMaterialId fromOffset(u16 offset)
    {
        return MaybeBrickOffsetOrMaterialId(offset);
    }
    
    NODISCARD u16 getMaterial() CONST_MEMBER_FUNCTION
    {
        return ~_data;
    }

    NODISCARD bool isMaterial() CONST_MEMBER_FUNCTION
    {
        return _data >= u16(512);
    }

    NODISCARD bool isPointer() CONST_MEMBER_FUNCTION
    {
        return !isMaterial();
    }

#ifndef __cplusplus
    NODISCARD BrickPointer getPointer(u32 chunkId) CONST_MEMBER_FUNCTION
    {
        return BrickPointer(in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunkId].range_allocation.offset + uint(_data));
    }
#endif


    u16 _data = 65535;
};

struct IdxBrickResult
{
    u32 idx;
    u32 bit;
};

struct BooleanBrickCompactResult
{
    bool is_compact;
    bool solid_or_empty;
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

    NODISCARD BooleanBrickCompactResult isCompact() CONST_MEMBER_FUNCTION
    {
        const bool compare = ((data[0] & 1u) == 1u);

        const u32 test = compare ? ~0u : 0u;

        for (int i = 0; i < 16; ++i)
        {
            if (test != data[i])
            {
                return BooleanBrickCompactResult(false, false);
            }
        }

        return BooleanBrickCompactResult(true, compare);
    }

};

struct MaterialBrickCompactResult
{
    bool is_compact;
    u16 voxel;
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

    NODISCARD MaterialBrickCompactResult isCompact() CONST_MEMBER_FUNCTION
    {
        const u16 compare = data[0][0][0];

        for (i32 x = 0; x < 8; ++x)
        {
            for (i32 y = 0; y < 8; ++y)
            {
                for (i32 z = 0; z < 8; ++z)
                {
                    if (compare != data[x][y][z])
                    {
                        return MaterialBrickCompactResult(false, 0);
                    }
                }
            }
        }
        
        return MaterialBrickCompactResult(true, compare);
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

   NODISCARD CombinedBrickReadResult isCompact() CONST_MEMBER_FUNCTION
    {
        const BooleanBrickCompactResult boolCompact = boolean_brick.isCompact();

        if (!boolCompact.is_compact)
        {
            return CombinedBrickReadResult(0, false);
        }

        if (!boolCompact.solid_or_empty)
        {
            return CombinedBrickReadResult(0, false);
        }

        const MaterialBrickCompactResult materialCompact = material_brick.isCompact();

        if (materialCompact.is_compact)
        {
            return CombinedBrickReadResult(materialCompact.voxel, true);
        }

        return CombinedBrickReadResult(0, false);
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

struct ChunkData
{
    float3 world_chunk_corner = {0.0f, 0.0f, 0.0f};
    u32 align_0;
    util::RangeAllocation range_allocation;
    u32 align_1;
    u32 align_2;
    MaybeBrickOffsetOrMaterialId brick_map[8][8][8] = {};
};


struct PBRVoxelMaterial
{
    // xyz - linear rgb color
    // w - roughness
    float4 albedo_roughness;
    // xyz - emissive color (values over 1 indicate more than 1 unit of brightness)
    // w - metallic
    float4 emission_metallic;
};
