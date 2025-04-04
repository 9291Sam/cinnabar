#pragma once

#define CINNABAR_EPSILON 0.00001

bool isApproxEqual(const float a, const float b)
{
    const float maxMagnitude = max(abs(a), abs(b));
    const float epsilon      = maxMagnitude * CINNABAR_EPSILON * 0.1;

    return abs(a - b) < epsilon;
}

struct IntersectionResult
{
    __init()
    {
        this.intersection_occurred = false;
        this.maybe_distance = 0.0;
        this.maybe_hit_point = float3(0.0);
        this.maybe_normal = float3(0.0);
        this.t_far = 0.0;
    }
    
    bool  intersection_occurred;
    float maybe_distance;
    float3  maybe_hit_point;
    float3  maybe_normal;
    float t_far;

    static IntersectionResult getMiss()
    {
        return IntersectionResult();
    }
};


struct Ray
{
    float3 origin;
    float3 direction;

    float3 at(float t)
    {
        return this.origin + this.direction * t;
    }
};

struct Cube
{
    float3  center;
    float edge_length;

    IntersectionResult tryIntersect(Ray ray)
    {
        float tmin, tmax, tymin, tymax, tzmin, tzmax;

        float3 invdir = 1 / ray.direction;

        float3 bounds[2] = float3[2](this.center - this.edge_length / 2, this.center + this.edge_length / 2);

        tmin  = (bounds[int(invdir[0] < 0)].x - ray.origin.x) * invdir.x;
        tmax  = (bounds[1 - int(invdir[0] < 0)].x - ray.origin.x) * invdir.x;
        tymin = (bounds[int(invdir[1] < 0)].y - ray.origin.y) * invdir.y;
        tymax = (bounds[1 - int(invdir[1] < 0)].y - ray.origin.y) * invdir.y;

        if ((tmin > tymax) || (tymin > tmax))
        {
            return IntersectionResult::getMiss();
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
            return IntersectionResult::getMiss();
        }

        if (tzmin > tmin)
        {
            tmin = tzmin;
        }
        if (tzmax < tmax)
        {
            tmax = tzmax;
        }

        if (tmin < 0.0 && tmax < 0.0)
        {
            // The intersection point is behind the ray's origin, consider it a miss
            return IntersectionResult::getMiss();
        }

        if (tmin > tmax)
        {
            float temp = tmax;
            tmax       = tmin;
            tmin       = temp;
        }

        IntersectionResult result;
        result.intersection_occurred = true;

        // Calculate the normal vector based on which face is hit
        float3 hit_point         = ray.origin + tmin * ray.direction;
        result.maybe_hit_point = hit_point;

        result.t_far = tmax;

        result.maybe_distance = length(ray.origin - hit_point);
        float3 normal;

        if (isApproxEqual(hit_point.x, bounds[1].x))
        {
            normal = float3(-1.0, 0.0, 0.0); // Hit right face
        }
        else if (isApproxEqual(hit_point.x, bounds[0].x))
        {
            normal = float3(1.0, 0.0, 0.0); // Hit left face
        }
        else if (isApproxEqual(hit_point.y, bounds[1].y))
        {
            normal = float3(0.0, -1.0, 0.0); // Hit top face
        }
        else if (isApproxEqual(hit_point.y, bounds[0].y))
        {
            normal = float3(0.0, 1.0, 0.0); // Hit bottom face
        }
        else if (isApproxEqual(hit_point.z, bounds[1].z))
        {
            normal = float3(0.0, 0.0, -1.0); // Hit front face
        }
        else if (isApproxEqual(hit_point.z, bounds[0].z))
        {
            normal = float3(0.0, 0.0, 1.0); // Hit back face
        }

        result.maybe_normal = normal;

        return result;
    }

    bool contains(float3 point)
    {
        const float3 p0 = this.center - (this.edge_length / 2);
        const float3 p1 = this.center + (this.edge_length / 2);

        if (all(p0 < point) && all(point < p1))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
};
