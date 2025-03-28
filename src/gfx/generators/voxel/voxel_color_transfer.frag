#version 460

#include "intersectables.glsl"
#include "misc.glsl"
#include "types.glsl"

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants
{
    uint face_hash_map_offset;
    uint visible_voxel_image_offset;
}
in_push_constants;

layout(set = 0, binding = 2, rgba32f) uniform image2D visible_voxel_image[];

#define VOXEL_HASH_MAP_OFFSET in_push_constants.visible_voxel_image_offset

#include "voxel_faces.glsl"

void main()
{
    const vec4 rawPixelData =
        imageLoad(visible_voxel_image[in_push_constants.visible_voxel_image_offset], ivec2(floor(gl_FragCoord.xy)));
    const vec3 worldStrikePosition = rawPixelData.xyz;
    const u32  inChunkInfo         = floatBitsToUint(rawPixelData.w);

    out_color = vec4(worldStrikePosition, 1.0);
}