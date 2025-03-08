#version 460

// https://nullprogram.com/blog/2018/07/31/
uint gpu_hashuint(uint x)
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

uint rotate_right(uint x, uint r)
{
    return (x >> r) | (x << (32u - r));
}

uint gpu_hashCombineuint(uint a, uint h)
{
    a *= 0xcc9e2d51u;
    a = rotate_right(a, 17u);
    a *= 0x1b873593u;
    h ^= a;
    h = rotate_right(h, 19u);
    return h * 5u + 0xe6546b64u;
}

float random(vec2 pos)
{
    uint seed = 474838454;

    seed = gpu_hashCombineuint(seed, gpu_hashuint(floatBitsToUint(pos.x)));

    seed = gpu_hashCombineuint(seed, gpu_hashuint(floatBitsToUint(pos.y)));

    // if ((seed & 7u) == 0u)
    // {
    //     seed += gpu_hashCombineuint(seed - seed * 727382, gpu_hashuint(floatBitsToUint(pos.y)));
    // }

    // seed = gpu_hashCombineuint(gpu_hashuint(floatBitsToUint(pos.z)), seed);

    return float(seed) / float(uint(-1));
}

// Based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise(in vec2 st)
{
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

#define OCTAVES 8
float fbm(in vec2 st)
{
    // Initial values
    float value     = 0.0;
    float amplitude = .5;
    float frequency = 0.;
//
// Loop of octaves
#pragma unroll
    for (int i = 0; i < OCTAVES; i++)
    {
        value += amplitude * noise(st);
        st *= 2.;
        amplitude *= .5;
    }
    return value;
}
vec3 Sky(in vec3 ro, in vec3 rd, in float iTime)
{
    const float SC = 1e5;

    // Calculate sky plane
    float dist = (SC - ro.y) / rd.y;
    vec2  p    = (ro + dist * rd).xz;
    p *= 10.2 / SC;

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    vec3  lightDir = normalize(vec3(-.8, .15, -.3));
    float sundot   = clamp(dot(rd, lightDir), 0.0, 1.0);

    vec3 cloudCol = vec3(1.);
    // vec3 skyCol = vec3(.6, .71, .85) - rd.y * .2 * vec3(1., .5, 1.) + .15 * .5;
    vec3 skyCol   = vec3(0.3, 0.5, 0.85); //  - rd.y * rd.y * 0.5;
    // skyCol        = mix(skyCol, 0.85 * vec3(0.7, 0.75, 0.85), pow(1.0 - max(rd.y, 0.0), 4.0));

    // sun
    vec3 sun = 0.25 * vec3(1.0, 0.7, 0.4) * pow(sundot, 5.0);
    sun += 0.25 * vec3(1.0, 0.8, 0.6) * pow(sundot, 64.0);
    sun += 0.2 * vec3(1.0, 0.8, 0.6) * pow(sundot, 512.0);
    skyCol += sun;

    // clouds
    float t   = iTime * 0.1;
    float den = fbm(vec2(p.x - t, p.y - t));
    skyCol    = mix(skyCol, cloudCol, smoothstep(.4, .8, den));

    // horizon
    // skyCol = mix(skyCol, 0.68 * vec3(.418, .394, .372), pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol;
}

layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform Camera
{
    vec4  camForward;  // Camera forward direction (normalized)
    vec4  camRight;    // Camera right direction (normalized)
    vec4  camUp;       // Camera up direction (normalized)
    float aspectRatio; // Screen aspect ratio (width / height)
    float tanHalfFovY; // Field of view in radians (vertical)
    float time_alive;
}
in_push_constants;

void main()
{
    const float fovScale = in_push_constants.tanHalfFovY;

    const vec3 rayDir = normalize(
        vec3(in_push_constants.camForward)
        + ndc.x * vec3(in_push_constants.camRight) * in_push_constants.aspectRatio * fovScale
        + ndc.y * vec3(in_push_constants.camUp) * fovScale);

    out_color = vec4(Sky(vec3(0.0), rayDir, in_push_constants.time_alive), 1.0);
}