struct GlobalGpuData
{
    float4x4  view_matrix;
    float4x4  projection_matrix;
    float4x4  view_projection_matrix;
    float4  camera_forward_vector;
    float4  camera_right_vector;
    float4  camera_up_vector;
    float4  camera_position;
    float fov_y;
    float tan_half_fov_y;
    float aspect_ratio;
    float time_alive;
    float2 framebuffer_size;
};

[[vk::binding(3, 0)]] ConstantBuffer<GlobalGpuData> global_gpu_data[];
[[vk::binding(4, 0)]] StructuredBuffer<float3> in_position_buffers[];


struct VertexOutput
{
    float3 color : COLOR0;
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint VertexIndex : SV_VertexID)
{    
    const Array<float3, 3> trianglePositions = {
        float3(0.0, 0.5, 0.0),
        float3(-0.5, -0.5, 0.0),
        float3(0.5, -0.5, 0.0)
    };

    const Array<float3, 3> triangleColors = {
        float3(1.0, 0.0, 0.0), 
        float3(0.0, 1.0, 0.0),
        float3(0.0, 0.0, 1.0)
    };


    const uint  triangle_id              = VertexIndex / 3;
    const uint  position_within_triangle = VertexIndex % 3;
    const float scale                    = 10.0;

    const float4 world_vertex_coordinate = mul(
        global_gpu_data[0].view_projection_matrix,
        float4(in_position_buffers[6][triangle_id] + trianglePositions[position_within_triangle] * scale, 1.0)
    );
                                            
    VertexOutput o;

    o.position = world_vertex_coordinate;
    o.color = triangleColors[position_within_triangle];
    
    return o;
}



[shader("fragment")]
float4 fragmentMain(float3 color: COLOR0) : COLOR0
{
    return float4(color, 1.0);
}