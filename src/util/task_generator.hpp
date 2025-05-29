#pragma once

#include "util/util.hpp"
#include <chrono>

namespace util
{
    struct TimeStamp
    {
        f32         duration;
        const char* name;
    };

    struct TimestampStamper
    {
        TimestampStamper()
            : previous_stamp {std::chrono::steady_clock::now()}
        {}

        void stamp(const char* name)
        {
            const std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

            this->tasks.push_back(
                TimeStamp {
                    .duration {std::chrono::duration<float> {now - this->previous_stamp}.count()}, .name {name}});

            this->previous_stamp = now;
        }

        [[nodiscard]] std::vector<TimeStamp> getTasks() const
        {
            return this->tasks;
        }

    private:
        std::chrono::time_point<std::chrono::steady_clock> previous_stamp;
        std::vector<TimeStamp>                             tasks;
    };
} // namespace util