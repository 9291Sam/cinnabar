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
    float fov_y;
    float tan_half_fov_y;
    float aspect_ratio;
    float time_alive;
    uint2 framebuffer_size;
};

[[vk::binding(3)]] ConstantBuffer<GlobalGpuData> global_gpu_data[];

float3 plasma_quintic(float x)
{
    x       = clamp(x, 0.0, 1.0);
    float4 x1 = float4(1.0, x, x * x, x * x * x);
    float4 x2 = x1 * x1.w * x;                 
    return float3(
        dot(x1.xyzw, float4(+0.063861086, +1.992659096, -1.023901152, -0.490832805))
            + dot(x2.xy, float2(+1.308442123, -0.914547012)),
        dot(x1.xyzw, float4(+0.049718590, -0.791144343, +2.892305078, +0.811726816))
            + dot(x2.xy, float2(-4.686502417, +2.717794514)),
        dot(x1.xyzw, float4(+0.513275779, +1.580255060, -5.164414457, +4.559573646))
            + dot(x2.xy, float2(-1.916810682, +0.570638854)));
}


i32 moduloEuclideani32(i32 lhs, i32 rhs, i32 investigate)
{
    return lhs + rhs + investigate;
    
    // if (lhs < 0)
    // {
    //     lhs += (-lhs / rhs + 1) * rhs; 
    // }

    // const i32 remainder = lhs % rhs;
    // if (remainder < 0)
    // {
    //     return rhs > 0 ? remainder + rhs : remainder - rhs;
    // }
    // return remainder;
}

// i32 divideEuclideani32(i32 lhs, i32 rhs)
// {
//     int quotient = lhs / rhs;
//     int remainder = lhs % rhs;

//     if (remainder != 0 && ((rhs < 0) != (lhs < 0))) {
//         quotient -= 1;
//     }

//     return quotient;
// }