#pragma once

#include "globals.slang" 

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
    f32  n00     = dot(g00, float2(fx.x, fy.x));
    f32  n10     = dot(g10, float2(fx.y, fy.y));
    f32  n01     = dot(g01, float2(fx.z, fy.z));
    f32  n11     = dot(g11, float2(fx.w, fy.w));
    float2 fade_xy = fade(Pf.xy);
    float2 n_x     = lerp(float2(n00, n01), float2(n10, n11), fade_xy.x);
    f32  n_xy    = lerp(n_x.x, n_x.y, fade_xy.y);
    return 2.4 * n_xy;
}

#define OCTAVES 4
float fbm(float2 st)
{
    // Initial values
    f32 value     = 0.0;
    f32 amplitude = .5;
    f32 frequency = 0.;
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
float3 Sky(float3 ro, float3 rd, f32 iTime, float3 sunDir)
{
    const f32 SC = 1e5;

    if (rd.y < 0.0)
    {
        rd.x *= -1.0;

        rd.z *= -1.0;
    }

    // Calculate sky plane
    f32  dist = (SC - ro.y) / rd.y;
    float2 p    = (ro + dist * rd).xz;
    p *= 4.2 / SC;

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    float3 lightDir = normalize(sunDir * ((rd.y < 0.0) ? float3(-1, 1, -1) : float3(1.0)));
    f32  sundot   = clamp(dot(rd, lightDir), 0.0, 1.0);

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
    f32 t   = iTime * 0.1;
    f32 den = fbm(float2(p.x - t, p.y - t) + ((rd.y < 0.0) ? float2(302.04, -2838.3) : float2(0.0)));
    skyCol    = lerp(skyCol, cloudCol, smoothstep(0.0, 1.0, den));

    // horizon
    // skyCol = mix(skyCol, 0.68 * float3(.418, .394, .372), pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol;
}

float3 uncharted2_tonemap_partial(float3 x)
{
    f32 A = 0.15f;
    f32 B = 0.50f;
    f32 C = 0.10f;
    f32 D = 0.20f;
    f32 E = 0.02f;
    f32 F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 uncharted2_filmic(float3 v)
{
    f32  exposure_bias = 2.0f;
    float3 curr          = uncharted2_tonemap_partial(v * exposure_bias);

    float3 W           = float3(11.2f);
    float3 white_scale = float3(1.0f) / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}

// https://iquilezles.org/articles/intersectors
float2 traceSphere(float3 ro, float3 rd, float3 ce, f32 ra)
{
    float3 oc = ro - ce;
    f32  b  = dot(oc, rd);
    f32  c  = dot(oc, oc) - ra * ra;
    f32  h  = b * b - c;
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
    f32  tN = max(max(t1.x, t1.y), t1.z);
    f32  tF = min(min(t2.x, t2.y), t2.z);
    outNormal = -sign(rd) * step(t1.yzx, t1.xyz) * step(t1.zxy, t1.xyz);
    if (tN > tF || tF < 0.0)
    {
        return float2(-1.0); // no intersection
    }
    return float2(tN, tF);
}

float scene(float3 ro, float3 rd)
{
    f32 res = -1.0;

    float3 rd2   = normalize(float3((-1.0 + 2.0 * float2(0.5)), -1.00));
    f32  scale = 100.0 * 100.0;
    float3 pos   = normalize(rd2 + float3(0.0, 0.0, 0.0)) * (scale * 2.0) * 2.0 + float3(0.0, scale / 1.0, 0.0);

    f32 sphere = traceSphere(ro, rd, pos, scale).x;
    sphere       = sphere < 0.0 ? traceSphere(ro, rd, pos, scale).y : sphere;
    // res          = res < 0.0 ? sphere : ((sphere >= 0.0) ? min(res, sphere) : res);

    // float3 normal;
    // return boxIntersection(ro - rd2 * 80000.0, rd, float3(20000.0), normal).x;

    return res;
}

float3 atmosphere(float3 ro, float3 rd, float3 sd)
{
    const f32  atmosphereRayleighHeight       = 8000.0;
    const f32  atmosphereMieHeight            = 1200.0;
    const f32  planetHeight                   = 6360e3;
    const f32  atmosphereHeight               = planetHeight + atmosphereRayleighHeight; // 6420e3;
    const float3 atmospherePosition             = float3(0.0, -(planetHeight + 1000.0), 0.0);
    const float3 rayleighScatteringCoefficients = float3(5.8e-6, 1.35e-5, 3.31e-5);
    const f32  mieScatteringCoefficient       = 2e-6;
    const f32  pi                             = 3.141592;
    const f32  g                              = 0.758;
    // const float3  rayleighScatteringCoefficients = float3(5.47e-6, 1.28e-5, 3.12e-5);

    float2 atmosphereSphere = traceSphere(ro, rd, atmospherePosition, atmosphereHeight);
    float2 planetSphere     = traceSphere(ro, rd, atmospherePosition, planetHeight);
    // f32 start            = (atmosphereSphere.x >= 0.0) ? atmosphereSphere.x : distance(ro, atmospherePosition);
    f32  end              = (planetSphere.x >= 0.0) ? planetSphere.x : atmosphereSphere.y;
    if (end < 0.0)
    {
        return float3(0.0);
    }

    f32 hit = scene(ro, rd);
    if (hit >= 0.0)
    {
        end = hit - 10000.0;
    }

    f32 theta         = dot(rd, sd);
    f32 theta2        = theta * theta;
    f32 g2            = g * g;
    f32 rayleighPhase = (3.0 / (16.0 * pi)) * (1.0 + theta2);
    f32 miePhase      = (1.0 - g2) / (4.0 * pi * pow(1.0 + g2 - 2.0 * g * theta, 1.5));

    float3 rayleighTotalScattering = float3(0.0), mieTotalScattering = float3(0.0);

    f32 dither = 0.0f; // fract(dot(float3(0.75487765, 0.56984026, 0.61803398875), float3(gl_FragCoord.xy, 0.0)));

    // DEBUG
    float3 tr = float3(0.0);
    // DEBUG

    const f32 viewStepCount  = 48.0 - 1.0;
    f32       viewOdRayleigh = 0.0, viewOdMie = 0.0;
    float3      viewStart = (atmosphereSphere.x >= 0.0) ? (ro + rd * atmosphereSphere.x) : ro, viewEnd = ro + rd * end;
    f32       viewSize = distance(viewStart, viewEnd) / viewStepCount;
    for (f32 viewStep = 0.0; viewStep <= viewStepCount; ++viewStep)
    {
        float3 viewSample =
            lerp(viewStart, viewEnd, (viewStep + dither) / viewStepCount); // + (rd * viewSize * dither);
        f32 viewSampleHeight = (distance(atmospherePosition, viewSample) - planetHeight);

        f32 viewSampleOdRayleigh = exp(-viewSampleHeight / atmosphereRayleighHeight) * viewSize;
        f32 viewSampleOdMie      = exp(-viewSampleHeight / atmosphereMieHeight) * viewSize;

        viewOdRayleigh += viewSampleOdRayleigh;
        viewOdMie += viewSampleOdMie;

        const f32 sunStepCount  = 8.0 - 1.0;
        f32       sunOdRayleigh = 0.0, sunOdMie = 0.0;
        f32       sunAtmosphereDistance = traceSphere(viewSample, sd, atmospherePosition, atmosphereHeight).y;

        f32 hit = scene(viewSample, sd);
        if (hit >= 0.0)
        {
            continue; // sunAtmosphereDistance = hit;
        }

        float3 sunStart = viewSample, sunEnd = viewSample + sd * sunAtmosphereDistance;
        f32  sunSize = distance(sunStart, sunEnd) / sunStepCount;
        for (f32 sunStep = 0.0; sunStep <= sunStepCount; ++sunStep)
        {
            float3 sunSample = lerp(sunStart, sunEnd, (sunStep + dither) / sunStepCount); // + (sd * sunSize * dither);
            f32  sunSampleHeight = (distance(atmospherePosition, sunSample) - planetHeight);

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

float3 totalSkybox(float3 rayDir, bool includeExpensive)
{

    float3 sunDir = normalize(float3(-1.0, 0.0, 0.0));
    f32  time   = global_gpu_data[UBO_GLOBAL_DATA].time_alive / 10;
    sunDir        = normalize(float3(0.0, sin(time), -cos(time)));
    sunDir.y = abs(sunDir.y);

    const float3 cloudColor      = Sky(float3(0.0), rayDir, global_gpu_data[UBO_GLOBAL_DATA].time_alive, sunDir);
    const float3 atmosphereColor = atmosphere(float3(0.0), rayDir, sunDir);

    return cloudColor * (includeExpensive ? atmosphereColor : float3(1));
}