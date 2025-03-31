#version 460

#define SBO_SRGB_TRIANGLE_DATA 6

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

#define GlobalData in_global_gpu_data[0].data

const vec3 triangle_positions[3] = vec3[](vec3(0.0, 0.5, 0.0), vec3(-0.5, -0.5, 0.0), vec3(0.5, -0.5, 0.0));

const vec3 colors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

layout(set = 0, binding = 4) readonly buffer PositionBuffer
{
    vec3 positions[];
}
in_position_buffers[];

layout(location = 0) out vec3 fragColor;

void main()
{
    const uint  triangle_id              = gl_VertexIndex / 3;
    const uint  position_within_triangle = gl_VertexIndex % 3;
    const float scale                    = 10.0;

    const vec4 world_vertex_coordinate = GlobalData.view_projection_matrix
                                       * vec4(
                                             in_position_buffers[SBO_SRGB_TRIANGLE_DATA].positions[triangle_id]
                                                 + triangle_positions[position_within_triangle] * scale,
                                             1.0);

    gl_Position = world_vertex_coordinate;
    fragColor   = colors[position_within_triangle];
}