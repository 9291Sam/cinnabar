#include "globals.slang"

typedef uint32_t u32;

[shader("vertex")]
float4 vertexMain(uint vertexIndex : SV_VertexID) : SV_Position
{
    const float4 positions[3] = float4[3](float4(-1.0, -1.0, 0.0, 1.0), float4(-1.0, 3.0, 0.0, 1.0), float4(3.0, -1.0, 0.0, 1.0));

    return positions[vertexIndex];
}



struct GpuColorHashMapNode
{
    u32 key;
    u32 r_1024;
    u32 g_1024;
    u32 b_1024;
    u32 samples;
};

u32 gpu_hashU32(u32 x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bbU;
    x ^= x >> 11;
    x *= 0xac4c1b51U;
    x ^= x >> 15;
    x *= 0x31848babU;
    x ^= x >> 14;

    return x;
}


[[vk::binding(4, 0)]] StructuredBuffer<GpuColorHashMapNode> in_voxel_hash_map[];

[format("rgba32f")]
[[vk::binding(2, 0)]] RWTexture2D<float4> voxel_render_target[];


[shader("fragment")]
float4 fragmentMain(float4 pos : SV_Position) : COLOR0
{

const u32 kHashTableCapacity = 1u << 20u;
const u32 kEmpty             = ~0u;


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