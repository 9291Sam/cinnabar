#pragma once

#include "util.hpp"
#include <compare>
#include <span>

namespace util
{
    class UUID final
    {
    public:
        struct ZeroInitTag
        {};
        struct MaxInitTag
        {};
    public:
        explicit UUID();
        explicit UUID(ZeroInitTag);
        explicit UUID(MaxInitTag);

        ~UUID() = default;

        UUID(const UUID&)             = default;
        UUID(UUID&&)                  = default;
        UUID& operator= (const UUID&) = default;
        UUID& operator= (UUID&&)      = default;

        [[nodiscard]] std::span<const std::byte, 16> asBytes() const
        {
            return {
                reinterpret_cast<const std::byte*>(this->data.cbegin()),
                reinterpret_cast<const std::byte*>(this->data.cend())};
        }
        [[nodiscard]] std::span<u64, 2> asIntegers() const {}

        constexpr std::strong_ordering operator<=> (const UUID&) const = default;
        constexpr bool                 operator== (const UUID&) const  = default;

    private:
        std::array<u64, 2> data {};
    };
    static_assert(sizeof(UUID) == 2 * sizeof(u64))
} // namespace util