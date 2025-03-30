#ifndef SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL
#define SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL

#include "globals.glsl"
#include "misc.glsl"

struct GpuColorHashMapNode
{
    u32 key;
    u32 r_1024;
    u32 g_1024;
    u32 b_1024;
    u32 samples;
};

const u32 kHashTableCapacity = 1u << 20u;
const u32 kEmpty             = ~0u;

layout(set = 0, binding = 4) buffer VoxelHashMap
{
    GpuColorHashMapNode nodes[kHashTableCapacity];
}
in_voxel_hash_map[];

const vec3 available_normals[6] = {
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, -1.0, 0.0),
    vec3(-1.0, 0.0, 0.0),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 0.0, -1.0),
    vec3(0.0, 0.0, 1.0),
};

vec3 unpackNormalId(u32 id)
{
    return available_normals[id];
}

u32 packNormalId(vec3 normal)
{
    for (int i = 0; i < 6; ++i)
    {
        if (normal == available_normals[i])
        {
            return i;
        }
    }

    return 0;
}

u32 packUniqueFaceId(vec3 normal, uvec3 positionInChunk, uint chunkId)
{
    u32 res = 0;

    res = bitfieldInsert(res, positionInChunk.x, 0, 6);
    res = bitfieldInsert(res, positionInChunk.y, 6, 6);
    res = bitfieldInsert(res, positionInChunk.z, 12, 6);

    res = bitfieldInsert(res, packNormalId(normal), 18, 3);

    res = bitfieldInsert(res, chunkId, 21, 11);

    return res;
}

struct UnpackUniqueFaceIdResult
{
    vec3  normal;
    uvec3 position_in_chunk;
    uint  chunk_id;
};

UnpackUniqueFaceIdResult unpackUniqueFaceId(u32 packed)
{
    UnpackUniqueFaceIdResult res;

    res.position_in_chunk.x = bitfieldExtract(packed, 0, 6);
    res.position_in_chunk.y = bitfieldExtract(packed, 6, 6);
    res.position_in_chunk.z = bitfieldExtract(packed, 12, 6);

    res.normal = unpackNormalId(bitfieldExtract(packed, 18, 3));

    res.chunk_id = bitfieldExtract(packed, 21, 11);

    return res;
}

#endif // SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL