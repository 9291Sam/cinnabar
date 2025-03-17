#version 460

#include "globals.glsl"
#include "intersectables.glsl"
#include "misc.glsl"
#include "types.glsl"

layout(push_constant) uniform PushConstants
{
    uint chunk_brick_maps_buffer_offset;
    uint visibility_bricks_buffer_offset;
    uint material_bricks_buffer_offset;
    uint voxel_material_buffer_offset;
}
in_push_constants;

#define BRICK_MAPS_OFFSET        in_push_constants.chunk_brick_maps_buffer_offset
#define VISIBILITY_BRICKS_OFFSET in_push_constants.visibility_bricks_buffer_offset
#define MATERIAL_BRICKS_OFFSET   in_push_constants.material_bricks_buffer_offset
#define VOXEL_MATERIALS_OFFSET   in_push_constants.voxel_material_buffer_offset

#include "voxel_tracing.glsl"

layout(location = 0) in vec3 in_uvw;
layout(location = 1) in vec3 in_world_position;
layout(location = 2) in vec3 in_cube_corner_location;

layout(location = 0) out vec4 out_color;

layout(depth_greater) out float gl_FragDepth;

void main()
{
    const Cube chunkBoundingCube = Cube(in_cube_corner_location + 32, 64);

    const vec3 camera_position = GlobalData.camera_position.xyz;
    const vec3 dir             = normalize(in_world_position - GlobalData.camera_position.xyz);

    const Ray                camera_ray                    = Ray(camera_position, dir);
    const IntersectionResult WorldBoundingCubeIntersection = Cube_tryIntersectFast(chunkBoundingCube, camera_ray);

    if (!WorldBoundingCubeIntersection.intersection_occurred)
    {
        discard;
    }

    const vec3 box_corner_negative = in_cube_corner_location;
    const vec3 box_corner_positive = in_cube_corner_location + 64;

    const bool is_camera_inside_box = Cube_contains(chunkBoundingCube, camera_position);

    const vec3 traversalRayStart = is_camera_inside_box
                                     ? (camera_position - box_corner_negative)
                                     : (WorldBoundingCubeIntersection.maybe_hit_point - box_corner_negative);

    // TODO: hone this further and see if you can get rid of some bounds checks in the traversal shader
    const vec3 traversalRayEnd = traversalRayStart + dir * (WorldBoundingCubeIntersection.t_far + (8 * 1.73205080757));

    const VoxelTraceResult result = traceDDARay(0, traversalRayStart, traversalRayEnd);

    vec3 worldStrikePosition = result.chunk_local_fragment_position + in_cube_corner_location;

    if (!result.intersect_occur)
    {
        worldStrikePosition = Ray_at(camera_ray, WorldBoundingCubeIntersection.t_far);
    }

    const bool showTrace = false;

    if (showTrace)
    {
        out_color = vec4(plasma_quintic(float(result.steps) / 64.0), 1.0);
    }
    else
    {
        if (!result.intersect_occur)
        {
            discard;
        }

        const GpuRaytracedLight light = GpuRaytracedLight(vec4(-22.0, 42.434, -9.32, 8.0), vec4(1.0, 1.0, 1.0, 4.0));

        const CalculatedLightPower power =
            newLightPower(camera_position, worldStrikePosition, result.voxel_normal, light, result.material);

        const vec3 calculatedColor = result.material.diffuse_color.xyz * power.diffuse_strength
                                   + result.material.specular_color.xyz * power.specular_strength;

        out_color = vec4(calculatedColor, 1.0);
    }

    const vec4  clipPos = GlobalData.view_projection_matrix * vec4(worldStrikePosition, float(1.0));
    const float depth   = (clipPos.z / clipPos.w);
    gl_FragDepth        = showTrace ? max(depth, 0.000000000001) : depth;
}