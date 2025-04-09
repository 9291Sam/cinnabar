#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <type_traits>
#include <vector>

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
    template<class T>
    using Fn = T*;

    template<typename T>
    consteval std::string_view getNameOfType()
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

    constexpr u64 hashCombine(u64 hash, u64 add)
    {
        hash ^= add;
        hash *= 1099511628211ULL; // FNV-1a Prime

        return hash;
    }

    constexpr u64 hashStringViewConstexpr(std::string_view str)
    {
        // FNV-1a offset basis
        u64 hash = 14695981039346656037ULL;
        for (char c : str)
        {
            hash = hashCombine(hash, static_cast<u64>(c));
        }
        return hash;
    }

    template<class T>
    consteval u64 getIdOfType()
    {
        return hashStringViewConstexpr(getNameOfType<T>());
    }

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

    std::size_t getMemoryUsage();

    enum class SuffixType : u8
    {
        Short,
        Full
    };

    std::string bytesAsSiNamed(std::size_t, SuffixType = SuffixType::Full);
    std::string bytesAsSiNamed(long double bytes, SuffixType = SuffixType::Full);

    inline bool isApproxEqual(const float a, const float b, float similarity = 0.00001f)
    {
        const float maxMagnitude = std::max(std::abs(a), std::abs(b));
        const float epsilon      = maxMagnitude * similarity;

        return abs(a - b) < epsilon;
    }

    template<class Fn>
        requires std::is_nothrow_invocable_v<Fn>
    class Defer
    {
    public:
        explicit Defer(Fn&& fn_)
            : fn {std::move(fn_)}
        {}

        ~Defer()
        {
            this->fn();
        }

        Defer(const Defer&)             = delete;
        Defer(Defer&&)                  = delete;
        Defer& operator= (const Defer&) = delete;
        Defer& operator= (Defer&&)      = delete;

    private:
        Fn fn;
    };

    // template<class I>
    // I divideEuclidean(I lhs, I rhs) // NOLINT
    // {
    //     const I quotient = lhs / rhs;

    //     if (lhs % rhs < 0)
    //     {
    //         return rhs > 0 ? quotient - 1 : quotient + 1;
    //     }

    //     return quotient;
    // }

    // template<class I>
    // I moduloEuclidean(I lhs, I rhs)
    // {
    //     const I remainder = lhs % rhs;

    //     if (remainder < 0)
    //     {
    //         return rhs > 0 ? remainder + rhs : remainder - rhs;
    //     }

    //     return remainder;
    // }

    inline i32 moduloEuclideani32(i32 lhs, i32 rhs)
    {
        if (lhs < 0)
        {
            lhs += (-lhs / rhs + 1) * rhs;
        }

        const i32 remainder = lhs % rhs;
        if (remainder < 0)
        {
            return rhs > 0 ? remainder + rhs : remainder - rhs;
        }
        return remainder;
    }

    inline i32 divideEuclideani32(i32 lhs, i32 rhs)
    {
        int quotient  = lhs / rhs;
        int remainder = lhs % rhs;

        if (remainder != 0 && ((rhs < 0) != (lhs < 0)))
        {
            quotient -= 1;
        }

        return quotient;
    }
    template<class T>
    [[nodiscard]] T map(T x, T inMin, T inMax, T outMin, T outMax)
    {
        return ((x - inMin) * (outMax - outMin) / (inMax - inMin)) + outMin;
    }

} // namespace util
