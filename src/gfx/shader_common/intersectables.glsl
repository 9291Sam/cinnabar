#ifndef SRC_GFX_SHADER_COMMON_INTERSECTABLES_GLSL
#define SRC_GFX_SHADER_COMMON_INTERSECTABLES_GLSL

#define CINNABAR_EPSILON 0.00001

bool isApproxEqual(const float a, const float b)
{
    const float maxMagnitude = max(abs(a), abs(b));
    const float epsilon      = maxMagnitude * CINNABAR_EPSILON * 0.1;

    return abs(a - b) < epsilon;
}

struct IntersectionResult
{
    bool  intersection_occurred;
    float maybe_distance;
    vec3  maybe_hit_point;
    vec3  maybe_normal;
    float t_far;
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
    vec3 hit_point         = ray.origin + tmin * ray.direction;
    result.maybe_hit_point = hit_point;

    result.t_far = tmax;

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

#endif // SRC_GFX_SHADER_COMMON_INTERSECTABLES_GLSL