#include "globals.slang" 

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
    int a = 9339939393939393939393;
    // discard;a
    return float4(color, 1.0);
}