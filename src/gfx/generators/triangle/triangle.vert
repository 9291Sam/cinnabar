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

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    uint position_buffer;
}
in_push_constants;

layout(location = 0) out vec3 fragColor;

void main()
{
    const uint  triangle_id              = gl_VertexIndex / 3;
    const uint  position_within_triangle = gl_VertexIndex % 3;
    const float scale                    = 10.0;

    const vec4 world_vertex_coordinate =
        in_push_constants.mvp
        * vec4(
            in_position_buffers[in_push_constants.position_buffer].positions[triangle_id]
                + triangle_positions[position_within_triangle] * scale,
            1.0);

    gl_Position = world_vertex_coordinate;
    fragColor   = colors[position_within_triangle];
}