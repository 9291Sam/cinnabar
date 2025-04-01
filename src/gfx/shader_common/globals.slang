#pragma once 

// Storage Image Offsets (binding = 2)
#define SIO_VOXEL_RENDER_TARGET 0
#define SIO_IMGUI_RENDER_TARGET 1

// Uniform Buffer Offsets (binding = 3)
#define UBO_GLOBAL_DATA 0

// Storage Buffer Offsets (binding = 4)
#define SBO_CHUNK_BRICKS          0
#define SBO_VOXEL_LIGHTS          1
#define SBO_VISIBILITY_BRICKS     2
#define SBO_MATERIAL_BRICKS       3
#define SBO_VOXEL_MATERIAL_BUFFER 4
#define SBO_FACE_HASH_MAP         5
#define SBO_SRGB_TRIANGLE_DATA    6

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