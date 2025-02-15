#pragma once

#include "intersections.h"

// CHECKITOUT
/**
 * Computes a cosine-weighted random direction in a hemisphere.
 * Used for diffuse lighting.
 */
__host__ __device__
glm::vec3 calculateRandomDirectionInHemisphere(
        glm::vec3 normal, thrust::default_random_engine &rng) {
    thrust::uniform_real_distribution<float> u01(0, 1);

    float up = sqrt(u01(rng)); // cos(theta)
    float over = sqrt(1 - up * up); // sin(theta)
    float around = u01(rng) * TWO_PI;

    // Find a direction that is not the normal based off of whether or not the
    // normal's components are all equal to sqrt(1/3) or whether or not at
    // least one component is less than sqrt(1/3). Learned this trick from
    // Peter Kutz.

    glm::vec3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(1, 0, 0);
    } else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(0, 1, 0);
    } else {
        directionNotNormal = glm::vec3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    glm::vec3 perpendicularDirection1 =
        glm::normalize(glm::cross(normal, directionNotNormal));
    glm::vec3 perpendicularDirection2 =
        glm::normalize(glm::cross(normal, perpendicularDirection1));

    return up * normal
        + cos(around) * over * perpendicularDirection1
        + sin(around) * over * perpendicularDirection2;
}

/**
 * Scatter a ray with some probabilities according to the material properties.
 * For example, a diffuse surface scatters in a cosine-weighted hemisphere.
 * A perfect specular surface scatters in the reflected ray direction.
 * In order to apply multiple effects to one surface, probabilistically choose
 * between them.
 *
 * The visual effect you want is to straight-up add the diffuse and specular
 * components. You can do this in a few ways. This logic also applies to
 * combining other types of materias (such as refractive).
 *
 * - Always take an even (50/50) split between a each effect (a diffuse bounce
 *   and a specular bounce), but divide the resulting color of either branch
 *   by its probability (0.5), to counteract the chance (0.5) of the branch
 *   being taken.
 *   - This way is inefficient, but serves as a good starting point - it
 *     converges slowly, especially for pure-diffuse or pure-specular.
 * - Pick the split based on the intensity of each material color, and divide
 *   branch result by that branch's probability (whatever probability you use).
 *
 * This method applies its changes to the Ray parameter `ray` in place.
 * It also modifies the color `color` of the ray in place.
 *
 * You may need to change the parameter list for your purposes!
 */
__host__ __device__ void scatterRay(PathSegment& pathSegment,
                                    glm::vec3 intersect,
                                    glm::vec3 normal,
                                    glm::vec2 uv,
                                    const Material& m,
                                    const glm::vec3* texData,
                                    thrust::default_random_engine& rng) 
{
    glm::vec3 dir = pathSegment.ray.direction;

    if (m.hasRefractive)
    {
        float ior = m.indexOfRefraction;
        float cosAngle = glm::dot(dir, -normal);
        float fresnel = (1.f - ior) / (1.f + ior);
        fresnel *= fresnel;
        fresnel = fresnel + (1.f - fresnel) * pow((1.f - cosAngle), 5);

        thrust::uniform_real_distribution<float> u01(0, 1);
        if (u01(rng) < fresnel)
        {
            pathSegment.ray.origin = intersect;
            pathSegment.ray.direction = glm::reflect(dir, normal);
            pathSegment.color *= m.specular.color;
        }
        else
        {
            pathSegment.ray.origin = intersect + dir * .0002f;
            pathSegment.ray.direction = glm::refract(dir, normal, cosAngle > 0.f ? 1.f / ior : ior);
            pathSegment.color *= m.color;
        }
    }
    else if (m.hasReflective)
    {
        pathSegment.ray.origin = intersect;
        pathSegment.ray.direction = glm::reflect(dir, normal);
        pathSegment.color *= m.specular.color;
    }
    else
    {
        pathSegment.ray.origin = intersect;
        pathSegment.ray.direction = calculateRandomDirectionInHemisphere(normal, rng);
        glm::vec3 col = m.color;
        int offset = m.tex.offset;
        if (offset >= 0)
        {
            int w = m.tex.width;
            int x = uv.x * (w - 1);
            int y = uv.y * (m.tex.height - 1);
            col *= texData[offset + y * w + x];
            //col = glm::vec3(uv.x, uv.y, 0.95f);
        }
        pathSegment.color *= col;
    }
}
