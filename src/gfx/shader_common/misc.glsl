#ifndef SRC_GFX_SHADER_COMMON_MISC_GLSL
#define SRC_GFX_SHADER_COMMON_MISC_GLSL

#include "types.glsl"

vec3 plasma_quintic(float x)
{
    x       = clamp(x, 0.0, 1.0);
    vec4 x1 = vec4(1.0, x, x * x, x * x * x); // 1 x x2 x3
    vec4 x2 = x1 * x1.w * x;                  // x4 x5 x6 x7
    return vec3(
        dot(x1.xyzw, vec4(+0.063861086, +1.992659096, -1.023901152, -0.490832805))
            + dot(x2.xy, vec2(+1.308442123, -0.914547012)),
        dot(x1.xyzw, vec4(+0.049718590, -0.791144343, +2.892305078, +0.811726816))
            + dot(x2.xy, vec2(-4.686502417, +2.717794514)),
        dot(x1.xyzw, vec4(+0.513275779, +1.580255060, -5.164414457, +4.559573646))
            + dot(x2.xy, vec2(-1.916810682, +0.570638854)));
}

u32 gpu_hashU32(u32 x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bbU;
    x ^= x >> 11;
    x *= 0xac4c1b51U;
    x ^= x >> 15;
    x *= 0x31848babU;
    x ^= x >> 14;

    return x;
}

u32 rotate_right(u32 x, u32 r)
{
    return (x >> r) | (x << (32u - r));
}

u32 gpu_hashCombineU32(u32 a, u32 h)
{
    a *= 0xcc9e2d51u;
    a = rotate_right(a, 17u);
    a *= 0x1b873593u;
    h ^= a;
    h = rotate_right(h, 19u);
    return h * 5u + 0xe6546b64u;
}

float gpu_randomUniformFloat(uint seed)
{
    return float(gpu_hashU32(seed)) * uintBitsToFloat(0x2f800004u);
}

#endif // SRC_GFX_SHADER_COMMON_MISC_GLSL