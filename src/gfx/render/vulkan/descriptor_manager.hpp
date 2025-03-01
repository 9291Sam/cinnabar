#pragma once

#include "util/util.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace gfx::render::vulkan
{

    // Primary template (undefined to catch invalid cases early)
    template<vk::DescriptorType D>
    struct DescriptorTypeMap;

    // Specializations for valid descriptor types
    template<>
    struct DescriptorTypeMap<vk::DescriptorType::eSampler>
    {
        using Type = vk::Sampler;
    };

    template<>
    struct DescriptorTypeMap<vk::DescriptorType::eSampledImage>
    {
        using Type = vk::Image;
    };

    template<>
    struct DescriptorTypeMap<vk::DescriptorType::eStorageImage>
    {
        using Type = vk::Image;
    };

    template<>
    struct DescriptorTypeMap<vk::DescriptorType::eUniformBuffer>
    {
        using Type = vk::Buffer;
    };

    template<>
    struct DescriptorTypeMap<vk::DescriptorType::eStorageBuffer>
    {
        using Type = vk::Buffer;
    };

    template<vk::DescriptorType D>
        requires (
            D == vk::DescriptorType::eSampler || D == vk::DescriptorType::eSampledImage
            || D == vk::DescriptorType::eStorageImage || D == vk::DescriptorType::eUniformBuffer
            || D == vk::DescriptorType::eStorageBuffer)
    class DescriptorHandle
    {
    public:
        using DescriptorType = DescriptorTypeMap<D>::Type;
    public:
        ~DescriptorHandle() = default;

        DescriptorHandle(const DescriptorHandle&)             = default;
        DescriptorHandle(DescriptorHandle&&)                  = default;
        DescriptorHandle& operator= (const DescriptorHandle&) = default;
        DescriptorHandle& operator= (DescriptorHandle&&)      = default;

        [[nodiscard]] u8 getOffset() const;
    private:
        explicit DescriptorHandle(u8 offset);

        u8 data;
    };

    static_assert(std::is_same_v<DescriptorHandle<vk::DescriptorType::eSampler>::DescriptorType, vk::Sampler>);
    static_assert(std::is_same_v<DescriptorHandle<vk::DescriptorType::eSampledImage>::DescriptorType, vk::Image>);
    static_assert(std::is_same_v<DescriptorHandle<vk::DescriptorType::eStorageImage>::DescriptorType, vk::Image>);
    static_assert(std::is_same_v<DescriptorHandle<vk::DescriptorType::eUniformBuffer>::DescriptorType, vk::Buffer>);
    static_assert(std::is_same_v<DescriptorHandle<vk::DescriptorType::eStorageBuffer>::DescriptorType, vk::Buffer>);

    // class DescriptorManager
    // {
    // public:
    //     DescriptorManager(...);
    //     ~DescriptorManager() = default;

    //     DescriptorManager(const DescriptorManager&)             = default;
    //     DescriptorManager(DescriptorManager&&)                  = default;
    //     DescriptorManager& operator= (const DescriptorManager&) = default;
    //     DescriptorManager& operator= (DescriptorManager&&)      = default;

    //     void                allocateDescriptor template<vk::DescriptorType D>
    //     DescriptorHandle<D> registerDescriptor()
    // };

} // namespace gfx::render::vulkan