#version 460

#include "globals.glsl"
#include "intersectables.glsl"
#include "misc.glsl"
#include "types.glsl"

layout(push_constant) uniform PushConstants
{
    uint chunk_brick_maps_buffer_offset;
    uint lights_buffer_offset;
    uint visibility_bricks_buffer_offset;
    uint material_bricks_buffer_offset;
    uint voxel_material_buffer_offset;
}
in_push_constants;

#define BRICK_MAPS_OFFSET        in_push_constants.chunk_brick_maps_buffer_offset
#define LIGHT_BUFFER_OFFSET      in_push_constants.lights_buffer_offset
#define VISIBILITY_BRICKS_OFFSET in_push_constants.visibility_bricks_buffer_offset
#define MATERIAL_BRICKS_OFFSET   in_push_constants.material_bricks_buffer_offset
#define VOXEL_MATERIALS_OFFSET   in_push_constants.voxel_material_buffer_offset
#define VOXEL_HASH_MAP_OFFSET    5

#include "voxel_faces.glsl"
#include "voxel_tracing.glsl"

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_cube_negative_corner;

layout(location = 0) out vec4 out_position_and_id;

layout(depth_greater) out float gl_FragDepth;

u32 packUniqueFaceId(vec3 normal, uvec3 positionInChunk, uint chunkId)
{
    u32 res = 0;

    res = bitfieldInsert(res, positionInChunk.x, 0, 6);
    res = bitfieldInsert(res, positionInChunk.y, 6, 6);
    res = bitfieldInsert(res, positionInChunk.z, 12, 6);

    res = bitfieldInsert(res, packNormalId(normal), 18, 3);

    res = bitfieldInsert(res, chunkId, 21, 11);

    return res;
}

struct UnpackUniqueFaceIdResult
{
    vec3  normal;
    uvec3 position_in_chunk;
    uint  chunk_id;
};

UnpackUniqueFaceIdResult unpackUniqueFaceId(u32 packed)
{
    UnpackUniqueFaceIdResult res;

    res.position_in_chunk.x = bitfieldExtract(packed, 0, 6);
    res.position_in_chunk.y = bitfieldExtract(packed, 6, 6);
    res.position_in_chunk.z = bitfieldExtract(packed, 12, 6);

    res.normal = unpackNormalId(bitfieldExtract(packed, 18, 3));

    res.chunk_id = bitfieldExtract(packed, 21, 11);

    return res;
}

void main()
{
    const u32 chunkId = 0;

    const Cube chunkBoundingCube = Cube(in_cube_negative_corner + 32, 64);

    const vec3 camera_position = GlobalData.camera_position.xyz;
    const vec3 dir             = normalize(in_world_position - GlobalData.camera_position.xyz);

    const Ray                camera_ray                    = Ray(camera_position, dir);
    const IntersectionResult WorldBoundingCubeIntersection = Cube_tryIntersectFast(chunkBoundingCube, camera_ray);

    if (!WorldBoundingCubeIntersection.intersection_occurred)
    {
        discard;
    }

    const vec3 box_corner_negative = in_cube_negative_corner;
    const vec3 box_corner_positive = in_cube_negative_corner + 64;

    const bool is_camera_inside_box = Cube_contains(chunkBoundingCube, camera_position);

    const vec3 traversalRayStart = is_camera_inside_box
                                     ? (camera_position - box_corner_negative)
                                     : (WorldBoundingCubeIntersection.maybe_hit_point - box_corner_negative);

    // TODO: hone this further and see if you can get rid of some bounds checks in the traversal shader
    const vec3 traversalRayEnd = traversalRayStart + dir * WorldBoundingCubeIntersection.t_far;

    const VoxelTraceResult result = traceDDARay(0, traversalRayStart, traversalRayEnd);

    vec3 worldStrikePosition = result.chunk_local_fragment_position + in_cube_negative_corner;

    if (!result.intersect_occur)
    {
        worldStrikePosition = Ray_at(camera_ray, WorldBoundingCubeIntersection.t_far);
    }

    const bool showTrace = false;

    if (showTrace)
    {
        // out_color = vec4(plasma_quintic(float(result.steps) / 64.0), 1.0);
    }
    else
    {
        if (!result.intersect_occur)
        {
            discard;
        }

        const u32 uniqueFaceId = packUniqueFaceId(result.voxel_normal, result.chunk_local_voxel_position, chunkId);
        const u32 kHashTableCapacity = 1U << 20U;
        const u32 kEmpty             = ~0u;

        const u32 startSlot = gpu_hashU32(uniqueFaceId) % kHashTableCapacity;

        for (uint i = 0; i < 32; ++i)
        {
            const u32 thisSlot = (startSlot + i) % kHashTableCapacity;

            const u32 prev = atomicCompSwap(in_voxel_hash_map[5].nodes[thisSlot].key, kEmpty, uniqueFaceId);

            if (prev == kEmpty)
            {
                in_voxel_hash_map[5].nodes[thisSlot].value = 47;
                break;
            }

            if (prev == uniqueFaceId)
            {
                break;
            }
        }

        out_position_and_id = vec4(worldStrikePosition, uintBitsToFloat(uniqueFaceId));
    }

    const vec4  clipPos = GlobalData.view_projection_matrix * vec4(worldStrikePosition, float(1.0));
    const float depth   = (clipPos.z / clipPos.w);
    gl_FragDepth        = showTrace ? max(depth, 0.000000000001) : depth;
}