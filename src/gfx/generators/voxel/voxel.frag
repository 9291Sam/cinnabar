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

VoxelTraceResult traceChunkNaive(uint chunk, vec3 rayPos, vec3 rayDir, float tMax)
{
    vec3       mapPos    = floor(rayPos);
    vec3       raySign   = sign(rayDir);
    const vec3 deltaDist = 1.0 / rayDir;
    vec3       sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    vec3       mask      = stepMask(sideDist);

    int i;
    for (i = 0; i < MAX_RAY_STEPS; i++)
    {
        const ivec3 integerPos = ivec3(mapPos);

        vec3  mini                              = ((mapPos - rayPos) + 0.5 - 0.5 * vec3(raySign)) * deltaDist;
        float rayTraversalDistanceSinceFragment = max(mini.x, max(mini.y, mini.z));

        if (rayTraversalDistanceSinceFragment > tMax)
        {
            return VoxelTraceResult_getMiss(i);
        }

        if (any(lessThan(integerPos, ivec3(-1))) || any(greaterThan(integerPos, ivec3(64))))
        {
            return VoxelTraceResult_getMiss(i);
        }

        const uint maybeThisBrick = tryLoadBrickFromChunkAndCoordinate(chunk, integerPos / 8);

        if (maybeThisBrick != -1 && loadVoxelFromBrick(maybeThisBrick, integerPos % 8))
        {
            // Okay, we've struck a voxel, let's do a ray-cube intersection to determine other parameters)

            vec3 intersectionPositionBrick = rayPos + rayDir * rayTraversalDistanceSinceFragment;
            vec3 voxelLocalUVW3D           = intersectionPositionBrick - mapPos;

            // if (mapPos == floor(rayPos)) // Handle edge case where camera origin is inside of block
            // {
            //     voxelLocalUVW3D = rayPos - mapPos;
            // }

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
                true, intersectionPositionBrick, normal, voxelLocalUVW3D, rayTraversalDistanceSinceFragment, i);
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return VoxelTraceResult_getMiss(i);
}

// VoxelTraceResult traceBrick(uint brick, vec3 rayPos, vec3 rayDir, float tMax)
// {
//     vec3       mapPos    = floor(rayPos);
//     vec3       raySign   = sign(rayDir);
//     const vec3 deltaDist = 1.0 / rayDir;
//     vec3       sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
//     vec3       mask      = stepMask(sideDist);

//     int i;
//     for (i = 0; i < MAX_RAY_STEPS; i++)
//     {
//         const ivec3 integerPos = ivec3(mapPos);

//         vec3  mini                              = ((mapPos - rayPos) + 0.5 - 0.5 * vec3(raySign)) * deltaDist;
//         float rayTraversalDistanceSinceFragment = max(mini.x, max(mini.y, mini.z));

//         if (rayTraversalDistanceSinceFragment > tMax)
//         {
//             return VoxelTraceResult_getMiss(i);
//         }

//         if (getBlockVoxel(integerPos))
//         {
//             // Okay, we've struck a voxel, let's do a ray-cube intersection to determine other parameters)

//             vec3 intersectionPositionBrick = rayPos + rayDir * rayTraversalDistanceSinceFragment;
//             vec3 voxelLocalUVW3D           = intersectionPositionBrick - mapPos;

//             if (mapPos == floor(rayPos)) // Handle edge case where camera origin is inside of block
//             {
//                 voxelLocalUVW3D = rayPos - mapPos;
//             }

//             vec3 normal = vec3(0.0);
//             if (mini.x > mini.y && mini.x > mini.z)
//             {
//                 normal.x = -raySign.x;
//             }
//             else if (mini.y > mini.z)
//             {
//                 normal.y = -raySign.y;
//             }
//             else
//             {
//                 normal.z = -raySign.z;
//             }

//             return VoxelTraceResult(
//                 true, intersectionPositionBrick, normal, voxelLocalUVW3D, rayTraversalDistanceSinceFragment, i);
//         }

//         if (any(lessThan(integerPos, ivec3(-1))) || any(greaterThan(integerPos, ivec3(64))))
//         {
//             break;
//         }

//         mask = stepMask(sideDist);
//         mapPos += mask * raySign;
//         sideDist += mask * raySign * deltaDist;
//     }

//     return VoxelTraceResult_getMiss(i);
// }

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

    const VoxelTraceResult result = traceChunkNaive(0, traversalRayOrigin, dir, 128.0);

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