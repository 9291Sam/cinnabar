#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

// // https://nullprogram.com/blog/2018/07/31/
// uint hash(uint x)
// {
//     x ^= x >> 17;
//     x *= 0xed5ad4bbU;
//     x ^= x >> 11;
//     x *= 0xac4c1b51U;
//     x ^= x >> 15;
//     x *= 0x31848babU;
//     x ^= x >> 14;

//     return x;
// }

// uint rotate_right(uint x, uint r)
// {
//     return (x >> r) | (x << (32u - r));
// }

// uint combine(uint a, uint h)
// {
//     a *= 0xcc9e2d51u;
//     a = rotate_right(a, 17u);
//     a *= 0x1b873593u;
//     h ^= a;
//     h = rotate_right(h, 19u);
//     return h * 5u + 0xe6546b64u;
// }

// float random(vec2 pos)
// {
//     uint seed = 44;

//     seed = combine(seed, hash(floatBitsToUint(pos.x)));

//     seed = combine(seed, hash(hash(floatBitsToUint(pos.y))));

//     return float(seed) / float(uint(-1));
// }

// // Based on Morgan McGuire @morgan3d
// // https://www.shadertoy.com/view/4dS3Wd
// float noise(in vec2 st)
// {
//     vec2 i = floor(st);
//     vec2 f = fract(st);

//     // Four corners in 2D of a tile
//     float a = random(i);
//     float b = random(i + vec2(1.0, 0.0));
//     float c = random(i + vec2(0.0, 1.0));
//     float d = random(i + vec2(1.0, 1.0));

//     vec2 u = f * f * (3.0 - 2.0 * f);

//     return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
// }

vec4 permute(vec4 x)
{
    return mod(((x * 34.0) + 1.0) * x, 289.0);
}

vec2 fade(vec2 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float noise(vec2 P)
{
    vec4 Pi   = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf   = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi        = mod(Pi, 289.0); // To avoid truncation effects in permutation
    vec4 ix   = Pi.xzxz;
    vec4 iy   = Pi.yyww;
    vec4 fx   = Pf.xzxz;
    vec4 fy   = Pf.yyww;
    vec4 i    = permute(permute(ix) + iy);
    vec4 gx   = 2.0 * fract(i * 0.0243902439) - 1.0; // 1/41 = 0.024...
    vec4 gy   = abs(gx) - 0.5;
    vec4 tx   = floor(gx + 0.5);
    gx        = gx - tx;
    vec2 g00  = vec2(gx.x, gy.x);
    vec2 g10  = vec2(gx.y, gy.y);
    vec2 g01  = vec2(gx.z, gy.z);
    vec2 g11  = vec2(gx.w, gy.w);
    vec4 norm = 1.79284291400159 - 0.85373472095314 * vec4(dot(g00, g00), dot(g01, g01), dot(g10, g10), dot(g11, g11));
    g00 *= norm.x;
    g01 *= norm.y;
    g10 *= norm.z;
    g11 *= norm.w;
    float n00     = dot(g00, vec2(fx.x, fy.x));
    float n10     = dot(g10, vec2(fx.y, fy.y));
    float n01     = dot(g01, vec2(fx.z, fy.z));
    float n11     = dot(g11, vec2(fx.w, fy.w));
    vec2  fade_xy = fade(Pf.xy);
    vec2  n_x     = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
    float n_xy    = mix(n_x.x, n_x.y, fade_xy.y);
    return 2.3 * n_xy;
}

#define OCTAVES 4
float fbm(in vec2 st)
{
    // Initial values
    float value     = 0.0;
    float amplitude = .5;
    float frequency = 0.;
    //
    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++)
    {
        value += amplitude * noise(st) * noise(st.yx + vec2(3040.20492, -2939.3)) * 3;
        st *= 2.41;
        st += vec2(23.423, 293.4);
        amplitude *= .8;
    }
    return value;
}
vec3 Sky(in vec3 ro, in vec3 rd, in float iTime)
{
    const float SC = 1e5;

    if (rd.y < 0.0)
    {
        rd.x *= -1.0;

        rd.z *= -1.0;
    }

    // Calculate sky plane
    float dist = (SC - ro.y) / rd.y;
    vec2  p    = (ro + dist * rd).xz;
    p *= 4.2 / SC;

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    vec3  lightDir = normalize(vec3(-.8, .15, -.3) * ((rd.y < 0.0) ? vec3(-1, 1, -1) : vec3(1.0)));
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
    float den = fbm(vec2(p.x - t, p.y - t) + ((rd.y < 0.0) ? vec2(302.04, -2838.3) : vec2(0.0)));
    skyCol    = mix(skyCol, cloudCol, smoothstep(0.0, 1.0, den));

    // horizon
    // skyCol = mix(skyCol, 0.68 * vec3(.418, .394, .372), pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol;
}

layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 out_color;

struct GlobalGpuData
{
    mat4  view_matrix;
    mat4  projection_matrix;
    mat4  view_projection_matrix;
    vec4  camera_forward_vector;
    vec4  camera_right_vector;
    vec4  camera_up_vector;
    vec4  camera_position;
    float fov_y;
    float tan_half_fov_y;
    float aspect_ratio;
    float time_alive;
};

layout(set = 0, binding = 4) buffer GlobalGpuDataBuffer
{
    GlobalGpuData data;
}
in_global_gpu_data[];

layout(push_constant) uniform Camera
{
    uint8_t global_data_offset;
}
in_push_constants;

void main()
{
    const GlobalGpuData globalData = in_global_gpu_data[in_push_constants.global_data_offset].data;

    const vec3 rayDir = normalize(
        vec3(globalData.camera_forward_vector.xyz)
        + ndc.x * vec3(globalData.camera_right_vector.xyz) * globalData.aspect_ratio * globalData.tan_half_fov_y
        + ndc.y * vec3(globalData.camera_up_vector) * globalData.tan_half_fov_y);

    out_color = vec4(Sky(vec3(0.0), rayDir, globalData.time_alive), 1.0);
}