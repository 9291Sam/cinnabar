

struct VertexOutput
{
    float2 ndc: COLOR0;
    float4 position: SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint VertexIndex : SV_VertexID)
{
    const Array<float2, 3> joint_position_uv = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};

    VertexOutput o;

    o.ndc = joint_position_uv[VertexIndex];
    o.position = float4(joint_position_uv[VertexIndex], 0.0000000000001, 1.0);

    return o;
}


[shader("fragment")]
float4 fragmentMain(float2 color: COLOR0) : COLOR0
{



    // const vec3 rayDir = normalize(
    //     vec3(GlobalData.camera_forward_vector.xyz)
    //     + ndc.x * vec3(GlobalData.camera_right_vector.xyz) * GlobalData.aspect_ratio * GlobalData.tan_half_fov_y
    //     + ndc.y * vec3(GlobalData.camera_up_vector) * GlobalData.tan_half_fov_y);


    // discard;
    // return float4(rayDir, 1.0, 1.0);
    return float4(color, 1.0, 1.0);

    // return float4(0.0);
}