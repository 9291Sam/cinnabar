#include "device.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <algorithm>
#include <functional>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace gfx::core::vulkan
{
    Device::Device(vk::Instance instance, vk::SurfaceKHR surface) // NOLINT
    {
        const std::vector<vk::PhysicalDevice> availablePhysicalDevices = instance.enumeratePhysicalDevices();

        const auto getRatingOfDevice = [](vk::PhysicalDevice d) -> std::size_t
        {
            std::size_t score = 0;

            const vk::PhysicalDeviceProperties properties = d.getProperties();

            score += properties.limits.maxImageDimension2D;
            score += properties.limits.maxImageDimension3D;

            return score;
        };

        this->physical_device = *std::ranges::max_element(
            availablePhysicalDevices,
            [&](vk::PhysicalDevice l, vk::PhysicalDevice r)
            {
                return getRatingOfDevice(l) < getRatingOfDevice(r);
            });

        this->physical_device_type = this->physical_device.getProperties().deviceType;

        this->is_physical_device_amd = this->physical_device.getProperties().vendorID == 0x1002;

        const std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
            this->physical_device.getQueueFamilyProperties();

        std::optional<u32> graphicsFamily      = std::nullopt;
        std::optional<u32> asyncComputeFamily  = std::nullopt;
        std::optional<u32> asyncTransferFamily = std::nullopt;

        for (u32 i = 0; i < queueFamilyProperties.size(); ++i)
        {
            const vk::QueueFlags flags = queueFamilyProperties[i].queueFlags;

            if (flags & vk::QueueFlagBits::eVideoDecodeKHR || flags & vk::QueueFlagBits::eVideoEncodeKHR
                || flags & vk::QueueFlagBits::eOpticalFlowNV)
            {
                continue;
            }

            if (flags & vk::QueueFlagBits::eGraphics && graphicsFamily == std::nullopt)
            {
                graphicsFamily = i;

                const vk::Bool32 isSurfaceSupported = this->physical_device.getSurfaceSupportKHR(i, surface);

                assert::critical(
                    flags & vk::QueueFlagBits::eCompute && flags & vk::QueueFlagBits::eTransfer
                        && static_cast<bool>(isSurfaceSupported),
                    "Tried to instantiate a graphics queue with flags {}, {}",
                    vk::to_string(flags),
                    isSurfaceSupported);

                continue;
            }

            if (flags & vk::QueueFlagBits::eCompute && asyncComputeFamily == std::nullopt)
            {
                asyncComputeFamily = i;

                assert::critical(
                    static_cast<bool>(flags & vk::QueueFlagBits::eTransfer),
                    "Tried to instantiate a compute queue with flags {}",
                    vk::to_string(flags));

                continue;
            }

            if (flags & vk::QueueFlagBits::eTransfer && asyncTransferFamily == std::nullopt)
            {
                asyncTransferFamily = i;

                continue;
            }
        }

        this->queue_family_indexes = std::array {graphicsFamily, asyncComputeFamily, asyncTransferFamily};

        auto getStringOfFamily = [](std::optional<u32> f) -> std::string
        {
            if (f.has_value()) // NOLINT
            {
                return std::to_string(*f);
            }
            else
            {
                return "std::nullopt";
            }
        };

        std::vector<vk::DeviceQueueCreateInfo> queuesToCreate {};

        std::vector<f32> queuePriorities {};
        queuePriorities.resize(1024, 1.0);

        u32 numberOfGraphicsQueues      = 0;
        u32 numberOfAsyncComputeQueues  = 0;
        u32 numberOfAsyncTransferQueues = 0;

        if (graphicsFamily.has_value())
        {
            numberOfGraphicsQueues = queueFamilyProperties.at(*graphicsFamily).queueCount;

            queuesToCreate.push_back(vk::DeviceQueueCreateInfo {
                .sType {vk::StructureType::eDeviceQueueCreateInfo},
                .pNext {nullptr},
                .flags {},
                .queueFamilyIndex {*graphicsFamily},
                .queueCount {numberOfGraphicsQueues},
                .pQueuePriorities {queuePriorities.data()},
            });
        }
        else
        {
            panic("No graphics queue available!");
        }

        if (asyncComputeFamily.has_value())
        {
            numberOfAsyncComputeQueues = queueFamilyProperties.at(*asyncComputeFamily).queueCount;

            queuesToCreate.push_back(vk::DeviceQueueCreateInfo {
                .sType {vk::StructureType::eDeviceQueueCreateInfo},
                .pNext {nullptr},
                .flags {},
                .queueFamilyIndex {*asyncComputeFamily},
                .queueCount {numberOfAsyncComputeQueues},
                .pQueuePriorities {queuePriorities.data()},
            });
        }

        if (asyncTransferFamily.has_value())
        {
            numberOfAsyncTransferQueues = queueFamilyProperties.at(*asyncTransferFamily).queueCount;

            queuesToCreate.push_back(vk::DeviceQueueCreateInfo {
                .sType {vk::StructureType::eDeviceQueueCreateInfo},
                .pNext {nullptr},
                .flags {},
                .queueFamilyIndex {*asyncTransferFamily},
                .queueCount {numberOfAsyncTransferQueues},
                .pQueuePriorities {queuePriorities.data()},
            });
        }

        log::debug(
            "Instantiating queues! | Graphics: {} @ {} | Async Compute: {} @ {} | Async Transfer: {} @ {}",
            numberOfGraphicsQueues,
            getStringOfFamily(graphicsFamily),
            numberOfAsyncComputeQueues,
            getStringOfFamily(asyncComputeFamily),
            numberOfAsyncTransferQueues,
            getStringOfFamily(asyncTransferFamily));

        this->queue_family_numbers =
            std::array {numberOfGraphicsQueues, numberOfAsyncComputeQueues, numberOfAsyncTransferQueues};

        std::array requiredExtensions {
            vk::KHRShaderNonSemanticInfoExtensionName,
            vk::KHRVariablePointersExtensionName,
            vk::KHRDynamicRenderingExtensionName,
            vk::KHRSwapchainExtensionName,
#ifdef __APPLE__
            "VK_KHR_portability_subset",
#endif // __APPLE__
        };

        for (const char* requestedExtension : requiredExtensions)
        {
            for (const vk::ExtensionProperties& availableExtension :
                 this->physical_device.enumerateDeviceExtensionProperties())
            {
                if (std::strcmp(availableExtension.extensionName.data(), requestedExtension) == 0)
                {
                    goto next_extension;
                }
            }

            panic("Required extension {} was not available!", requestedExtension);
        next_extension: {}
        }

        vk::PhysicalDeviceVulkan12Features features12 {};
        features12.sType                                         = vk::StructureType::ePhysicalDeviceVulkan12Features;
        features12.pNext                                         = nullptr;
        features12.timelineSemaphore                             = vk::True;
        features12.descriptorBindingPartiallyBound               = vk::True;
        features12.descriptorBindingUpdateUnusedWhilePending     = vk::True;
        features12.descriptorBindingSampledImageUpdateAfterBind  = vk::True;
        features12.shaderSampledImageArrayNonUniformIndexing     = vk::True;
        features12.runtimeDescriptorArray                        = vk::True;
        features12.descriptorBindingStorageImageUpdateAfterBind  = vk::True;
        features12.descriptorBindingUniformBufferUpdateAfterBind = vk::True;
        features12.descriptorBindingVariableDescriptorCount      = vk::True;
        features12.hostQueryReset                                = vk::True;
        features12.shaderOutputLayer                             = vk::True;
        features12.runtimeDescriptorArray                        = vk::True;
        features12.descriptorBindingStorageBufferUpdateAfterBind = vk::True;

        vk::PhysicalDeviceVulkan11Features features11 {};
        features11.sType                         = vk::StructureType::ePhysicalDeviceVulkan11Features;
        features11.pNext                         = &features12;
        features11.storageBuffer16BitAccess      = vk::True;
        features11.shaderDrawParameters          = vk::True;
        features11.variablePointersStorageBuffer = vk::True;
        features11.variablePointers              = vk::True;

        // vk::PhysicalDeviceShaderQuadControlFeaturesKHR subgroupProperties {
        //     .sType {vk::StructureType::ePhysicalDeviceShaderQuadControlFeaturesKHR},
        //     .pNext {&features11},
        //     .shaderQuadControl {vk::True},
        // };

        vk::PhysicalDeviceFeatures2 features102 {};
        features102.sType                             = vk::StructureType::ePhysicalDeviceFeatures2;
        features102.pNext                             = &features11;
        features102.features.shaderInt64              = vk::True;
        features102.features.shaderInt16              = vk::True;
        features102.features.multiDrawIndirect        = vk::True;
        features102.features.fragmentStoresAndAtomics = vk::True;

        const vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures {
            .sType {vk::StructureType::ePhysicalDeviceDynamicRenderingFeatures},
            .pNext {&features102},
            .dynamicRendering {vk::True}};

        const vk::DeviceCreateInfo deviceCreateInfo {
            .sType {vk::StructureType::eDeviceCreateInfo},
            .pNext {&dynamicRenderingFeatures},
            .flags {},
            .queueCreateInfoCount {static_cast<u32>(queuesToCreate.size())},
            .pQueueCreateInfos {queuesToCreate.data()},
            .enabledLayerCount {0},
            .ppEnabledLayerNames {nullptr},
            .enabledExtensionCount {static_cast<u32>(requiredExtensions.size())},
            .ppEnabledExtensionNames {requiredExtensions.data()},
            .pEnabledFeatures {nullptr},
        };

        this->device = this->physical_device.createDeviceUnique(deviceCreateInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, *this->device);

        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            this->device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eDevice},
                .objectHandle {std::bit_cast<u64>(*this->device)},
                .pObjectName {"Device"},
            });
        }

        std::vector<util::Mutex<vk::Queue>> graphicsQueues {};
        std::vector<util::Mutex<vk::Queue>> asyncComputeQueues {};
        std::vector<util::Mutex<vk::Queue>> asyncTransferQueues {};

        for (u32 idx = 0; idx < numberOfGraphicsQueues; ++idx)
        {
            vk::Queue q = this->device->getQueue(*graphicsFamily, idx);

            if constexpr (CINNABAR_DEBUG_BUILD)
            {
                std::string name = std::format("Graphics Queue #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eQueue},
                    .objectHandle {std::bit_cast<u64>(q)},
                    .pObjectName {name.c_str()},
                });
            }

            graphicsQueues.push_back(util::Mutex {std::move(q)}); // NOLINT
        }

        for (u32 idx = 0; idx < numberOfAsyncComputeQueues; ++idx)
        {
            vk::Queue q = this->device->getQueue(*asyncComputeFamily, idx);

            if constexpr (CINNABAR_DEBUG_BUILD)
            {
                std::string name = std::format("Async Compute Queue #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eQueue},
                    .objectHandle {std::bit_cast<u64>(q)},
                    .pObjectName {name.c_str()},
                });
            }

            asyncComputeQueues.push_back(util::Mutex {std::move(q)}); // NOLINT
        }

        for (u32 idx = 0; idx < numberOfAsyncTransferQueues; ++idx)
        {
            vk::Queue q = this->device->getQueue(*asyncTransferFamily, idx);

            if constexpr (CINNABAR_DEBUG_BUILD)
            {
                std::string name = std::format("Async Transfer Queue #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eQueue},
                    .objectHandle {std::bit_cast<u64>(q)},
                    .pObjectName {name.c_str()},
                });
            }

            asyncTransferQueues.push_back(util::Mutex {std::move(q)}); // NOLINT
        }

        this->queues[static_cast<std::size_t>(QueueType::Graphics)]      = std::move(graphicsQueues);
        this->queues[static_cast<std::size_t>(QueueType::AsyncCompute)]  = std::move(asyncComputeQueues);
        this->queues[static_cast<std::size_t>(QueueType::AsyncTransfer)] = std::move(asyncTransferQueues);

        std::string deviceExtensionsRequestedString {};

        for (const char* str : requiredExtensions)
        {
            deviceExtensionsRequestedString += std::format("{}, ", str);
        }

        if (!deviceExtensionsRequestedString.empty())
        {
            deviceExtensionsRequestedString.pop_back();
            deviceExtensionsRequestedString.pop_back();
        }

        log::debug("Created device with extensions {}", deviceExtensionsRequestedString);
    }

    std::optional<u32> Device::getFamilyOfQueueType(QueueType t) const noexcept
    {
        return this->queue_family_indexes.at(static_cast<std::size_t>(std::to_underlying(t)));
    }

    u32 Device::getNumberOfQueues(QueueType t) const noexcept
    {
        return this->queue_family_numbers.at(static_cast<std::size_t>(std::to_underlying(t)));
    }

    bool Device::isIntegrated() const noexcept
    {
        return this->physical_device_type == vk::PhysicalDeviceType::eIntegratedGpu;
    }

    vk::PhysicalDevice Device::getPhysicalDevice() const noexcept
    {
        return this->physical_device;
    }

    bool Device::isAmd() const noexcept
    {
        return this->is_physical_device_amd;
    }

    vk::Device Device::getDevice() const noexcept
    {
        return *this->device;
    }

    const vk::Device* Device::operator->() const noexcept
    {
        return &*this->device;
    }

    void Device::acquireQueue(QueueType queueType, std::function<void(vk::Queue)> accessFunc) const noexcept
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

} // namespace gfx::core::vulkan
