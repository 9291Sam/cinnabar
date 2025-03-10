#version 460

#extension GL_EXT_nonuniform_qualifier : require

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
vec3 Sky(in vec3 ro, in vec3 rd, in float iTime, in vec3 sunDir)
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
    vec3  lightDir = normalize(sunDir * ((rd.y < 0.0) ? vec3(-1, 1, -1) : vec3(1.0)));
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

vec3 uncharted2_tonemap_partial(vec3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 uncharted2_filmic(vec3 v)
{
    float exposure_bias = 2.0f;
    vec3  curr          = uncharted2_tonemap_partial(v * exposure_bias);

    vec3 W           = vec3(11.2f);
    vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}

const float atmosphereRayleighHeight       = 8000.0;
const float atmosphereMieHeight            = 1200.0;
const float planetHeight                   = 6360e3;
const float atmosphereHeight               = planetHeight + atmosphereRayleighHeight; // 6420e3;
const vec3  atmospherePosition             = vec3(0.0, -(planetHeight + 1000.0), 0.0);
const vec3  rayleighScatteringCoefficients = vec3(5.8e-6, 1.35e-5, 3.31e-5);
const float mieScatteringCoefficient       = 2e-6;
const float pi                             = 3.141592;
const float g                              = 0.758;
// const vec3  rayleighScatteringCoefficients = vec3(5.47e-6, 1.28e-5, 3.12e-5);

// https://iquilezles.org/articles/intersectors
vec2 traceSphere(in vec3 ro, in vec3 rd, in vec3 ce, float ra)
{
    vec3  oc = ro - ce;
    float b  = dot(oc, rd);
    float c  = dot(oc, oc) - ra * ra;
    float h  = b * b - c;
    if (h < 0.0)
    {
        return vec2(-1.0); // no intersection
    }
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

// https://iquilezles.org/articles/intersectors
vec2 boxIntersection(in vec3 ro, in vec3 rd, in vec3 boxSize, out vec3 outNormal)
{
    vec3  m  = 1.0 / rd; // can precompute if traversing a set of aligned boxes
    vec3  n  = m * ro;   // can precompute if traversing a set of aligned boxes
    vec3  k  = abs(m) * boxSize;
    vec3  t1 = -n - k;
    vec3  t2 = -n + k;
    float tN = max(max(t1.x, t1.y), t1.z);
    float tF = min(min(t2.x, t2.y), t2.z);
    if (tN > tF || tF < 0.0)
    {
        return vec2(-1.0); // no intersection
    }
    outNormal = -sign(rd) * step(t1.yzx, t1.xyz) * step(t1.zxy, t1.xyz);
    return vec2(tN, tF);
}

float scene(in vec3 ro, in vec3 rd)
{
    float res = -1.0;

    vec3  rd2   = normalize(vec3((-1.0 + 2.0 * vec2(0.5)), -1.00));
    float scale = 100.0 * 100.0;
    vec3  pos   = normalize(rd2 + vec3(0.0, 0.0, 0.0)) * (scale * 2.0) * 2.0 + vec3(0.0, scale / 1.0, 0.0);

    float sphere = traceSphere(ro, rd, pos, scale).x;
    sphere       = sphere < 0.0 ? traceSphere(ro, rd, pos, scale).y : sphere;
    // res          = res < 0.0 ? sphere : ((sphere >= 0.0) ? min(res, sphere) : res);

    // vec3 normal;
    // return boxIntersection(ro - rd2 * 80000.0, rd, vec3(20000.0), normal).x;

    return res;
}

vec3 atmosphere(in vec3 ro, in vec3 rd, in vec3 sd)
{
    vec2  atmosphereSphere = traceSphere(ro, rd, atmospherePosition, atmosphereHeight);
    vec2  planetSphere     = traceSphere(ro, rd, atmospherePosition, planetHeight);
    // float start            = (atmosphereSphere.x >= 0.0) ? atmosphereSphere.x : distance(ro, atmospherePosition);
    float end              = (planetSphere.x >= 0.0) ? planetSphere.x : atmosphereSphere.y;
    if (end < 0.0)
    {
        return vec3(0.0);
    }

    float hit = scene(ro, rd);
    if (hit >= 0.0)
    {
        end = hit - 10000.0;
    }

    float theta         = dot(rd, sd);
    float theta2        = theta * theta;
    float g2            = g * g;
    float rayleighPhase = (3.0 / (16.0 * pi)) * (1.0 + theta2);
    float miePhase      = (1.0 - g2) / (4.0 * pi * pow(1.0 + g2 - 2.0 * g * theta, 1.5));

    vec3 rayleighTotalScattering = vec3(0.0), mieTotalScattering = vec3(0.0);

    float dither = fract(dot(vec3(0.75487765, 0.56984026, 0.61803398875), vec3(gl_FragCoord.xy, 0.0)));

    // DEBUG
    vec3 tr = vec3(0.0);
    // DEBUG

    const float viewStepCount  = 32.0 - 1.0;
    float       viewOdRayleigh = 0.0, viewOdMie = 0.0;
    vec3        viewStart = (atmosphereSphere.x >= 0.0) ? (ro + rd * atmosphereSphere.x) : ro, viewEnd = ro + rd * end;
    float       viewSize = distance(viewStart, viewEnd) / viewStepCount;
    for (float viewStep = 0.0; viewStep <= viewStepCount; ++viewStep)
    {
        vec3  viewSample = mix(viewStart, viewEnd, (viewStep + dither) / viewStepCount); // + (rd * viewSize * dither);
        float viewSampleHeight = (distance(atmospherePosition, viewSample) - planetHeight);

        float viewSampleOdRayleigh = exp(-viewSampleHeight / atmosphereRayleighHeight) * viewSize;
        float viewSampleOdMie      = exp(-viewSampleHeight / atmosphereMieHeight) * viewSize;

        viewOdRayleigh += viewSampleOdRayleigh;
        viewOdMie += viewSampleOdMie;

        const float sunStepCount  = 16.0 - 1.0;
        float       sunOdRayleigh = 0.0, sunOdMie = 0.0;
        float       sunAtmosphereDistance = traceSphere(viewSample, sd, atmospherePosition, atmosphereHeight).y;

        float hit = scene(viewSample, sd);
        if (hit >= 0.0)
        {
            continue; // sunAtmosphereDistance = hit;
        }

        vec3  sunStart = viewSample, sunEnd = viewSample + sd * sunAtmosphereDistance;
        float sunSize = distance(sunStart, sunEnd) / sunStepCount;
        for (float sunStep = 0.0; sunStep <= sunStepCount; ++sunStep)
        {
            vec3  sunSample = mix(sunStart, sunEnd, (sunStep + dither) / sunStepCount); // + (sd * sunSize * dither);
            float sunSampleHeight = (distance(atmospherePosition, sunSample) - planetHeight);

            sunOdRayleigh += exp(-sunSampleHeight / atmosphereRayleighHeight) * sunSize;
            sunOdMie += exp(-sunSampleHeight / atmosphereMieHeight) * sunSize;
        }

        vec3 transmittance = exp(
            -(rayleighScatteringCoefficients * (viewOdRayleigh + sunOdRayleigh)
              + mieScatteringCoefficient * 1.11 * (viewOdMie + sunOdMie)));
        // DEBUG
        tr += transmittance / viewStepCount;
        // DEBUG

        rayleighTotalScattering += viewSampleOdRayleigh * transmittance;
        mieTotalScattering += viewSampleOdMie * transmittance;
    }
    vec3 color = hit >= 0.0 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 0.0, 0.0);
    return /*color * clamp(tr, 0.0, 1.0) + */ (
               rayleighTotalScattering * rayleighScatteringCoefficients * rayleighPhase
               + mieTotalScattering * mieScatteringCoefficient * miePhase)
         * 22.0;
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
    uint global_data_offset;
}
in_push_constants;

void main()
{
    const GlobalGpuData globalData = in_global_gpu_data[in_push_constants.global_data_offset].data;

    const vec3 rayDir = normalize(
        vec3(globalData.camera_forward_vector.xyz)
        + ndc.x * vec3(globalData.camera_right_vector.xyz) * globalData.aspect_ratio * globalData.tan_half_fov_y
        + ndc.y * vec3(globalData.camera_up_vector) * globalData.tan_half_fov_y);

    vec3  sunDir = normalize(vec3(-1.0, 0.0, 0.0));
    float time   = globalData.time_alive / 10.0;
    sunDir       = normalize(vec3(0.0, sin(time), -cos(time)));

    const vec3 cloudColor      = Sky(vec3(0.0), rayDir, globalData.time_alive, sunDir);
    const vec3 atmosphereColor = atmosphere(vec3(0.0), rayDir, sunDir);

    out_color = vec4(atmosphereColor * cloudColor, 1.0);
}