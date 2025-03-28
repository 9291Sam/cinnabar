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

#include "voxel_tracing.glsl"

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_cube_negative_corner;

layout(location = 0) out vec4 out_position_and_id;

layout(depth_greater) out float gl_FragDepth;

void main()
{
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

        const u32 chunkId = 0;

        u32 outGlobalId = 0;

        outGlobalId = bitfieldInsert(outGlobalId, result.chunk_local_voxel_position.x, 0, 6);
        outGlobalId = bitfieldInsert(outGlobalId, result.chunk_local_voxel_position.y, 6, 6);
        outGlobalId = bitfieldInsert(outGlobalId, result.chunk_local_voxel_position.z, 12, 6);
        outGlobalId = bitfieldInsert(outGlobalId, chunkId, 18, 14);

        out_position_and_id = vec4(worldStrikePosition, uintBitsToFloat(outGlobalId));

        // const GpuRaytracedLight light = in_raytraced_lights[LIGHT_BUFFER_OFFSET].lights[0];

        // vec3 calculatedColor = calculatePixelColor(
        //     worldStrikePosition,
        //     result.voxel_normal,
        //     normalize(camera_position - worldStrikePosition),
        //     normalize(light.position_and_half_intensity_distance.xyz - worldStrikePosition),
        //     light.position_and_half_intensity_distance.xyz,
        //     light.color_and_power.xyz,
        //     result.material.albedo_roughness.xyz,
        //     result.material.emission_metallic.w,
        //     result.material.albedo_roughness.w,
        //     light.color_and_power.w,
        //     light.position_and_half_intensity_distance.w);

        // const VoxelTraceResult shadowResult = traceDDARay(
        //     0,
        //     result.chunk_local_fragment_position + 0.05 * result.voxel_normal,
        //     light.position_and_half_intensity_distance.xyz - box_corner_negative);

        // if (shadowResult.intersect_occur)
        // {
        //     calculatedColor = vec3(0);
        // }

        // const ivec3 pos = result.chunk_local_voxel_position;

        // u32 hash = gpu_hashCombineU32(pos.x, pos.y);
        // hash     = gpu_hashCombineU32(hash, pos.z);

        // const vec3 c = vec3(
        //     gpu_randomUniformFloat(hash - 84492),
        //     gpu_randomUniformFloat(hash - 8478193),
        //     gpu_randomUniformFloat(hash + 32));

        // out_color = vec4(calculatedColor, 1.0);
    }

    // const vec4  clipPos = GlobalData.view_projection_matrix * vec4(worldStrikePosition, float(1.0));
    // const float depth   = (clipPos.z / clipPos.w);
    // gl_FragDepth        = showTrace ? max(depth, 0.000000000001) : depth;
}