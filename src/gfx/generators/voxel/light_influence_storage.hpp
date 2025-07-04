#pragma once

#include "data_structures.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/core/cs.hpp>

namespace gfx::generators::voxel
{
    class LightInfluenceStorage
    {
    public:

        explicit LightInfluenceStorage(usize maxLightId);
        ~LightInfluenceStorage();

        LightInfluenceStorage(const LightInfluenceStorage&)             = delete;
        LightInfluenceStorage(LightInfluenceStorage&&)                  = delete;
        LightInfluenceStorage& operator= (const LightInfluenceStorage&) = delete;
        LightInfluenceStorage& operator= (LightInfluenceStorage&&)      = delete;

        // these lightIds must be below the constructor parameter maxLightId
        void insert(u16 lightId, GpuRaytracedLight);
        void update(u16 lightId, GpuRaytracedLight);
        void remove(u16 lightId);

        // returns true if any lights changed
        bool pack();

        std::vector<u16> poll(ChunkLocation);

    private:

        using R3Point = boost::geometry::model::point<float, 3, boost::geometry::cs::cartesian>;
        using R3Box   = boost::geometry::model::box<R3Point>;

        using LightWithId = std::pair<R3Box, u16>;

        bool                                                                          would_pack_benefit;
        boost::geometry::index::rtree<LightWithId, boost::geometry::index::rstar<16>> tree_storage;
        std::vector<GpuRaytracedLight>                                                light_lookup;

        friend R3Box getAABBFromLight(GpuRaytracedLight);
    };
} // namespace gfx::generators::voxel