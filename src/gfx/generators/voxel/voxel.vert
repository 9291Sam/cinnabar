#version 460

layout(push_constant) uniform PushConstants
{
    mat4 model_view_proj;
    vec4 camera_position;
}
in_push_constants;

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

void main()
{
    const vec3  center_location = vec3(8.0, 8.2, -12.24);
    const float voxel_size      = 8.0;
    const vec4  voxel_color     = vec4(1.0, 0.0, 0.0, 1.0);

    const vec3 box_corner_negative = center_location - vec3(voxel_size) / 2;
    const vec3 box_corner_positive = center_location + vec3(voxel_size) / 2;

    const vec3 cube_vertex    = CUBE_TRIANGLE_LIST[gl_VertexIndex % 36];
    const vec3 world_location = center_location + voxel_size * cube_vertex;

    gl_Position              = in_push_constants.model_view_proj * vec4(world_location, 1.0);
    out_uvw                  = cube_vertex + 0.5;
    out_world_position       = world_location;
    out_cube_center_location = center_location;
}
