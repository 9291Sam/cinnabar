#version 460

layout(push_constant) uniform PushConstants
{
    mat4 model_view_proj;
    vec4 camera_position;
}
in_push_constants;

layout(location = 0) out vec3 out_uvw;
layout(location = 1) out vec3 out_world_position;
layout(location = 2) out uint out_bool_is_camera_in_box;

vec3 CUBE_STRIP_OFFSETS[] = {
    vec3(-0.5f, 0.5f, 0.5f),   // Front-top-left
    vec3(0.5f, 0.5f, 0.5f),    // Front-top-right
    vec3(-0.5f, -0.5f, 0.5f),  // Front-bottom-left
    vec3(0.5f, -0.5f, 0.5f),   // Front-bottom-right
    vec3(0.5f, -0.5f, -0.5f),  // Back-bottom-right
    vec3(0.5f, 0.5f, 0.5f),    // Front-top-right
    vec3(0.5f, 0.5f, -0.5f),   // Back-top-right
    vec3(-0.5f, 0.5f, 0.5f),   // Front-top-left
    vec3(-0.5f, 0.5f, -0.5f),  // Back-top-left
    vec3(-0.5f, -0.5f, 0.5f),  // Front-bottom-left
    vec3(-0.5f, -0.5f, -0.5f), // Back-bottom-left
    vec3(0.5f, -0.5f, -0.5f),  // Back-bottom-right
    vec3(-0.5f, 0.5f, -0.5f),  // Back-top-left
    vec3(0.5f, 0.5f, -0.5f),   // Back-top-right
};

void main()
{
    const vec3  center_location = vec3(0.2, 0.3, 0.3);
    const float voxel_size      = 16.0;
    const vec4  voxel_color     = vec4(1.0, 0.0, 0.0, 1.0);

    const vec3 box_corner_negative = center_location - vec3(voxel_size) / 2;
    const vec3 box_corner_positive = center_location + vec3(voxel_size) / 2;

    out_bool_is_camera_in_box = uint(
        all(lessThan(box_corner_negative, in_push_constants.camera_position.xyz))
        && all(lessThan(in_push_constants.camera_position.xyz, box_corner_positive)));

    const vec3 cube_vertex    = CUBE_STRIP_OFFSETS[gl_VertexIndex % 14];
    const vec3 world_location = center_location + voxel_size * cube_vertex;

    gl_Position        = in_push_constants.model_view_proj * vec4(world_location, 1.0);
    out_uvw            = cube_vertex + 0.5;
    out_world_position = world_location;
}