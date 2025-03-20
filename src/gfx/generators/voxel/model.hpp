#pragma once

#include "material.hpp"
#include "util/util.hpp"
#include <glm/vec3.hpp>
#include <mdspan>

namespace gfx::generators::voxel
{
    class StaticVoxelModel
    {
    public:
        using ConstSpanType = std::mdspan<const Voxel, std::dextents<u16, 3>>;
    public:
        StaticVoxelModel();
        StaticVoxelModel(std::vector<Voxel>, glm::u16vec3 extent);
        // static StaticVoxelModel fromVoxFile(std::span<const std::byte>);

        ~StaticVoxelModel() = default;

        StaticVoxelModel(const StaticVoxelModel&) = delete;
        StaticVoxelModel(StaticVoxelModel&&) noexcept;
        StaticVoxelModel& operator= (const StaticVoxelModel&) = delete;
        StaticVoxelModel& operator= (StaticVoxelModel&&) noexcept;

        [[nodiscard]] ConstSpanType getModel() const;
        [[nodiscard]] glm::u16vec3  getExtent() const;

    private:
        glm::u16vec3       extent;
        std::vector<Voxel> data;
    };

    class AnimatedVoxelModel
    {
    public:
        struct AnimatedVoxelModelFrame
        {
            float            start_time;
            float            duration;
            StaticVoxelModel model;
        };

        struct Looping
        {};

        using FrameNumber   = u32;
        using ConstSpanType = StaticVoxelModel::ConstSpanType;
    public:
        AnimatedVoxelModel();
        explicit AnimatedVoxelModel(std::vector<AnimatedVoxelModelFrame>);
        // static StaticVoxelModel fromVoxFile(std::span<const std::byte>);

        ~AnimatedVoxelModel() = default;

        AnimatedVoxelModel(const AnimatedVoxelModel&) = delete;
        AnimatedVoxelModel(AnimatedVoxelModel&&) noexcept;
        AnimatedVoxelModel& operator= (const AnimatedVoxelModel&) = delete;
        AnimatedVoxelModel& operator= (AnimatedVoxelModel&&) noexcept;

        [[nodiscard]] ConstSpanType getFrame(FrameNumber) const;
        [[nodiscard]] ConstSpanType getFrame(float time, std::optional<Looping> = {}) const;
        [[nodiscard]] FrameNumber   getFrameNumberAtTime(float, std::optional<Looping> = {}) const;
        [[nodiscard]] FrameNumber   getNumberOfFrames() const;
        [[nodiscard]] float         getTotalTime() const;
        [[nodiscard]] glm::u16vec3  getExtent() const;

    private:
        std::vector<float>            frame_start_times;
        std::vector<StaticVoxelModel> frames;
        glm::u16vec3                  extent;
        float                         total_time;
    };

} // namespace gfx::generators::voxel
