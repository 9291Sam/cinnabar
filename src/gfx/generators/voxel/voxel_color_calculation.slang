#pragma once 

#include "globals.slang" 
#include "cube_ray.slang"
#include "voxel_faces.slang"
#include "voxel_tracing.slang"
#include "shared_data_structures.slang"


[format("rgba32f")]
[[vk::binding(2)]] RWTexture2D<float4> visible_voxel_image[];

[shader("compute")]
[numthreads(32, 32, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    const int2 thisPixel           = int2(threadId.xy);

    if (all(thisPixel < global_gpu_data[UBO_GLOBAL_DATA].framebuffer_size)
        && sizeof(ChunkData) == 1056)
    {
        const float4 rawPixelData        = visible_voxel_image[SIO_VOXEL_RENDER_TARGET].Load(thisPixel);
        const float3 worldStrikePosition = rawPixelData.xyz;
        const u32  uniqueFaceId        = reinterpret<uint>(rawPixelData.w);
        const UnpackUniqueFaceIdResult unpacked = UnpackUniqueFaceIdResult::unpack(uniqueFaceId);

        const float3 chunkStrikePosition = worldStrikePosition - in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].getWorldChunkCorner();

        if (uniqueFaceId != ~0u)
        {
          
            const uint3 bC = unpacked.position_in_chunk / 8;
            const uint3 bP = unpacked.position_in_chunk % 8;

            const PBRVoxelMaterial material = getMaterialFromPosition( 
                in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].brick_map[bC.x][bC.y][bC.z], unpacked.chunk_id, bP);
 
            const u32 startSlot = gpu_hashU32(uniqueFaceId) % faceHashTableCapacity;

            for (uint i = 0; i < 32; ++i)
            {
                const u32 thisSlot = (startSlot + i) % faceHashTableCapacity;
                
                u32 prev;
                InterlockedCompareExchange(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].key, faceHashTableNullHash, uniqueFaceId, prev);

                if (prev == faceHashTableNullHash || prev == uniqueFaceId)
                {
                    const float3 calculatedColor = calculateColor(unpacked.chunk_id, chunkStrikePosition, material, unpacked);

                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].r_1024, uint(calculatedColor.x * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].g_1024, uint(calculatedColor.y * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].b_1024, uint(calculatedColor.z * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].samples, 1);

                    if (prev == faceHashTableNullHash)
                    {
                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].r_1024, 1);
                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].g_1024, 1);
                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].b_1024, 1);
                        InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].samples, 1);
                    }

                    break;
                }
            }
        
        }
    }

}
