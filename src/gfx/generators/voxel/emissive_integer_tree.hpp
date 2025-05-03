#pragma once

#include "data_structures.hpp"
#include "util/util.hpp"
#include <boost/container/flat_map.hpp>
#include <compare>
#include <vector>

namespace gfx::generators::voxel
{

    // The world is 2m voxels per axis because of floats
    static constexpr i32 PlayBound = 1u << 20u;

    class EmissiveIntegerTree
    {
    public:
        EmissiveIntegerTree();
        ~EmissiveIntegerTree();

        EmissiveIntegerTree(const EmissiveIntegerTree&)             = delete;
        EmissiveIntegerTree(EmissiveIntegerTree&&)                  = delete;
        EmissiveIntegerTree& operator= (const EmissiveIntegerTree&) = delete;
        EmissiveIntegerTree& operator= (EmissiveIntegerTree&&)      = delete;

        /// Insert an element into the tree
        /// Returns true if the insert was successful
        bool insert(WorldPosition, float radius, bool warnIfAlreadyExisting = true);

        /// try remove the element, return false if there was no element in the tree
        void erase(WorldPosition);

        // returns an unordered list of all elements inside the distance from the given point
        std::vector<WorldPosition> getPossibleInfluencingPoints(AlignedChunkCoordinate);


    private:
        struct OrdVec3
        {
            bool operator() (const glm::vec3& l, const glm::vec3& r) const
            {
                return std::tuple {l.x, l.y, l.z} < std::tuple {r.x, r.y, r.z};
            }
        };

        boost::container::flat_map<glm::vec3, float, OrdVec3> data;
    };
} // namespace gfx::generators::voxel