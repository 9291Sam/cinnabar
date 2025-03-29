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

VoxelTraceResult traceSingleRayInChunk(Ray ray)
{
    return traceDDARay(0, ray.origin, ray.origin + ray.direction * 128);
}

// PCG (permuted congruential generator). Thanks to:
// www.pcg-random.org and www.shadertoy.com/view/XlGcRh
uint NextRandom(inout uint state)
{
    state       = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result      = (result >> 22) ^ result;
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
    float rho   = sqrt(-2 * log(RandomValue(state)));
    return rho * cos(theta);
}

// Calculate a random direction
vec3 RandomDirection(inout uint state)
{
    // Thanks to https://math.stackexchange.com/a/1585996
    float x = RandomValueNormalDistribution(state);
    float y = RandomValueNormalDistribution(state);
    float z = RandomValueNormalDistribution(state);
    return normalize(vec3(x, y, z));
}

vec3 RandomHemisphericalDirection(vec3 normal, inout uint rngState)
{
    vec3 dir = RandomDirection(rngState);

    return dir * sign(dot(normal, dir));
}

// vec3 calculateColor(
//     vec3 chunkStrikePosition, PBRVoxelMaterial firstMaterial, UnpackUniqueFaceIdResult unpacked, uint rngState)
// {
//     // const GpuRaytracedLight light = in_raytraced_lights[1].lights[0];

//     const vec3 worldStrikePosition = chunkStrikePosition + vec3(-16.0);

// vec3 calculatedColor = calculatePixelColor(
//     worldStrikePosition,
//     unpacked.normal,
//     normalize(GlobalData.camera_position.xyz - worldStrikePosition),
//     normalize(light.position_and_half_intensity_distance.xyz - worldStrikePosition),
//     light,
//     firstMaterial);

// const VoxelTraceResult shadowResult = traceDDARay(
//     0, chunkStrikePosition + 0.05 * unpacked.normal, light.position_and_half_intensity_distance.xyz -
//     vec3(-16.0));

// if (shadowResult.intersect_occur)
// {
//     calculatedColor = vec3(0);
// }

// return calculatedColor;

// vec3 incomingLight = vec3(0.0);
// vec3 rayColor      = vec3(firstMaterial.albedo_roughness.xyz);
// Ray  ray;
// ray.origin    = chunkStrikePosition + 0.05 * unpacked.normal;
// ray.direction = RandomHemisphericalDirection(unpacked.normal, rngState);

// const u32 maxBounceCount = 3;

// for (int i = 0; i < maxBounceCount; ++i)
// {
//     VoxelTraceResult traceResult = traceSingleRayInChunk(ray);

//     if (traceResult.intersect_occur)
//     {
//         ray.origin    = traceResult.chunk_local_fragment_position + 0.05 * traceResult.voxel_normal;
//         ray.direction = RandomHemisphericalDirection(traceResult.voxel_normal, rngState);

//         const PBRVoxelMaterial material     = traceResult.material;
//         const vec3             emittedLight = material.emission_metallic.xyz / 64.0;
//         incomingLight += emittedLight * rayColor;
//         rayColor *= (material.albedo_roughness.xyz);
//     }
//     else
//     {
//         break;
//     }
// }

// return incomingLight;
// }

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

        // const vec3 calculatedColor = calculateColor(
        //     worldStrikePosition - vec3(-16.0),
        //     result.material,
        //     unpackUniqueFaceId(uniqueFaceId),
        //     gpu_hashCombineU32(gpu_hashU32(uniqueFaceId), uint(gl_FragCoord.x + gl_FragCoord.y * 10293)));

        out_position_and_id = vec4(worldStrikePosition, uintBitsToFloat(uniqueFaceId));
    }

    const vec4  clipPos = GlobalData.view_projection_matrix * vec4(worldStrikePosition, float(1.0));
    const float depth   = (clipPos.z / clipPos.w);
    gl_FragDepth        = showTrace ? max(depth, 0.000000000001) : depth;
}