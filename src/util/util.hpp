#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

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

namespace util
{
    template<typename T>
    constexpr std::string_view getNameOfType()
    {
#ifdef __clang__
        std::string_view name = __PRETTY_FUNCTION__;
        name.remove_prefix(name.find('=') + 2);
        name.remove_suffix(1);
#elif defined(__GNUC__)
        std::string_view name = __PRETTY_FUNCTION__;
        name.remove_prefix(name.find('=') + 2);
        name.remove_suffix(1);
#elif defined(_MSC_VER)
        std::string_view name = __FUNCSIG__;
        name.remove_prefix(name.find('<') + 1);
        name.remove_suffix(7);
#endif
        return name;
    }

    constexpr inline void hashCombine(std::size_t& seed_, std::size_t hash_) noexcept
    {
        hash_ += 0x9e3779b9 + (seed_ << 6) + (seed_ >> 2);
        seed_ ^= hash_;
    }

} // namespace util

inline std::filesystem::path getCanonicalPathOfShaderFile(std::string_view file)
{
    using namespace std::literals;

    const std::string_view prepend {"../"sv};

    const std::filesystem::path totalPath {std::filesystem::current_path() / prepend / file};
    std::filesystem::path       canonicalPath {std::filesystem::weakly_canonical(totalPath)};

    canonicalPath.make_preferred(); // non const member function

    return canonicalPath;
}

std::vector<std::byte> loadEntireFileFromPath(const std::filesystem::path& path);
