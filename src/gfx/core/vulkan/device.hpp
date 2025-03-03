#pragma once

#include "util/logger.hpp"
#include "util/threads.hpp"
#include "util/util.hpp"
#include <magic_enum/magic_enum.hpp>
#include <thread>
#include <utility>
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

        void acquireQueue(QueueType queueType, std::invocable<vk::Queue> auto accessFunc) const noexcept
        {
            const std::size_t idx = static_cast<std::size_t>(std::to_underlying(queueType));

            assert::critical(
                idx < static_cast<std::size_t>(QueueType::NumberOfQueueTypes),
                "Tried to lookup an invalid queue of type {}",
                idx);

            const std::vector<util::Mutex<vk::Queue>>& qs = this->queues.at(std::to_underlying(queueType));

            while (true)
            {
                for (const util::Mutex<vk::Queue>& q : qs)
                {
                    const bool wasLockedAndExecuted = q.tryLock(
                        [&](vk::Queue lockedQueue)
                        {
                            accessFunc(lockedQueue);
                        });

                    if (wasLockedAndExecuted)
                    {
                        return;
                    }
                }

                log::warn("Failed to acquire a queue of type {}, retrying", magic_enum::enum_name(queueType));

                std::this_thread::yield();
            }
        }

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
