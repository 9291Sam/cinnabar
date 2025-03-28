#ifndef SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL
#define SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL

#include "globals.glsl"
#include "misc.glsl"

#ifndef VOXEL_HASH_MAP_OFFSET
#error "VOXEL_HASH_MAP_OFFSET must be defined"
#endif // VOXEL_HASH_MAP_OFFSET

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

#endif // SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL