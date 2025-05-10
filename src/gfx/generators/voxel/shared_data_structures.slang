#pragma once

#ifdef __cplusplus
    #include "util/util.hpp"
    #include <glm/vec3.hpp>
    #include <glm/vec4.hpp>
    // #include <glm/gtx/string_cast.hpp.hpp>
    // #include "util/allocators/range_allocator.hpp"
    
    #define float3 glm::vec3
    #define float4 glm::vec4
    #define int3 glm::ivec3
    #define SHARED_log2 std::log2
    #define euclidean_distance glm::distance
    #define XYZ_VEC .xyz()

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
    #define euclidean_distance distance
    #define XYZ_VEC .xyz
    #define SHARED_log2 log2

    typedef int3 AlignedChunkCoordinate;
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

[[vk::binding(4)]] RWStructuredBuffer<GpuChunkData> in_global_chunk_data[];
[[vk::binding(4)]] StructuredBuffer<GpuChunkData> in_const_global_chunk_data[];

[[vk::binding(4)]] RWStructuredBuffer<PBRVoxelMaterial> in_voxel_materials[];
[[vk::binding(4)]] RWStructuredBuffer<GpuRaytracedLight> in_raytraced_lights[];

[[vk::binding(4)]] RWStructuredBuffer<ChunkHashMapNode> in_chunk_hash_map[];

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
        return BrickPointer(in_global_chunk_data[SBO_CHUNK_DATA][chunkId].offset + uint(_data));
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
    u32 data[16] = {};

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
        const u32 test = data[0];
    
        for (int i = 1; i < 16; ++i)
        {
            if (data[i] != test)
            {
                return BooleanBrickCompactResult(false, false);
            }
        }
    
        // The compact value is whatever bit 0 of `test` represents (solid or empty)
        const bool value = (test & 1u) != 0;
        return BooleanBrickCompactResult(true, value);
    }    

};

struct MaterialBrickCompactResult
{
    bool is_compact;
    u16 voxel;
};

struct MaterialBrick
{
    u16 data[8][8][8] = {}; // voxels of type Voxel in C++


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
    BooleanBrick  boolean_brick = {};
    MaterialBrick material_brick = {};

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

    bool canInfluencePoint(float3 position, uint numberOfLights = 1)
    {
        const float minimumPossibleInfluence = (1.0f / (256.0f * numberOfLights)); 

        float r = position_and_half_intensity_distance.w;
        float p = color_and_power.w;

        float maxDistance = r * sqrt(p / minimumPossibleInfluence - 1.0f);
        float actualDistance = euclidean_distance(position_and_half_intensity_distance XYZ_VEC, position);

        return maxDistance > actualDistance;
    }

};

struct GpuColorHashMapNode
{
    u32 key;
    u32 r_1024;
    u32 g_1024;
    u32 b_1024;
    u32 samples_high_bit_is_bool_is_reflective;

    u32 getSamples()
    {
        return samples_high_bit_is_bool_is_reflective & ~(1u << 31u);
    }

    bool isReflective()
    {
        return bool(samples_high_bit_is_bool_is_reflective & (1u << 31u));
    }


    MUTABLE_MEMBER_FUNCTION
    void setSamplesNonAtomic(u32 samples)
    {
        samples_high_bit_is_bool_is_reflective = u32(isReflective()) << 31u | (samples & ~(1u << 31u));
    }

};

const static u32 faceHashTableCapacity = 1u << 22u;
const static u32 faceHashTableNullHash = ~0u;

struct ChunkLocalEmissiveOffset
{
    const static i32 x_min = -1024;
    const static i32 x_max = 1023;

    const static i32 y_min = -512;
    const static i32 y_max = 511;

    const static i32 z_min = -1024;
    const static i32 z_max = 1023;

    i32 x : 11;
    i32 y : 10;
    i32 z : 11;

    float3 getCenterPositionOfVoxel()
    {
        return float3(0.5) + float3(x, y, z);
    }
};

#ifdef __cplusplus
template<class T, std::size_t N>
using Array = std::array<T, N>;
#endif

typedef Array<Array<Array<MaybeBrickOffsetOrMaterialId, 8>, 8>, 8> BrickMap;


struct GpuChunkData
{
    AlignedChunkCoordinate aligned_chunk_coordinate;
    u32 offset = ~0u;
    BrickMap brick_map = {};
    float3 padding;
    u32 number_of_emissives = 0;
    ChunkLocalEmissiveOffset emissive_data[1024];

    float3 getWorldChunkCorner()
    {
        return float3(aligned_chunk_coordinate * 64);
    }
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

INLINE u32 hashAlignedChunkCoordinate(int3 c)
{
    return gpu_hashCombineU32(
        gpu_hashU32(u32(c.x)),
        gpu_hashCombineU32(
            gpu_hashU32(u32(c.y)),
            gpu_hashU32(u32(c.z))
        )
    );
}

struct ChunkHashMapNode
{
    u32 key = ~0u;
    u32 id = ~0u;
};

const static u32 chunkHashTableCapacity = 1u << 16u;
const static u32 chunkHashTableNullHash = ~0u;

#ifndef __cplusplus
/// returns the chunk's id or ~0u
INLINE u32 tryReadChunkHashTable(int3 c)
{
    const u32 hash = hashAlignedChunkCoordinate(c);
    const u32 startSlot = hash % chunkHashTableCapacity;

    for (u32 i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % chunkHashTableCapacity;
    
        const ChunkHashMapNode thisNode = in_chunk_hash_map[SBO_CHUNK_HASH_MAP][thisSlot];

        if (thisNode.key == hash)
        {
            return thisNode.id;
        }
    }

    return ~0u;
}
#endif // __cplusplus

#ifdef __cplusplus

INLINE void insertUniqueChunkHashTable(gfx::core::vulkan::CpuCachedBuffer<ChunkHashMapNode>& table, int3 c, u32 chunkId)
{
    const u32 hash = hashAlignedChunkCoordinate(c);
    const u32 startSlot = hash % chunkHashTableCapacity;

    for (u32 i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % chunkHashTableCapacity;

        const ChunkHashMapNode thisNode = table.read(thisSlot);

        if (thisNode.key == chunkHashTableNullHash)
        {
            table.write(thisSlot, {
                .key {hash},
                .id {chunkId}
            });
            return;
        }
        else if (thisNode.key == hash)
        {
            log::warn("Unexpected duplicate insertion of chunkID {}", chunkId);
            return;
        }

    }

    log::warn("Unable to insert chunkId {}, is the table large enough? Current size is {}", chunkId, chunkHashTableCapacity);
}

// TODO: this is tombstoning, maybe not...

INLINE void removeUniqueChunkHashTable(gfx::core::vulkan::CpuCachedBuffer<ChunkHashMapNode>& table, int3 c, u32 chunkId)
{
    const u32 hash = hashAlignedChunkCoordinate(c);
    const u32 startSlot = hash % chunkHashTableCapacity;

    for (u32 i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % chunkHashTableCapacity;

        const ChunkHashMapNode thisNode = table.read(thisSlot);

        if (thisNode.key == hash)
        {
            table.write(thisSlot, {
                .key {hash},
                .id {~0u}
            });
            return;
        }
        else if (thisNode.key == chunkHashTableNullHash)
        {
            log::warn("Unexpected lack of chunkId {}", chunkId);
            return;
        }

    }

    log::warn("Unable to remove chunkId {}, is the table large enough? Current size is {}", chunkId, chunkHashTableCapacity);
}


#endif // __cplusplus
