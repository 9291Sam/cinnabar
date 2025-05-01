#pragma once

#include "globals.slang"
#include "shared_data_structures.slang"


[[vk::binding(4)]] RWStructuredBuffer<GpuColorHashMapNode> in_voxel_hash_map[];

[format("rgba32f")]
[[vk::binding(2)]] RWTexture2D<float4> voxel_render_target[];

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

u32 hashFaceId(UnpackUniqueFaceIdResult unpacked, uint3 chunk_position)
{
    u32 res = 0;

    res = bitfieldInsert(res, chunk_position.x, 0, 6);
    res = bitfieldInsert(res, chunk_position.y, 6, 6);
    res = bitfieldInsert(res, chunk_position.z, 12, 6);

    res = bitfieldInsert(res, packNormalId(unpacked.normal), 18, 3);

    return gpu_hashCombineU32(gpu_hashU32(res) ,gpu_hashU32(unpacked.chunk_id));
}

struct UnpackUniqueFaceIdResult
{
    static UnpackUniqueFaceIdResult unpack(u32 packed)
    {
        UnpackUniqueFaceIdResult res;
        res.normal = unpackNormalId(bitfieldExtract(packed, 0, 3));

        res.chunk_id = bitfieldExtract(packed, 3, 29);

        return res;
    }

    __init(float3 in_normal, uint in_chunk_id)
    {
        this.normal = in_normal;
        this.chunk_id = in_chunk_id;
    }

    float3  normal;
    uint  chunk_id;

    u32 pack()
    {
        u32 res = 0;

        res = bitfieldInsert(res, packNormalId(this.normal), 0, 3);

        res = bitfieldInsert(res, this.chunk_id, 3, 29);

        return res;
    }
};
