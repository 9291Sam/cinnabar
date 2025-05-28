#pragma once

#include "index_allocator.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <array>
#include <compare>
#include <cstring>
#include <source_location>
#include <type_traits>

namespace util
{
    template<std::size_t N>
    struct StringSneaker
    {
        constexpr StringSneaker(const char (&string)[N]) // NOLINT: We want implicit
            : data {std::to_array<const char>(string)}
        {}

        [[nodiscard]]
        constexpr std::string_view getStringView() const noexcept
        {
            return std::string_view {this->data.data(), N - 1};
        }

        std::array<char, N> data;
    };

    struct NoFriendDeclaration
    {};

    template<class Handle>
    class OpaqueHandleAllocator;

    template<StringSneaker Name, class I, class FriendedClass = NoFriendDeclaration>
        requires (std::is_integral_v<I> && !std::is_floating_point_v<I>)
    struct [[nodiscard]] OpaqueHandle
    {
    public:
        static constexpr StringSneaker HandleName = Name;
        using FriendedClassType                   = FriendedClass;
        using IndexType                           = I;
        static constexpr I NullValue              = std::numeric_limits<I>::max();
        static constexpr I MaxValidElement        = NullValue - 1;

        constexpr OpaqueHandle()
            : value {NullValue}
        {}
        constexpr ~OpaqueHandle()
        {
            if (!this->isNull())
            {
                log::warn("Leak of {} with value {}", Name.getStringView(), this->value);
            }
        }

        OpaqueHandle(const OpaqueHandle&) = delete;
        OpaqueHandle(OpaqueHandle&& other) noexcept
            : value {other.value}
        {
            other.value = NullValue;
        }
        OpaqueHandle& operator= (const OpaqueHandle&) = delete;
        OpaqueHandle& operator= (OpaqueHandle&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~OpaqueHandle();

            new (this) OpaqueHandle {std::move(other)};

            return *this;
        }

        [[nodiscard]] bool isNull() const
        {
            return this->value == NullValue;
        }

        std::strong_ordering operator<=> (const OpaqueHandle& other) const = default;

        bool operator== (const OpaqueHandle&) const = default;

    protected:
        static constexpr I Null = NullValue;

        constexpr explicit OpaqueHandle(I value_)
            : value {value_}
        {}

        [[nodiscard]] I release() noexcept
        {
            return std::exchange(this->value, NullValue);
        }

        [[nodiscard]] I getValue() const noexcept
        {
            return this->value;
        }

        friend OpaqueHandleAllocator<OpaqueHandle>;
        friend FriendedClass;

    private:
        I value;
    };

    template<class Handle, auto PtrToMemberDeleter>
    class UniqueOpaqueHandle : public Handle
    {
    public:
        using DeleterType     = decltype(PtrToMemberDeleter);
        using TypeWithDeleter = util::ContainerTypeT<DeleterType>;
        using OwnerType =
            std::conditional_t<IsConstMemberFunctionPointerV<DeleterType>, const TypeWithDeleter, TypeWithDeleter>;
    public:
        UniqueOpaqueHandle()
            : Handle {}
        {}
        UniqueOpaqueHandle(Handle h, OwnerType* owner_)
            : Handle {std::move(h)}
            , owner {owner_}
        {}
        ~UniqueOpaqueHandle();

        UniqueOpaqueHandle(const UniqueOpaqueHandle&)             = delete;
        UniqueOpaqueHandle(UniqueOpaqueHandle&&)                  = default;
        UniqueOpaqueHandle& operator= (const UniqueOpaqueHandle&) = delete;
        UniqueOpaqueHandle& operator= (UniqueOpaqueHandle&&)      = default;
    private:
        OwnerType* owner;
    };

    template<class Handle>
    class OpaqueHandleAllocator
    {
    public:
        explicit OpaqueHandleAllocator(Handle::IndexType numberOfElementsToAllocate)
            : allocator {static_cast<u32>(numberOfElementsToAllocate)}
        {}
        ~OpaqueHandleAllocator() = default;

        OpaqueHandleAllocator(const OpaqueHandleAllocator&)             = delete;
        OpaqueHandleAllocator(OpaqueHandleAllocator&&)                  = default;
        OpaqueHandleAllocator& operator= (const OpaqueHandleAllocator&) = delete;
        OpaqueHandleAllocator& operator= (OpaqueHandleAllocator&&)      = default;

        void updateAvailableBlockAmount(Handle::IndexType newAmount)
        {
            this->allocator.updateAvailableBlockAmount(newAmount);
        }

        [[nodiscard]] u32 getNumberAllocated() const
        {
            return this->allocator.getNumberAllocated();
        }

        [[nodiscard]] f32 getPercentAllocated() const
        {
            return this->getPercentAllocated();
        }

        [[nodiscard]] Handle allocateOrPanic(std::source_location loc = std::source_location::current())
        {
            return Handle {static_cast<Handle::IndexType>(this->allocator.allocateOrPanic(loc))};
        }

        [[nodiscard]] std::expected<Handle, IndexAllocator::OutOfBlocks> allocate()
        {
            return this->allocator.allocate().transform(
                [](const typename Handle::IndexType rawHandle)
                {
                    return Handle {rawHandle};
                });
        }

        void free(Handle handle)
        {
            this->allocator.free(handle.release());
        }

        [[nodiscard]] auto getValueOfHandle(const Handle& handle) const -> Handle::IndexType
        {
            return handle.value;
        }

        void iterateThroughAllocatedElements(std::invocable<typename Handle::IndexType> auto func)
            requires std::same_as<void, std::invoke_result_t<decltype(func), typename Handle::IndexType>>
        {
            this->allocator.iterateThroughAllocatedElements(
                [&](const util::IndexAllocator::IndexType i)
                {
                    func(static_cast<Handle::IndexType>(i));
                });
        }

        Handle::IndexType getUpperBoundOnAllocatedElements() const
        {
            return static_cast<Handle::IndexType>(this->allocator.getUpperBoundOnAllocatedElements());
        }

    private:
        util::IndexAllocator allocator;
    };

    template<class Handle, auto PtrToMemberDeleter>
    UniqueOpaqueHandle<Handle, PtrToMemberDeleter>::~UniqueOpaqueHandle()
    {
        if (!this->isNull())
        {
            (this->owner->*PtrToMemberDeleter)(std::move(*static_cast<Handle*>(this)));
        }
    }

} // namespace util