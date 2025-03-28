#ifndef SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL
#define SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL

#include "globals.glsl"
#include "misc.glsl"

#ifndef VOXEL_HASH_MAP_OFFSET
#error "VOXEL_HASH_MAP_OFFSET must be defined"
#endif // VOXEL_HASH_MAP_OFFSET

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

layout(set = 0, binding = 4) buffer VoxelHashMap
{
    GpuColorHashMapNode nodes[MaxFaceHashMapNodes];
}
in_voxel_hash_map[];

u32 getHashOfFace(const vec3 normal, ivec3 position_in_chunk, uint chunkId)
{
    u32 workingHash = 82234234;

    workingHash = gpu_hashCombineU32(workingHash, u32(normal.x));
    workingHash = gpu_hashCombineU32(workingHash, u32(normal.y));
    workingHash = gpu_hashCombineU32(workingHash, u32(normal.z));

    workingHash = gpu_hashCombineU32(workingHash, u32(position_in_chunk.x));
    workingHash = gpu_hashCombineU32(workingHash, u32(position_in_chunk.y));
    workingHash = gpu_hashCombineU32(workingHash, u32(position_in_chunk.z));

    workingHash = gpu_hashCombineU32(workingHash, chunkId);

    return workingHash;
}

void doInsert(u32 hash, vec3 c)
{
    uvec3 integerColor = uvec3(c * 1024);

    u32 slot = hash % MaxFaceHashMapNodes;

    for (int i = 0; i < MaxFaceHashMapNodes; ++i)
    {
        u32 prev = atomicCompSwap(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].hash, NullHash, hash);

        if (prev == NullHash || prev == hash)
        {
            u32 samplesToAdd = 1;
            // everything starts out at ~0 so we need to overflow it
            if (prev == NullHash)
            {
                integerColor += 1;
                samplesToAdd += 1;
            }

            atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].r, integerColor.x);
            atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].g, integerColor.y);
            atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].b, integerColor.z);
            atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].number_of_samples, samplesToAdd);
            break;
        }

        slot = (slot + 1) % MaxFaceHashMapNodes;
    }
}

struct DoLoadResult
{
    bool found;
    vec3 maybe_loaded_color;
};

DoLoadResult doLoad(u32 hash)
{
    u32 slot = hash % MaxFaceHashMapNodes;

    for (int i = 0; i < MaxFaceHashMapNodes; ++i)
    {
        if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].hash == hash)
        {
            const GpuColorHashMapNode wholeNode = in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot];

            return DoLoadResult(true, vec3(wholeNode.r, wholeNode.g, wholeNode.b) / float(wholeNode.number_of_samples));
        }
        if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].hash == NullHash)
        {
            return DoLoadResult(false, vec3(0));
        }
        slot = (slot + 1) % MaxFaceHashMapNodes;
    }

    return DoLoadResult(false, vec3(0));
}

#endif // SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL