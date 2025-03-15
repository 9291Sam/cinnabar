#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 2, rgba32f) uniform image2D menu_transfer_image[];

layout(push_constant) uniform PushConstants
{
    uint menuTransferImageId;
}
in_push_constants;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 color = imageLoad(menu_transfer_image[in_push_constants.menuTransferImageId], ivec2(gl_FragCoord.xy));

    color.rgb = color.rgb * color.rgb; // fast srgb approximation

    out_color = color;
}