#version 460

layout(location = 0) out vec3 fragColor;

vec3 positions[3] = vec3[](
    vec3(0.0, 0.5, 0.0),   // Top
    vec3(-0.5, -0.5, 0.0), // Bottom left
    vec3(0.5, -0.5, 0.0)   // Bottom right
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // Red
    vec3(0.0, 1.0, 0.0), // Green
    vec3(0.0, 0.0, 1.0)  // Blue
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 1.0);
    fragColor   = colors[gl_VertexIndex];
}