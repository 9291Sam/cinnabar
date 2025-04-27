

/*
OrthoTree::TreePointContainerND<3, i32> a {decltype(a)::Create<false>(
    std::span<const decltype(a)::TEntity> {},
    OrthoTree::detail::MortonSpaceIndexing<3>::MAX_THEORETICAL_DEPTH_ID,
    decltype(a)::TBox {{-PlayBound, -PlayBound, -PlayBound}, {PlayBound, PlayBound, PlayBound}})};

a.Add({-13, -23, 2});
a.Add({1, 2, 3});
a.Add({1, -2, 3});
a.Add({1, 2, -3});
a.Add({13, 2, 3});
a.Add({1, 21, 3});

a.Get(0);

util::Timer ta {"find"};
auto        v = a.GetNearestNeighbors({0, 0, 0}, 7);

for (auto i : v)
{
    const glm::i32vec3 vec = std::bit_cast<glm::i32vec3>(a.Get(i));
    log::trace("{} -> {} {}", i, glm::to_string(vec), glm::length(static_cast<glm::f32vec3>(vec)));
}

*/

#include <thread>
// dumbass library author doesn't understand headers

#include "emissive_integer_tree.hpp"
#include "glm/gtx/string_cast.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <glm/fwd.hpp>
#include <kdtree++/kdtree.hpp>
#include <ranges>
#include <tuple>

namespace gfx::generators::voxel
{
    struct EmissiveIntegerTreeImpl
    {
        KDTree::KDTree<3, glm::i32vec3> tree;
    };

    EmissiveIntegerTree::EmissiveIntegerTree()
        : impl {new EmissiveIntegerTreeImpl {}}
    {}

    EmissiveIntegerTree::~EmissiveIntegerTree() = default;

    bool EmissiveIntegerTree::insert(glm::i32vec3 v, bool warnIfAlreadyExisting)
    {
        auto it = this->impl->tree.find(v);

        if (it == this->impl->tree.end())
        {
            this->impl->tree.insert(v);

            return true;
        }
        else if (warnIfAlreadyExisting)
        {
            log::warn("duplicate insertion of {}", glm::to_string(v));
        }

        return false;
    }

    void EmissiveIntegerTree::bulkInsertAndOptimize(std::vector<glm::i32vec3> v)
    {
        this->impl->tree.efficient_replace_and_optimise(v);
    }

    void EmissiveIntegerTree::remove(glm::i32vec3 v)
    {
        this->impl->tree.erase(v);
    }

    std::vector<glm::i32vec3> EmissiveIntegerTree::getNearestElements(glm::i32vec3 searchPoint, i32 maxDistance)
    {
        const glm::f32vec3        floatSearchPos = static_cast<glm::f32vec3>(searchPoint);
        std::vector<glm::i32vec3> out {};

        std::ignore = this->impl->tree.find_within_range(
            searchPoint, typename decltype(this->impl->tree)::subvalue_type {maxDistance}, std::back_inserter(out));

        return out;
    }

    void EmissiveIntegerTree::optimize()
    {
        this->impl->tree.optimize();
    }

} // namespace gfx::generators::voxel