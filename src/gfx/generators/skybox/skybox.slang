

#include "globals.slang"

struct VertexOutput
{
    float2 ndc : COLOR0;
    float4 position : SV_Position;
};

[shader("vertex")] VertexOutput vertexMain(uint VertexIndex : SV_VertexID)
{
    const Array<float2, 3> joint_position_uv = {float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0)};

    VertexOutput o;

    o.ndc      = joint_position_uv[VertexIndex];
    o.position = float4(joint_position_uv[VertexIndex], 0.0000000000001, 1.0);

    return o;
}

float4 permute(float4 x)
{
    return fmod(((x * 34.0) + 1.0) * x, 289.0);
}

float2 fade(float2 t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float noise(float2 P)
{
    float4 Pi  = floor(P.xyxy) + float4(0.0, 0.0, 1.0, 1.0);
    float4 Pf  = fract(P.xyxy) - float4(0.0, 0.0, 1.0, 1.0);
    Pi         = fmod(Pi, 289.0); // To avoid truncation effects permutation
    float4 ix  = Pi.xzxz;
    float4 iy  = Pi.yyww;
    float4 fx  = Pf.xzxz;
    float4 fy  = Pf.yyww;
    float4 i   = permute(permute(ix) + iy);
    float4 gx  = 2.0 * fract(i * 0.0243902439) - 1.0; // 1/41 = 0.024...
    float4 gy  = abs(gx) - 0.5;
    float4 tx  = floor(gx + 0.5);
    gx         = gx - tx;
    float2 g00 = float2(gx.x, gy.x);
    float2 g10 = float2(gx.y, gy.y);
    float2 g01 = float2(gx.z, gy.z);
    float2 g11 = float2(gx.w, gy.w);
    float4 norm =
        1.79284291400159 - 0.85373472095314 * float4(dot(g00, g00), dot(g01, g01), dot(g10, g10), dot(g11, g11));
    g00 *= norm.x;
    g01 *= norm.y;
    g10 *= norm.z;
    g11 *= norm.w;
    float  n00     = dot(g00, float2(fx.x, fy.x));
    float  n10     = dot(g10, float2(fx.y, fy.y));
    float  n01     = dot(g01, float2(fx.z, fy.z));
    float  n11     = dot(g11, float2(fx.w, fy.w));
    float2 fade_xy = fade(Pf.xy);
    float2 n_x     = lerp(float2(n00, n01), float2(n10, n11), fade_xy.x);
    float  n_xy    = lerp(n_x.x, n_x.y, fade_xy.y);
    return 2.4 * n_xy;
}

#define OCTAVES 4
float fbm(float2 st)
{
    // Initial values
    float value     = 0.0;
    float amplitude = .5;
    float frequency = 0.;
    //
    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++)
    {
        value += amplitude * noise(st) * noise(st.yx + float2(3040.20492, -2939.3)) * 3;
        st *= 2.41;
        st += float2(23.423, 293.4);
        amplitude *= .8;
    }
    return value;
}
float3 Sky(float3 ro, float3 rd, float iTime, float3 sunDir)
{
    const float SC = 1e5;

    if (rd.y < 0.0)
    {
        rd.x *= -1.0;

        rd.z *= -1.0;
    }

    // Calculate sky plane
    float  dist = (SC - ro.y) / rd.y;
    float2 p    = (ro + dist * rd).xz;
    p *= 4.2 / SC;

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    float3 lightDir = normalize(sunDir * ((rd.y < 0.0) ? float3(-1, 1, -1) : float3(1.0)));
    float  sundot   = clamp(dot(rd, lightDir), 0.0, 1.0);

    float3 cloudCol = float3(1.);
    // float3 skyCol = float3(.6, .71, .85) - rd.y * .2 * float3(1., .5, 1.) + .15 * .5;
    float3 skyCol   = float3(0.3, 0.5, 0.85); //  - rd.y * rd.y * 0.5;
    // skyCol        = mix(skyCol, 0.85 * float3(0.7, 0.75, 0.85), pow(1.0 - max(rd.y, 0.0), 4.0));

    // sun
    float3 sun = 0.25 * float3(1.0, 0.7, 0.4) * pow(sundot, 5.0);
    sun += 0.25 * float3(1.0, 0.8, 0.6) * pow(sundot, 64.0);
    sun += 0.2 * float3(1.0, 0.8, 0.6) * pow(sundot, 512.0);
    skyCol += sun;

    // clouds
    float t   = iTime * 0.1;
    float den = fbm(float2(p.x - t, p.y - t) + ((rd.y < 0.0) ? float2(302.04, -2838.3) : float2(0.0)));
    skyCol    = lerp(skyCol, cloudCol, smoothstep(0.0, 1.0, den));

    // horizon
    // skyCol = mix(skyCol, 0.68 * float3(.418, .394, .372), pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol;
}

float3 uncharted2_tonemap_partial(float3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 uncharted2_filmic(float3 v)
{
    float  exposure_bias = 2.0f;
    float3 curr          = uncharted2_tonemap_partial(v * exposure_bias);

    float3 W           = float3(11.2f);
    float3 white_scale = float3(1.0f) / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}

// https://iquilezles.org/articles/intersectors
float2 traceSphere(float3 ro, float3 rd, float3 ce, float ra)
{
    float3 oc = ro - ce;
    float  b  = dot(oc, rd);
    float  c  = dot(oc, oc) - ra * ra;
    float  h  = b * b - c;
    if (h < 0.0)
    {
        return float2(-1.0); // no intersection
    }
    h = sqrt(h);
    return float2(-b - h, -b + h);
}

// https://iquilezles.org/articles/intersectors
float2 boxIntersection(float3 ro, float3 rd, float3 boxSize, out float3 outNormal)
{
    float3 m  = 1.0 / rd; // can precompute if traversing a set of aligned boxes
    float3 n  = m * ro;   // can precompute if traversing a set of aligned boxes
    float3 k  = abs(m) * boxSize;
    float3 t1 = -n - k;
    float3 t2 = -n + k;
    float  tN = max(max(t1.x, t1.y), t1.z);
    float  tF = min(min(t2.x, t2.y), t2.z);
    if (tN > tF || tF < 0.0)
    {
        return float2(-1.0); // no intersection
    }
    outNormal = -sign(rd) * step(t1.yzx, t1.xyz) * step(t1.zxy, t1.xyz);
    return float2(tN, tF);
}

float scene(float3 ro, float3 rd)
{
    float res = -1.0;

    float3 rd2   = normalize(float3((-1.0 + 2.0 * float2(0.5)), -1.00));
    float  scale = 100.0 * 100.0;
    float3 pos   = normalize(rd2 + float3(0.0, 0.0, 0.0)) * (scale * 2.0) * 2.0 + float3(0.0, scale / 1.0, 0.0);

    float sphere = traceSphere(ro, rd, pos, scale).x;
    sphere       = sphere < 0.0 ? traceSphere(ro, rd, pos, scale).y : sphere;
    // res          = res < 0.0 ? sphere : ((sphere >= 0.0) ? min(res, sphere) : res);

    // float3 normal;
    // return boxIntersection(ro - rd2 * 80000.0, rd, float3(20000.0), normal).x;

    return res;
}

float3 atmosphere(float3 ro, float3 rd, float3 sd)
{
    const float  atmosphereRayleighHeight       = 8000.0;
    const float  atmosphereMieHeight            = 1200.0;
    const float  planetHeight                   = 6360e3;
    const float  atmosphereHeight               = planetHeight + atmosphereRayleighHeight; // 6420e3;
    const float3 atmospherePosition             = float3(0.0, -(planetHeight + 1000.0), 0.0);
    const float3 rayleighScatteringCoefficients = float3(5.8e-6, 1.35e-5, 3.31e-5);
    const float  mieScatteringCoefficient       = 2e-6;
    const float  pi                             = 3.141592;
    const float  g                              = 0.758;
    // const float3  rayleighScatteringCoefficients = float3(5.47e-6, 1.28e-5, 3.12e-5);

    float2 atmosphereSphere = traceSphere(ro, rd, atmospherePosition, atmosphereHeight);
    float2 planetSphere     = traceSphere(ro, rd, atmospherePosition, planetHeight);
    // float start            = (atmosphereSphere.x >= 0.0) ? atmosphereSphere.x : distance(ro, atmospherePosition);
    float  end              = (planetSphere.x >= 0.0) ? planetSphere.x : atmosphereSphere.y;
    if (end < 0.0)
    {
        return float3(0.0);
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

    float3 rayleighTotalScattering = float3(0.0), mieTotalScattering = float3(0.0);

    float dither = 0.0f; // fract(dot(float3(0.75487765, 0.56984026, 0.61803398875), float3(gl_FragCoord.xy, 0.0)));

    // DEBUG
    float3 tr = float3(0.0);
    // DEBUG

    const float viewStepCount  = 48.0 - 1.0;
    float       viewOdRayleigh = 0.0, viewOdMie = 0.0;
    float3      viewStart = (atmosphereSphere.x >= 0.0) ? (ro + rd * atmosphereSphere.x) : ro, viewEnd = ro + rd * end;
    float       viewSize = distance(viewStart, viewEnd) / viewStepCount;
    for (float viewStep = 0.0; viewStep <= viewStepCount; ++viewStep)
    {
        float3 viewSample =
            lerp(viewStart, viewEnd, (viewStep + dither) / viewStepCount); // + (rd * viewSize * dither);
        float viewSampleHeight = (distance(atmospherePosition, viewSample) - planetHeight);

        float viewSampleOdRayleigh = exp(-viewSampleHeight / atmosphereRayleighHeight) * viewSize;
        float viewSampleOdMie      = exp(-viewSampleHeight / atmosphereMieHeight) * viewSize;

        viewOdRayleigh += viewSampleOdRayleigh;
        viewOdMie += viewSampleOdMie;

        const float sunStepCount  = 8.0 - 1.0;
        float       sunOdRayleigh = 0.0, sunOdMie = 0.0;
        float       sunAtmosphereDistance = traceSphere(viewSample, sd, atmospherePosition, atmosphereHeight).y;

        float hit = scene(viewSample, sd);
        if (hit >= 0.0)
        {
            continue; // sunAtmosphereDistance = hit;
        }

        float3 sunStart = viewSample, sunEnd = viewSample + sd * sunAtmosphereDistance;
        float  sunSize = distance(sunStart, sunEnd) / sunStepCount;
        for (float sunStep = 0.0; sunStep <= sunStepCount; ++sunStep)
        {
            float3 sunSample = lerp(sunStart, sunEnd, (sunStep + dither) / sunStepCount); // + (sd * sunSize * dither);
            float  sunSampleHeight = (distance(atmospherePosition, sunSample) - planetHeight);

            sunOdRayleigh += exp(-sunSampleHeight / atmosphereRayleighHeight) * sunSize;
            sunOdMie += exp(-sunSampleHeight / atmosphereMieHeight) * sunSize;
        }

        float3 transmittance = exp(
            -(rayleighScatteringCoefficients * (viewOdRayleigh + sunOdRayleigh)
              + mieScatteringCoefficient * 1.11 * (viewOdMie + sunOdMie)));
        // DEBUG
        tr += transmittance / viewStepCount;
        // DEBUG

        rayleighTotalScattering += viewSampleOdRayleigh * transmittance;
        mieTotalScattering += viewSampleOdMie * transmittance;
    }
    float3 color = hit >= 0.0 ? float3(1.0, 0.0, 0.0) : float3(0.0, 0.0, 0.0);
    return /*color * clamp(tr, 0.0, 1.0) + */ (
               rayleighTotalScattering * rayleighScatteringCoefficients * rayleighPhase
               + mieTotalScattering * mieScatteringCoefficient * miePhase)
         * 32.0;
}

[shader("fragment")] float4 fragmentMain(float2 ndc : COLOR0)
    : COLOR0
{
    const float3 rayDir = normalize(
        float3(global_gpu_data[0].camera_forward_vector.xyz)
        + ndc.x * float3(global_gpu_data[0].camera_right_vector.xyz) * global_gpu_data[0].aspect_ratio
              * global_gpu_data[0].tan_half_fov_y
        + ndc.y * float3(global_gpu_data[0].camera_up_vector.xyz) * global_gpu_data[0].tan_half_fov_y);

    float3 sunDir = normalize(float3(-1.0, 0.0, 0.0));
    float  time   = global_gpu_data[0].time_alive / 10;
    sunDir        = normalize(float3(0.0, sin(time), -cos(time)));

    const float3 cloudColor      = Sky(float3(0.0), rayDir, global_gpu_data[0].time_alive, sunDir);
    const float3 atmosphereColor = atmosphere(float3(0.0), rayDir, sunDir);

    // discard;
    // return float4(rayDir, 1.0, 1.0);
    return float4(cloudColor * atmosphereColor, 1.0);

    // return float4(0.0);
}