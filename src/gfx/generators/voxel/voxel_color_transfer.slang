#include "globals.slang"
#include "voxel_faces.slang"
#include "shared_data_structures.slang"
#include "voxel_tracing.slang" 


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
    const u32 startSlot = gpu_hashU32(uniqueFaceId) % faceHashTableCapacity;

    float3 loaded = float3(0);
    bool  had_color_calculated = false;
    for (u32 i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % faceHashTableCapacity;

        const GpuColorHashMapNode node = in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot];

        had_color_calculated = node.samples_high_bit_is_bool_is_reflective != 0;

        if (node.key == uniqueFaceId && had_color_calculated)
        {
            loaded = float3(
                         node.r_1024,
                         node.g_1024,
                         node.b_1024)
                   / (float(node.getSamples()) * 1024);

            break;
        }
    }

    // if (!had_color_calculated)
    // {
    //     const UnpackUniqueFaceIdResult unpacked = UnpackUniqueFaceIdResult::unpack(uniqueFaceId);

    //     const uint3 positionInChunk = uint3(-0.01 * unpacked.normal + worldStrikePosition - float3(in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].getWorldChunkCorner()));

    //     const uint3 bC = positionInChunk / 8;
    //     const uint3 bP = positionInChunk % 8;

    //     const int2 workgroupSize = int2(8, 4);
    //     // const int2 workgroupSize = int2(1, 1);


    //     const PBRVoxelMaterial material = getMaterialFromPosition( 
    //         in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].brick_map[bC.x][bC.y][bC.z], unpacked.chunk_id, bP);
    
    //     const float3 calculatedColor = calculateWorldColor(worldStrikePosition, material, unpacked,
    //         reinterpret<u32>(global_gpu_data[UBO_GLOBAL_DATA].time_alive) + u32(pos.x) / workgroupSize.x + (u32(pos.y) / workgroupSize.y)  * 39384,
    //         bool(global_gpu_data[UBO_GLOBAL_DATA].bool_enable_reflections));

    //     loaded = calculatedColor;
    // }


    return float4(loaded, 1.0);
}
