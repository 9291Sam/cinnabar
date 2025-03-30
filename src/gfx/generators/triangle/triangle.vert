#version 460

#include "globals.glsl"

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