#ifndef SRC_GFX_SHADER_COMMON_GLOBAL_GLSL
#define SRC_GFX_SHADER_COMMON_GLOBAL_GLSL

#include "binding_offsets.glsl"
#include "types.glsl"

struct GlobalGpuData
{
    mat4  view_matrix;
    mat4  projection_matrix;
    mat4  view_projection_matrix;
    vec4  camera_forward_vector;
    vec4  camera_right_vector;
    vec4  camera_up_vector;
    vec4  camera_position;
    float fov_y;
    float tan_half_fov_y;
    float aspect_ratio;
    float time_alive;
    uvec2 framebuffer_size;
};

layout(set = 0, binding = 3) readonly uniform GlobalGpuDataBuffer
{
    GlobalGpuData data;
}
in_global_gpu_data[];

// Not a bikeshed, this makes it uniform access
// https://godbolt.org/z/9G9MG6d4G
// https://discord.com/channels/318590007881236480/591343919598534681/1350287992970809354
#define GlobalData in_global_gpu_data[UBO_GLOBAL_DATA].data

#define PI 3.1415926535897932384626433

#endif // SRC_GFX_SHADER_COMMON_GLOBAL_GLSL