#pragma once

#include "util/util.hpp"
#include <glm/vec3.hpp>
#include <mdspan>
#include <optional>
#include <span>

namespace util
{
    /// Sane wrapper around a GIF that is loaded from memory and doesn't leak memory!
    /// https://github.com/khalladay/gif_read
    class Gif
    {
    public:
        struct Color
        {
            glm::u8vec3 color;
            u8          a;
        };

        struct Looping
        {};

        using FrameNumber = u32;
    public:
        Gif();
        explicit Gif(std::span<const std::byte>);

        ~Gif() = default;

        Gif(const Gif&)             = delete;
        Gif(Gif&&)                  = default;
        Gif& operator= (const Gif&) = delete;
        Gif& operator= (Gif&&)      = default;

        [[nodiscard]] std::mdspan<const Color, std::dextents<usize, 2>> getFrame(FrameNumber) const;
        [[nodiscard]] std::mdspan<const Color, std::dextents<usize, 2>>
                                           getFrameAtTime(f32, std::optional<Looping> = {}) const;
        [[nodiscard]] FrameNumber          getFrameNumberAtTime(f32, std::optional<Looping> = {}) const;
        /// Width, height
        [[nodiscard]] std::pair<u32, u32>  getExtents() const;
        [[nodiscard]] f32                  getTotalTime() const;
        [[nodiscard]] std::span<const f32> getAllStartTimes() const;

    private:
        std::vector<f32>                frame_start_times;
        std::vector<std::vector<Color>> frames;
        std::pair<u32, u32>             extent;
        f32                             total_time;
    };
} // namespace util