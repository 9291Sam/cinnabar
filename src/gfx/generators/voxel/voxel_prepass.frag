#version 460

#include "intersectables.glsl"
#include "voxel_faces.glsl"
#include "voxel_tracing.glsl"

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_cube_negative_corner;

layout(location = 0) out vec4 out_position_and_id;

layout(depth_greater) out float gl_FragDepth;

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

        out_position_and_id = vec4(worldStrikePosition, uintBitsToFloat(uniqueFaceId));
    }

    const vec4  clipPos = GlobalData.view_projection_matrix * vec4(worldStrikePosition, float(1.0));
    const float depth   = (clipPos.z / clipPos.w);
    gl_FragDepth        = showTrace ? max(depth, 0.000000000001) : depth;
}