#ifndef SRC_GFX_SHADER_COMMON_VOXEL_MATERIAL_GLSL
#define SRC_GFX_SHADER_COMMON_VOXEL_MATERIAL_GLSL

#ifndef VOXEL_MATERIALS_OFFSET
#error "VOXEL_MATERIALS_OFFSET must be defined"
#endif // VOXEL_MATERIALS_OFFSET

struct PBRVoxelMaterial
{
    // xyz - linear rgb color
    // w - roughness
    vec4 albedo_roughness;
    // xyz - emissive color (values over 1 indicate more than 1 unit of brightness)
    // w - metallic
    vec4 emission_metallic;
};

PBRVoxelMaterial PBRVoxelMaterial_getUninitialized()
{
    PBRVoxelMaterial m;

    return m;
}

layout(set = 0, binding = 4) readonly buffer PBRVoxelMaterialsStorage
{
    PBRVoxelMaterial materials[];
}
in_voxel_materials[];

struct GpuRaytracedLight
{
    vec4 position_and_half_intensity_distance;
    vec4 color_and_power;
};

vec3 calculateLightColor(
    vec3 camera_position, vec3 voxel_position, vec3 voxel_normal, GpuRaytracedLight light, PBRVoxelMaterial material)
{
    // Basic light direction and view vectors
    vec3 light_vector = light.position_and_half_intensity_distance.xyz - voxel_position;
    vec3 light_dir    = normalize(light_vector);
    vec3 view_dir     = normalize(camera_position - voxel_position);
    vec3 half_vector  = normalize(light_dir + view_dir);

    // Material properties
    vec3  albedo    = material.albedo_roughness.xyz;
    float roughness = material.albedo_roughness.w;
    float metallic  = material.emission_metallic.w;
    vec3  emission  = material.emission_metallic.xyz;

    // Dot products
    float n_dot_l = max(dot(voxel_normal, light_dir), 0.0);
    float n_dot_h = max(dot(voxel_normal, half_vector), 0.0);
    float n_dot_v = max(dot(voxel_normal, view_dir), 0.0);
    float h_dot_v = max(dot(half_vector, view_dir), 0.0);

    // Light color and intensity
    vec3  light_color       = light.color_and_power.xyz;
    float distance_to_light = length(light_vector);
    float light_power       = light.color_and_power.w;
    float light_radius      = light.position_and_half_intensity_distance.w;

    // Exponential light falloff
    float light_intensity = pow(2.0, -distance_to_light / light_radius) * light_power;
    light_intensity       = max(light_intensity, 0.0);

    // Specular highlight for metals
    float roughness_sq      = roughness * roughness;
    float spec_distribution = roughness_sq / (3.14159 * pow((n_dot_h * n_dot_h * (roughness_sq - 1.0) + 1.0), 2.0));

    // Fresnel effect
    vec3 base_reflectivity = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel           = base_reflectivity + (1.0 - base_reflectivity) * pow(1.0 - h_dot_v, 5.0);

    // Geometry approximation
    float k        = (roughness + 1.0) * (roughness + 1.0) * 0.125;
    float geo_l    = n_dot_l / (n_dot_l * (1.0 - k) + k);
    float geo_v    = n_dot_v / (n_dot_v * (1.0 - k) + k);
    float geometry = geo_l * geo_v;

    // Specular calculation
    float specular_intensity = (spec_distribution * geometry * fresnel.r) / max(4.0 * n_dot_v * n_dot_l, 0.001);

    // Base color contribution
    vec3 color_contribution = albedo * n_dot_l * light_color * light_intensity;

    // Enhanced metallic rendering
    vec3 metal_contribution = color_contribution * (1.0 + specular_intensity * 2.0);

    // Metallic scaling and specular enhancement
    vec3 final_color = mix(color_contribution, metal_contribution, metallic);

    // Add emission
    return final_color + emission;
}

#endif // SRC_GFX_SHADER_COMMON_VOXEL_MATERIAL_GLSL
