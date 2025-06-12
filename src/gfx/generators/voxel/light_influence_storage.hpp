#pragma once

#include "data_structures.hpp"

namespace gfx::generators::voxel
{
    class LightInfluenceStorage
    {
    public:

        explicit LightInfluenceStorage(usize maxCapacity);
        ~LightInfluenceStorage();

        LightInfluenceStorage(const LightInfluenceStorage&)             = delete;
        LightInfluenceStorage(LightInfluenceStorage&&)                  = delete;
        LightInfluenceStorage& operator= (const LightInfluenceStorage&) = delete;
        LightInfluenceStorage& operator= (LightInfluenceStorage&&)      = delete;

        void insert(u16 lightId, GpuRaytracedLight);
        void update(u16 lightId, GpuRaytracedLight);
        void remove(u16 lightId);

    private:
        boost::geome
    };
} // namespace gfx::generators::voxel