#pragma once

#include "util/allocators/index_allocator.hpp"
#include "util/threads.hpp"
#include "util/util.hpp"
#include <map>
#include <source_location>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace gfx::core::vulkan
{
    class Device;

    template<vk::DescriptorType D>
    struct RegisterDescriptorArgs;

    template<>
    struct RegisterDescriptorArgs<vk::DescriptorType::eSampler>
    {
        vk::Sampler sampler;
    };

    template<>
    struct RegisterDescriptorArgs<vk::DescriptorType::eSampledImage>
    {
        vk::ImageView   view;
        vk::ImageLayout layout;
    };

    template<>
    struct RegisterDescriptorArgs<vk::DescriptorType::eStorageImage>
    {
        vk::ImageView   view;
        vk::ImageLayout layout;
    };

    template<>
    struct RegisterDescriptorArgs<vk::DescriptorType::eUniformBuffer>
    {
        vk::Buffer buffer;
        usize      size_bytes;
    };

    template<>
    struct RegisterDescriptorArgs<vk::DescriptorType::eStorageBuffer>
    {
        vk::Buffer buffer;
        usize      size_bytes;
    };

    u32 getShaderBindingLocation(vk::DescriptorType);

    template<vk::DescriptorType D>
        requires (
            D == vk::DescriptorType::eSampler || D == vk::DescriptorType::eSampledImage
            || D == vk::DescriptorType::eStorageImage || D == vk::DescriptorType::eUniformBuffer
            || D == vk::DescriptorType::eStorageBuffer)
    class DescriptorHandle
    {
    public:
        using StorageType = u8;
    public:
        ~DescriptorHandle() = default;

        DescriptorHandle(const DescriptorHandle&)             = default;
        DescriptorHandle(DescriptorHandle&&)                  = default;
        DescriptorHandle& operator= (const DescriptorHandle&) = default;
        DescriptorHandle& operator= (DescriptorHandle&&)      = default;

        [[nodiscard]] u8 getOffset() const
        {
            return this->data;
        }
    private:
        friend class DescriptorManager;
        explicit DescriptorHandle(u8 offset)
            : data {offset}
        {}

        u8 data;
    };

    class DescriptorManager
    {
    public:
        explicit DescriptorManager(const Device&);
        ~DescriptorManager() = default;

        DescriptorManager(const DescriptorManager&)             = delete;
        DescriptorManager(DescriptorManager&&)                  = default;
        DescriptorManager& operator= (const DescriptorManager&) = delete;
        DescriptorManager& operator= (DescriptorManager&&)      = default;

        struct DescriptorReport
        {
            u32                  offset;
            std::string          name;
            // Is nullopt when a size can't be determined (images)
            std::optional<usize> maybe_size_bytes;
        };

        std::map<vk::DescriptorType, std::vector<DescriptorReport>> getAllDescriptorsDebugInfo() const;

        [[nodiscard]] vk::DescriptorSet  getGlobalDescriptorSet() const;
        [[nodiscard]] vk::PipelineLayout getGlobalPipelineLayout() const;

        template<vk::DescriptorType D>
        [[nodiscard]] DescriptorHandle<D> registerDescriptor(
            RegisterDescriptorArgs<D>, std::string name, std::source_location = std::source_location::current()) const;
        template<vk::DescriptorType D>
        void deregisterDescriptor(DescriptorHandle<D>) const;

    private:
        vk::Device device;

        vk::UniqueDescriptorPool      bindless_descriptor_pool;
        vk::UniqueDescriptorSetLayout bindless_descriptor_set_layout;
        vk::UniquePipelineLayout      bindless_pipeline_layout;
        vk::DescriptorSet             bindless_descriptor_set;

        struct DebugDescriptorInfo
        {
            std::string          name;
            std::optional<usize> size_bytes;
        };

        struct CriticalSection
        {
            std::unordered_map<vk::DescriptorType, util::IndexAllocator>             binding_allocators;
            std::unordered_map<vk::DescriptorType, std::vector<DebugDescriptorInfo>> debug_descriptor_info;
        };

        util::Mutex<CriticalSection> critical_section;
    };

} // namespace gfx::core::vulkan