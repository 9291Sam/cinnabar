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

layout(location = 0) in vec3 in_uvw;
layout(location = 1) in vec3 in_world_position;
layout(location = 2) in vec3 in_cube_corner_location;

layout(location = 0) out vec4 out_color;

layout(depth_greater) out float gl_FragDepth;

#define PI 3.14159265358979

float distributionGGX(vec3 N, vec3 H, float roughness)
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

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    return geometrySchlickGGX(max(dot(N, L), 0.0), roughness) * geometrySchlickGGX(max(dot(N, V), 0.0), roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 calculatePixelColor(
    vec3  worldPos,
    vec3  N,
    vec3  V,
    vec3  L,
    vec3  lightPos,
    vec3  lightColor,
    vec3  albedo,
    float metallic,
    float roughness,
    float lightPower,
    float lightRadius)
{
    // HACK!
    roughness = max(roughness, metallic / 2);

    vec3 H = normalize(V + L);

    vec3  F0  = mix(vec3(0.04), pow(albedo, vec3(2.2)), metallic);
    float NDF = distributionGGX(N, H, roughness);
    float G   = geometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3  kD  = vec3(1.0) - F;

    kD *= 1.0 - metallic;

    // return D

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3  specular    = numerator / max(denominator, 0.001);

    float NdotL = max(dot(N, L), 0.0);

    // Custom light intensity calculation with exponential falloff
    float distanceToLight = length(lightPos - worldPos);
    float lightIntensity  = pow(2.0, -distanceToLight / lightRadius) * lightPower;
    lightIntensity        = max(lightIntensity, 0.0);

    // Calculate final color with custom light intensity
    vec3 color =
        lightColor * (kD * pow(albedo, vec3(2.2)) / PI + specular) * (NdotL / distanceToLight) * lightIntensity;

    return color;
}

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
    const vec3 traversalRayEnd = traversalRayStart + dir * WorldBoundingCubeIntersection.t_far;

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

        const GpuRaytracedLight light = in_raytraced_lights[LIGHT_BUFFER_OFFSET].lights[0];

        vec3 calculatedColor = calculatePixelColor(
            worldStrikePosition,
            result.voxel_normal,
            normalize(camera_position - worldStrikePosition),
            normalize(light.position_and_half_intensity_distance.xyz - worldStrikePosition),
            light.position_and_half_intensity_distance.xyz,
            light.color_and_power.xyz,
            result.material.albedo_roughness.xyz,
            result.material.emission_metallic.w,
            result.material.albedo_roughness.w,
            light.color_and_power.w,
            light.position_and_half_intensity_distance.w);

        const VoxelTraceResult shadowResult = traceDDARay(
            0,
            result.chunk_local_fragment_position + 0.05 * result.voxel_normal,
            light.position_and_half_intensity_distance.xyz - box_corner_negative);

        if (shadowResult.intersect_occur)
        {
            calculatedColor = vec3(0);
        }

        out_color = vec4(calculatedColor, 1.0);
    }

    const vec4  clipPos = GlobalData.view_projection_matrix * vec4(worldStrikePosition, float(1.0));
    const float depth   = (clipPos.z / clipPos.w);
    gl_FragDepth        = showTrace ? max(depth, 0.000000000001) : depth;
}