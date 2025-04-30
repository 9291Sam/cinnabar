#pragma once 

#include "globals.slang" 
#include "cube_ray.slang"
#include "voxel_faces.slang"
#include "voxel_tracing.slang"
#include "shared_data_structures.slang"

u32 subgroupMatch(u32 value)
{
    u32 result = 0;
    const u32 activeWaves = WaveActiveBallot(true).x;

    for (uint w = 0; w < 32; ++w)
    {
        const bool isWaveActive = bitfieldExtract(activeWaves, w, 1) == 1;

        if (isWaveActive && value == WaveReadLaneAt(value, w))
        {
            result = bitfieldInsert(result, 1, w, 1);
        }
    }
    
    return result;
}


[format("rgba32f")]
[[vk::binding(2)]] RWTexture2D<float4> visible_voxel_image[];

struct PushConstant
{
    u32 bool_is_quarter_res;
};

[shader("compute")]
[numthreads(32, 32, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID, uint workgroup : SV_GroupIndex, [vk_push_constant] uniform PushConstant pushConstant)
{
    const u32 rdSeed = gpu_hashU32(reinterpret<u32>(global_gpu_data[UBO_GLOBAL_DATA].time_alive));
    int2 thisPixel           = int2(threadId.xy);

    if (pushConstant.bool_is_quarter_res != 0)
    {
        thisPixel *= 2;
        thisPixel.x += (((rdSeed & 1u) == 0u) ? 1u : 0u);
        thisPixel.y += (((rdSeed & 2u) == 0u) ? 1u : 0u);
    }

    if (all(thisPixel < global_gpu_data[UBO_GLOBAL_DATA].framebuffer_size))
    {
        const float4 rawPixelData        = visible_voxel_image[SIO_VOXEL_RENDER_TARGET].Load(thisPixel);
        const float3 worldStrikePosition = rawPixelData.xyz;
        const u32  uniqueFaceId        = reinterpret<uint>(rawPixelData.w);
        const UnpackUniqueFaceIdResult unpacked = UnpackUniqueFaceIdResult::unpack(uniqueFaceId);

        const uint3 positionInChunk = uint3(-0.01 * unpacked.normal + worldStrikePosition - float3(in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].getWorldChunkCorner()));

        if (uniqueFaceId != ~0u )
        {   
            const uint3 bC = positionInChunk / 8;
            const uint3 bP = positionInChunk % 8;

            const PBRVoxelMaterial material = getMaterialFromPosition( 
                in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].brick_map[bC.x][bC.y][bC.z], unpacked.chunk_id, bP);
 
            const u32 hash = hashFaceId(unpacked, positionInChunk);
            const u32 startSlot = hash % faceHashTableCapacity;

            for (uint i = 0; i < 32; ++i)
            {
                const u32 thisSlot = (startSlot + i) % faceHashTableCapacity;
                
                u32 prev;
                InterlockedCompareExchange(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].key, faceHashTableNullHash, hash, prev);

                if (prev == faceHashTableNullHash || prev == hash)
                {
    
                    float3 calculatedColor = calculateWorldColor(worldStrikePosition, material, unpacked,
                        reinterpret<u32>(global_gpu_data[UBO_GLOBAL_DATA].time_alive) + u32(workgroup) + rdSeed,
                        bool(global_gpu_data[UBO_GLOBAL_DATA].bool_enable_reflections));

                    calculatedColor = min(calculatedColor, float3(1.0));

                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].r_1024, uint(calculatedColor.x * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].g_1024, uint(calculatedColor.y * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].b_1024, uint(calculatedColor.z * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].samples_high_bit_is_bool_is_reflective, 1);

                    if (prev == faceHashTableNullHash)
                    {
                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].r_1024, 1);
                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].g_1024, 1);
                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].b_1024, 1);

                        const bool isReflectiveVoxel = material.albedo_roughness.w != 1.0;

                        const u32 storeValue = 1 + (isReflectiveVoxel ? (1u << 31u) : 0u);

                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].samples_high_bit_is_bool_is_reflective, storeValue);
                    }

                    break;
                }
            }
        
        }
    }

}
