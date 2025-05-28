#include "emissive_integer_tree.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/transform.hpp"
#include "glm/gtx/string_cast.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <glm/fwd.hpp>
// #include <kdtree++/kdtree.hpp> TODO: some other time it sucks

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
    EmissiveIntegerTree::EmissiveIntegerTree() = default;

    EmissiveIntegerTree::~EmissiveIntegerTree() = default;

    bool EmissiveIntegerTree::insert(WorldPosition p, f32 r, bool warnIfAlreadyExisting)
    {
        const decltype(this->data)::const_iterator it = this->data.find(p);

        if (it == this->data.cend())
        {
            this->data.insert({static_cast<glm::vec3>(p.asVector()), r});

            return true;
        }
        else if (warnIfAlreadyExisting)
        {
            log::warn("duplicate insertion of {}", glm::to_string(p.asVector()));
        }

        return false;
    }

    void EmissiveIntegerTree::erase(WorldPosition p)
    {
        this->data.erase(static_cast<glm::vec3>(p.asVector()));
        // this->impl->tree.efficient_replace_and_optimise(p);
    }

    std::vector<WorldPosition> EmissiveIntegerTree::getPossibleInfluencingPoints(AlignedChunkCoordinate chunk)
    {
        const WorldPosition searchPoint = WorldPosition::assemble(chunk, {});

        std::vector<WorldPosition> out {};

        const glm::vec3 chunkMin = searchPoint.asVector();
        const glm::vec3 chunkMax = searchPoint.asVector() + static_cast<i32>(ChunkSizeVoxels);

        for (const auto& [wP, r] : this->data)
        {
            if (doesCubeIntersectSphere(wP, r, chunkMin, chunkMax))
            {
                out.push_back(WorldPosition {static_cast<glm::i32vec3>(wP)});
            }
        }

        return out;
    }

} // namespace gfx::generators::voxel