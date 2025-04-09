#pragma once 

#include "globals.slang"
#include "shared_data_structures.slang"
#include "cube_ray.slang"
#include "voxel_faces.slang"

[[vk::binding(4)]] RWStructuredBuffer<CombinedBrick> in_combined_bricks[];

[[vk::binding(4)]] RWStructuredBuffer<ChunkData> in_global_chunk_bricks[];
[[vk::binding(4)]] StructuredBuffer<ChunkData> in_const_global_chunk_bricks[];

[[vk::binding(4)]] RWStructuredBuffer<PBRVoxelMaterial> in_voxel_materials[];
[[vk::binding(4)]] RWStructuredBuffer<GpuRaytracedLight> in_raytraced_lights[];




PBRVoxelMaterial getMaterialFromPosition(uint brickPointer, uint3 bP)
{
    const uint16_t materialId = in_combined_bricks[SBO_COMBINED_BRICKS][brickPointer].material_brick.data[bP.x][bP.y][bP.z];

    return in_voxel_materials[SBO_VOXEL_MATERIAL_BUFFER][uint(materialId)];
}

MaybeBrickOffsetOrMaterialId tryLoadBrickFromChunkAndCoordinate(uint chunk, uint3 bC)
{
    if (all(bC >= int3(0)) && all(bC <= int3(7)))
    {
        return in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunk].brick_map[bC.x][bC.y][bC.z];       
    }

    return MaybeBrickOffsetOrMaterialId();
}

bool loadVoxelFromBrick(uint brickPointer, int3 c)
{
    // if (all(greaterThanEqual(c, int3(0))) && all(lessThanEqual(c, int3(7))))
    // {
    uint3 bP = c;

    uint       linearIndex = bP.x + (8 * bP.y) + (64 * bP.z);
    const uint idx         = linearIndex / 32;
    const uint bit         = linearIndex % 32;

    const uint loadedIdx = in_combined_bricks[SBO_COMBINED_BRICKS][brickPointer].boolean_brick.data[idx];

    return (loadedIdx & (1u << bit)) != 0;

    // It's just **barely** slower on mac
    // return in_combined_bricks[SBO_COMBINED_BRICKS][brickPointer].material_brick.data[bP.x][bP.y][bP.z] != 0;
}

float3 stepMask(float3 sideDist)
{
    bool3 mask;
    bool3 b1 = (sideDist.xyz < sideDist.yzx);
    bool3 b2 = (sideDist.xyz <= sideDist.zxy);
    mask.z   = b1.z && b2.z;
    mask.x   = b1.x && b2.x;
    mask.y   = b1.y && b2.y;
    if (!any(mask)) // Thank you Spalmer
    {
        mask.z = true;
    }

    return float3(mask);
}

struct VoxelTraceResult
{
    bool             intersect_occur;
    float3             chunk_local_fragment_position;
    int3            chunk_local_voxel_position;
    float3             voxel_normal;
    float3             local_voxel_uvw;
    float            t;
    uint             steps;
    PBRVoxelMaterial material;
};

VoxelTraceResult VoxelTraceResult_getMiss(uint steps)
{
    return VoxelTraceResult(
        false, float3(0.0), int3(0), float3(0.0), float3(0.0), 0.0, steps, PBRVoxelMaterial(float4(0.0), float4(0.0)));
}

VoxelTraceResult traceBlock(u32 brick, float3 rayPos, float3 rayDir, float3 iMask)
{
    rayPos         = clamp(rayPos, float3(0.0001), float3(7.9999));
    float3 mapPos    = floor(rayPos);
    float3 raySign   = sign(rayDir);
    float3 deltaDist = 1.0 / rayDir;
    float3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    float3 mask      = iMask;

    int i;
    // ccrazy range check maopos in 0-7
    for (i = 0; i < 27 && all(abs(mapPos - float3(3.5)) <= float3(3.5)); ++i)
    {
        if (loadVoxelFromBrick(brick, int3(mapPos)))
        {
            float3  mini      = ((mapPos - rayPos) + 0.5 - 0.5 * float3(raySign)) * deltaDist;
            float d         = max(mini.x, max(mini.y, mini.z));
            float3  intersect = rayPos + rayDir * d;
            float3  uv3d      = intersect - mapPos;

            if (all(mapPos == floor(rayPos))) // Handle edge case where camera origin is inside ofblock block
            {
                uv3d = rayPos - mapPos;
            }

            float3 normal = float3(0.0);
            if (mini.x > mini.y && mini.x > mini.z)
            {
                normal.x = -raySign.x;
            }
            else if (mini.y > mini.z)
            {
                normal.y = -raySign.y;
            }
            else
            {
                normal.z = -raySign.z;
            }

            return VoxelTraceResult(
                true, intersect, int3(mapPos), normal, uv3d, d, i + 1, getMaterialFromPosition(brick, uint3(mapPos)));
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return VoxelTraceResult_getMiss(i);
}

VoxelTraceResult traceChunkFallible(uint chunk, float3 rayPos, float3 rayDir, float max_dist)
{
    rayPos /= 8.0;
    max_dist /= 8.0;

    float3 mapPos    = floor(clamp(rayPos, float3(0.0001), float3(7.9999)));
    float3 raySign   = sign(rayDir);
    float3 deltaDist = 1.0 / rayDir;
    float3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    float3 mask      = stepMask(sideDist);

    uint extra_steps = 0;

    int i;
    for (i = 0; i < 32; i++)
    {
        MaybeBrickOffsetOrMaterialId brick = tryLoadBrickFromChunkAndCoordinate(chunk, int3(mapPos));
        if (brick.isPointer())
        {
            float3  mini      = ((mapPos - rayPos) + 0.5 - 0.5 * float3(raySign)) * deltaDist;
            float d         = max(mini.x, max(mini.y, mini.z));
            float3  intersect = rayPos + rayDir * d;
            float3  uv3d      = intersect - mapPos;

            if (all(mapPos == floor(rayPos))) // Handle edge case where camera origin is inside ofblock block
            {
                uv3d = rayPos - mapPos;
            }

            VoxelTraceResult result = traceBlock(brick.data + in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunk].range_allocation.offset, uv3d * 8.0, rayDir, mask);

            extra_steps += result.steps;

            float strikeT = d * 8.0;

            if (result.intersect_occur)
            {
                const float strikeT =
                    distance(rayPos * 8.0, floor(mapPos) * 8.0 + result.chunk_local_fragment_position);

                // if (strikeT > max_dist * 8.0)
                // {
                //     break;
                // }

                return VoxelTraceResult(
                    true,
                    floor(mapPos) * 8.0 + result.chunk_local_fragment_position,
                    int3(mapPos) * 8 + result.chunk_local_voxel_position,
                    result.voxel_normal,
                    result.local_voxel_uvw,
                    strikeT,
                    extra_steps + i + 1,
                    result.material);
            }
        }
        else {
            // Brick is material!
            #warning implement
        }

        if (any(mapPos < float3(-1.1)) || any(mapPos > float3(8.1)))
        {
            break;
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return VoxelTraceResult_getMiss(i + extra_steps);
}

// VoxelTraceResult VoxelChunk_traceBoundedRay(uint chunkID, Ray ray, t_max) {}

// VoxelTraceResult VoxelChunk_traceBoundedRayBetweenTwoPoints(uint chunkID, float3 start, float3 end) {}

// VoxelTraceResult traceBoundedRay(uint chunkId, float3 start, float3 end) {}

VoxelTraceResult traceDDARay(uint chunkId, float3 start, float3 end)
{
    if (distance(end, start) > 512)
    {
        return VoxelTraceResult_getMiss(0);
    }

    const VoxelTraceResult result = traceChunkFallible(chunkId, start, normalize(end - start), length(end - start));

    if (result.intersect_occur && result.t > length(end - start))
    {
        return VoxelTraceResult_getMiss(result.steps);
    }
    // TODO: use the int3 chunk local position thing to determine if the strike is in bounds
    // if (result.intersect_occur
    //     && (any(lessThanEqual(result.chunk_local_fragment_position, float3(0.0)))
    //         || any(greaterThanEqual(result.chunk_local_fragment_position, float3(64.0)))))
    // {
    //     return VoxelTraceResult_getMiss(result.steps);
    // }

    return result;
}



float distributionGGX(float3 N, float3 H, float roughness)
{
    float a2    = roughness * roughness * roughness * roughness;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return geometrySchlickGGX(max(dot(N, L), 0.0), roughness) * geometrySchlickGGX(max(dot(N, V), 0.0), roughness);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 calculatePixelColor(
    float3              worldPos,
    float3              N,
    float3              V,
    float3              L,
    GpuRaytracedLight light,
    PBRVoxelMaterial  material,
    bool              includeSpecular,
    bool              includeEmissive)
{
    // HACK!
    material.albedo_roughness.w = max(material.albedo_roughness.w, material.emission_metallic.w / 2);

    float3 H = normalize(V + L);

    float3  F0  = lerp(float3(0.04), pow(material.albedo_roughness.xyz, float3(2.2)), material.emission_metallic.w);
    float NDF = distributionGGX(N, H, material.albedo_roughness.w);
    float G   = geometrySmith(N, V, L, material.albedo_roughness.w);
    float3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float3  kD  = float3(1.0) - F;

    kD *= 1.0 - material.emission_metallic.w;

    float3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3  specular    = numerator / max(denominator, 0.001);

    if (!includeSpecular)
    {
        specular = float3(0.0);
    }

    float NdotL = max(dot(N, L), 0.0);

    // Custom light intensity calculation with exponential falloff
    float distanceToLight = length(light.position_and_half_intensity_distance.xyz - worldPos);
    float lightIntensity =
        pow(2.0, -distanceToLight / light.position_and_half_intensity_distance.w) * light.color_and_power.w;
    lightIntensity = max(lightIntensity, 0.0);

    // Calculate final color with custom light intensity
    float3 color = light.color_and_power.xyz * (kD * pow(material.albedo_roughness.xyz, float3(2.2)) / PI + specular)
               * (NdotL / distanceToLight) * lightIntensity;

    if (includeEmissive)
    {
        color += material.emission_metallic.xyz / 8.0;
    }

    return color;
}


VoxelTraceResult traceSingleRayInChunk(uint chunkId, Ray ray, float dist)
{
    return traceDDARay(chunkId, ray.origin, ray.origin + ray.direction * dist);
}

float3 calculateColor(
    uint chunkId, 
    float3 chunkStrikePosition, PBRVoxelMaterial firstMaterial, UnpackUniqueFaceIdResult unpacked)
{
    const GpuRaytracedLight light = in_raytraced_lights[SBO_VOXEL_LIGHTS][0];

    const float3 worldStrikePosition = chunkStrikePosition + in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunkId].world_chunk_corner;

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
        0, chunkStrikePosition + 0.05 * unpacked.normal, light.position_and_half_intensity_distance.xyz - in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunkId].world_chunk_corner);

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

    const Cube chunk = Cube(float3(32.0f), 64.0f); // chunkspace
    
    if (chunk.contains(giRay.origin))
    {
        const VoxelTraceResult giResult = traceSingleRayInChunk(chunkId, giRay, 96.0f);

        if (giResult.intersect_occur)
        {
            // TODO: add shadow ray for secondary
    // const VoxelTraceResult shadowResult = traceDDARay(
    //     0, chunkStrikePosition + 0.05 * unpacked.normal, light.position_and_half_intensity_distance.xyz - in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunkId].world_chunk_corner);


            if (firstMaterial.albedo_roughness.w != 0.0)
            {
                const float3 reflectionColor = calculatePixelColor(
                    giResult.chunk_local_fragment_position + in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunkId].world_chunk_corner,
                    giResult.voxel_normal,
                    normalize(giRay.origin - giResult.chunk_local_fragment_position),
                    normalize(
                        light.position_and_half_intensity_distance.xyz -  in_global_chunk_bricks[SBO_CHUNK_BRICKS][chunkId].world_chunk_corner),
                    light,
                    giResult.material,
                    false,
                    true);

                const float3 diffuseColor = firstMaterial.albedo_roughness.xyz;

                calculatedColor += reflectionColor * (1.0 - firstMaterial.albedo_roughness.w);
            }

        }
    }

    return calculatedColor;
}