#pragma once 

#include "bindings.slang" 

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float16_t f16;
typedef float32_t f32;
typedef float64_t f64;


struct GlobalGpuData
{
    column_major float4x4  view_matrix;
    column_major float4x4  projection_matrix;
    column_major float4x4  view_projection_matrix;
    float4  camera_forward_vector;
    float4  camera_right_vector;
    float4  camera_up_vector;
    float4  camera_position;
    f32 fov_y;
    f32 tan_half_fov_y;
    f32 aspect_ratio;
    f32 time_alive;
    uint2 framebuffer_size;
    uint bool_enable_reflections;
    uint bool_moved_this_frame;
};

[[vk::binding(3)]] ConstantBuffer<GlobalGpuData> global_gpu_data[];

#define GlobalData global_gpu_data[UBO_GLOBAL_DATA]

#define PI 3.1415926535897932384626433