#pragma once

#include "allocator.hpp"
#include "descriptor_manager.hpp"
#include "device.hpp"
#include "gfx/core/renderer.hpp"
#include "util/allocators/range_allocator.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <atomic>
#include <bit>
#include <limits>
#include <source_location>
#include <type_traits>
#include <utility>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace gfx::core::vulkan
{
    class Allocator;
    class Device;
    class BufferStager;

    struct FlushData
    {
        vk::DeviceSize offset_bytes;
        vk::DeviceSize size_bytes;
    };

    extern std::atomic<std::size_t> bufferBytesAllocated;            // NOLINT
    extern std::atomic<std::size_t> hostVisibleBufferBytesAllocated; // NOLINT

    template<class T>
        requires std::is_trivially_copyable_v<T>
    class GpuOnlyBuffer
    {
    public:

        GpuOnlyBuffer()
            : renderer {nullptr}
            , buffer {nullptr}
            , allocation {nullptr}
            , elements {0}
        {}
        GpuOnlyBuffer(
            const Renderer*         renderer_,
            vk::BufferUsageFlags    usage_,
            vk::MemoryPropertyFlags memoryPropertyFlags,
            std::size_t             elements_,
            std::string             name_,
            std::optional<u8>       maybeDescriptorBindingLocation,
            std::source_location    loc = std::source_location::current())
            : name {std::move(name_)}
            , renderer {renderer_}
            , buffer {nullptr}
            , allocation {nullptr}
            , elements {elements_}
            , usage {usage_}
            , memory_property_flags {memoryPropertyFlags}
            , maybe_descriptor_binding_location {maybeDescriptorBindingLocation}
        {
            const u64 totalBufferSizeBytes = u64 {elements_} * u64 {sizeof(T)};
            assert::critical(
                totalBufferSizeBytes < u64 {std::numeric_limits<u32>::max()},
                "Tried to create a buffer of size {}, which is larger than the maximum allowed value of {}",
                util::bytesAsSiNamed(static_cast<long double>(totalBufferSizeBytes)),
                util::bytesAsSiNamed(static_cast<long double>(std::numeric_limits<u32>::max())));
            assert::critical(!this->name.empty(), "Tried to create buffer with empty name!");

            assert::critical(
                !(memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent), "Tried to create coherent buffer!");

            if (this->renderer->getDevice()->isIntegrated() && this->renderer->getDevice()->isAmd()
                && memoryPropertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
            {
                memoryPropertyFlags &= ~vk::MemoryPropertyFlagBits::eDeviceLocal;
                log::warn("Paving over excessive Device Local {}", vk::to_string(memoryPropertyFlags));
            }

            const VkBufferCreateInfo bufferCreateInfo {
                .sType {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO},
                .pNext {nullptr},
                .flags {},
                .size {this->elements * sizeof(T)},
                .usage {static_cast<VkBufferUsageFlags>(this->usage)},
                .sharingMode {VK_SHARING_MODE_EXCLUSIVE},
                .queueFamilyIndexCount {0},
                .pQueueFamilyIndices {nullptr},
            };

            const VmaAllocationCreateInfo allocationCreateInfo {
                .flags {VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT},
                .usage {
                    this->renderer->getDevice()->isIntegrated() ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
                                                                : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE},
                .requiredFlags {static_cast<VkMemoryPropertyFlags>(memoryPropertyFlags)},
                .preferredFlags {},
                .memoryTypeBits {},
                .pool {nullptr},
                .pUserData {},
                .priority {},
            };

            VkBuffer outputBuffer = nullptr;

            const vk::Result result {::vmaCreateBuffer(
                **this->renderer->getAllocator(),
                &bufferCreateInfo,
                &allocationCreateInfo,
                &outputBuffer,
                &this->allocation,
                nullptr)};

            assert::critical(
                result == vk::Result::eSuccess,
                "Failed to allocate buffer {} | Size: {}",
                vk::to_string(vk::Result {result}),
                this->elements * sizeof(T));

            this->buffer = outputBuffer;

            if constexpr (CINNABAR_DEBUG_BUILD)
            {
                this->renderer->getDevice()->getDevice().setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eBuffer},
                    .objectHandle {std::bit_cast<u64>(this->buffer)},
                    .pObjectName {this->name.c_str()},
                });
            }

            bufferBytesAllocated.fetch_add(this->elements * sizeof(T), std::memory_order_release);

            if (vk::MemoryPropertyFlagBits::eHostVisible & memoryPropertyFlags)
            {
                hostVisibleBufferBytesAllocated.fetch_add(this->elements * sizeof(T), std::memory_order_release);
            }

            if (this->usage & vk::BufferUsageFlagBits::eUniformBuffer)
            {
                const u8 descriptorBindingLocation = *this->maybe_descriptor_binding_location.or_else(
                    [] -> std::optional<u8>
                    {
                        panic("Buffer was not given a binding location!");

                        return std::nullopt;
                    });

                this->maybe_uniform_descriptor_handle = this->renderer->getDescriptorManager()->registerDescriptor(
                    RegisterDescriptorArgs<vk::DescriptorType::eUniformBuffer> {
                        .buffer {this->buffer}, .size_bytes {this->elements * sizeof(T)}},
                    this->name,
                    descriptorBindingLocation,
                    loc);
            }

            if (this->usage & vk::BufferUsageFlagBits::eStorageBuffer)
            {
                const u8 descriptorBindingLocation = *this->maybe_descriptor_binding_location.or_else(
                    [] -> std::optional<u8>
                    {
                        panic("Buffer was not given a binding location!");

                        return std::nullopt;
                    });

                this->maybe_storage_descriptor_handle = this->renderer->getDescriptorManager()->registerDescriptor(
                    RegisterDescriptorArgs<vk::DescriptorType::eStorageBuffer> {
                        .buffer {this->buffer}, .size_bytes {this->elements * sizeof(T)}},
                    this->name,
                    descriptorBindingLocation,
                    loc);
            }
        }
        ~GpuOnlyBuffer()
        {
            if (this->maybe_uniform_descriptor_handle.has_value())
            {
                this->renderer->getDescriptorManager()->deregisterDescriptor(*this->maybe_uniform_descriptor_handle);
            }

            if (this->maybe_storage_descriptor_handle.has_value())
            {
                this->renderer->getDescriptorManager()->deregisterDescriptor(*this->maybe_storage_descriptor_handle);
            }
            this->free();
        }

        GpuOnlyBuffer(const GpuOnlyBuffer&) = delete;
        GpuOnlyBuffer& operator= (GpuOnlyBuffer&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~GpuOnlyBuffer();

            new (this) GpuOnlyBuffer {std::move(other)};

            return *this;
        }
        GpuOnlyBuffer& operator= (const GpuOnlyBuffer&) = delete;
        GpuOnlyBuffer(GpuOnlyBuffer&& other) noexcept
            : renderer {other.renderer}
            , buffer {other.buffer}
            , allocation {other.allocation}
            , elements {other.elements}
            , usage {other.usage}
            , memory_property_flags {other.memory_property_flags}
            , maybe_uniform_descriptor_handle {std::exchange(other.maybe_uniform_descriptor_handle, std::nullopt)}
            , maybe_storage_descriptor_handle {std::exchange(other.maybe_storage_descriptor_handle, std::nullopt)}
        {
            other.renderer              = nullptr;
            other.buffer                = nullptr;
            other.allocation            = nullptr;
            other.elements              = 0;
            other.usage                 = {};
            other.memory_property_flags = {};
        }

        vk::Buffer operator* () const
        {
            return vk::Buffer {this->buffer};
        }

    protected:

        friend class BufferStager;

        void free()
        {
            if (this->renderer == nullptr)
            {
                return;
            }

            ::vmaDestroyBuffer(**this->renderer->getAllocator(), this->buffer, this->allocation);

            bufferBytesAllocated.fetch_sub(this->elements * sizeof(T), std::memory_order_release);

            if (this->memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                hostVisibleBufferBytesAllocated.fetch_sub(this->elements * sizeof(T), std::memory_order_release);
            }
        }

        std::string             name;
        const Renderer*         renderer;
        vk::Buffer              buffer;
        VmaAllocation           allocation;
        std::size_t             elements;
        vk::BufferUsageFlags    usage;
        vk::MemoryPropertyFlags memory_property_flags;

        std::optional<DescriptorHandle<vk::DescriptorType::eUniformBuffer>> maybe_uniform_descriptor_handle;
        std::optional<DescriptorHandle<vk::DescriptorType::eStorageBuffer>> maybe_storage_descriptor_handle;
        std::optional<u8>                                                   maybe_descriptor_binding_location;
    };

    template<class T>
    class WriteOnlyBuffer : public GpuOnlyBuffer<T>
    {
    public:

        WriteOnlyBuffer(
            const Renderer*         renderer_,
            vk::BufferUsageFlags    usage_,
            vk::MemoryPropertyFlags memoryPropertyFlags,
            std::size_t             elements_,
            std::string             name_,
            std::optional<u8>       maybeDescriptorBindingLocation,
            std::source_location    loc = std::source_location::current())
            : gfx::core::vulkan::GpuOnlyBuffer<T> {
                  renderer_,
                  usage_,
                  memoryPropertyFlags,
                  elements_,
                  std::move(name_),
                  maybeDescriptorBindingLocation,
                  loc}
        {}
        ~WriteOnlyBuffer()
        {
            this->free();
        }

        WriteOnlyBuffer(const WriteOnlyBuffer&) = delete;
        WriteOnlyBuffer& operator= (WriteOnlyBuffer&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~Buffer();

            new (this) WriteOnlyBuffer {std::move(other)};

            return *this;
        }
        WriteOnlyBuffer& operator= (const WriteOnlyBuffer&) = delete;
        WriteOnlyBuffer(WriteOnlyBuffer&& other) noexcept
            : GpuOnlyBuffer<T> {std::move(other)}
            , mapped_memory {other.mapped_memory.load()}
        {
            other.mapped_memory = nullptr;
        }

        void uploadImmediate(u32 offset, std::span<const T> payload)
            requires std::is_copy_constructible_v<T>
        {
            std::copy(payload.begin(), payload.end(), this->getGpuDataNonCoherent().data() + offset);

            const gfx::core::vulkan::FlushData flush {
                .offset_bytes {offset * sizeof(T)}, .size_bytes {payload.size_bytes()}};

            this->flush({&flush, 1});
        }

        void fillImmediate(const T& value)
        {
            const std::span<T> data = this->getGpuDataNonCoherent();

            std::fill(data.begin(), data.end(), value);

            const gfx::core::vulkan::FlushData flush {
                .offset_bytes {0},
                .size_bytes {this->elements * sizeof(T)},
            };

            this->flush({&flush, 1});
        }

        std::size_t getSizeBytes()
        {
            return this->elements * sizeof(T);
        }

        friend class BufferStager;

        std::span<const T> getGpuDataNonCoherent() const
        {
            return {this->getMappedData(), this->elements};
        }

        std::span<T> getGpuDataNonCoherent()
        {
            return {this->getMappedData(), this->elements};
        }

        void flush(std::span<const FlushData> flushes)
        {
            VkResult result = VK_ERROR_UNKNOWN;

            if (flushes.size() == 1)
            {
                result = vmaFlushAllocation(
                    **this->renderer->getAllocator(), this->allocation, flushes[0].offset_bytes, flushes[0].size_bytes);
            }
            else
            {
                const std::size_t numberOfFlushes = flushes.size();

                std::vector<VmaAllocation> allocations {};
                allocations.resize(numberOfFlushes, this->allocation);

                std::vector<vk::DeviceSize> offsets;
                std::vector<vk::DeviceSize> sizes;

                offsets.reserve(numberOfFlushes);
                sizes.reserve(numberOfFlushes);

                for (const FlushData& f : flushes)
                {
                    offsets.push_back(f.offset_bytes);
                    sizes.push_back(f.size_bytes);
                }

                result = vmaFlushAllocations(
                    **this->renderer->getAllocator(),
                    static_cast<u32>(numberOfFlushes),
                    allocations.data(),
                    offsets.data(),
                    sizes.data());
            }

            assert::critical(result == VK_SUCCESS, "Buffer flush failed | {}", vk::to_string(vk::Result {result}));
        }

    private:

        void free()
        {
            if (this->mapped_memory != nullptr)
            {
                ::vmaUnmapMemory(**this->renderer->getAllocator(), this->allocation);
                this->mapped_memory = nullptr;
            }
        }

        T* getMappedData() const
        {
            /// This suspicious usage of atomics isn't actually a data race
            /// because of how vmaMapMemory is implemented
            if (this->mapped_memory == nullptr)
            {
                void* outputMappedMemory = nullptr;

                const VkResult result =
                    ::vmaMapMemory(**this->renderer->getAllocator(), this->allocation, &outputMappedMemory);

                assert::critical(
                    result == VK_SUCCESS, "Failed to map buffer memory {}", vk::to_string(vk::Result {result}));

                assert::critical(
                    outputMappedMemory != nullptr, "Mapped ptr was nullptr! | {}", vk::to_string(vk::Result {result}));

                this->mapped_memory.store(static_cast<T*>(outputMappedMemory), std::memory_order_seq_cst);
            }

            return this->mapped_memory;
        }

        mutable std::atomic<T*> mapped_memory;
    };

    template<class T>
    class CpuCachedBuffer : public WriteOnlyBuffer<T>
    {
    public:

        CpuCachedBuffer(
            const Renderer*         renderer_,
            vk::BufferUsageFlags    usage_,
            vk::MemoryPropertyFlags memoryPropertyFlags,
            std::size_t             elements_,
            std::string             name_,
            std::optional<u8>       maybeDescriptorBindingLocation,
            std::source_location    loc = std::source_location::current())
            : gfx::core::vulkan::WriteOnlyBuffer<T> {
                  renderer_,
                  usage_,
                  memoryPropertyFlags,
                  elements_,
                  std::move(name_),
                  maybeDescriptorBindingLocation,
                  loc}
        {
            assert::warn(
                static_cast<bool>(vk::BufferUsageFlagBits::eTransferDst | usage_),
                "Creating CpuCachedBuffer<{}> without vk::BufferUsageFlagBits::eTransferDst",
                util::getNameOfType<T>());

            this->cpu_buffer.resize(this->elements);
        }
        ~CpuCachedBuffer() = default;

        CpuCachedBuffer(const CpuCachedBuffer&) = delete;
        CpuCachedBuffer& operator= (CpuCachedBuffer&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            this->~Buffer();

            new (this) CpuCachedBuffer {std::move(other)};

            return *this;
        }
        CpuCachedBuffer& operator= (const CpuCachedBuffer&) = delete;
        CpuCachedBuffer(CpuCachedBuffer&& other) noexcept
            : WriteOnlyBuffer<T> {std::move(other)}
            , cpu_buffer {std::move(other.cpu_buffer)}
            , flushes {std::move(other.flushes)}
        {}

        std::span<const T> read(std::size_t offset, std::size_t size) const
        {
            return std::span<const T> {&this->cpu_buffer[offset], size};
        }
        const T& read(std::size_t offset) const
        {
            return this->cpu_buffer[offset];
        }

        void write(std::size_t offset, std::span<const T> data)
        {
            this->flushes.push_back(FlushData {.offset_bytes {offset * sizeof(T)}, .size_bytes {data.size_bytes()}});

            std::memcpy(&this->cpu_buffer[offset], data.data(), data.size_bytes());
        }
        void write(std::size_t offset, const T& t)
        {
            this->write(offset, std::span<const T> {&t, 1});
        }

        template<auto Ptr>
        void write(std::size_t offsetElements, const util::MemberTypeT<decltype(Ptr)>& write)
            requires std::is_member_pointer_v<decltype(Ptr)>
        {
            const std::size_t byteOffset = util::getOffsetOfPointerToMember(Ptr);

            this->flushes.push_back(
                FlushData {.offset_bytes {(offsetElements * sizeof(T)) + byteOffset}, .size_bytes {sizeof(write)}});

            std::memcpy(reinterpret_cast<char*>(&this->cpu_buffer[offsetElements]) + byteOffset, &write, sizeof(write));
        }

        std::span<T> modify(std::size_t offset, std::size_t size)
        {
            assert::critical(size > 0, "dont do this");

            this->flushes.push_back(FlushData {.offset_bytes {offset * sizeof(T)}, .size_bytes {size * sizeof(T)}});

            return std::span<T> {&this->cpu_buffer[offset], size};
        }
        T& modify(std::size_t offset)
        {
            assert::critical(!this->cpu_buffer.empty(), "hmmm");

            return this->modify(offset, 1)[0];
        }
        template<auto Ptr>
        util::MemberTypeT<decltype(Ptr)>& modify(std::size_t offsetElements)
            requires std::is_member_pointer_v<decltype(Ptr)>
        {
            const std::size_t byteOffset = util::getOffsetOfPointerToMember(Ptr);

            this->flushes.push_back(FlushData {
                .offset_bytes {(offsetElements * sizeof(T)) + byteOffset},
                .size_bytes {sizeof(util::MemberTypeT<decltype(Ptr)>)}});

            return this->cpu_buffer[offsetElements].*Ptr;
        }

        void flushViaStager(const BufferStager& stager, std::source_location = std::source_location::current());
        void flushCachedChangesImmediate()
        {
            std::vector<FlushData> localFlushes = this->grabFlushes();
            const std::span<T>     gpuData      = this->getGpuDataNonCoherent();

            for (FlushData& f : localFlushes)
            {
                static_assert(std::is_trivially_copyable_v<T>);
                std::memcpy(
                    static_cast<char*>(gpuData.begin()) + f.offset_bytes,
                    static_cast<const char*>(this->cpu_buffer.begin()) + f.offset_bytes,
                    f.size_bytes);
            }

            this->flush(std::move(localFlushes));
        }

        std::vector<FlushData> grabFlushes()
        {
            return std::move(this->flushes);
        }
    private:
        std::vector<T>         cpu_buffer;
        std::vector<FlushData> flushes;
    };

    class BufferStager
    {
    public:

        explicit BufferStager(const Renderer*);
        ~BufferStager();

        BufferStager(const BufferStager&)             = delete;
        BufferStager(BufferStager&&)                  = delete;
        BufferStager& operator= (const BufferStager&) = delete;
        BufferStager& operator= (BufferStager&&)      = delete;

        template<class T>
            requires std::is_trivially_copyable_v<T>
        void enqueueTransfer(
            const GpuOnlyBuffer<T>& buffer,
            u32                     offset,
            std::span<const T>      data,
            std::source_location    location = std::source_location::current()) const
        {
            this->enqueueByteTransfer(
                *buffer,
                offset * sizeof(T), // NOLINTNEXTLINE
                std::span {reinterpret_cast<const std::byte*>(data.data()), data.size_bytes()},
                location);
        }

        void enqueueByteTransfer(vk::Buffer, u32 offset, std::span<const std::byte>, std::source_location) const;

        void flushTransfers(vk::CommandBuffer, std::shared_ptr<vk::UniqueFence> flushFinishFence) const;

        std::pair<std::size_t, std::size_t> getUsage() const;

    private:

        struct BufferTransfer
        {
            util::RangeAllocation staging_allocation;
            vk::Buffer            output_buffer;
            u32                   output_offset;
            u32                   size;
        };

        mutable std::atomic<std::size_t>                      allocated;
        const Renderer*                                       renderer;
        mutable gfx::core::vulkan::WriteOnlyBuffer<std::byte> staging_buffer;

        struct OverflowTransfer
        {
            vk::Buffer             buffer;
            u32                    offset;
            std::vector<std::byte> data;
            std::source_location   location;
        };

        util::Mutex<std::vector<OverflowTransfer>> overflow_transfers;

        util::Mutex<util::RangeAllocator>        transfer_allocator;
        util::Mutex<std::vector<BufferTransfer>> transfers;
        util::Mutex<std::unordered_map<std::shared_ptr<vk::UniqueFence>, std::vector<BufferTransfer>>>
            transfers_to_free;
    };

    template<class T>
    void CpuCachedBuffer<T>::flushViaStager(const BufferStager& stager, std::source_location location)
    {
        std::vector<FlushData> mergedFlushes = this->grabFlushes();

        for (const FlushData& f : mergedFlushes)
        {
            assert::critical(f.offset_bytes < static_cast<u64>(std::numeric_limits<u32>::max()), "oop offset ");
            assert::critical(f.size_bytes < static_cast<u64>(std::numeric_limits<u32>::max()), "oop size");

            stager.enqueueByteTransfer(
                **this,
                static_cast<u32>(f.offset_bytes),
                {reinterpret_cast<const std::byte*>(this->cpu_buffer.data()) + f.offset_bytes, f.size_bytes},
                location);
        }
    }

} // namespace gfx::core::vulkan
