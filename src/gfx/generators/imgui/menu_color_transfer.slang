#include "globals.slang"


[format("rgba32f")]
[[vk::binding(2)]] RWTexture2D<float4> menu_transfer_image[];

[shader("vertex")]
float4 vertexMain(uint vertexIndex : SV_VertexID) : SV_Position
{
    const float4 positions[3] = float4[3](float4(-1.0, -1.0, 0.0, 1.0), float4(-1.0, 3.0, 0.0, 1.0), float4(3.0, -1.0, 0.0, 1.0));

    return positions[vertexIndex];
}


[shader("fragment")]
float4 fragmentMain(float4 pos : SV_Position) : COLOR0
{
    float4 color = menu_transfer_image[SIO_IMGUI_RENDER_TARGET].Load(int2(floor(pos.xy)));

    color.xyz = color.xyz * color.xyz;

    return color;
}