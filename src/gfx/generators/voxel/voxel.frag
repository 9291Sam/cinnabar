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
    uint chunk_bricks_offset;
}
in_push_constants;

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

layout(set = 0, binding = 3) readonly uniform GlobalGpuDataBuffer
{
    GlobalGpuData data;
}
in_global_gpu_data[];

struct BooleanBrick
{
    uint data[16];
};

layout(set = 0, binding = 4) readonly buffer GlobalGpuBricks
{
    BooleanBrick data[];
}
in_global_bricks[];

struct ChunkBrickStorage
{
    uint16_t data[8][8][8];
};

layout(set = 0, binding = 4) readonly buffer GlobalChunkBrickStorage
{
    ChunkBrickStorage storage[];
}
in_global_chunk_bricks[];

const int MAX_RAY_STEPS = 256;

bool getBlockVoxel(ivec3 c)
{
    vec3 f = vec3(c);

    if (all(greaterThanEqual(c, ivec3(0))) && all(lessThanEqual(c, ivec3(63))))
    {
        // return true;
        uvec3 bC = c / 8;
        uvec3 bP = c % 8;

        const uint16_t brickPointer =
            in_global_chunk_bricks[in_push_constants.chunk_bricks_offset].storage[0].data[bC.x][bC.y][bC.z];

        // if (brickPointer == uint16_t(in_global_gpu_data[0].data.time_alive * 64) % 512)
        // {
        //     return true;
        // }

        uint       linearIndex = bP.x + (8 * bP.y) + (64 * bP.z);
        const uint idx         = linearIndex / 32;
        const uint bit         = linearIndex % 32;

        return (in_global_bricks[in_push_constants.brick_data_offset].data[uint(brickPointer)].data[idx] & (1u << bit))
            != 0;

        // return in_global_bricks[in_push_constants.brick_data_offset].data[uint(brickPointer)]

        // return sin(f.x / 16) * 16 + cos(-1.25 + f.z / 16) * 16 > c.y - 10;
        // return (c.z + c.z) / 2 > c.y;
    }

    return false;

    // int index = c.y * 8 * 8 + c.z * 8 + c.x;
    // return ((in_global_bricks[in_push_constants.brick_data_offset].data[index / 32] >> (index % 32)) & 1) != 0;
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
    vec3 chunk_local_fragment_position;
    vec3 voxel_normal;
    vec3 local_voxel_uvw;
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
        const ivec3 integerPos = ivec3(mapPos);

        if (getBlockVoxel(integerPos))
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

        if (any(lessThan(integerPos, ivec3(-1))) || any(greaterThan(integerPos, ivec3(64))))
        {
            discard;
        }

        mask = stepMask(sideDist);
        mapPos += mask * raySign;
        sideDist += mask * raySign * deltaDist;
    }

    discard;
}

layout(location = 0) in vec3 in_uvw;
layout(location = 1) in vec3 in_world_position;
layout(location = 2) in vec3 in_cube_corner_location;

layout(location = 0) out vec4 out_color;

layout(depth_greater) out float gl_FragDepth;

#define VERDIGRIS_EPSILON_MULTIPLIER 0.00001

bool isApproxEqual(const float a, const float b)
{
    const float maxMagnitude = max(abs(a), abs(b));
    const float epsilon      = maxMagnitude * VERDIGRIS_EPSILON_MULTIPLIER * 0.1;

    return abs(a - b) < epsilon;
}

struct IntersectionResult
{
    bool  intersection_occurred;
    float maybe_distance;
    vec3  maybe_hit_point;
    vec3  maybe_normal;
    vec4  maybe_color;
};

IntersectionResult IntersectionResult_getMiss()
{
    IntersectionResult result;
    result.intersection_occurred = false;

    return result;
}

struct Cube
{
    vec3  center;
    float edge_length;
};

struct Ray
{
    vec3 origin;
    vec3 direction;
};

vec3 Ray_at(in Ray self, in float t)
{
    return self.origin + self.direction * t;
}

IntersectionResult Cube_tryIntersectFast(const Cube self, in Ray ray)
{
    float tmin, tmax, tymin, tymax, tzmin, tzmax;

    vec3 invdir = 1 / ray.direction;

    vec3 bounds[2] = vec3[2](self.center - self.edge_length / 2, self.center + self.edge_length / 2);

    tmin  = (bounds[int(invdir[0] < 0)].x - ray.origin.x) * invdir.x;
    tmax  = (bounds[1 - int(invdir[0] < 0)].x - ray.origin.x) * invdir.x;
    tymin = (bounds[int(invdir[1] < 0)].y - ray.origin.y) * invdir.y;
    tymax = (bounds[1 - int(invdir[1] < 0)].y - ray.origin.y) * invdir.y;

    if ((tmin > tymax) || (tymin > tmax))
    {
        return IntersectionResult_getMiss();
    }

    if (tymin > tmin)
    {
        tmin = tymin;
    }
    if (tymax < tmax)
    {
        tmax = tymax;
    }

    tzmin = (bounds[int(invdir[2] < 0)].z - ray.origin.z) * invdir.z;
    tzmax = (bounds[1 - int(invdir[2] < 0)].z - ray.origin.z) * invdir.z;

    if ((tmin > tzmax) || (tzmin > tmax))
    {
        return IntersectionResult_getMiss();
    }

    if (tzmin > tmin)
    {
        tmin = tzmin;
    }
    if (tzmax < tmax)
    {
        tmax = tzmax;
    }

    if (tmin < 0.0)
    {
        // The intersection point is behind the ray's origin, consider it a miss
        return IntersectionResult_getMiss();
    }

    IntersectionResult result;
    result.intersection_occurred = true;

    // Calculate the normal vector based on which face is hit
    vec3 hit_point         = ray.origin + tmin * ray.direction; // TODO: inverse?
    result.maybe_hit_point = hit_point;

    result.maybe_distance = length(ray.origin - hit_point);
    vec3 normal;

    if (isApproxEqual(hit_point.x, bounds[1].x))
    {
        normal = vec3(-1.0, 0.0, 0.0); // Hit right face
    }
    else if (isApproxEqual(hit_point.x, bounds[0].x))
    {
        normal = vec3(1.0, 0.0, 0.0); // Hit left face
    }
    else if (isApproxEqual(hit_point.y, bounds[1].y))
    {
        normal = vec3(0.0, -1.0, 0.0); // Hit top face
    }
    else if (isApproxEqual(hit_point.y, bounds[0].y))
    {
        normal = vec3(0.0, 1.0, 0.0); // Hit bottom face
    }
    else if (isApproxEqual(hit_point.z, bounds[1].z))
    {
        normal = vec3(0.0, 0.0, -1.0); // Hit front face
    }
    else if (isApproxEqual(hit_point.z, bounds[0].z))
    {
        normal = vec3(0.0, 0.0, 1.0); // Hit back face
    }

    result.maybe_normal = normal;
    result.maybe_color  = vec4(0.0, 1.0, 1.0, 1.0);

    return result;
}

bool Cube_contains(const Cube self, const vec3 point)
{
    const vec3 p0 = self.center - (self.edge_length / 2);
    const vec3 p1 = self.center + (self.edge_length / 2);

    if (all(lessThan(p0, point)) && all(lessThan(point, p1)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void main()
{
    const Cube c = Cube(in_cube_corner_location + 32, 64);

    const vec3 camera_position = in_global_gpu_data[in_push_constants.global_data_offset].data.camera_position.xyz;

    const vec3 dir = normalize(
        in_world_position - in_global_gpu_data[in_push_constants.global_data_offset].data.camera_position.xyz);

    Ray ray = Ray(camera_position, dir);

    IntersectionResult res = Cube_tryIntersectFast(c, ray);

    const vec3 box_corner_negative = in_cube_corner_location;
    const vec3 box_corner_positive = in_cube_corner_location + 64;

    const bool is_camera_inside_box = Cube_contains(c, camera_position);

    const vec3 traversalRayOrigin =
        is_camera_inside_box ? (camera_position - box_corner_negative) : (res.maybe_hit_point - box_corner_negative);

    const WorldTraceResult result = traceBrick(traversalRayOrigin, dir);

    out_color = vec4(result.voxel_normal / 2 + 0.5, 1.0);

    vec4 clipPos = in_global_gpu_data[in_push_constants.global_data_offset].data.view_projection_matrix
                 * vec4(result.chunk_local_fragment_position + in_cube_corner_location, float(1.0));
    gl_FragDepth = (clipPos.z / clipPos.w);
}