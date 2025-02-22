#pragma once

#include <source_location>
#include <spdlog/spdlog.h>

using u8    = std::uint8_t;   // NOLINT
using u16   = std::uint16_t;  // NOLINT
using u32   = std::uint32_t;  // NOLINT
using u64   = std::uint64_t;  // NOLINT
using usize = std::size_t;    // NOLINT
using i8    = std::int8_t;    // NOLINT
using i16   = std::int16_t;   // NOLINT
using i32   = std::int32_t;   // NOLINT
using i64   = std::int64_t;   // NOLINT
using isize = std::ptrdiff_t; // NOLINT
using f32   = float;          // NOLINT
using f64   = double;         // NOLINT
using b8    = u8;             // NOLINT
using b32   = u32;            // NOLINT

namespace log
{
// NOLINTNEXTLINE
#define MAKE_LOG(LEVEL, ENUM)                                                                      \
    template<class... Ts>                                                                          \
    struct LEVEL /* NOLINT */                                                                      \
    {                                                                                              \
        LEVEL(/* NOLINT*/                                                                          \
              fmt::format_string<Ts...> fmt,                                                       \
              Ts&&... args,                                                                        \
              const std::source_location& location = std::source_location::current())              \
        {                                                                                          \
            spdlog::default_logger_raw()->log(                                                     \
                spdlog::source_loc {                                                               \
                    location.file_name(),                                                          \
                    static_cast<int>(location.line()),                                             \
                    location.function_name()},                                                     \
                spdlog::level::ENUM,                                                               \
                fmt,                                                                               \
                std::forward<Ts&&>(args)...);                                                      \
        }                                                                                          \
    };                                                                                             \
    template<class... Ts> /* NOLINTNEXTLINE*/                                                      \
    LEVEL(fmt::format_string<Ts...>, Ts&&...) -> LEVEL<Ts...>;                                     \
    template<class... J> /* NOLINTNEXTLINE*/                                                       \
    LEVEL(fmt::format_string<J...>, J&&..., std::source_location) -> LEVEL<J...>;

    MAKE_LOG(trace, trace);
    MAKE_LOG(debug, debug);
    MAKE_LOG(info, info);
    MAKE_LOG(warn, warn);
    MAKE_LOG(error, err);
    MAKE_LOG(critical, critical);
#undef MAKE_LOG

} // namespace log

namespace assert
{
// NOLINTNEXTLINE
#define MAKE_ASSERT(LEVEL, ENUM, THROW_ON_FAIL)                                                    \
    template<class... Ts>                                                                          \
    struct LEVEL /* NOLINT */                                                                      \
    {                                                                                              \
        LEVEL(/* NOLINT*/                                                                          \
              bool                      condition,                                                 \
              fmt::format_string<Ts...> fmt,                                                       \
              Ts&&... args,                                                                        \
              const std::source_location& location = std::source_location::current())              \
        {                                                                                          \
            if (!condition && spdlog::should_log(spdlog::level::ENUM))                             \
            {                                                                                      \
                spdlog::default_logger_raw()->log(                                                 \
                    spdlog::source_loc {                                                           \
                        location.file_name(),                                                      \
                        static_cast<int>(location.line()),                                         \
                        location.function_name()},                                                 \
                    spdlog::level::ENUM,                                                           \
                    fmt,                                                                           \
                    std::forward<Ts&&>(args)...);                                                  \
                                                                                                   \
                if constexpr (THROW_ON_FAIL)                                                       \
                {                                                                                  \
                    throw std::runtime_error {"assertion failure"};                                \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
    };                                                                                             \
    template<class... Ts> /* NOLINTNEXTLINE*/                                                      \
    LEVEL(bool, fmt::format_string<Ts...>, Ts&&...) -> LEVEL<Ts...>;                               \
    template<class... J> /* NOLINTNEXTLINE*/                                                       \
    LEVEL(bool, fmt::format_string<J...>, J&&..., std::source_location) -> LEVEL<J...>;

    MAKE_ASSERT(trace, trace, false);
    MAKE_ASSERT(debug, debug, false);
    MAKE_ASSERT(info, info, false);
    MAKE_ASSERT(warn, warn, false);
    MAKE_ASSERT(error, err, true);
    MAKE_ASSERT(critical, critical, true);
#undef MAKE_ASSERT

} // namespace assert
