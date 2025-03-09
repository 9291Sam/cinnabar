#version 460

const int MAX_RAY_STEPS = 64;

#define B8(x)                                                                                                          \
    (bool(x & 0x0000000F) ? 1 : 0) + (bool(x & 0x000000F0) ? 2 : 0) + (bool(x & 0x00000F00) ? 4 : 0)                   \
        + (bool(x & 0x0000F000) ? 8 : 0) + (bool(x & 0x000F0000) ? 16 : 0) + (bool(x & 0x00F00000) ? 32 : 0)           \
        + (bool(x & 0x0F000000) ? 64 : 0) + (bool(x & 0xF0000000) ? 128 : 0)

#define B32(x0, x1, x2, x3) (B8(x3) << 0x18) + (B8(x2) << 0x10) + (B8(x1) << 0x08) + (B8(x0) << 0x00)

int block[8 * 2] = int[](
    B32(0x11111111, 0x10000001, 0x10000001, 0x10000001),
    B32(0x10000001, 0x10000001, 0x10000001, 0x11111111),

    B32(0x10000001, 0x01000010, 0x00000000, 0x00000000),
    B32(0x00000000, 0x00000000, 0x01000010, 0x10000001),

    B32(0x10000001, 0x00000000, 0x00100100, 0x00000000),
    B32(0x00000000, 0x00100100, 0x00000000, 0x10000001),

    B32(0x10000001, 0x00000000, 0x00000000, 0x00011000),
    B32(0x00011000, 0x00000000, 0x00000000, 0x10000001),

    B32(0x10000001, 0x00000000, 0x00000000, 0x00011000),
    B32(0x00011000, 0x00000000, 0x00000000, 0x10000001),

    B32(0x10000001, 0x00000000, 0x00100100, 0x00000000),
    B32(0x00000000, 0x00100100, 0x00000000, 0x10000001),

    B32(0x10000001, 0x01000010, 0x00000000, 0x00000000),
    B32(0x00000000, 0x00000000, 0x01000010, 0x10000001),

    B32(0x11111111, 0x10000001, 0x10000001, 0x10000001),
    B32(0x10000001, 0x10000001, 0x10000001, 0x11111111));

float sdSphere(vec3 p, float d)
{
    return length(p) - d;
}

float sdBox(vec3 p, vec3 b)
{
    vec3 d = abs(p) - b;

    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

bool getVoxel(ivec3 c)
{
    vec3  p = vec3(c) + vec3(0.5);
    float d = min(max(-sdSphere(p, 7.5), sdBox(p, vec3(6.0))), -sdSphere(p, 25.0));

    return d < 0.0;
}

bool getBlockVoxel(ivec3 c)
{
    int index = c.y * 8 * 8 + c.z * 8 + c.x;
    return ((block[index / 32] >> (index % 32)) & 1) != 0;
}

vec2 rotate2d(vec2 v, float a)
{
    float sinA = sin(a);
    float cosA = cos(a);

    return vec2(v.x * cosA - v.y * sinA, v.y * cosA + v.x * sinA);
}

vec3 stepMask(vec3 sideDist)
{
    bvec3 mask;
    bvec3 b1 = lessThan(sideDist.xyz, sideDist.yzx);
    bvec3 b2 = lessThanEqual(sideDist.xyz, sideDist.zxy);
    mask.z   = b1.z && b2.z;
    mask.x   = b1.x && b2.x;
    mask.y   = b1.y && b2.y;
    if (!any(mask)) // Thank you Spalmer
    {
        mask.z = true;
    }

    return vec3(mask);
}

vec4 traceBlock(vec3 rayPos, vec3 rayDir, vec3 iMask)
{
    rayPos         = clamp(rayPos, vec3(0.0001), vec3(7.9999));
    vec3 mapPos    = floor(rayPos);
    vec3 raySign   = sign(rayDir);
    vec3 deltaDist = 1.0 / rayDir;
    vec3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    vec3 mask      = iMask;

    int i = 0;

    while (mapPos.x <= 7.0 && mapPos.x >= 0.0 && mapPos.y <= 7.0 && mapPos.y >= 0.0 && mapPos.z <= 7.0
           && mapPos.z >= 0.0 && i++ < 64)
    {
        if (getBlockVoxel(ivec3(mapPos)))
        {
            return vec4(floor(mapPos) / 8.0, 1.0);
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return vec4(0.0);
}

vec4 traceWorld(vec3 rayPos, vec3 rayDir)
{
    vec3 mapPos    = floor(rayPos);
    vec3 raySign   = sign(rayDir);
    vec3 deltaDist = 1.0 / rayDir;
    vec3 sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    vec3 mask      = stepMask(sideDist);

    for (int i = 0; i < MAX_RAY_STEPS; i++)
    {
        if (getVoxel(ivec3(mapPos)))
        {
            vec3  mini      = ((mapPos - rayPos) + 0.5 - 0.5 * vec3(raySign)) * deltaDist;
            float d         = max(mini.x, max(mini.y, mini.z));
            vec3  intersect = rayPos + rayDir * d;
            vec3  uv3d      = intersect - mapPos;

            if (mapPos == floor(rayPos)) // Handle edge case where camera origin is inside of block
            {
                uv3d = rayPos - mapPos;
            }

            vec4 hit = traceBlock(uv3d * 8.0, rayDir, mask);

            if (hit.a > 0.95)
            {
                return hit;
            }
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    return vec4(0.0);
}

layout(push_constant) uniform PushConstants
{
    mat4 model_view_proj;
    vec4 camera_position;
}
in_push_constants;

layout(location = 0) in vec3 in_uvw;
layout(location = 1) in vec3 in_world_position;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3       origin = in_world_position;
    const vec3 dir    = normalize(in_world_position - in_push_constants.camera_position.xyz);

    out_color = traceWorld(origin, dir);
}