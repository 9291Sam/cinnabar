#pragma once

#include "globals.slang"
#include "shared_data_structures.slang"

struct PBRVoxelMaterial
{
    // xyz - linear rgb color
    // w - roughness
    float4 albedo_roughness;
    // xyz - emissive color (values over 1 indicate more than 1 unit of brightness)
    // w - metallic
    float4 emission_metallic;
};


[[vk::binding(4)]] RWStructuredBuffer<PBRVoxelMaterial> in_voxel_materials[];
[[vk::binding(4)]] RWStructuredBuffer<GpuRaytracedLight> in_raytraced_lights[];



float distributionGGX(float3 N, float3 H, float roughness)
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

float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return geometrySchlickGGX(max(dot(N, L), 0.0), roughness) * geometrySchlickGGX(max(dot(N, V), 0.0), roughness);
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 calculatePixelColor(
    float3              worldPos,
    float3              N,
    float3              V,
    float3              L,
    GpuRaytracedLight light,
    PBRVoxelMaterial  material,
    bool              includeSpecular,
    bool              includeEmissive)
{
    // HACK!
    material.albedo_roughness.w = max(material.albedo_roughness.w, material.emission_metallic.w / 2);

    float3 H = normalize(V + L);

    float3  F0  = lerp(float3(0.04), pow(material.albedo_roughness.xyz, float3(2.2)), material.emission_metallic.w);
    float NDF = distributionGGX(N, H, material.albedo_roughness.w);
    float G   = geometrySmith(N, V, L, material.albedo_roughness.w);
    float3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float3  kD  = float3(1.0) - F;

    kD *= 1.0 - material.emission_metallic.w;

    float3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3  specular    = numerator / max(denominator, 0.001);

    if (!includeSpecular)
    {
        specular = float3(0.0);
    }

    float NdotL = max(dot(N, L), 0.0);

    // Custom light intensity calculation with exponential falloff
    float distanceToLight = length(light.position_and_half_intensity_distance.xyz - worldPos);
    float lightIntensity =
        pow(2.0, -distanceToLight / light.position_and_half_intensity_distance.w) * light.color_and_power.w;
    lightIntensity = max(lightIntensity, 0.0);

    // Calculate final color with custom light intensity
    float3 color = light.color_and_power.xyz * (kD * pow(material.albedo_roughness.xyz, float3(2.2)) / PI + specular)
               * (NdotL / distanceToLight) * lightIntensity;

    if (includeEmissive)
    {
        color += material.emission_metallic.xyz / 8.0;
    }

    return color;
}