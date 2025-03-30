#ifndef SRC_GFX_SHADER_COMMON_BINDING_OFFSETS_GLSL
#define SRC_GFX_SHADER_COMMON_BINDING_OFFSETS_GLSL

// Storage Image Offsets (binding = 2)
#define SIO_VOXEL_RENDER_TARGET 0
#define SIO_IMGUI_RENDER_TARGET 1

// Uniform Buffer Offsets (binding = 3)
#define UBO_GLOBAL_DATA 0

// Storage Buffer Offsets (binding = 4)
#define SBO_CHUNK_BRICKS          0
#define SBO_VOXEL_LIGHTS          1
#define SBO_VISIBILITY_BRICKS     2
#define SBO_MATERIAL_BRICKS       3
#define SBO_VOXEL_MATERIAL_BUFFER 4
#define SBO_FACE_HASH_MAP         5
#define SBO_SRGB_TRIANGLE_DATA    6

#endif // SRC_GFX_SHADER_COMMON_BINDING_OFFSETS_GLSL