#version 460

#include "globals.glsl"
#include "intersectables.glsl"
#include "misc.glsl"
#include "types.glsl"

layout(push_constant) uniform PushConstants
{
    uint global_data_offset;
    uint brick_data_offset;
    uint chunk_bricks_offset;
}
in_push_constants;

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

const int MAX_RAY_STEPS = 256;

bool getBlockVoxel(ivec3 c)
{
    if (all(greaterThanEqual(c, ivec3(0))) && all(lessThanEqual(c, ivec3(63))))
    {
        uvec3 bC = c / 8;
        uvec3 bP = c % 8;

        const uint16_t brickPointer =
            in_global_chunk_bricks[in_push_constants.chunk_bricks_offset].storage[0].data[bC.x][bC.y][bC.z];

        uint       linearIndex = bP.x + (8 * bP.y) + (64 * bP.z);
        const uint idx         = linearIndex / 32;
        const uint bit         = linearIndex % 32;

        return (in_global_bricks[in_push_constants.brick_data_offset].data[uint(brickPointer)].data[idx] & (1u << bit))
            != 0;
    }

    return false;
}

uint tryLoadBrickFromChunkAndCoordinate(uint chunk, uvec3 bC)
{
    if (all(greaterThanEqual(bC, ivec3(0))) && all(lessThanEqual(bC, ivec3(7))))
    {
        const uint16_t brickPointer =
            in_global_chunk_bricks[in_push_constants.chunk_bricks_offset].storage[chunk].data[bC.x][bC.y][bC.z];

        if (brickPointer != u16(-1))
        {
            return uint(brickPointer);
        }
    }

    return ~0u;
}

bool loadVoxelFromBrick(uint brickPointer, ivec3 c)
{
    if (all(greaterThanEqual(c, ivec3(0))) && all(lessThanEqual(c, ivec3(7))))
    {
        uvec3 bP = c;

        uint       linearIndex = bP.x + (8 * bP.y) + (64 * bP.z);
        const uint idx         = linearIndex / 32;
        const uint bit         = linearIndex % 32;

        const uint loadedIdx = in_global_bricks[in_push_constants.brick_data_offset].data[brickPointer].data[idx];

        return (loadedIdx & (1u << bit)) != 0;
    }

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
    bool  intersect_occur;
    vec3  chunk_local_fragment_position;
    vec3  voxel_normal;
    vec3  local_voxel_uvw;
    float t;
    uint  steps;
};

VoxelTraceResult VoxelTraceResult_getMiss(uint steps)
{
    return VoxelTraceResult(false, vec3(0.0), vec3(0.0), vec3(0.0), 0.0, steps);
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

            // TODO: normal
            return VoxelTraceResult(true, intersect, normal, uv3d, d, i);
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return VoxelTraceResult_getMiss(i);
}

VoxelTraceResult traceChunk(uint chunk, vec3 rayPos, vec3 rayDir, float max_dist)
{
    rayPos /= 8.0;
    max_dist /= 8.0;

    vec3 mapPos    = floor(rayPos);
    vec3 raySign   = sign(rayDir);
    vec3 deltaDist = 1.0 / rayDir;
    vec3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    vec3 mask      = stepMask(sideDist);

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

            if (result.intersect_occur)
            {
                return VoxelTraceResult(
                    true,
                    floor(mapPos) * 8.0 + result.chunk_local_fragment_position,
                    result.voxel_normal,
                    result.local_voxel_uvw,
                    result.t + d * 8.0,
                    result.steps + i);
            }
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return VoxelTraceResult_getMiss(i);
}

VoxelTraceResult traceDDARay(vec3 start, vec3 end)
{
    if (distance(end, start) > 512)
    {
        return VoxelTraceResult_getMiss(0);
    }

    return traceChunk(0, start, normalize(end - start), length(end - start));
}
layout(location = 0) in vec3 in_uvw;
layout(location = 1) in vec3 in_world_position;
layout(location = 2) in vec3 in_cube_corner_location;

layout(location = 0) out vec4 out_color;

layout(depth_greater) out float gl_FragDepth;

void main()
{
    const Cube c = Cube(in_cube_corner_location + 32, 64);

    const vec3 camera_position = GlobalData.camera_position.xyz;

    const vec3 dir = normalize(in_world_position - GlobalData.camera_position.xyz);

    Ray ray = Ray(camera_position, dir);

    IntersectionResult res = Cube_tryIntersectFast(c, ray);

    const vec3 box_corner_negative = in_cube_corner_location;
    const vec3 box_corner_positive = in_cube_corner_location + 64;

    const bool is_camera_inside_box = Cube_contains(c, camera_position);

    const vec3 traversalRayOrigin =
        is_camera_inside_box ? (camera_position - box_corner_negative) : (res.maybe_hit_point - box_corner_negative);

    const VoxelTraceResult result = traceDDARay(traversalRayOrigin, traversalRayOrigin + dir * 96.0);

    const bool showTrace = false;

    if (showTrace)
    {
        out_color = vec4(plasma_quintic(float(result.steps) / 128.0), 1.0);
    }
    else
    {
        if (!result.intersect_occur)
        {
            discard;
        }
        out_color = vec4(result.local_voxel_uvw, 1.0);
    }

    vec4 clipPos = GlobalData.view_projection_matrix
                 * vec4(result.chunk_local_fragment_position + in_cube_corner_location, float(1.0));
    gl_FragDepth = (clipPos.z / clipPos.w);
}