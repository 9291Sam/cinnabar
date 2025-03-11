#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 4) readonly buffer PositionBuffer
{
    vec3 positions[];
}
in_position_buffers[];

vec3 triangle_positions[3] = vec3[](
    vec3(0.0, 0.5, 0.0),   // Top
    vec3(-0.5, -0.5, 0.0), // Bottom left
    vec3(0.5, -0.5, 0.0)   // Bottom right
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // Red
    vec3(0.0, 1.0, 0.0), // Green
    vec3(0.0, 0.0, 1.0)  // Blue
);

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
};

layout(set = 0, binding = 3) readonly uniform GlobalGpuDataBuffer
{
    GlobalGpuData data;
}
in_global_gpu_data[];

layout(push_constant) uniform PushConstants
{
    uint position_buffer_offset;
    uint global_data_offset;
}
in_push_constants;

layout(location = 0) out vec3 fragColor;

void main()
{
    const uint  triangle_id              = gl_VertexIndex / 3;
    const uint  position_within_triangle = gl_VertexIndex % 3;
    const float scale                    = 10.0;

    const vec4 world_vertex_coordinate =
        in_global_gpu_data[in_push_constants.global_data_offset].data.view_projection_matrix
        * vec4(
            in_position_buffers[in_push_constants.position_buffer_offset].positions[triangle_id]
                + triangle_positions[position_within_triangle] * scale,
            1.0);

    gl_Position = world_vertex_coordinate;
    fragColor   = colors[position_within_triangle];
}