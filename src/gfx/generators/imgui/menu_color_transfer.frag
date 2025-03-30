#version 460

#include "binding_offsets.glsl"

layout(set = 0, binding = 2, rgba32f) uniform image2D menu_transfer_image[];

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 color = imageLoad(menu_transfer_image[SIO_IMGUI_RENDER_TARGET], ivec2(gl_FragCoord.xy));

    color.rgb = color.rgb * color.rgb; // fast srgb approximation

    out_color = color;
}