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
    float4 position : SV_Position;
}

[shader("vertex")]
VertexOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    const float3  center_location = float3(16.0f); // TODO: fix this is hardcoded in color calculatin
    const float voxel_size      = 64.0;

    const float3 box_corner_negative = center_location - float3(voxel_size) / 2;
    const float3 box_corner_positive = center_location + float3(voxel_size) / 2;

    const float3 cube_vertex    = CUBE_TRIANGLE_LIST[vertexIndex % 36];
    const float3 world_location = center_location + voxel_size * cube_vertex;

    VertexOutput o;

    o.position              = mul(global_gpu_data[0].view_projection_matrix, float4(world_location, 1.0));
    o.world_position       = world_location;
    o.out_negative_corner = box_corner_negative;
    
    return o;
}

struct FragmentOutput
{
    float4 out_position_and_id : COLOR0;
    float depth : SV_DepthGreaterEqual;
};

[shader("fragment")]
FragmentOutput fragmentMain(float3 world_position : COLOR0, float3 in_cube_negative_corner : COLOR1)
{

    const u32 chunkId = 0;

    const Cube chunkBoundingCube = Cube(in_cube_negative_corner + 32, 64);
    const float3 camera_position = global_gpu_data[UBO_GLOBAL_DATA].camera_position.xyz;
    const float3 dir             = normalize(world_position - camera_position);

    const Ray                camera_ray                    = Ray(camera_position, dir);
    const IntersectionResult WorldBoundingCubeIntersection = chunkBoundingCube.tryIntersect(camera_ray);

    if (!WorldBoundingCubeIntersection.intersection_occurred)
    {
        discard;
    }

    const float3 box_corner_negative = in_cube_negative_corner;
    const float3 box_corner_positive = in_cube_negative_corner + 64;

    const bool is_camera_inside_box = chunkBoundingCube.contains(camera_position);

    const float3 traversalRayStart = is_camera_inside_box
                                     ? (camera_position - box_corner_negative)
                                     : (WorldBoundingCubeIntersection.maybe_hit_point - box_corner_negative);

    // TODO: hone this further and see if you can get rid of some bounds checks in the traversal shader
    const float3 traversalRayEnd = traversalRayStart + dir * WorldBoundingCubeIntersection.t_far;

    const VoxelTraceResult result = traceDDARay(0, traversalRayStart, traversalRayEnd);

    float3 worldStrikePosition = result.chunk_local_fragment_position + in_cube_negative_corner;

    if (!result.intersect_occur)
    {
        worldStrikePosition = camera_ray.at(WorldBoundingCubeIntersection.t_far);
    }

    const bool showTrace = false;

    FragmentOutput out;

    if (showTrace)
    {
        // out_color = float4(plasma_quintic(float(result.steps) / 64.0), 1.0);
    }
    else
    {
        if (!result.intersect_occur)
        {
            discard;
        }

        const u32 uniqueFaceId = UnpackUniqueFaceIdResult(result.voxel_normal, result.chunk_local_voxel_position, chunkId).pack();
        const u32 kHashTableCapacity = 1U << 20U;
        const u32 kEmpty             = ~0u;

        out.out_position_and_id = float4(worldStrikePosition, reinterpret<f32>(uniqueFaceId));
    }

    const float4  clipPos = mul(global_gpu_data[UBO_GLOBAL_DATA].view_projection_matrix, float4(worldStrikePosition, float(1.0)));
    const float depth   = (clipPos.z / clipPos.w);
    out.depth        = showTrace ? max(depth, 0.000000000001) : depth;

    return out;
}