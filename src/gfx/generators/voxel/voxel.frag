#version 460

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_float32 : require

layout(push_constant) uniform PushConstants
{
    uint global_data_offset;
    uint brick_data_offset;
}
in_push_constants;

layout(set = 0, binding = 4) readonly buffer GlobalGpuBricks
{
    uint data[16];
}
in_global_bricks[];

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
    vec3  p           = vec3(c) + vec3(0.5);
    float outerSphere = sdSphere(p, 25.0);
    float d           = min(max(-sdSphere(p, 4.6), sdBox(p, vec3(4.0))), -outerSphere);

    if (outerSphere > 0.0)
    {
        discard;
    }

    return d < 0.0;
}

bool getBlockVoxel(ivec3 c)
{
    if (any(lessThan(c, ivec3(0))) || any(greaterThan(c, ivec3(7))))
    {
        return false;
    }

    int index = c.y * 8 * 8 + c.z * 8 + c.x;
    return ((in_global_bricks[in_push_constants.brick_data_offset].data[index / 32] >> (index % 32)) & 1) != 0;
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

struct WorldTraceResult
{
    vec3 brick_local_fragment_position;
    vec3 normal;
    vec3 local_voxel_uvw;
    // TODO: normal
    // TODO: brick UV
};

WorldTraceResult traceBrick(vec3 rayPos, vec3 rayDir)
{
    vec3       mapPos    = floor(rayPos);
    vec3       raySign   = sign(rayDir);
    const vec3 deltaDist = 1.0 / rayDir;
    vec3       sideDist  = ((mapPos - rayPos) + 0.5 + raySign * 0.5) * deltaDist;
    vec3       mask      = stepMask(sideDist);

    for (int i = 0; i < MAX_RAY_STEPS; i++)
    {
        if (getBlockVoxel(ivec3(mapPos)))
        {
            // Okay, we've struck a voxel, let's do a ray-cube intersection to determine other parameters)
            vec3  mini                              = ((mapPos - rayPos) + 0.5 - 0.5 * vec3(raySign)) * deltaDist;
            float rayTraversalDistanceSinceFragment = max(mini.x, max(mini.y, mini.z));
            vec3  intersectionPositionBrick         = rayPos + rayDir * rayTraversalDistanceSinceFragment;
            vec3  voxelLocalUVW3D                   = intersectionPositionBrick - mapPos;

            if (mapPos == floor(rayPos)) // Handle edge case where camera origin is inside of block
            {
                voxelLocalUVW3D = rayPos - mapPos;
            }

            vec3 normal = vec3(0.0);
            if (mini.x > mini.y && mini.x > mini.z)
            {
                normal.x = -raySign.x;
            }
            else if (mini.y > mini.z)
            {
                normal.y = -raySign.y;
            }
            else
            {
                normal.z = -raySign.z;
            }

            return WorldTraceResult(intersectionPositionBrick, normal, voxelLocalUVW3D);
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    discard;
}

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

layout(set = 0, binding = 4) readonly buffer GlobalGpuDataBuffer
{
    GlobalGpuData data;
}
in_global_gpu_data[];

layout(location = 0) in vec3 in_uvw;
layout(location = 1) in vec3 in_world_position;
layout(location = 2) in vec3 in_cube_corner_location;

layout(location = 0) out vec4 out_color;

layout(depth_any) out float gl_FragDepth;

void main()
{
    vec3       origin = in_world_position;
    const vec3 dir    = normalize(
        in_world_position - in_global_gpu_data[in_push_constants.global_data_offset].data.camera_position.xyz);

    const WorldTraceResult result = traceBrick(in_uvw * 8, dir);

    out_color = vec4(result.local_voxel_uvw, 1.0);
    // out_color = vec4(, 1.0);

    // TODO: integrate tracing with proper fragment position

    vec4 clipPos = in_global_gpu_data[in_push_constants.global_data_offset].data.view_projection_matrix
                 * vec4(result.brick_local_fragment_position + in_cube_corner_location, float(1.0));
    gl_FragDepth = (clipPos.z / clipPos.w);

    // out_color = vec4(in_uvw, 1.0);
}