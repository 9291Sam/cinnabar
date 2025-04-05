#include "globals.slang"
#include "voxel_faces.slang"
#include "shared_data_structures.slang"


[shader("vertex")]
float4 vertexMain(uint vertexIndex : SV_VertexID) : SV_Position
{
    const float4 positions[3] = float4[3](float4(-1.0, -1.0, 0.0, 1.0), float4(-1.0, 3.0, 0.0, 1.0), float4(3.0, -1.0, 0.0, 1.0));

    return positions[vertexIndex];
}


[shader("fragment")]
float4 fragmentMain(float4 pos : SV_Position) : COLOR0
{
    
    float4 rawPixelData = voxel_render_target[SIO_VOXEL_RENDER_TARGET].Load(int2(floor(pos.xy)));

    const float3 worldStrikePosition = rawPixelData.xyz;
    const u32  uniqueFaceId        = reinterpret<u32>(rawPixelData.w);
    const u32 startSlot = gpu_hashU32(uniqueFaceId) % kHashTableCapacity;

    float3 loaded;
    u32  i;
    for (i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % kHashTableCapacity;

        const u32 thisSlotKey = in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].key;

        if (thisSlotKey == uniqueFaceId)
        {
            loaded = float3(
                         in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].r_1024,
                         in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].g_1024,
                         in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].b_1024)
                   / (float(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].samples) * 1024);

            break;
        }
    }

    return float4(loaded, 1.0);
}
