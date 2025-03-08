#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 2, rgba32f) uniform image2D menu_transfer_image[];

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants
{
    uint menuTransferImageId;
}
in_push_constants;

void main()
{
    vec4 color = imageLoad(menu_transfer_image[in_push_constants.menuTransferImageId], ivec2(floor(gl_FragCoord.xy)));

    color.rgb = pow(color.rgb, vec3(2.2));

    out_color = color;
}