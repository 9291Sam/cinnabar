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
    
    const float4 rawPixelData = voxel_render_target[SIO_VOXEL_RENDER_TARGET].Load(int2(floor(pos.xy)));
    const float3 worldStrikePosition = rawPixelData.xyz;
    const u32  uniqueFaceId        = reinterpret<uint>(rawPixelData.w);
    const UnpackUniqueFaceIdResult unpacked = UnpackUniqueFaceIdResult::unpack(uniqueFaceId);

    const float chunkVoxelSize = in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].chunk_location.getVoxelSizeUnits();

        
    const uint3 positionInChunk = uint3(-0.01 * unpacked.normal + (worldStrikePosition - float3(in_global_chunk_data[SBO_CHUNK_DATA][unpacked.chunk_id].getWorldChunkCorner())) / chunkVoxelSize);


    const u32 hash = hashFaceId(unpacked, positionInChunk) ;
    const u32 startSlot = hashFaceId(unpacked, positionInChunk) % faceHashTableCapacity;

    float3 loaded = float3(0);
    bool  had_color_calculated = false;
    for (u32 i = 0; i < 32; ++i)
    {
        const u32 thisSlot = (startSlot + i) % faceHashTableCapacity;

        const GpuColorHashMapNode node = in_voxel_hash_map[SBO_FACE_HASH_MAP][thisSlot];

        had_color_calculated = node.samples_high_bit_is_bool_is_reflective != 0;

        if (node.key == hash && had_color_calculated)
        {
            loaded = float3(
                         node.r_1024,
                         node.g_1024,
                         node.b_1024)
                   / (f32(node.getSamples()) * 1024);

            break;
        }
    }

    const float3 cameraPos = global_gpu_data[UBO_GLOBAL_DATA].camera_position.xyz;
    const float3 dir = normalize(worldStrikePosition - cameraPos);

    const Ray r = Ray(cameraPos, dir);

    const VoxelTraceResult res = traceWorld(r, 256);

    return float4(res.material.albedo_roughness.xyz, 1.0);
}
