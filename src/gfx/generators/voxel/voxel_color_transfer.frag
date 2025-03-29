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

// #define BRICK_MAPS_OFFSET        in_push_constants.chunk_brick_maps_buffer_offset
// #define LIGHT_BUFFER_OFFSET      in_push_constants.lights_buffer_offset
// #define VISIBILITY_BRICKS_OFFSET in_push_constants.visibility_bricks_buffer_offset
// #define MATERIAL_BRICKS_OFFSET   in_push_constants.material_bricks_buffer_offset
// #define VOXEL_MATERIALS_OFFSET   in_push_constants.voxel_material_buffer_offset
// #define VOXEL_HASH_MAP_OFFSET    5
#define VOXEL_HASH_MAP_OFFSET in_push_constants.visible_voxel_image_offset

#include "voxel_faces.glsl"

void main()
{
    const vec4 rawPixelData =
        imageLoad(visible_voxel_image[in_push_constants.visible_voxel_image_offset], ivec2(floor(gl_FragCoord.xy)));
    const vec3 worldStrikePosition = rawPixelData.xyz;
    const u32  uniqueFaceId        = floatBitsToUint(rawPixelData.w);

    const u32 startSlot = gpu_hashU32(uniqueFaceId) % kHashTableCapacity;

    vec3 loaded;
    uint i;
    for (i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % kHashTableCapacity;

        const u32 thisSlotKey = in_voxel_hash_map[5].nodes[thisSlot].key;

        if (thisSlotKey == uniqueFaceId)
        {
            loaded = vec3(
                         in_voxel_hash_map[5].nodes[thisSlot].r_1024,
                         in_voxel_hash_map[5].nodes[thisSlot].g_1024,
                         in_voxel_hash_map[5].nodes[thisSlot].b_1024)
                   / (float(in_voxel_hash_map[5].nodes[thisSlot].samples) * 1024);

            break;
        }
    }

    out_color = vec4(loaded, 1.0);
}