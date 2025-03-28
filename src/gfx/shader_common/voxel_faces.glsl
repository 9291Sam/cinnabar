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
    u32 value;
};

const u32 kHashTableCapacity = 1u << 20u;
const u32 kEmpty             = ~0u;

layout(set = 0, binding = 4) buffer VoxelHashMap
{
    GpuColorHashMapNode nodes[kHashTableCapacity];
}
in_voxel_hash_map[];

u32 getHashOfFace(const vec3 normal, ivec3 position_in_chunk, uint chunkId)
{
    u32 workingHash = 2;

    workingHash = gpu_hashCombineU32(workingHash, u32(normal.x));
    workingHash = gpu_hashCombineU32(workingHash, u32(normal.y));
    workingHash = gpu_hashCombineU32(workingHash, u32(normal.z));

    workingHash = gpu_hashCombineU32(workingHash, u32(position_in_chunk.x));
    workingHash = gpu_hashCombineU32(workingHash, u32(position_in_chunk.y));
    workingHash = gpu_hashCombineU32(workingHash, u32(position_in_chunk.z));

    workingHash = gpu_hashCombineU32(workingHash, chunkId);

    return workingHash;
}

u32 integerHash(u32 h)
{
    h ^= h >> 17;
    h *= 0xed5ad4bbU;
    h ^= h >> 11;
    h *= 0xac4c1b51U;
    h ^= h >> 15;
    h *= 0x31848babU;
    h ^= h >> 14;

    return h;
}

void face_id_map_write(u32 key, u32 value)
{
    u32 slot = integerHash(key) & (kHashTableCapacity - 1);

    for (int i = 0; i < 16; ++i)
    {
        u32 prev = atomicCompSwap(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].key, kEmpty, key);

        if (prev == kEmpty || prev == key)
        {
            in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].value = value;
            break;
        }

        slot = (slot + 1) & (kHashTableCapacity - 1);
    }
}

u32 face_id_map_read(u32 key)
{
    u32 slot = integerHash(key) & (kHashTableCapacity - 1);

    for (int i = 0; i < 64; ++i)
    {
        if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].key == key)
        {
            return in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].value;
        }
        if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].key == kEmpty)
        {
            return 0;
        }
        slot = (slot + 1) & (kHashTableCapacity - 1);
    }

    return kEmpty;
}

// void face_id_map_write(u32 key, uvec4 value)
// {
//     u32 slot = gpu_hashU32(key) & (kHashTableCapacity - 1);

//     for (int i = 0; i < 64; ++i)
//     {
//         u32 prev = atomicCompSwap(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].key, kEmpty, key);

//         if (prev == kEmpty || prev == key)
//         {
//             in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].value = value;
//             break;
//         }

//         slot = (slot + 1) & (kHashTableCapacity - 1);
//     }
// }

// uvec4 face_id_map_read(u32 key)
// {
//     u32 slot = gpu_hashU32(key) & (kHashTableCapacity - 1);

//     for (int i = 0; i < 64; ++i)
//     {
//         if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].key == key)
//         {
//             return in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].value;
//         }
//         if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].key == kEmpty)
//         {
//             return uvec4(~0u);
//         }
//         slot = (slot + 1) & (kHashTableCapacity - 1);
//     }

//     return uvec4(~0u);
// }

// void doInsert(u32 hash, vec3 c)
// {
//     uvec3 integerColor = uvec3(c * 64);

//     u32 slot = hash % MaxFaceHashMapNodes;

//     for (int i = 0; i < MaxIters; ++i)
//     {
//         u32 prev = atomicCompSwap(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].hash, NullHash, hash);

//         if (prev == NullHash || prev == hash)
//         {
//             u32 samplesToAdd = 1;
//             // everything starts out at ~0 so we need to overflow it
//             if (prev == NullHash)
//             {
//                 integerColor += 1;
//                 samplesToAdd += 1;
//             }

//             atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].r, integerColor.x);
//             atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].g, integerColor.y);
//             atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].b, integerColor.z);
//             atomicAdd(in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].number_of_samples, samplesToAdd);
//             break;
//         }

//         slot = (slot + 1) % MaxFaceHashMapNodes;
//     }
// }

// struct DoLoadResult
// {
//     bool found;
//     vec3 maybe_loaded_color;
// };

// DoLoadResult doLoad(u32 hash)
// {
//     u32 slot = hash % MaxFaceHashMapNodes;

//     for (int i = 0; i < MaxIters; ++i)
//     {
//         if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].hash == hash)
//         {
//             const GpuColorHashMapNode wholeNode = in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot];

//             return DoLoadResult(true, vec3(wholeNode.r, wholeNode.g, wholeNode.b) /
//             float(wholeNode.number_of_samples));
//         }
//         if (in_voxel_hash_map[VOXEL_HASH_MAP_OFFSET].nodes[slot].hash == NullHash)
//         {
//             return DoLoadResult(false, vec3(0));
//         }
//         slot = (slot + 1) % MaxFaceHashMapNodes;
//     }

//     return DoLoadResult(false, vec3(0));
// }

#endif // SRC_GFX_SHADER_COMMON_VOXEL_FACES_GLSL