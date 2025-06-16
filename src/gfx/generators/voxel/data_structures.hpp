#pragma once

// TODO: these are here for include order things with shared_data_structures.slang
#include "gfx/core/vulkan/buffer.hpp"
#include "util/allocators/range_allocator.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <array>
#include <boost/container/flat_set.hpp>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
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

        template<class TryCreateVectorType>
        static std::optional<Derived> tryCreate(TryCreateVectorType v)
        {
            if (MinValidValue <= v.x && v.x <= MaxValidValue && // format guide
                MinValidValue <= v.y && v.y <= MaxValidValue && // format guide
                MinValidValue <= v.z && v.z <= MaxValidValue)
            {
                return std::optional<Derived> {Derived {static_cast<V>(v), UncheckedInDebugTag {}}};
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

    /// Represents the position of a single chun in world
    struct AlignedChunkCoordinate : public VoxelCoordinateBase<
                                        AlignedChunkCoordinate,
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
                BrickCoordinate {this->asVector() / BrickSizeVoxels},
                BrickLocalPosition {this->asVector() % BrickSizeVoxels}};
        }
    };
    static_assert(sizeof(ChunkLocalPosition) == 3);
    static_assert(alignof(ChunkLocalPosition) == 1);

    /// Represents the position of a single voxel in world space
    struct WorldPosition : public VoxelCoordinateBase<
                               WorldPosition,
                               glm::i32vec3,
                               std::numeric_limits<i32>::min(),
                               std::numeric_limits<i32>::max()>
    {
        using VoxelCoordinateBase::VoxelCoordinateBase;

        static WorldPosition assemble(AlignedChunkCoordinate aC, ChunkLocalPosition cP)
        {
            return WorldPosition {
                aC.asVector() * static_cast<i32>(ChunkSizeVoxels) + static_cast<glm::i32vec3>(cP.asVector())};
        }

        [[nodiscard]] std::pair<AlignedChunkCoordinate, ChunkLocalPosition> splitIntoAlignedChunk() const
        {
            return {
                AlignedChunkCoordinate {
                    {util::divideEuclideani32(this->x, static_cast<i32>(ChunkSizeVoxels)),
                     util::divideEuclideani32(this->y, static_cast<i32>(ChunkSizeVoxels)),
                     util::divideEuclideani32(this->z, static_cast<i32>(ChunkSizeVoxels))}},
                ChunkLocalPosition {
                    {util::moduloEuclideani32(this->x, static_cast<i32>(ChunkSizeVoxels)),
                     util::moduloEuclideani32(this->y, static_cast<i32>(ChunkSizeVoxels)),
                     util::moduloEuclideani32(this->z, static_cast<i32>(ChunkSizeVoxels))}}};
        }
    };

// NOLINTNEXTLINE(unused-includes) HACK
#include "shared_data_structures.slang"

    namespace internal
    {
        using User = GpuChunkData;
    } // namespace internal

    enum class EmissiveVoxelUpdateChangeType : u8
    {
        Insert,
        Removal
    };

    struct EmissiveVoxelUpdateChange
    {
        ChunkLocalPosition            position {};
        EmissiveVoxelUpdateChangeType change_type {};
        i32                           radius {};

        bool operator== (const EmissiveVoxelUpdateChange&) const = default;
    };

    struct CpuChunkData
    {
        util::RangeAllocation brick_allocation; // change name

        bool operator== (const CpuChunkData&) const = default;
    };
} // namespace gfx::generators::voxel

namespace std
{
    template<>
    struct hash<gfx::generators::voxel::AlignedChunkCoordinate>
    {
        usize operator() (const gfx::generators::voxel::AlignedChunkCoordinate& aC) const
        {
            return util::hashCombine(gfx::generators::voxel::hashAlignedChunkCoordinate(aC), 12732028474994);
        }
    };
} // namespace std