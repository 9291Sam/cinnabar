#version 460

float random(in vec2 uv)
{
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(in vec2 uv)
{
    vec2 i = floor(uv);
    vec2 f = fract(uv);
    f      = f * f * (3. - 2. * f);

    float lb = random(i + vec2(0., 0.));
    float rb = random(i + vec2(1., 0.));
    float lt = random(i + vec2(0., 1.));
    float rt = random(i + vec2(1., 1.));

    return mix(mix(lb, rb, f.x), mix(lt, rt, f.x), f.y);
}

#define OCTAVES 8
float fbm(in vec2 uv)
{
    float value     = 0.;
    float amplitude = .5;

    for (int i = 0; i < OCTAVES; i++)
    {
        value += noise(uv) * amplitude;

        amplitude *= .5;

        uv *= 2.;
    }

    return value;
}

vec3 Sky(in vec3 ro, in vec3 rd, in float iTime)
{
    const float SC = 1e5;

    // Calculate sky plane
    float dist = (SC - ro.y) / rd.y;
    vec2  p    = (ro + dist * rd).xz;
    p *= 1.2 / SC;

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    vec3  lightDir = normalize(vec3(-.8, .15, -.3));
    float sundot   = clamp(dot(rd, lightDir), 0.0, 1.0);

    vec3 cloudCol = vec3(1.);
    // vec3 skyCol = vec3(.6, .71, .85) - rd.y * .2 * vec3(1., .5, 1.) + .15 * .5;
    vec3 skyCol   = vec3(0.3, 0.5, 0.85) - rd.y * rd.y * 0.5;
    skyCol        = mix(skyCol, 0.85 * vec3(0.7, 0.75, 0.85), pow(1.0 - max(rd.y, 0.0), 4.0));

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
    skyCol = mix(skyCol, 0.68 * vec3(.418, .394, .372), pow(1.0 - max(rd.y, 0.0), 16.0));

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
    float fov;         // Field of view in radians (vertical)
    float time_alive;
}
in_push_constants;

void main()
{
    const float fovScale = tan(in_push_constants.fov * 0.5);

    const vec3 rayDir = normalize(
        vec3(in_push_constants.camForward)
        + ndc.x * vec3(in_push_constants.camRight) * in_push_constants.aspectRatio * fovScale
        + ndc.y * vec3(in_push_constants.camUp) * fovScale);

    out_color = vec4(Sky(vec3(0.0), rayDir, in_push_constants.time_alive), 1.0);
}