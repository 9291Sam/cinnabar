

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
    const WorldPosition vec = std::bit_cast<WorldPosition>(a.Get(i));
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
        KDTree::KDTree<3, WorldPosition> tree;
    };

    EmissiveIntegerTree::EmissiveIntegerTree()
        : impl {new EmissiveIntegerTreeImpl {}}
    {}

    EmissiveIntegerTree::~EmissiveIntegerTree() = default;

    bool EmissiveIntegerTree::insert(WorldPosition v, bool warnIfAlreadyExisting)
    {
        auto it = this->impl->tree.find(v);

        if (it == this->impl->tree.end())
        {
            this->impl->tree.insert(v);

            return true;
        }
        else if (warnIfAlreadyExisting)
        {
            log::warn("duplicate insertion of {}", glm::to_string(v.asVector()));
        }

        return false;
    }

    void EmissiveIntegerTree::bulkInsertAndOptimize(std::vector<WorldPosition> v)
    {
        this->impl->tree.efficient_replace_and_optimise(v);
    }

    void EmissiveIntegerTree::remove(WorldPosition v)
    {
        this->impl->tree.erase(v);
    }

    std::vector<WorldPosition> EmissiveIntegerTree::getNearestElements(WorldPosition searchPoint, i32 maxDistance)
    {
        std::vector<WorldPosition> out {};

        std::ignore = this->impl->tree.find_within_range(
            searchPoint, typename decltype(this->impl->tree)::subvalue_type {maxDistance}, std::back_inserter(out));

        return out;
    }

    void EmissiveIntegerTree::optimize()
    {
        this->impl->tree.optimize();
    }

} // namespace gfx::generators::voxel