#version 460

#include "globals.glsl"

const vec3 CUBE_TRIANGLE_LIST[] = {
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

layout(location = 0) out vec3 out_uvw;
layout(location = 1) out vec3 out_world_position;
layout(location = 2) out vec3 out_cube_center_location;
layout(location = 3) out vec3 out_ray_start_position;

void main()
{
    const vec3  center_location = vec3(16.0f);
    const float voxel_size      = 64.0;

    const vec3 box_corner_negative = center_location - vec3(voxel_size) / 2;
    const vec3 box_corner_positive = center_location + vec3(voxel_size) / 2;

    const vec3 cube_vertex    = CUBE_TRIANGLE_LIST[gl_VertexIndex % 36];
    const vec3 world_location = center_location + voxel_size * cube_vertex;

    gl_Position              = GlobalData.view_projection_matrix * vec4(world_location, 1.0);
    out_uvw                  = cube_vertex + 0.5;
    out_world_position       = world_location;
    out_cube_center_location = box_corner_negative;
}
