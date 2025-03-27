#ifndef SRC_GFX_SHADER_COMMON_VOXEL_MATERIAL_GLSL
#define SRC_GFX_SHADER_COMMON_VOXEL_MATERIAL_GLSL

#ifndef VOXEL_MATERIALS_OFFSET
#error "VOXEL_MATERIALS_OFFSET must be defined"
#endif // VOXEL_MATERIALS_OFFSET

#ifndef LIGHT_BUFFER_OFFSET
#error "LIGHT_BUFFER_OFFSET must be defined"
#endif // LIGHT_BUFFER_OFFSET

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

layout(set = 0, binding = 4) readonly buffer GpuRaytracedLightStorage
{
    GpuRaytracedLight lights[];
}
in_raytraced_lights[];

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

#endif // SRC_GFX_SHADER_COMMON_VOXEL_MATERIAL_GLSL
