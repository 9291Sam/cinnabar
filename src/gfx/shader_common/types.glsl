#ifndef SRC_GFX_SHADER_COMMON_TYPES_GLSL
#define SRC_GFX_SHADER_COMMON_TYPES_GLSL

#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_float32 : require

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint
#define u64 uint64_t

#define i8  int8_t
#define i16 int16_t
#define i32 int
#define i64 int64_t

#define f32 float

#endif // SRC_GFX_SHADER_COMMON_TYPES_GLSL