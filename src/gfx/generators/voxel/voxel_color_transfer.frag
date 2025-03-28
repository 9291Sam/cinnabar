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
    const u32  uniqueFaceId        = floatBitsToUint(rawPixelData.w);

    const u32 startSlot = gpu_hashU32(uniqueFaceId) % kHashTableCapacity;

    u32  loaded;
    uint i;
    for (i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % kHashTableCapacity;

        const u32 thisSlotKey = in_voxel_hash_map[5].nodes[thisSlot].key;

        if (thisSlotKey == uniqueFaceId)
        {
            loaded = in_voxel_hash_map[5].nodes[thisSlot].value;
            break;
        }
    }

    out_color = vec4(unpackUnorm4x8(loaded).xyz, 1.0);
}