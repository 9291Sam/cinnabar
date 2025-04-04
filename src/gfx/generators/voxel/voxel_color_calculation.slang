#pragma once 

#include "globals.slang" 
#include "cube_ray.slang"
#include "voxel_faces.slang"
#include "voxel_tracing.slang"


[format("rgba32f")]
[[vk::binding(2, 0)]] RWTexture2D<float4> visible_voxel_image[];

VoxelTraceResult traceSingleRayInChunk(Ray ray, float dist)
{
    return traceDDARay(0, ray.origin, ray.origin + ray.direction * dist);
}

float3 calculateColor(
    float3 chunkStrikePosition, PBRVoxelMaterial firstMaterial, UnpackUniqueFaceIdResult unpacked, uint rngState)
{
    const GpuRaytracedLight light = in_raytraced_lights[SBO_VOXEL_LIGHTS][0];

    const float3 worldStrikePosition = chunkStrikePosition + float3(-16.0);

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
        0, chunkStrikePosition + 0.05 * unpacked.normal, light.position_and_half_intensity_distance.xyz - float3(-16.0));

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

    return calculatedColor;
}

[shader("compute")]
[numthreads(32, 32, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{


    const float3  box_corner_negative = float3(-16.0f); // TODO: fix
    const int2 thisPixel           = int2(threadId.xy);

    if (all(thisPixel < global_gpu_data[UBO_GLOBAL_DATA].framebuffer_size))
    {
        const float4 rawPixelData        = visible_voxel_image[SIO_VOXEL_RENDER_TARGET].Load(thisPixel);
        const float3 worldStrikePosition = rawPixelData.xyz;
        const float3 chunkStrikePosition = worldStrikePosition - box_corner_negative;
        const u32  uniqueFaceId        = reinterpret<uint>(rawPixelData.w);

        if (uniqueFaceId != ~0u)
        {
            UnpackUniqueFaceIdResult unpacked = {uniqueFaceId};
            
            const uint3 bC = unpacked.position_in_chunk / 8;
            const uint3 bP = unpacked.position_in_chunk % 8;

            const PBRVoxelMaterial material = getMaterialFromPosition(
                in_global_chunk_bricks[SBO_CHUNK_BRICKS][unpacked.chunk_id].data[bC.x][bC.y][bC.z], bP);

            const u32 startSlot = gpu_hashU32(uniqueFaceId) % kHashTableCapacity;

            for (uint i = 0; i < 32; ++i)
            {
                const u32 thisSlot = (startSlot + i) % kHashTableCapacity;
                
                // const u32 prev = atomicCompSwap(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].key, kEmpty, uniqueFaceId);

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
