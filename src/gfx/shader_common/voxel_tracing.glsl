

#ifndef BRICK_MAPS_OFFSET
#error "BRICK_MAPS_OFFSET must be defined"
#endif // BRICK_MAPS_OFFSET

#ifndef VISIBILITY_BRICKS_OFFSET
#error "VISIBILITY_BRICKS_OFFSET must be defined"
#endif // VISIBILITY_BRICKS_OFFSET

#ifndef MATERIAL_BRICKS_OFFSET
#error "MATERIAL_BRICKS_OFFSET must be defined"
#endif // MATERIAL_BRICKS_OFFSET

#ifndef VOXEL_MATERIALS_OFFSET
#error "VOXEL_MATERIALS_OFFSET must be defined"
#endif // VOXEL_MATERIALS_OFFSET

const bool removeCheckLate = false;

struct BooleanBrick
{
    uint data[16];
};

layout(set = 0, binding = 4) readonly buffer GlobalGpuBricks
{
    BooleanBrick data[];
}
in_global_bricks[];

struct ChunkBrickStorage
{
    uint16_t data[8][8][8];
};

layout(set = 0, binding = 4) readonly buffer GlobalChunkBrickStorage
{
    ChunkBrickStorage storage[];
}
in_global_chunk_bricks[];

struct VoxelMaterial
{
    vec4  ambient_color;
    vec4  diffuse_color;
    vec4  specular_color;
    vec4  emissive_color_power;
    vec4  coat_color_power;
    float diffuse_subsurface_weight;
    float specular;
    float roughness;
    float metallic;
};

VoxelMaterial VoxelMaterial_getUninitialized()
{
    VoxelMaterial m;

    return m;
}

layout(set = 0, binding = 4) readonly buffer VoxelMaterialsStorage
{
    VoxelMaterial materials[];
}
in_voxel_materials[];

struct MaterialBrick
{
    uint16_t data[8][8][8];
};

layout(set = 0, binding = 4) readonly buffer VoxelMaterialBricks
{
    MaterialBrick bricks[];
}
in_material_bricks[];

VoxelMaterial getMaterialFromPosition(uint brickPointer, uvec3 bP)
{
    const uint16_t materialId = in_material_bricks[MATERIAL_BRICKS_OFFSET].bricks[brickPointer].data[bP.x][bP.y][bP.z];

    return in_voxel_materials[VOXEL_MATERIALS_OFFSET].materials[uint(materialId)];
}

uint tryLoadBrickFromChunkAndCoordinate(uint chunk, uvec3 bC)
{
    if (removeCheckLate || (all(greaterThanEqual(bC, ivec3(0))) && all(lessThanEqual(bC, ivec3(7)))))
    {
        const uint16_t brickPointer = in_global_chunk_bricks[BRICK_MAPS_OFFSET].storage[chunk].data[bC.x][bC.y][bC.z];

        if (brickPointer != u16(-1))
        {
            return uint(brickPointer);
        }
    }

    return ~0u;
}

bool loadVoxelFromBrick(uint brickPointer, ivec3 c)
{
    // if (all(greaterThanEqual(c, ivec3(0))) && all(lessThanEqual(c, ivec3(7))))
    // {
    uvec3 bP = c;

    uint       linearIndex = bP.x + (8 * bP.y) + (64 * bP.z);
    const uint idx         = linearIndex / 32;
    const uint bit         = linearIndex % 32;

    const uint loadedIdx = in_global_bricks[VISIBILITY_BRICKS_OFFSET].data[brickPointer].data[idx];

    return (loadedIdx & (1u << bit)) != 0;
    // }

    return false;
}

vec3 stepMask(vec3 sideDist)
{
    bvec3 mask;
    bvec3 b1 = lessThan(sideDist.xyz, sideDist.yzx);
    bvec3 b2 = lessThanEqual(sideDist.xyz, sideDist.zxy);
    mask.z   = b1.z && b2.z;
    mask.x   = b1.x && b2.x;
    mask.y   = b1.y && b2.y;
    if (!any(mask)) // Thank you Spalmer
    {
        mask.z = true;
    }

    return vec3(mask);
}

struct VoxelTraceResult
{
    bool          intersect_occur;
    vec3          chunk_local_fragment_position;
    // ivec3 chunk_local_voxel_position;
    vec3          voxel_normal;
    vec3          local_voxel_uvw;
    float         t;
    uint          steps;
    VoxelMaterial material;
};

VoxelTraceResult VoxelTraceResult_getMiss(uint steps)
{
    return VoxelTraceResult(false, vec3(0.0), vec3(0.0), vec3(0.0), 0.0, steps, VoxelMaterial_getUninitialized());
}

VoxelTraceResult traceBlock(u32 brick, vec3 rayPos, vec3 rayDir, vec3 iMask, float max_dist)
{
    rayPos         = clamp(rayPos, vec3(0.0001), vec3(7.9999));
    vec3 mapPos    = floor(rayPos);
    vec3 raySign   = sign(rayDir);
    vec3 deltaDist = 1.0 / rayDir;
    vec3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    vec3 mask      = iMask;

    int i;

    for (i = 0; i < 27 && all(lessThanEqual(mapPos, vec3(7.0))) && all(greaterThanEqual(mapPos, vec3(0.0)))
                && length(mapPos - rayPos) <= max_dist;
         ++i)
    {
        if (loadVoxelFromBrick(brick, ivec3(mapPos)))
        {
            vec3  mini      = ((mapPos - rayPos) + 0.5 - 0.5 * vec3(raySign)) * deltaDist;
            float d         = max(mini.x, max(mini.y, mini.z));
            vec3  intersect = rayPos + rayDir * d;
            vec3  uv3d      = intersect - mapPos;

            if (mapPos == floor(rayPos)) // Handle edge case where camera origin is inside ofblock block
            {
                uv3d = rayPos - mapPos;
            }

            vec3 normal = vec3(0.0);
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
                true, intersect, normal, uv3d, d, i + 1, getMaterialFromPosition(brick, uvec3(mapPos)));
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return VoxelTraceResult_getMiss(i);
}

VoxelTraceResult traceChunkFallible(uint chunk, vec3 rayPos, vec3 rayDir, float max_dist)
{
    rayPos /= 8.0;
    max_dist /= 8.0;

    vec3 mapPos    = floor(clamp(rayPos, vec3(0.0001), vec3(7.9999)));
    vec3 raySign   = sign(rayDir);
    vec3 deltaDist = 1.0 / rayDir;
    vec3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    vec3 mask      = stepMask(sideDist);

    uint extra_steps = 0;

    int i;
    for (i = 0; i < 32 && length(mapPos - rayPos) < max_dist; i++)
    {
        u32 brick = tryLoadBrickFromChunkAndCoordinate(chunk, ivec3(mapPos));
        if (brick != ~0u)
        {
            vec3  mini      = ((mapPos - rayPos) + 0.5 - 0.5 * vec3(raySign)) * deltaDist;
            float d         = max(mini.x, max(mini.y, mini.z));
            vec3  intersect = rayPos + rayDir * d;
            vec3  uv3d      = intersect - mapPos;

            if (mapPos == floor(rayPos)) // Handle edge case where camera origin is inside ofblock block
            {
                uv3d = rayPos - mapPos;
            }

            VoxelTraceResult result =
                traceBlock(brick, uv3d * 8.0, rayDir, mask, (max_dist - length(mapPos - rayPos)) * 8.0);

            extra_steps += result.steps;

            if (result.intersect_occur)
            {
                return VoxelTraceResult(
                    true,
                    floor(mapPos) * 8.0 + result.chunk_local_fragment_position,
                    result.voxel_normal,
                    result.local_voxel_uvw,
                    result.t + d * 8.0,
                    extra_steps + i + 1,
                    result.material);
            }
        }

        if (any(lessThan(mapPos, vec3(-1.1))) || any(greaterThan(mapPos, vec3(8.1))))
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

// VoxelTraceResult VoxelChunk_traceBoundedRayBetweenTwoPoints(uint chunkID, vec3 start, vec3 end) {}

// VoxelTraceResult traceBoundedRay(uint chunkId, vec3 start, vec3 end) {}

VoxelTraceResult traceDDARay(uint chunkId, vec3 start, vec3 end)
{
    if (distance(end, start) > 512)
    {
        return VoxelTraceResult_getMiss(0);
    }

    const VoxelTraceResult result = traceChunkFallible(chunkId, start, normalize(end - start), length(end - start));

    // TODO: use the ivec3 chunk local position thing to determine if the strike is in bounds
    // if (result.intersect_occur
    //     && (any(lessThanEqual(result.chunk_local_fragment_position, vec3(0.0)))
    //         || any(greaterThanEqual(result.chunk_local_fragment_position, vec3(64.0)))))
    // {
    //     return VoxelTraceResult_getMiss(result.steps);
    // }

    return result;
}