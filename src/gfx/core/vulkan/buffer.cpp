#include "buffer.hpp"
#include "allocator.hpp"
#include "device.hpp"
#include "util/allocators/range_allocator.hpp"
#include "util/util.hpp"
#include <boost/container/small_vector.hpp>
#include <optional>
#include <source_location>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hash.hpp>

namespace gfx::core::vulkan
{
    std::atomic<std::size_t> bufferBytesAllocated            = 0; // NOLINT
    std::atomic<std::size_t> hostVisibleBufferBytesAllocated = 0; // NOLINT

    static constexpr std::size_t StagingBufferSize = std::size_t {32} * 1024 * 1024;

    BufferStager::BufferStager(const Renderer* renderer_)
        : renderer {renderer_}
        , staging_buffer {
              this->renderer,
              vk::BufferUsageFlagBits::eTransferSrc,
              vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible,  StagingBufferSize,
              "Staging Buffer",
              std::nullopt
          }
        , transfer_allocator {util::RangeAllocator {StagingBufferSize, 1024 * 128}}
        , transfers {std::vector<BufferTransfer> {}}
    {}

    BufferStager::~BufferStager()
    {
        this->transfer_allocator.lock(
            [this](util::RangeAllocator& allocator)
            {
                this->transfers.lock(
                    [&](std::vector<BufferTransfer>& ts)
                    {
                        for (BufferTransfer& t : ts)
                        {
                            allocator.free(std::move(t.staging_allocation));
                        }
                    });

                this->transfers_to_free.lock(
                    [&](std::unordered_map<std::shared_ptr<vk::UniqueFence>, std::vector<BufferTransfer>>& tsMap)
                    {
                        for (auto& [_, ts] : tsMap)
                        {
                            for (BufferTransfer& t : ts)
                            {
                                allocator.free(std::move(t.staging_allocation));
                            }
                        }
                    });
            });
    }

    void BufferStager::enqueueByteTransfer(
        vk::Buffer buffer, u32 offset, std::span<const std::byte> dataToWrite, std::source_location location) const
    {
        this->enqueueByteTransfer(
            buffer, offset, std::vector<std::byte> {dataToWrite.begin(), dataToWrite.end()}, location);
    }

    void BufferStager::enqueueByteTransfer(
        vk::Buffer buffer, u32 offset, std::vector<std::byte> dataToWrite, std::source_location location) const
    {
        assert::critical<std::size_t>(
            dataToWrite.size() < StagingBufferSize / 2,
            "Buffer::enqueueByteTransfer of size {} is too large",
            dataToWrite.size(),
            location);

        assert::warn<std::size_t>(
            dataToWrite.size() > 0,
            "BufferStager::enqueueByteTransfer of size {} is too small",
            dataToWrite.size(),
            location);

        std::expected<util::RangeAllocation, util::RangeAllocator::OutOfBlocks> maybeAllocation =
            this->transfer_allocator.lock(
                [&](util::RangeAllocator& a)
                {
                    return a.tryAllocate(static_cast<u32>(dataToWrite.size()));
                });

        if (maybeAllocation.has_value())
        {
            this->allocated += dataToWrite.size();

            std::span<std::byte> stagingBufferData = this->staging_buffer.getGpuDataNonCoherent();

            // static_assert(std::is_trivially_copyable_v<std::byte>);
            std::memcpy(
                stagingBufferData.data() + util::RangeAllocator::getOffsetofAllocation(*maybeAllocation),
                dataToWrite.data(),
                dataToWrite.size());

            this->transfers.lock(
                [&](std::vector<BufferTransfer>& t)
                {
                    t.push_back(BufferTransfer {
                        .staging_allocation {std::move(*maybeAllocation)},
                        .output_buffer {buffer},
                        .output_offset {offset},
                        .size {static_cast<u32>(dataToWrite.size())},
                    });
                });
        }
        else
        {
            // shit we need to overflow

            this->overflow_transfers.lock(
                [&](std::vector<OverflowTransfer>& overflowTransfers)
                {
                    overflowTransfers.push_back({OverflowTransfer {
                        .buffer {buffer}, .offset {offset}, .data {std::move(dataToWrite)}, .location {location}}});
                });
        }
    }

    void BufferStager::cleanupCompletedTransfers() const
    {
        auto [tt, bb] = this->getUsage();

        log::trace("{}/{}", tt, bb);
        // Free all allocations that have already completed.
        this->transfers_to_free.lock(
            [&](std::unordered_map<std::shared_ptr<vk::UniqueFence>, std::vector<BufferTransfer>>& toFreeMap)
            {
                std::vector<std::shared_ptr<vk::UniqueFence>> toRemove {};

                for (auto& [fence, allocations] : toFreeMap)
                {
                    if (this->renderer->getDevice()->getDevice().getFenceStatus(**fence) == vk::Result::eSuccess)
                    {
                        toRemove.push_back(fence);

                        this->transfer_allocator.lock(
                            [&](util::RangeAllocator& stagingAllocator)
                            {
                                for (BufferTransfer& transfer : allocations)
                                {
                                    this->allocated -= transfer.size;
                                    stagingAllocator.free(std::move(transfer.staging_allocation));
                                }
                            });
                    }
                }

                for (const std::shared_ptr<vk::UniqueFence>& f : toRemove)
                {
                    toFreeMap.erase(f);
                }
            });

        auto [t, b] = this->getUsage();

        log::trace("{}/{}", t, b);
    }

    void BufferStager::flushTransfers(
        vk::CommandBuffer commandBuffer, std::shared_ptr<vk::UniqueFence> flushFinishFence) const
    {
        std::vector<BufferTransfer> grabbedTransfers = this->transfers.moveInner();

        std::unordered_map<vk::Buffer, std::vector<vk::BufferCopy>> copies {};
        std::vector<FlushData>                                      stagingFlushes {};

        for (const BufferTransfer& transfer : grabbedTransfers)
        {
            if (transfer.size == 0)
            {
                log::warn("zst transfer!");

                continue;
            }
            const u32 offset = util::RangeAllocator::getOffsetofAllocation(transfer.staging_allocation);

            copies[transfer.output_buffer].push_back(vk::BufferCopy {
                .srcOffset {offset},
                .dstOffset {transfer.output_offset},
                .size {transfer.size},
            });

            stagingFlushes.push_back(FlushData {.offset_bytes {offset}, .size_bytes {transfer.size}});
        }

        for (const auto& [outputBuffer, bufferCopies] : copies)
        {
            if (bufferCopies.size() > 4096)
            {
                log::warn("Excessive copies on buffer {}", bufferCopies.size());
            }
            commandBuffer.copyBuffer(*this->staging_buffer, outputBuffer, bufferCopies);
        }

        this->staging_buffer.flush(stagingFlushes);

        this->transfers_to_free.lock(
            [&](std::unordered_map<std::shared_ptr<vk::UniqueFence>, std::vector<BufferTransfer>>& toFreeMap)
            {
                toFreeMap[flushFinishFence].insert(
                    toFreeMap[flushFinishFence].begin(),
                    std::make_move_iterator(grabbedTransfers.begin()),
                    std::make_move_iterator(grabbedTransfers.end()));
            });

        {
            std::vector<OverflowTransfer> overflows = this->overflow_transfers.moveInner();

            if (!overflows.empty())
            {
                log::warn("{} buffer transfer overflows this frame!", overflows.size());
            }

            for (OverflowTransfer& t : overflows)
            {
                this->enqueueByteTransfer(t.buffer, t.offset, std::move(t.data), t.location);
            }
        }
    }

    std::pair<std::size_t, std::size_t> BufferStager::getUsage() const
    {
        return {this->allocated.load(), StagingBufferSize};
    }

} // namespace gfx::core::vulkan
