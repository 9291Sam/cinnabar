#ifndef SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL
#define SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL

struct GpuColorHashMapNode
{
    u32 hash;
    u32 r;
    u32 g;
    u32 b;
    u32 number_of_samples;
};

const u32 MaxFaceHashMapNodes = 1u << 20u;
const u32 NullHash            = ~0u;

u32 getHashOfFace(const vec3 normal, ivec3 position_in_chunk, uint chunkId) {}

#endif // SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL