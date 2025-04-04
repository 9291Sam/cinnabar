#pragma once

#include "globals.slang"

const static u32 kHashTableCapacity = 1u << 20u;
const static u32 kEmpty             = ~0u;

struct GpuColorHashMapNode
{
    u32 key;
    u32 r_1024;
    u32 g_1024;
    u32 b_1024;
    u32 samples;
};

[[vk::binding(4, 0)]] RWStructuredBuffer<GpuColorHashMapNode> in_voxel_hash_map[];

[format("rgba32f")]
[[vk::binding(2, 0)]] RWTexture2D<float4> voxel_render_target[];

const static Array<float3, 6> available_normals = {
    float3(0.0, 1.0, 0.0),
    float3(0.0, -1.0, 0.0),
    float3(-1.0, 0.0, 0.0),
    float3(1.0, 0.0, 0.0),
    float3(0.0, 0.0, -1.0),
    float3(0.0, 0.0, 1.0),
};


float3 unpackNormalId(u32 id)
{
    return available_normals[id];
}

u32 packNormalId(float3 normal)
{
    for (int i = 0; i < 6; ++i)
    {
        if (all(normal == available_normals[i]))
        {
            return i;
        }
    }

    return 0;
}

struct UnpackUniqueFaceIdResult
{
    __init(u32 packed)
    {
        UnpackUniqueFaceIdResult res;

        res.position_in_chunk.x = bitfieldExtract(packed, 0, 6);
        res.position_in_chunk.y = bitfieldExtract(packed, 6, 6);
        res.position_in_chunk.z = bitfieldExtract(packed, 12, 6);

        res.normal = unpackNormalId(bitfieldExtract(packed, 18, 3));

        res.chunk_id = bitfieldExtract(packed, 21, 11);

        return res;
    }

    __init(float3 in_normal, uint3 in_chunk_position, uint in_chunk_id)
    {
        this.normal = in_normal;
        this.position_in_chunk = in_chunk_position;
        this.chunk_id = in_chunk_id;
    }

    float3  normal;
    uint3 position_in_chunk;
    uint  chunk_id;

    u32 pack()
    {
        u32 res = 0;

        res = bitfieldInsert(res, this.position_in_chunk.x, 0, 6);
        res = bitfieldInsert(res, this.position_in_chunk.y, 6, 6);
        res = bitfieldInsert(res, this.position_in_chunk.z, 12, 6);

        res = bitfieldInsert(res, packNormalId(this.normal), 18, 3);

        res = bitfieldInsert(res, this.chunk_id, 21, 11);

        return res;
    }
};
