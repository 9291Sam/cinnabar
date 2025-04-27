#pragma once

#include "glm/vec3.hpp"
#include "util/util.hpp"
#include <span>
#include <vector>

namespace gfx::generators::voxel
{

    // The world is 2m voxels per axis because of floats
    static constexpr i32 PlayBound = 1u << 20u;

    struct EmissiveIntegerTreeImpl;

    // A tree that holds
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
        bool insert(glm::i32vec3, bool warnIfAlreadyExisting = true);
        void bulkInsertAndOptimize(std::vector<glm::i32vec3>);

        /// try remove the element, return false if there was no element in the tree
        void remove(glm::i32vec3);

        // returns an unordered list of all elements inside the distance from the given point
        std::vector<glm::i32vec3> getNearestElements(glm::i32vec3 searchPoint, i32 maxDistance);

        void optimize();


    private:
        std::unique_ptr<EmissiveIntegerTreeImpl> impl;
    };
} // namespace gfx::generators::voxel