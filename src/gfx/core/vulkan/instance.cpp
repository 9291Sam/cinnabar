#include "instance.hpp"
#include "gfx/core/window.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <cstring>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

// NOLINTBEGIN
#define VMA_IMPLEMENTATION           1
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>
#undef VMA_IMPLEMENTATION
#undef VMA_STATIC_VULKAN_FUNCTIONS
#undef VMA_DYNAMIC_VULKAN_FUNCTIONS
// NOLINTEND

namespace gfx::core::vulkan
{

    Instance::Instance()
        : vulkan_api_version(vk::ApiVersion12)
    {
        assert::critical(
            this->vulkan_api_version >= vk::ApiVersion12,
            "MoltenVK is Bugged, it will gladly load 1.3+ but it only works with 1.2");
        assert::critical(this->loader.success(), "Failed to load Vulkan, is it supported on your system?");

        // Initialize vulkan hpp's global state from the loader
        // This lets you access only vkCreateInstance and a few layer functions
        vk::detail::defaultDispatchLoaderDynamic.init(this->loader);

        const vk::ApplicationInfo applicationInfo {
            .sType {vk::StructureType::eApplicationInfo},
            .pNext {nullptr},
            .pApplicationName {"cinnabar"},
            .applicationVersion {vk::makeApiVersion(
                CINNABAR_VERSION_TWEAK, CINNABAR_VERSION_MAJOR, CINNABAR_VERSION_MINOR, CINNABAR_VERSION_PATCH)},
            .pEngineName {"cinnabar"},
            .engineVersion {vk::makeApiVersion(
                CINNABAR_VERSION_TWEAK, CINNABAR_VERSION_MAJOR, CINNABAR_VERSION_MINOR, CINNABAR_VERSION_PATCH)},
            .apiVersion {this->vulkan_api_version},
        };

        const std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

        std::vector<const char*> layers {};
        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        for (const char* requestedLayer : layers)
        {
            for (const vk::LayerProperties& availableLayer : availableLayers)
            {
                if (std::strcmp(availableLayer.layerName.data(), requestedLayer) == 0)
                {
                    goto next_layer;
                }
            }

            panic("Required layer {} was not available!", requestedLayer);
        next_layer: {}
        }

        std::vector<const char*> extensions {};

#ifdef __APPLE__
        extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
#endif // __APPLE__
        for (const char* e : Window::getRequiredExtensions())
        {
            extensions.push_back(e);
        }

        if constexpr (CINNABAR_DEBUG_BUILD)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        const std::vector<vk::ExtensionProperties> availableExtensions = vk::enumerateInstanceExtensionProperties();

        for (const char* requestedExtension : extensions)
        {
            for (const vk::ExtensionProperties& availableExtension : availableExtensions)
            {
                if (std::strcmp(availableExtension.extensionName.data(), requestedExtension) == 0)
                {
                    goto next_extension;
                }
            }

            panic("Required extension {} was not available!", requestedExtension);
        next_extension: {}

            log::trace("Requesting instance extension {}", requestedExtension);
        }

        static vk::PFN_DebugUtilsMessengerCallbackEXT debugMessengerCallback =
            +[](vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                vk::DebugUtilsMessageTypeFlagsEXT,
                const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void*) -> vk::Bool32
        {
            const usize      len = std::strlen(pCallbackData->pMessage);
            std::string_view message {pCallbackData->pMessage, len};

            using namespace std::literals;

            if (!message.starts_with("loader_get_json: Failed to open JSON file"sv))
            {
                switch (messageSeverity)
                {
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                    log::trace("{}", message);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                    log::info("{}", message);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                    log::warn("{}", message);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                    log::error("{}", message);
                    break;
                default:
                    break;
                }
            }

            return vk::False;
        };

        const vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
            .sType {vk::StructureType::eDebugUtilsMessengerCreateInfoEXT},
            .pNext {nullptr},
            .flags {},
            .messageSeverity {
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning},
            .messageType {
                vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation},
            .pfnUserCallback {debugMessengerCallback},
            .pUserData {nullptr},
        };

        const vk::InstanceCreateInfo instanceCreateInfo {
            .sType {vk::StructureType::eInstanceCreateInfo},
            .pNext {CINNABAR_DEBUG_BUILD ? &debugMessengerCreateInfo : nullptr},
            .flags {
#ifdef __APPLE__
                vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR
#endif // __APPLE__
            },
            .pApplicationInfo {&applicationInfo},
            .enabledLayerCount {static_cast<u32>(layers.size())},
            .ppEnabledLayerNames {layers.data()},
            .enabledExtensionCount {static_cast<uint32_t>(extensions.size())},
            .ppEnabledExtensionNames {extensions.data()}};

        this->instance = vk::createInstanceUnique(instanceCreateInfo);

        vk::detail::defaultDispatchLoaderDynamic.init(*this->instance);

#if CINNABAR_DEBUG_BUILD
        {
            this->debug_messenger = this->instance->createDebugUtilsMessengerEXTUnique(debugMessengerCreateInfo);
        }
#endif // CINNABAR_DEBUG_BUILD

        log::trace("Created instance");
    }

    u32 Instance::getVulkanVersion() const noexcept
    {
        return this->vulkan_api_version;
    }

    vk::Instance Instance::operator* () const noexcept
    {
        return *this->instance;
    }
} // namespace gfx::core::vulkan