#include "gfx/generators/voxel/model.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <cmath>
#include <glm/ext/vector_uint3_sized.hpp>
#include <glm/gtx/string_cast.hpp>
#include <mdspan>

#define OGT_VOX_IMPLEMENTATION
#include "ogt_vox.h"

namespace gfx::generators::voxel
{
    StaticVoxelModel::StaticVoxelModel()
        : extent {}
    {}

    StaticVoxelModel::StaticVoxelModel(std::vector<Voxel> data_, glm::u16vec3 extent_)
        : extent {extent_}
        , data {std::move(data_)}
    {
        const usize totalExtent =
            static_cast<usize>(extent.x) * static_cast<usize>(extent.y) * static_cast<usize>(extent.z);

        assert::critical(
            this->data.size() == totalExtent,
            "Tried to construct a StaticVoxelModel of size {}x{}x{} ({}), with only {} voxels being supplied!",
            this->extent.x,
            this->extent.y,
            this->extent.z,
            totalExtent,
            this->data.size());
    }

    StaticVoxelModel::StaticVoxelModel(StaticVoxelModel&& other) noexcept
        : extent {other.extent}
        , data {std::move(other.data)}
    {
        other.extent = {};
    }

    StaticVoxelModel& StaticVoxelModel::operator= (StaticVoxelModel&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->~StaticVoxelModel();

        new (this) StaticVoxelModel {std::move(other)};

        return *this;
    }

    StaticVoxelModel::ConstSpanType StaticVoxelModel::getModel() const
    {
        return std::mdspan<const Voxel, std::dextents<u16, 3>>(
            this->data.data(),
            static_cast<u16>(this->extent.x),
            static_cast<u16>(this->extent.y),
            static_cast<u16>(this->extent.z));
    }

    glm::u16vec3 StaticVoxelModel::getExtent() const
    {
        return this->extent;
    }

    AnimatedVoxelModel::AnimatedVoxelModel()
        : extent {}
        , total_time {}
    {}

    AnimatedVoxelModel::AnimatedVoxelModel(std::vector<AnimatedVoxelModelFrame> newFrames)
        : extent {}
    {
        assert::critical(!newFrames.empty(), "An AnimatedVoxelModel needs at least one frame!");

        const glm::u16vec3 shouldBeExtent = newFrames.front().model.getExtent();
        this->extent                      = shouldBeExtent;

        std::ranges::sort(
            newFrames,
            [](const AnimatedVoxelModelFrame& l, const AnimatedVoxelModelFrame& r)
            {
                return l.start_time < r.start_time;
            });

        // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer) side effects exist
        this->total_time = newFrames.back().start_time + newFrames.back().duration;

        for (usize i = 0; i < newFrames.size() - 1; ++i)
        {
            const float shouldBeNextFrameStart = newFrames.at(i).start_time + newFrames.at(i).duration;
            const float nextFrameStart         = newFrames.at(i + 1).start_time;

            assert::critical(
                util::isApproxEqual(shouldBeNextFrameStart, nextFrameStart),
                "Frame Start Time are not consistent! | i: {} | Expected: {} | Actual: {}",
                i,
                shouldBeNextFrameStart,
                nextFrameStart);
        }

        this->frame_start_times.reserve(newFrames.size());
        this->frames.reserve(newFrames.size());

        usize idx = 0;
        for (AnimatedVoxelModelFrame& f : newFrames)
        {
            assert::critical(
                f.model.getExtent() == shouldBeExtent,
                "AnimatedVoxelModel Frame extent mismatch! | Expected: {} | Got: {} @ Index {}",
                glm::to_string(shouldBeExtent),
                glm::to_string(f.model.getExtent()),
                idx);

            this->frames.push_back(std::move(f.model));
            this->frame_start_times.push_back(f.start_time);

            assert::critical(
                std::isnormal(f.start_time) || f.start_time == 0.0f, "Strange frame start time of {}", f.start_time);

            idx += 1;
        }
    }

    AnimatedVoxelModel::AnimatedVoxelModel(AnimatedVoxelModel&& other) noexcept
        : frame_start_times {std::move(other.frame_start_times)}
        , frames {std::move(other.frames)}
        , extent {other.extent}
        , total_time {other.total_time}
    {
        other.extent     = {};
        other.total_time = {};
    }

    AnimatedVoxelModel& AnimatedVoxelModel::operator= (AnimatedVoxelModel&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        this->~AnimatedVoxelModel();

        new (this) AnimatedVoxelModel {std::move(other)};

        return *this;
    }

    AnimatedVoxelModel::ConstSpanType AnimatedVoxelModel::getFrame(FrameNumber frameNumber) const
    {
        return this->frames.at(frameNumber).getModel();
    }

    AnimatedVoxelModel::ConstSpanType AnimatedVoxelModel::getFrame(float time, std::optional<Looping> looping) const
    {
        return this->getFrame(this->getFrameNumberAtTime(time, looping));
    }

    AnimatedVoxelModel::FrameNumber
    AnimatedVoxelModel::getFrameNumberAtTime(float time, std::optional<Looping> looping) const
    {
        if (looping.has_value())
        {
            time = std::fmod(time, this->total_time);
        }

        const std::vector<float>::const_iterator it = std::ranges::lower_bound(this->frame_start_times, time);

        return static_cast<FrameNumber>(it - this->frame_start_times.cbegin());
    }

    AnimatedVoxelModel::FrameNumber AnimatedVoxelModel::getNumberOfFrames() const
    {
        return static_cast<FrameNumber>(this->frames.size());
    }

    float AnimatedVoxelModel::getTotalTime() const
    {
        return this->total_time;
    }

    glm::u16vec3 AnimatedVoxelModel::getExtent() const
    {
        return this->extent;
    }

} // namespace gfx::generators::voxel