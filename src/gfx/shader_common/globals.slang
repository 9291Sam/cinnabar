
struct GlobalGpuData
{
    column_major float4x4  view_matrix;
    column_major float4x4  projection_matrix;
    column_major float4x4  view_projection_matrix;
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

// Descrip

[[vk::binding(3, 0)]] ConstantBuffer<GlobalGpuData> global_gpu_data[];