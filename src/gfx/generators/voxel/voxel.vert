#version 460

#extension GL_EXT_nonuniform_qualifier : require

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

layout(set = 0, binding = 3) readonly buffer GlobalGpuDataBuffer
{
    GlobalGpuData data;
}
in_global_gpu_data[];

layout(location = 0) out vec3 out_uvw;
layout(location = 1) out vec3 out_world_position;
layout(location = 2) out vec3 out_cube_center_location;

vec3 CUBE_TRIANGLE_LIST[] = {
    // Front face
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f),

    // Back face
    vec3(0.5f, -0.5f, -0.5f),
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),
    vec3(0.5f, -0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),
    vec3(0.5f, 0.5f, -0.5f),

    // Left face
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, -0.5f),

    // Right face
    vec3(0.5f, -0.5f, 0.5f),
    vec3(0.5f, -0.5f, -0.5f),
    vec3(0.5f, 0.5f, -0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(0.5f, 0.5f, -0.5f),
    vec3(0.5f, 0.5f, 0.5f),

    // Top face
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f),
    vec3(0.5f, 0.5f, -0.5f),
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(0.5f, 0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),

    // Bottom face
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(-0.5f, -0.5f, 0.5f)};

layout(push_constant) uniform PushConstants
{
    uint global_data_offset;
}
in_push_constants;

void main()
{
    const vec3  center_location = vec3(8.0, 8.2, -12.24);
    const float voxel_size      = 8.0;
    const vec4  voxel_color     = vec4(1.0, 0.0, 0.0, 1.0);

    const vec3 box_corner_negative = center_location - vec3(voxel_size) / 2;
    const vec3 box_corner_positive = center_location + vec3(voxel_size) / 2;

    const vec3 cube_vertex    = CUBE_TRIANGLE_LIST[gl_VertexIndex % 36];
    const vec3 world_location = center_location + voxel_size * cube_vertex;

    gl_Position = in_global_gpu_data[in_push_constants.global_data_offset].data.view_projection_matrix
                * vec4(world_location, 1.0);
    out_uvw                  = cube_vertex + 0.5;
    out_world_position       = world_location;
    out_cube_center_location = box_corner_negative;
}
