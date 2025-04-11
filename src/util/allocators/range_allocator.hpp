#pragma once

#include "util/allocators/opaque_integer_handle_allocator.hpp"
#include "util/util.hpp"
#include <expected>
#include <memory>
#include <source_location>

namespace OffsetAllocator // NOLINT stupid library
{
    class Allocator;
} // namespace OffsetAllocator

namespace util
{

    struct InclusiveRange
    {
        std::size_t start;
        std::size_t end;

        constexpr std::strong_ordering operator<=> (const InclusiveRange& other) const
        {
            return this->start <=> other.start;
        }

        [[nodiscard]] std::size_t size() const
        {
            return end - start + 1;
        }
    };

    class RangeAllocator;

    // HACK: it works... and the null states align...
    using RangeAllocation = OpaqueHandle<"RangeAllocation", u64, RangeAllocator>;

    // sane wrapper around sebbbi's offset allocator to make it actually rule of
    // 5 compliant as well as fixing a bunch of idiotic design decisions (such
    // as not having header guards... and having a cmake script that screws
    // things up by default...)
    class RangeAllocator
    {
    public:
        struct OutOfBlocks : public std::bad_alloc
        {
            [[nodiscard]] const char* what() const noexcept override;
        };
    public:
        RangeAllocator(u32 size, u32 maxAllocations);
        ~RangeAllocator();

        RangeAllocator(const RangeAllocator&) = delete;
        RangeAllocator(RangeAllocator&&) noexcept;
        RangeAllocator& operator= (const RangeAllocator&) = delete;
        RangeAllocator& operator= (RangeAllocator&&) noexcept;

        [[nodiscard]] RangeAllocation allocate(u32 size, std::source_location = std::source_location::current());
        [[nodiscard]] std::expected<RangeAllocation, OutOfBlocks> tryAllocate(u32 size);
        void                                                      free(RangeAllocation);

        [[nodiscard]] static u32          getOffsetofAllocation(const RangeAllocation&);
        [[nodiscard]] u32                 getSizeOfAllocation(const RangeAllocation&) const;
        [[nodiscard]] std::pair<u32, u32> getStorageInfo() const;


    private:
        std::unique_ptr<OffsetAllocator::Allocator> internal_allocator;
    };
} // namespace util
