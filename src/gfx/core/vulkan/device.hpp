#pragma once

#include "util/threads.hpp"
#include "util/util.hpp"
#include <functional>
#include <vulkan/vulkan.hpp>

namespace gfx::core::vulkan
{
    class Device
    {
    public:
        enum class QueueType : u8
        {
            Graphics           = 0,
            AsyncCompute       = 1,
            AsyncTransfer      = 2,
            NumberOfQueueTypes = 3,
        };

    public:
        Device(vk::Instance, vk::SurfaceKHR);
        ~Device() = default;

        Device(const Device&)             = delete;
        Device(Device&&)                  = delete;
        Device& operator= (const Device&) = delete;
        Device& operator= (Device&&)      = delete;

        [[nodiscard]] std::optional<u32> getFamilyOfQueueType(QueueType) const noexcept;
        [[nodiscard]] u32                getNumberOfQueues(QueueType) const noexcept;
        [[nodiscard]] vk::PhysicalDevice getPhysicalDevice() const noexcept;
        [[nodiscard]] bool               isIntegrated() const noexcept;
        [[nodiscard]] vk::Device         getDevice() const noexcept;
        [[nodiscard]] bool               isAmd() const noexcept;
        [[nodiscard]] const vk::Device*  operator->() const noexcept;

        void acquireQueue(QueueType queueType, std::function<void(vk::Queue)> accessFunc) const noexcept;

    private:
        std::array<std::vector<util::Mutex<vk::Queue>>, static_cast<std::size_t>(QueueType::NumberOfQueueTypes)> queues;

        std::array<std::optional<u32>, static_cast<std::size_t>(QueueType::NumberOfQueueTypes)> queue_family_indexes;

        std::array<u32, static_cast<std::size_t>(QueueType::NumberOfQueueTypes)> queue_family_numbers;

        vk::PhysicalDevice     physical_device;
        vk::PhysicalDeviceType physical_device_type;
        bool                   is_physical_device_amd;
        vk::UniqueDevice       device;
    };
} // namespace gfx::core::vulkan
