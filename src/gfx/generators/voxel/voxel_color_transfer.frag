#version 460

#include "intersectables.glsl"
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

    const uvec3 positionInChunk = uvec3(
        bitfieldExtract(inChunkInfo, 0, 6), bitfieldExtract(inChunkInfo, 6, 6), bitfieldExtract(inChunkInfo, 12, 6));

    const u32 chunkId = bitfieldExtract(inChunkInfo, 18, 14);

    const Cube               voxel = Cube(vec3(positionInChunk) + 0.5 - 16, 18.0);
    const IntersectionResult res   = Cube_tryIntersectFast(
        voxel, Ray(GlobalData.camera_position.xyz, normalize(worldStrikePosition - GlobalData.camera_position.xyz)));

    if (inChunkInfo != ~0u)
    {
        const u32 face = getHashOfFace(res.maybe_normal, ivec3(positionInChunk), chunkId);

        const vec3 c = vec3(
            gpu_randomUniformFloat(face - 84492),
            gpu_randomUniformFloat(face - 8478193),
            gpu_randomUniformFloat(face + 32));

        out_color = vec4(worldStrikePosition, 1.0);

        // DoLoadResult result = doLoad(getHashOfFace(res.maybe_normal, ivec3(positionInChunk), chunkId));

        // if (result.found)
        // {
        //     out_color = vec4(result.maybe_loaded_color, 1.0);
        // }
        // else
        // {
        //     discard;
        // }
    }
    else
    {
        discard;
    }
}