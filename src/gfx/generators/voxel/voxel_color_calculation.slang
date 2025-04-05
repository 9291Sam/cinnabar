#pragma once 

#include "globals.slang" 
#include "cube_ray.slang"
#include "voxel_faces.slang"
#include "voxel_tracing.slang"
#include "shared_data_structures.slang"


[format("rgba32f")]
[[vk::binding(2)]] RWTexture2D<float4> visible_voxel_image[];

VoxelTraceResult traceSingleRayInChunk(Ray ray, float dist)
{
    return traceDDARay(0, ray.origin, ray.origin + ray.direction * dist);
}

float3 calculateColor(
    float3 chunkStrikePosition, PBRVoxelMaterial firstMaterial, UnpackUniqueFaceIdResult unpacked, uint rngState)
{
    const GpuRaytracedLight light = in_raytraced_lights[SBO_VOXEL_LIGHTS][0];

    const float3 worldStrikePosition = chunkStrikePosition + in_global_chunk_bricks[SBO_CHUNK_BRICKS][0].world_chunk_corner;

    float3 lightColor = calculatePixelColor(
        worldStrikePosition,
        unpacked.normal,
        normalize(global_gpu_data[UBO_GLOBAL_DATA].camera_position.xyz - worldStrikePosition),
        normalize(light.position_and_half_intensity_distance.xyz - worldStrikePosition),
        light,
        firstMaterial,
        true,
        true);

    const VoxelTraceResult shadowResult = traceDDARay(
        0, chunkStrikePosition + 0.05 * unpacked.normal, light.position_and_half_intensity_distance.xyz - in_global_chunk_bricks[SBO_CHUNK_BRICKS][0].world_chunk_corner);

    float3 calculatedColor = float3(0.0);

    if (!shadowResult.intersect_occur)
    {
        calculatedColor += lightColor;
        // calculatedColor *=
        // calculatedColor = 1 - calculatedColor;
    }

    const float3 toFragmentDir = normalize(worldStrikePosition - global_gpu_data[UBO_GLOBAL_DATA].camera_position.xyz);

    Ray giRay =
        Ray(chunkStrikePosition + 0.05 * unpacked.normal, normalize(reflect(toFragmentDir, unpacked.normal)));

    const Cube chunk = Cube(float3(32.0f), 64.0f);
    
    if (chunk.contains(giRay.origin))
    {
        const VoxelTraceResult giResult = traceSingleRayInChunk(giRay, 96.0f);

        float3 giColor = float3(0);

        if (giResult.intersect_occur)
        {
            giColor = calculatePixelColor(
                giResult.chunk_local_fragment_position + float3(-16.0),
                giResult.voxel_normal,
                normalize(giRay.origin - giResult.chunk_local_fragment_position),
                normalize(
                    light.position_and_half_intensity_distance.xyz - giResult.chunk_local_fragment_position + float3(-16.0)),
                light,
                giResult.material,
                false,
                true);
        }

        calculatedColor += giColor;
    }

    return calculatedColor;
}

[shader("compute")]
[numthreads(32, 32, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    const int2 thisPixel           = int2(threadId.xy);

    if (all(thisPixel < global_gpu_data[UBO_GLOBAL_DATA].framebuffer_size)
        && sizeof(ChunkData) == 1040)
    {
        const float4 rawPixelData        = visible_voxel_image[SIO_VOXEL_RENDER_TARGET].Load(thisPixel);
        const float3 worldStrikePosition = rawPixelData.xyz;
        const u32  uniqueFaceId        = reinterpret<uint>(rawPixelData.w);
        const UnpackUniqueFaceIdResult unpacked = UnpackUniqueFaceIdResult::unpack(uniqueFaceId);

        const float3 chunkStrikePosition = worldStrikePosition - in_global_chunk_bricks[SBO_CHUNK_BRICKS][unpacked.chunk_id].world_chunk_corner;

        if (uniqueFaceId != ~0u)
        {
          
            const uint3 bC = unpacked.position_in_chunk / 8;
            const uint3 bP = unpacked.position_in_chunk % 8;

            const uint offset = 0;

            const PBRVoxelMaterial material = getMaterialFromPosition( // TODO: bad!
                in_global_chunk_bricks[SBO_CHUNK_BRICKS][unpacked.chunk_id].brick_map[bC.x][bC.y][bC.z].data + offset, bP);
 
            const u32 startSlot = gpu_hashU32(uniqueFaceId) % kHashTableCapacity;

            for (uint i = 0; i < 32; ++i)
            {
                const u32 thisSlot = (startSlot + i) % kHashTableCapacity;
                
                u32 prev;
                InterlockedCompareExchange(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].key, kEmpty, uniqueFaceId, prev);

                if (prev == kEmpty || prev == uniqueFaceId)
                {
                    u32 rngState = gpu_hashCombineU32(
                        gpu_hashU32(uniqueFaceId),
                        reinterpret<u32>(global_gpu_data[UBO_GLOBAL_DATA].time_alive) + threadId.x + 872347
                            + threadId.y * 4444);

                    const float3 calculatedColor = calculateColor(chunkStrikePosition, material, unpacked, rngState);

                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].r_1024, uint(calculatedColor.x * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].g_1024, uint(calculatedColor.y * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].b_1024, uint(calculatedColor.z * 1024));
                    InterlockedAdd(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].samples, 1);

                    if (prev == kEmpty)
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
