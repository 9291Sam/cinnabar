#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0);
}
// #version 460

// #extension GL_EXT_nonuniform_qualifier : enable

// layout(location = 0) in vec3 fragColor; // Use fragColor as UV coordinates
// layout(location = 0) out vec4 outColor;

// // Descriptor set 0, binding 0: an array of sampled textures (bindless)
// layout(set = 0, binding = 0) uniform sampler2D textures[];

// void main() {
//     vec2 uv = fragColor.xy; // Use xy components as UV coordinates

//     // Select textures dynamically
//     int textureIndex1 = 0;
//     int textureIndex2 = 1;

//     // Fetch colors from the two textures using non-uniform indexing
//     vec4 color1 = texture(textures[textureIndex1], uv);
//     vec4 color2 = texture(textures[textureIndex2], uv);

//     // Blend the two textures (you can modify the blending logic)
//     outColor = mix(color1, color2, 0.5);
// }

// #version 460

// #extension GL_EXT_nonuniform_qualifier : enable

// layout(location = 0) in vec3 fragColor;
// layout(location = 0) out vec4 outColor;

// // Bindless storage buffer arrays (same binding for different types)
// layout(set = 0, binding = 0) readonly buffer BufferA {
//     float data[];
// } buffersA[]; // Array of descriptors at the same binding

// layout(set = 0, binding = 0) readonly buffer BufferB {
//     int data[];
// } buffersB[]; // Array of descriptors at the same binding

// void main() {
//     int index = int(fragColor.x * 255) % 256; // Generate an index from UVs

//     // Select buffers dynamically
//     int bufferIndexA = 0; // Choose a specific buffer from buffersA[]
//     int bufferIndexB = 1; // Choose a specific buffer from buffersB[]

//     // Read values from storage buffers using non-uniform indexing
//     float valueA = buffersA[bufferIndexA].data[index];
//     int valueB = buffersB[bufferIndexB].data[index];

//     // Normalize valueB for color usage
//     float normalizedB = clamp(valueB / 255.0, 0.0, 1.0);

//     // Compute final output color
//     outColor = vec4(valueA, normalizedB, 1.0 - normalizedB, 1.0);
// }
