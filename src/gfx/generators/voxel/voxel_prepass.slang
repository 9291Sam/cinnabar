#include "globals.slang"
#include "cube_ray.slang"
#include "voxel_faces.slang"
#include "voxel_tracing.slang"

const static Array<float3, 36> CUBE_TRIANGLE_LIST = {
    // Front face
    float3(-0.5f, -0.5f, 0.5f),
    float3(0.5f, -0.5f, 0.5f),
    float3(0.5f, 0.5f, 0.5f),
    float3(-0.5f, -0.5f, 0.5f),
    float3(0.5f, 0.5f, 0.5f),
    float3(-0.5f, 0.5f, 0.5f),

    // Back face
    float3(0.5f, -0.5f, -0.5f),
    float3(-0.5f, -0.5f, -0.5f),
    float3(-0.5f, 0.5f, -0.5f),
    float3(0.5f, -0.5f, -0.5f),
    float3(-0.5f, 0.5f, -0.5f),
    float3(0.5f, 0.5f, -0.5f),

    // Left face
    float3(-0.5f, -0.5f, -0.5f),
    float3(-0.5f, -0.5f, 0.5f),
    float3(-0.5f, 0.5f, 0.5f),
    float3(-0.5f, -0.5f, -0.5f),
    float3(-0.5f, 0.5f, 0.5f),
    float3(-0.5f, 0.5f, -0.5f),

    // Right face
    float3(0.5f, -0.5f, 0.5f),
    float3(0.5f, -0.5f, -0.5f),
    float3(0.5f, 0.5f, -0.5f),
    float3(0.5f, -0.5f, 0.5f),
    float3(0.5f, 0.5f, -0.5f),
    float3(0.5f, 0.5f, 0.5f),

    // Top face
    float3(-0.5f, 0.5f, 0.5f),
    float3(0.5f, 0.5f, 0.5f),
    float3(0.5f, 0.5f, -0.5f),
    float3(-0.5f, 0.5f, 0.5f),
    float3(0.5f, 0.5f, -0.5f),
    float3(-0.5f, 0.5f, -0.5f),

    // Bottom face
    float3(-0.5f, -0.5f, -0.5f),
    float3(0.5f, -0.5f, -0.5f),
    float3(0.5f, -0.5f, 0.5f),
    float3(-0.5f, -0.5f, -0.5f),
    float3(0.5f, -0.5f, 0.5f),
    float3(-0.5f, -0.5f, 0.5f)};

struct VertexOutput
{
    float3 world_position : COLOR0;
    float3 out_negative_corner : COLOR1;
    u32 chunk_id : COLOR2;
    float4 position : SV_Position;
}

[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    const uint chunkId = vertexIndex / 36;
    const f32 chunk_size      = in_const_global_chunk_data[SBO_CHUNK_DATA][chunkId].chunk_location.getChunkWidthUnits();

    const float3 box_corner_negative = in_const_global_chunk_data[SBO_CHUNK_DATA][chunkId].chunk_location.getChunkNegativeCornerLocation();
    const float3 center_location = box_corner_negative + chunk_size / 2.0;
    const float3 box_corner_positive = box_corner_negative + float3(chunk_size);

    const float3 cube_vertex    = CUBE_TRIANGLE_LIST[vertexIndex % 36];
    const float3 world_location = center_location + chunk_size * cube_vertex;

    VertexOutput o;

    o.position            = mul(global_gpu_data[UBO_GLOBAL_DATA].view_projection_matrix, float4(world_location, 1.0));
    o.world_position      = world_location;
    o.chunk_id = chunkId;
    o.out_negative_corner = box_corner_negative;
    
    return o;
}

struct FragmentOutput
{
    float4 out_position_and_id : COLOR0;
    f32 depth : SV_DepthGreaterEqual;
};

[shader("fragment")]
FragmentOutput fragmentMain(float3 world_position : COLOR0, float3 in_cube_negative_corner : COLOR1, u32 chunkId)
{
    // discard;
    if (in_global_chunk_data[SBO_CHUNK_DATA][chunkId].offset == ~0u)
    {
        discard;
    }

    const u32 chunk_size = in_const_global_chunk_data[SBO_CHUNK_DATA][chunkId].chunk_location.getChunkWidthUnits();
    const u32 chunk_voxel_size = in_const_global_chunk_data[SBO_CHUNK_DATA][chunkId].chunk_location.getVoxelSizeUnits();

    const Cube chunkBoundingCube = Cube(in_cube_negative_corner + chunk_size/2, chunk_size);
    const float3 camera_position = global_gpu_data[UBO_GLOBAL_DATA].camera_position.xyz;
    const float3 dir             = normalize(world_position - camera_position);

    const Ray                camera_ray                    = Ray(camera_position, dir);
    const IntersectionResult WorldBoundingCubeIntersection = chunkBoundingCube.tryIntersect(camera_ray);

    if (!WorldBoundingCubeIntersection.intersection_occurred)
    {
        discard;
    }

    const float3 box_corner_negative = in_cube_negative_corner;
    const float3 box_corner_positive = in_cube_negative_corner + chunk_size;

    const bool is_camera_inside_box = chunkBoundingCube.contains(camera_position);


    const float3 traversalRayStart_local = is_camera_inside_box
    ? (camera_position - box_corner_negative)
    : (WorldBoundingCubeIntersection.maybe_hit_point - box_corner_negative);

const float3 traversalRayEnd_local = traversalRayStart_local + dir * WorldBoundingCubeIntersection.t_far;

    
    const float3 traversalRayStart = (traversalRayStart_local / chunk_size) * 64.0;
    const float3 traversalRayEnd = (traversalRayEnd_local / chunk_size) * 64.0;


    const VoxelTraceResult result = traceDDARay(chunkId, traversalRayStart, traversalRayEnd);

    float3 worldStrikePosition = result.chunk_local_fragment_position * chunk_voxel_size + in_cube_negative_corner;
  
    if (!result.intersect_occur)
    {
        discard;
    }

    FragmentOutput out;


    const u32 uniqueFaceId = UnpackUniqueFaceIdResult(result.voxel_normal, chunkId).pack();

    out.out_position_and_id = float4(worldStrikePosition, reinterpret<f32>(uniqueFaceId));


    const float4  clipPos = mul(global_gpu_data[UBO_GLOBAL_DATA].view_projection_matrix, float4(worldStrikePosition, float(1.0)));
    const f32 depth   = (clipPos.z / clipPos.w);
    out.depth        = max(depth, 0.000000000001);

    return out;
}