#include "light_influence_storage.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "gfx/transform.hpp"
#include "util/logger.hpp"
#include "util/timer.hpp"
#include <boost/geometry/algorithms/detail/intersects/interface.hpp>
#include <boost/geometry/algorithms/detail/overlaps/interface.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/distance_predicates.hpp>
#include <boost/geometry/index/predicates.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/io/io.hpp>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace gfx::generators::voxel
{

    namespace
    {
        bool doesCubeIntersectSphere(glm::vec3 sphereCenter, f32 radius, glm::vec3 cubeMin, glm::vec3 cubeMax)
        {
            const f32 closestX = std::clamp(sphereCenter.x, cubeMin.x, cubeMax.x);
            const f32 closestY = std::clamp(sphereCenter.y, cubeMin.y, cubeMax.y);
            const f32 closestZ = std::clamp(sphereCenter.z, cubeMin.z, cubeMax.z);

            const glm::vec3 closestPointBetweenSphereAndCube(closestX, closestY, closestZ);

            return glm::distance2(sphereCenter, closestPointBetweenSphereAndCube) <= radius * radius;
        }
    } // namespace

    LightInfluenceStorage::LightInfluenceStorage(usize maxLightId)
        : would_pack_benefit {false}
    {
        this->light_lookup.resize(maxLightId);
    }

    LightInfluenceStorage::~LightInfluenceStorage() = default;

    LightInfluenceStorage::R3Box getAABBFromLight(GpuRaytracedLight light)
    {
        // TODO: this isnt great but dont make a shitton of weak lights! use an area light
        // TODO: add area lights :skull:
        // TODO: generate a list of lights per chunk and then do that to get this number correctly
        const f32 maxInfluenceDistance = light.getMaxInfluenceDistance(1);

        const glm::vec3 lightPositionGLM = light.position_and_half_intensity_distance.xyz();

        const LightInfluenceStorage::R3Point minLightAABB {
            lightPositionGLM.x - maxInfluenceDistance,
            lightPositionGLM.y - maxInfluenceDistance,
            lightPositionGLM.z - maxInfluenceDistance};
        const LightInfluenceStorage::R3Point maxLightAABB {
            lightPositionGLM.x + maxInfluenceDistance,
            lightPositionGLM.y + maxInfluenceDistance,
            lightPositionGLM.z + maxInfluenceDistance};

        return LightInfluenceStorage::R3Box {minLightAABB, maxLightAABB};
    }

    void LightInfluenceStorage::insert(u16 lightId, GpuRaytracedLight light)
    {
        this->tree_storage.insert(std::make_pair(getAABBFromLight(light), lightId));
        this->light_lookup[lightId] = light;

        this->would_pack_benefit = true;
    }

    void LightInfluenceStorage::update(u16 lightId, GpuRaytracedLight light)
    {
        if (light == this->light_lookup[lightId])
        {
            log::warn("Identical update of light {}", lightId);
            return;
        }

        this->would_pack_benefit = true;

        this->remove(lightId);

        this->insert(lightId, light);
    }

    void LightInfluenceStorage::remove(u16 lightId)
    {
        const bool removed =
            this->tree_storage.remove(std::make_pair(getAABBFromLight(this->light_lookup[lightId]), lightId)) == 1;

        assert::warn(removed, "Tried to removed light {} and failed", lightId);

        if (removed)
        {
            this->would_pack_benefit = true;
        }
    }

    bool LightInfluenceStorage::pack()
    {
        const bool didPackOccur = this->would_pack_benefit;

        if (this->would_pack_benefit)
        {
            util::Timer t {"LightInfluenceStorage::pack()"};

            decltype(this->tree_storage) newTree {this->tree_storage.begin(), this->tree_storage.end()};

            this->tree_storage = std::move(newTree);

            this->would_pack_benefit = false;

            if (auto e = t.end(false); e > 1000)
            {
                log::warn("is this too long? {}", e);
            }
        }

        return didPackOccur;
    }

    std::vector<u16> LightInfluenceStorage::poll(ChunkLocation cL)
    {
        const glm::vec3 glmMinimum = static_cast<glm::vec3>(cL.getChunkNegativeCornerLocation());
        const glm::vec3 glmMaximum =
            static_cast<glm::vec3>(cL.getChunkNegativeCornerLocation() + static_cast<i32>(cL.getChunkWidthUnits()));

        const R3Point minimum {glmMinimum.x, glmMinimum.y, glmMinimum.z};
        const R3Point maximum {glmMaximum.x, glmMaximum.y, glmMaximum.z};

        const R3Box sampleBox {minimum, maximum};

        std::vector<LightWithId> queryResult {};

        std::ignore =
            this->tree_storage.query(boost::geometry::index::intersects(sampleBox), std::back_inserter(queryResult));

        std::vector<u16> lightIds {};

        for (const auto& [box, lightId] : queryResult)
        {
            const GpuRaytracedLight& light = this->light_lookup[lightId];

            if (doesCubeIntersectSphere(
                    light.position_and_half_intensity_distance.xyz(),
                    light.getMaxInfluenceDistance(),
                    {
                        box.min_corner().get<0>(),
                        box.min_corner().get<1>(),
                        box.min_corner().get<2>(),
                    },
                    {
                        box.max_corner().get<0>(),
                        box.max_corner().get<1>(),
                        box.max_corner().get<2>(),
                    }))
            {
                lightIds.push_back(lightId);
            }
        }

        return lightIds;
    }

} // namespace gfx::generators::voxel