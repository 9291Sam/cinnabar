

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
#include "octree.h"
#include "octree_container.h"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <glm/fwd.hpp>
#include <ranges>

namespace gfx::generators::voxel
{
    struct EmissiveIntegerTreeImpl
    {
        OrthoTree::TreePointContainerND<3, i32> tree;
    };

    EmissiveIntegerTree::EmissiveIntegerTree()
        : impl {new EmissiveIntegerTreeImpl {decltype(EmissiveIntegerTreeImpl::tree)::Create<false>(
              std::span<const decltype(EmissiveIntegerTreeImpl::tree)::TEntity> {},
              OrthoTree::detail::MortonSpaceIndexing<3>::MAX_THEORETICAL_DEPTH_ID,
              decltype(EmissiveIntegerTreeImpl::tree)::TBox {
                  {-PlayBound, -PlayBound, -PlayBound}, {PlayBound, PlayBound, PlayBound}})}}
    {}

    EmissiveIntegerTree::~EmissiveIntegerTree() = default;

    bool EmissiveIntegerTree::insert(glm::i32vec3 v, bool warnIfAlreadyExisting)
    {
        const bool inserted = this->impl->tree.AddUnique(std::bit_cast<std::array<i32, 3>>(v), 0);

        if (warnIfAlreadyExisting && !inserted)
        {
            log::warn("Duplicate insert of element @ {}", glm::to_string(v));
        }

        return inserted;
    }

    bool EmissiveIntegerTree::remove(glm::i32vec3 v)
    {
        panic("wrong");
        const u32  id      = this->impl->tree.GetCore().FindSmallestNode(std::bit_cast<std::array<i32, 3>>(v));
        const auto element = this->impl->tree.Get(id);

        log::trace("{} {} {} {}", id, element[0], element[1], element[2]);

        if (std::bit_cast<glm::i32vec3>(element) == v)
        {
            this->impl->tree.Erase(id);

            log::trace("dest");

            return true;
        }

        return false;
    }

    std::vector<glm::i32vec3> EmissiveIntegerTree::getNearestElements(glm::i32vec3 searchPoint, std::size_t maxElements)
    {
        std::vector ids =
            this->impl->tree.GetNearestNeighbors(std::bit_cast<std::array<i32, 3>>(searchPoint), maxElements);

        std::vector<glm::i32vec3> result {};
        result.resize(ids.size());

        for (auto [id, newPosition] : std::ranges::zip_view(ids, result))
        {
            newPosition = std::bit_cast<glm::i32vec3>(this->impl->tree.Get(id));
        }

        return result;
    }

} // namespace gfx::generators::voxel