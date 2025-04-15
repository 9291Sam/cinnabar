

#include "globals.slang"
#include "skybox_common.slang" 

struct VertexOutput
{
    float2 ndc : COLOR0;
    float4 position : SV_Position;
};

[shader("vertex")] VertexOutput vertexMain(uint VertexIndex : SV_VertexID)
{
    const Array<float2, 3> joint_position_uv = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};

    VertexOutput o;

    o.ndc      = joint_position_uv[VertexIndex];
    o.position = float4(joint_position_uv[VertexIndex], 0.0000000000001, 1.0);

    return o;
}

[shader("fragment")] float4 fragmentMain(float2 ndc : COLOR0)
    : COLOR0
{
    const float3 rayDir = normalize(
        float3(global_gpu_data[UBO_GLOBAL_DATA].camera_forward_vector.xyz)
        + ndc.x * float3(global_gpu_data[UBO_GLOBAL_DATA].camera_right_vector.xyz) * global_gpu_data[UBO_GLOBAL_DATA].aspect_ratio
              * global_gpu_data[UBO_GLOBAL_DATA].tan_half_fov_y
        + ndc.y * float3(global_gpu_data[UBO_GLOBAL_DATA].camera_up_vector.xyz) * global_gpu_data[UBO_GLOBAL_DATA].tan_half_fov_y);

    return float4(totalSkybox(rayDir, false), 1.0);
}