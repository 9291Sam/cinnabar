#include "instance.hpp"
#include "gfx/render/window.hpp"
#include "util.hpp"
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace gfx::render::vulkan
{

    Instance::Instance()
        : vulkan_api_version(vk::ApiVersion13)
    {
        assert::critical(
            this->loader.success(), "Failed to load Vulkan, is it supported on your system?");

        vk::detail::defaultDispatchLoaderDynamic.init(this->loader);

        const vk::ApplicationInfo applicationInfo {
            .sType {vk::StructureType::eApplicationInfo},
            .pNext {nullptr},
            .pApplicationName {"lavender"},
            .applicationVersion {vk::makeApiVersion(
                CINNABAR_VERSION_TWEAK,
                CINNABAR_VERSION_MAJOR,
                CINNABAR_VERSION_MINOR,
                CINNABAR_VERSION_PATCH)},
            .pEngineName {"lavender"},
            .engineVersion {vk::makeApiVersion(
                CINNABAR_VERSION_TWEAK,
                CINNABAR_VERSION_MAJOR,
                CINNABAR_VERSION_MINOR,
                CINNABAR_VERSION_PATCH)},
            .apiVersion {this->vulkan_api_version},
        };

        const std::vector<vk::LayerProperties> availableLayers =
            vk::enumerateInstanceLayerProperties();

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

        const std::vector<vk::ExtensionProperties> availableExtensions =
            vk::enumerateInstanceExtensionProperties();

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

        static auto debugMessengerCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               [[maybe_unused]] void*                      pUserData) -> vk::Bool32
        {
            switch (static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity))
            {
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                log::trace("{}", pCallbackData->pMessage);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                log::info("{}", pCallbackData->pMessage);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                log::warn("{}", pCallbackData->pMessage);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                log::error("{}", pCallbackData->pMessage);
                break;
            default:
                break;
            }

            return vk::False;
        };

        const vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
            .sType {vk::StructureType::eDebugUtilsMessengerCreateInfoEXT},
            .pNext {nullptr},
            .flags {},
            .messageSeverity {
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning},
            .messageType {
                vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
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
            this->debug_messenger =
                this->instance->createDebugUtilsMessengerEXTUnique(debugMessengerCreateInfo);
        }
#endif // CINNABAR_DEBUG_BUILD

        log::trace("Created instance");
    }

    vk::Instance Instance::operator* () const noexcept
    {
        return *this->instance;
    }
} // namespace gfx::render::vulkan