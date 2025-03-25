#include "gfx/generators/voxel/model.hpp"
#include "material.hpp"
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

    StaticVoxelModel::StaticVoxelModel(glm::u32vec3 extent_, Voxel fillVoxel)
        : StaticVoxelModel {
              std::vector {
                  static_cast<usize>(extent_.x) * static_cast<usize>(extent_.y) * static_cast<usize>(extent_.z),
                  fillVoxel},
              extent_}
    {}

    StaticVoxelModel::StaticVoxelModel(std::vector<Voxel> data_, glm::u32vec3 extent_)
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

    StaticVoxelModel StaticVoxelModel::fromVoxFile(std::span<const std::byte> data)
    {
        const ogt_vox_scene* scene =
            ogt_vox_read_scene(reinterpret_cast<const u8*>(data.data()), static_cast<u32>(data.size_bytes()));

        util::Defer _ {[&] noexcept
                       {
                           ogt_vox_destroy_scene(scene);
                       }};

        std::span<const ogt_vox_model*> models {scene->models, scene->num_models};

        assert::critical(
            models.size() == 1, "Can't load a StaticVoxelModel from a Scene with {} models in it!", models.size());

        std::mdspan<const u8, std::dextents<std::size_t, 3>> voxelStorage =
            std::mdspan(models[0]->voxel_data, models[0]->size_z, models[0]->size_y, models[0]->size_x);

        auto hash = [](u32 foo)
        {
            foo ^= foo >> 17;
            foo *= 0xed5ad4bbU;
            foo ^= foo >> 11;
            foo *= 0xac4c1b51U;
            foo ^= foo >> 15;
            foo *= 0x31848babU;
            foo ^= foo >> 14;

            return foo;
        };

        StaticVoxelModel out {
            glm::u32vec3 {models[0]->size_x, models[0]->size_z, models[0]->size_y}, Voxel::NullAirEmpty};

        // uint32_t solid_voxel_count = 0;
        uint32_t voxel_index = 0;

        for (uint32_t z = 0; z < models[0]->size_z; z++)
        {
            for (uint32_t y = 0; y < models[0]->size_y; y++)
            {
                for (uint32_t x = 0; x < models[0]->size_x; x++, voxel_index++)
                {
                    const u8 val = models[0]->voxel_data[voxel_index];

                    if (val != 0)
                    {
                        out.getModelMutable()[x, z, y] = static_cast<Voxel>((hash(static_cast<u32>(val)) % 17) + 1);
                    }
                }
            }
        }

        return out;
    }

    StaticVoxelModel StaticVoxelModel::createCornelBox()
    {
        StaticVoxelModel model {glm::u32vec3 {64, 64, 64}, Voxel::NullAirEmpty};

        auto voxels = model.getModelMutable();

        glm::u32vec3 size = model.getExtent();

        // Define materials
        Voxel white = Voxel::Marble;
        Voxel red   = Voxel::Ruby;
        Voxel green = Voxel::Jade;
        Voxel light = Voxel::Gold;

        // Fill walls, floor, and ceiling
        for (uint32_t x = 0; x < size.x; ++x)
        {
            for (uint32_t y = 0; y < size.y; ++y)
            {
                for (uint32_t z = 0; z < size.z; ++z)
                {
                    if (x == 0)
                    {
                        voxels[x, y, z] = red; // Left wall
                    }
                    if (x == size.x - 1)
                    {
                        voxels[x, y, z] = green; // Right wall
                    }
                    if (z == 0)
                    {
                        voxels[x, y, z] = white; // Back wall
                    }
                    if (y == 0)
                    {
                        voxels[x, y, z] = white; // Floor
                    }
                    if (y == size.y - 1)
                    {
                        voxels[x, y, z] = white; // Ceiling
                    }
                }
            }
        }

        // Add a light source at the center of the ceiling
        for (uint32_t x = size.x / 3; x < 2 * size.x / 3; ++x)
        {
            for (uint32_t z = size.z / 3; z < 2 * size.z / 3; ++z)
            {
                voxels[x, size.y - 1, z] = light;
            }
        }

        for (u32 x = 24; x < 36; ++x)
        {
            for (u32 y = 0; y < 16; ++y)
            {
                for (u32 z = 14; z < 42; ++z)
                {
                    voxels[x, y, z] = Voxel::Copper;
                }
            }
        }

        return model;
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

    StaticVoxelModel::MutableSpanType StaticVoxelModel::getModelMutable()
    {
        return std::mdspan<Voxel, std::dextents<u32, 3>>(
            this->data.data(),
            static_cast<u32>(this->extent.x),
            static_cast<u32>(this->extent.y),
            static_cast<u32>(this->extent.z));
    }

    StaticVoxelModel::ConstSpanType StaticVoxelModel::getModel() const
    {
        return std::mdspan<const Voxel, std::dextents<u32, 3>>(
            this->data.data(),
            static_cast<u32>(this->extent.x),
            static_cast<u32>(this->extent.y),
            static_cast<u32>(this->extent.z));
    }

    glm::u32vec3 StaticVoxelModel::getExtent() const
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

        const glm::u32vec3 shouldBeExtent = newFrames.front().model.getExtent();
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

    namespace
    {
        std::vector<AnimatedVoxelModel::AnimatedVoxelModelFrame> staticVectorHelper(StaticVoxelModel staticModel)
        {
            std::vector<AnimatedVoxelModel::AnimatedVoxelModelFrame> frames {};

            frames.push_back(AnimatedVoxelModel::AnimatedVoxelModelFrame {
                .start_time {0.0f}, .duration {INFINITY}, .model {std::move(staticModel)}});

            return frames;
        }
    } // namespace

    AnimatedVoxelModel::AnimatedVoxelModel(StaticVoxelModel staticModel)
        : AnimatedVoxelModel {staticVectorHelper(std::move(staticModel))}
    {}

    AnimatedVoxelModel::AnimatedVoxelModel(AnimatedVoxelModel&& other) noexcept
        : frame_start_times {std::move(other.frame_start_times)}
        , frames {std::move(other.frames)}
        , extent {other.extent}
        , total_time {other.total_time}
    {
        other.extent     = {};
        other.total_time = {};
    }

    AnimatedVoxelModel AnimatedVoxelModel::fromGif(const util::Gif& gif)
    {
        std::span<const float> allFrameStartTimes = gif.getAllStartTimes();

        std::vector<AnimatedVoxelModelFrame> frames {};
        frames.resize(allFrameStartTimes.size());

        for (u32 i = 0; i < frames.size(); ++i)
        {
            const auto [xExtent, zExtent] = gif.getExtents();
            const u32 yExtent             = 64; // HACK

            StaticVoxelModel thisFrameModel {glm::u32vec3 {xExtent, yExtent, zExtent}, Voxel::NullAirEmpty};

            for (u32 x = 0; x < xExtent; ++x)
            {
                // for (u32 y = 0; y < yExtent; ++y)
                // {
                for (u32 z = 0; z < zExtent; ++z)
                {
                    const util::Gif::Color thisColor = gif.getFrame(i)[x, z];

                    const f32 length = glm::length(glm::vec3 {thisColor.color}) / 8.0f;

                    // const bool shouldBeSolid = static_cast<f32>(y) < length; // (x + z) / 2 > y;
                    const bool shouldBeSolid = length > 1.0f;

                    auto hash = [](u32 foo)
                    {
                        foo ^= foo >> 17U;
                        foo *= 0xed5ad4bbU;
                        foo ^= foo >> 11U;
                        foo *= 0xac4c1b51U;
                        foo ^= foo >> 15U;
                        foo *= 0x31848babU;
                        foo ^= foo >> 14U;

                        return foo;
                    };

                    const Voxel v = static_cast<Voxel>(((hash(x) + hash(z)) % 17) + 1);

                    if (shouldBeSolid)
                    {
                        thisFrameModel.getModelMutable()[x, 0, z] = v;
                    }
                }
                // }
            }

            const float thisFrameStartTime = allFrameStartTimes[i];
            // HACK: assume 30fps last frame of a gif, we lack information
            // TODO: FIX
            const float nextFrameStartTime =
                (i < allFrameStartTimes.size() - 1) ? allFrameStartTimes[i + 1] : allFrameStartTimes[i] + (1.0f / 30);

            frames.at(i) = AnimatedVoxelModelFrame {
                .start_time {thisFrameStartTime},
                .duration {nextFrameStartTime - thisFrameStartTime},
                .model {std::move(thisFrameModel)}};
        }

        return AnimatedVoxelModel {std::move(frames)};
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

        assert::critical(!this->frame_start_times.empty(), "no frames!");

        if (this->frame_start_times.size() == 1)
        {
            return 0;
        }

        const std::vector<float>::const_iterator it = std::ranges::lower_bound(this->frame_start_times, time);

        if (it == this->frame_start_times.cend())
        {
            log::critical("invalid time: {}", time);

            for (float f : this->frame_start_times)
            {
                log::debug("{}", f);
            }
            panic("panic");
        }

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

    glm::u32vec3 AnimatedVoxelModel::getExtent() const
    {
        return this->extent;
    }

} // namespace gfx::generators::voxel