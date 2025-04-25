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

    float3 loaded;
    // u32  i;
    // for (i = 0; i < 32; ++i)
    // {
    //     const u32 thisSlot = (startSlot + i) % faceHashTableCapacity;

    //     const u32 thisSlotKey = in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].key;

    //     if (thisSlotKey == uniqueFaceId)
    //     {
    //         loaded = float3(
    //                      in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].r_1024,
    //                      in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].g_1024,
    //                      in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].b_1024)
    //                / (float(in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot].samples) * 1024);

    //         break;
    //     }
    // }

    const UnpackUniqueFaceIdResult unpacked = UnpackUniqueFaceIdResult::unpack(uniqueFaceId);
    const uint3 bC = unpacked.position_in_chunk / 8;
    const uint3 bP = unpacked.position_in_chunk % 8;


    const PBRVoxelMaterial material = getMaterialFromPosition( 
        in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].brick_map[bC.x][bC.y][bC.z], unpacked.chunk_id, bP);
 
    const float3 calculatedColor = calculateWorldColor(worldStrikePosition, material, unpacked,
            gpu_hashCombineU32(
                gpu_hashU32(reinterpret<u32>(pos.x)),
                gpu_hashU32(reinterpret<u32>(pos.y) + reinterpret<u32>(global_gpu_data[UBO_GLOBAL_DATA].time_alive)),
            ), 
            bool(global_gpu_data[UBO_GLOBAL_DATA].bool_enable_reflections),
            bool(global_gpu_data[UBO_GLOBAL_DATA].bool_enable_global_illumination));

    loaded = calculatedColor;

    return float4(loaded, 1.0);
}
