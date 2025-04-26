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

        /// try remove the element, return false if there was no element in the tree
        void remove(glm::i32vec3);

        std::vector<glm::i32vec3>
        getNearestElements(glm::i32vec3 searchPoint, std::size_t maxElements, i32 maxDistance);

    private:
        std::unique_ptr<EmissiveIntegerTreeImpl> impl;
    };
} // namespace gfx::generators::voxel