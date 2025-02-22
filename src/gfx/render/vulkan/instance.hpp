#pragma once

#include "util.hpp"
#include <vulkan/vulkan.hpp>

namespace gfx::render::vulkan
{
    class Instance
    {
    public:
        Instance();
        ~Instance() = default;

        Instance(const Instance&)             = delete;
        Instance(Instance&&)                  = delete;
        Instance& operator= (const Instance&) = delete;
        Instance& operator= (Instance&&)      = delete;

        [[nodiscard]] u32                      getVulkanVersion() const noexcept;
        [[nodiscard]] const vk::DynamicLoader& getLoader() const noexcept;

        vk::Instance operator* () const noexcept;

    private:
        vk::DynamicLoader  loader;
        vk::UniqueInstance instance;
#if CINNABAR_DEBUG_BUILD
        vk::UniqueDebugUtilsMessengerEXT debug_messenger;
#endif // CINNABAR_DEBUG_BUILD
        u32 vulkan_api_version;
    };
} // namespace gfx::render::vulkan