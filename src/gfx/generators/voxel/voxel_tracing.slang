#pragma once 

#include "globals.slang"
#include "shared_data_structures.slang"
#include "cube_ray.slang"
#include "voxel_faces.slang"
#include "skybox_common.slang" 





PBRVoxelMaterial getMaterialFromPosition(MaybeBrickOffsetOrMaterialId maybeBrickPointer, uint chunkId, uint3 bP)
{
    if (maybeBrickPointer.isPointer())
    {
        const uint16_t materialId = in_combined_bricks[SBO_COMBINED_BRICKS][maybeBrickPointer.getPointer(chunkId).brick_pointer].material_brick.data[bP.x][bP.y][bP.z];

        return in_voxel_materials[SBO_VOXEL_MATERIAL_BUFFER][uint(materialId)];
    }
    else 
    {
        return in_voxel_materials[SBO_VOXEL_MATERIAL_BUFFER][uint(maybeBrickPointer.getMaterial())];
    }
}

MaybeBrickOffsetOrMaterialId tryLoadBrickFromChunkAndCoordinate(uint chunk, uint3 bC)
{
    if (all(bC >= int3(0)) && all(bC <= int3(7)))
    {
        return in_global_chunk_data[SBO_CHUNK_DATA][chunk].brick_map[bC.x][bC.y][bC.z];       
    }

    return MaybeBrickOffsetOrMaterialId();
}

bool loadVoxelFromBrick(BrickPointer brickPointer, int3 c)
{
    // if (all(greaterThanEqual(c, int3(0))) && all(lessThanEqual(c, int3(7))))
    // {
    uint3 bP = c;

    uint       linearIndex = bP.x + (8 * bP.y) + (64 * bP.z);
    const uint idx         = linearIndex / 32;
    const uint bit         = linearIndex % 32;

    const uint loadedIdx = in_combined_bricks[SBO_COMBINED_BRICKS][brickPointer.brick_pointer].boolean_brick.data[idx];

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

// TODO: put in chunkId
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

VoxelTraceResult strikeBlockImmediate(u16 material, float3 rayPos, float3 rayDir, float3 iMask)
{
    rayPos         = clamp(rayPos, float3(0.0001), float3(7.9999));
    float3 mapPos    = floor(rayPos);
    float3 raySign   = sign(rayDir);
    float3 deltaDist = 1.0 / rayDir;
    float3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    float3 mask      = iMask;

   
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
        true, intersect, int3(mapPos), normal, uv3d, d, 0, in_voxel_materials[SBO_VOXEL_MATERIAL_BUFFER][uint(material)]);

}

VoxelTraceResult traceBlock(BrickPointer brick, float3 rayPos, float3 rayDir, float3 iMask)
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

            
            const uint16_t materialId = in_combined_bricks[SBO_COMBINED_BRICKS][brick.brick_pointer].material_brick.data[uint(mapPos.x)][uint(mapPos.y)][uint(mapPos.z)];

            PBRVoxelMaterial thisMaterial = in_voxel_materials[SBO_VOXEL_MATERIAL_BUFFER][uint(materialId)];

            return VoxelTraceResult(
                true, intersect, int3(mapPos), normal, uv3d, d, i + 1, thisMaterial);
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
        if (brick.getMaterial() != 0)
        {
            float3  mini      = ((mapPos - rayPos) + 0.5 - 0.5 * float3(raySign)) * deltaDist;
            float d         = max(mini.x, max(mini.y, mini.z));
            float3  intersect = rayPos + rayDir * d;
            float3  uv3d      = intersect - mapPos;
            
            if (all(mapPos == floor(rayPos))) // Handle edge case where camera origin is inside ofblock block
            {
                uv3d = rayPos - mapPos;
            }

            VoxelTraceResult result;
            if (brick.isMaterial())
            {
                result = strikeBlockImmediate(brick.getMaterial(), uv3d * 8.0, rayDir, mask);
            }
            else
            {
                result = traceBlock(brick.getPointer(chunk), uv3d * 8.0, rayDir, mask);
            }

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

float3 calculateSampleColor(
    float3              worldPos,
    float3              N,
    float3              V,
    float3              L,
    GpuRaytracedLight light,
    PBRVoxelMaterial  material,
    bool              includeSpecular,
    bool              includeEmissive)
{
    if (dot(N, V) <= 0.0) {
        return float3(0, 0, 0);
    }
    // HACK! I mean it loolks pretty
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

    float distanceToLight = length(light.position_and_half_intensity_distance.xyz - worldPos);
    float lightIntensity =
        pow(2.0, -distanceToLight / light.position_and_half_intensity_distance.w) * light.color_and_power.w;
    lightIntensity = max(lightIntensity, 0.0);

    // Calculate final color with custom light intensity
    float3 color = light.color_and_power.xyz * (kD * pow(material.albedo_roughness.xyz, float3(2.2)) / PI + specular)
               * (NdotL / distanceToLight) * lightIntensity;

    if (includeEmissive)
    {
        color += material.emission_metallic.xyz ;
    }

    return color;
}


VoxelTraceResult traceSingleRayInChunk(uint chunkId, Ray ray, float dist)
{
    return traceDDARay(chunkId, ray.origin, ray.origin + ray.direction * dist);
}


uint NextRandom(inout uint state)
{
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}

float RandomValue(inout uint state)
{
    return NextRandom(state) / 4294967295.0; // 2^32 - 1
}

// Random value in normal distribution (with mean=0 and sd=1)
float RandomValueNormalDistribution(inout uint state)
{
    // Thanks to https://stackoverflow.com/a/6178290
    float theta = 2 * 3.1415926 * RandomValue(state);
    float rho = sqrt(-2 * log(RandomValue(state)));
    return rho * cos(theta);
}

// Calculate a random direction
float3 RandomDirection(inout uint state)
{
    // Thanks to https://math.stackexchange.com/a/1585996
    float x = RandomValueNormalDistribution(state);
    float y = RandomValueNormalDistribution(state);
    float z = RandomValueNormalDistribution(state);
    return normalize(float3(x, y, z));
}

float3 RandomHemisphereDirection(float3 normal, inout uint rngState)
{
    const float3 dir = RandomDirection(rngState);
    return dir * sign(dot(normal, dir));
}

float3 calculateWorldColor(float3 firstWorldStrikePosition, PBRVoxelMaterial firstMaterial, UnpackUniqueFaceIdResult unpacked, uint rngState, bool shouldDoReflect)
{
    GpuRaytracedLight light = in_raytraced_lights[SBO_VOXEL_LIGHTS][0];

    const u32 numberOfPossibleEmissives = in_const_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].number_of_emissives;

    const u32 thisEmissiveRandomPosition = NextRandom(rngState) % numberOfPossibleEmissives;
    if (numberOfPossibleEmissives == 0)
    {
        return float3(0);
    }
    light.position_and_half_intensity_distance.xyz = in_const_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].getWorldChunkCorner() + in_const_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].emissive_data[thisEmissiveRandomPosition].getCenterPositionOfVoxel();
    light.color_and_power.w *= numberOfPossibleEmissives;
    

    const float3 toCameraVector = normalize(global_gpu_data[UBO_GLOBAL_DATA].camera_position.xyz - firstWorldStrikePosition);

    float3 lightColor = calculateSampleColor(
        firstWorldStrikePosition,
        unpacked.normal,
        toCameraVector,
        normalize(light.position_and_half_intensity_distance.xyz - firstWorldStrikePosition),
        light,
        firstMaterial,
        true,
        true);

    const float3 start = firstWorldStrikePosition + 0.05 * unpacked.normal;
    const float3 end = light.position_and_half_intensity_distance.xyz;
    const float3 dir = normalize(end - start);
    const f32 len = distance(end, start);


    float3 calculatedColor = float3(0.0);


    if (light.canInfluencePoint(firstWorldStrikePosition))
    {
        const VoxelTraceResult shadowResult = traceWorld(Ray(start, dir), len);
        
        if (!shadowResult.intersect_occur || (all(shadowResult.material.emission_metallic.xyz != float3(0.0))))
        {
            calculatedColor += lightColor;
        }
    }
    if (shouldDoReflect && firstMaterial.albedo_roughness.w != 1.0)
    {
        const float3 toFragmentDir = normalize(firstWorldStrikePosition - global_gpu_data[UBO_GLOBAL_DATA].camera_position.xyz);

        Ray bounceRay =
            Ray(firstWorldStrikePosition + 0.05 * unpacked.normal, normalize(reflect(toFragmentDir, unpacked.normal)));

        const VoxelTraceResult bounceResult = traceWorld(bounceRay, 128.0f);
\
        if (bounceResult.intersect_occur)
        {
            const float3 secondWorldStrikePosition = bounceRay.origin + bounceResult.t * bounceRay.direction;
            const float3 L = normalize(light.position_and_half_intensity_distance.xyz - secondWorldStrikePosition);

            const Ray shadowRay = Ray(secondWorldStrikePosition + 0.05 * bounceResult.voxel_normal, L);

            if (light.canInfluencePoint(shadowRay.origin))
            {
                const VoxelTraceResult bounceShadow = traceWorld(shadowRay, distance(light.position_and_half_intensity_distance.xyz, secondWorldStrikePosition));
                
                if (!bounceShadow.intersect_occur || (all(bounceShadow.material.emission_metallic.xyz != float3(0.0))))
                {
                    float3 reflectionColor = calculateSampleColor(
                        secondWorldStrikePosition,
                        bounceResult.voxel_normal,
                        -bounceRay.direction,
                        L,
                        light,
                        bounceResult.material,
                        true,
                        true) * firstMaterial.albedo_roughness.xyz;

                    calculatedColor += reflectionColor * (1.0 - firstMaterial.albedo_roughness.w) ;
                }
            }
        }
        else
        {
            calculatedColor += totalSkybox(bounceRay.direction, false) * firstMaterial.albedo_roughness.xyz;
        }

    }

    return calculatedColor;

}


VoxelTraceResult traceWorld(Ray ray, float max_t)
{
    ray.origin /= 64.0f;
    max_t /= 64.0f;

    float3 mapPos    = floor(ray.origin);
    float3 raySign   = sign(ray.direction);
    float3 deltaDist = 1.0 / ray.direction;
    float3 sideDist  = ((mapPos - ray.origin) + 0.5 + raySign * 0.5) * deltaDist;
    float3 mask      = stepMask(sideDist);

    uint extra_steps = 0;

    int i;
    for (i = 0; i < 32; i++)
    {
        const u32 maybeChunkId = tryReadChunkHashTable(int3(mapPos));

        if (maybeChunkId != ~0)
        {
            float3  mini      = ((mapPos - ray.origin) + 0.5 - 0.5 * float3(raySign)) * deltaDist;
            float d         = max(mini.x, max(mini.y, mini.z));
            float3  intersect = ray.origin + ray.direction * d;
            float3  uv3d      = intersect - mapPos;

            if (all(mapPos == floor(ray.origin))) // Handle edge case where camera origin is inside ofblock block
            {
                uv3d = ray.origin - mapPos;
            }

            const float3 worldPositionStartStrike = uv3d * 64.0f + floor(mapPos) * 64.0f;
            if (distance(ray.origin * 64.0f, worldPositionStartStrike) > max_t * 64.0f)
            {
                break;
            }

            // TODO;
            // max_t
            // comp between start and worldPosition start strike along the dir
            // subtract that to get the t
            const float tTraversed = distance(worldPositionStartStrike, ray.origin * 64.0f);
            const float tTotal = max_t * 64.0f;
            const float tLeft = tTotal - tTraversed;

            // TODO: make less shit
            const VoxelTraceResult result = traceSingleRayInChunk(maybeChunkId, Ray(uv3d * 64.0f, ray.direction), tLeft);

            extra_steps += result.steps;

            if (result.intersect_occur)
            {
                // const float strikeT =
                //     distance(ray.origin * 64.0f, floor(mapPos) * 64.0f + result.chunk_local_fragment_position);

                const float strikeT = distance(ray.origin * 64.0f, floor(mapPos) * 64.0f + result.chunk_local_fragment_position);


                if (strikeT > max_t * 64.0f)
                {
                    break;
                }

                return VoxelTraceResult(
                    true,
                    result.chunk_local_fragment_position,
                    result.chunk_local_voxel_position,
                    result.voxel_normal,
                    result.local_voxel_uvw,
                    strikeT,
                    extra_steps + i + 1,
                    result.material);
            }
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return VoxelTraceResult_getMiss(i + extra_steps);
}