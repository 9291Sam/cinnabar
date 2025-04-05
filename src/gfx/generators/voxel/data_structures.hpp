#pragma once

#include "util/logger.hpp"
#include "util/util.hpp"
#include <array>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <optional>

namespace gfx::generators::voxel
{

    static constexpr u8 ChunkSizeVoxels = 64;
    static constexpr u8 ChunkSizeBricks = 8;
    static constexpr u8 BrickSizeVoxels = 8;

    struct UncheckedInDebugTag
    {};

    /// V is a glm::vec of something
    template<class Derived, class V, V::value_type MinValidValue, V::value_type MaxValidValue>
        requires (V::length() == 3)
    struct VoxelCoordinateBase : V
    {
        using Base                           = VoxelCoordinateBase;
        using VectorType                     = V;
        static constexpr V::value_type Bound = MaxValidValue + 1;

        // NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
        explicit VoxelCoordinateBase(V v)
            : V {v}
        {
            if (!VoxelCoordinateBase<Derived, V, MinValidValue, MaxValidValue>::tryCreate(v).has_value())
            {
                panic(
                    "Failed to create VectorCoordinate Base. Requested value was {{{}, {}, {}}} while the bounds are "
                    "[{}, {}]",
                    v.x,
                    v.y,
                    v.z,
                    MinValidValue,
                    MaxValidValue);
            }
        }

        // NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
        explicit VoxelCoordinateBase(V v, UncheckedInDebugTag)
            : V {v}
        {}

        template<class... Args>
        explicit VoxelCoordinateBase(Args&&... args)
            : VoxelCoordinateBase {Base::VectorType {std::forward<Args>(args)...}}
        {}

        static std::optional<Derived> tryCreate(V v)
        {
            if (MinValidValue <= v.x && v.x <= MaxValidValue && // format guide
                MinValidValue <= v.y && v.y <= MaxValidValue && // format guide
                MinValidValue <= v.z && v.z <= MaxValidValue)
            {
                return std::optional<Derived> {Derived {v, UncheckedInDebugTag {}}};
            }
            else
            {
                return std::nullopt;
            }
        }

        static Derived fromLinearIndex(std::size_t linearIndex)
        {
            static_assert(MinValidValue == 0, "I'm not dealing with that");
            static_assert(Bound != static_cast<V::value_type>(-1));

            const typename V::value_type z = static_cast<V::value_type>(linearIndex / (Bound * Bound));
            const typename V::value_type y = (linearIndex / Bound) % Bound;
            const typename V::value_type x = linearIndex % Bound;

            return Derived {V {x, y, z}, UncheckedInDebugTag {}};
        }

        [[nodiscard]] V asVector() const noexcept
        {
            return *this;
        }

        [[nodiscard]] std::size_t asLinearIndex() const noexcept
        {
            static_assert(MinValidValue == 0, "I'm not dealing with that");
            static_assert(Bound != static_cast<V::value_type>(-1));

            return this->x + (Bound * this->y) + (Bound * Bound * this->z);
        }

        constexpr std::strong_ordering operator<=> (const VoxelCoordinateBase& other) const
        {
            return std::tie(this->x, this->y, this->z) <=> std::tie(other.x, other.y, other.z);
        }
    };

    /// Represents the position of a single voxel in world space
    struct WorldPosition : public VoxelCoordinateBase<
                               WorldPosition,
                               glm::i32vec3,
                               std::numeric_limits<i32>::min(),
                               std::numeric_limits<i32>::max()>
    {
        using VoxelCoordinateBase::VoxelCoordinateBase;
    };

    /// Represents the position of a single voxel in the space of a brick
    struct BrickLocalPosition : public VoxelCoordinateBase<BrickLocalPosition, glm::u8vec3, 0, BrickSizeVoxels - 1>
    {
        using VoxelCoordinateBase::VoxelCoordinateBase;
    };

    /// represents the position of a brick within a chunk
    struct BrickCoordinate : public VoxelCoordinateBase<BrickCoordinate, glm::u8vec3, 0, ChunkSizeBricks - 1>
    {
        using VoxelCoordinateBase::VoxelCoordinateBase;
    };

    /// Represents the position of a single voxel in the space of a chunk
    struct ChunkLocalPosition : public VoxelCoordinateBase<ChunkLocalPosition, glm::u8vec3, 0, ChunkSizeVoxels - 1>
    {
        using VoxelCoordinateBase::VoxelCoordinateBase;

        static ChunkLocalPosition assemble(BrickCoordinate bC, BrickLocalPosition bP)
        {
            return ChunkLocalPosition {bC.asVector() * BrickSizeVoxels + bP.asVector()};
        }

        [[nodiscard]] std::pair<BrickCoordinate, BrickLocalPosition> split() const
        {
            return {
                BrickCoordinate {this->asVector() / ChunkSizeBricks},
                BrickLocalPosition {this->asVector() % ChunkSizeBricks}};
        }
    };

// NOLINTNEXTLINE(unused-includes) HACK
#include "shared_data_structures.slang"

    namespace internal
    {
        using User = ChunkData;
    } // namespace internal
} // namespace gfx::generators::voxel