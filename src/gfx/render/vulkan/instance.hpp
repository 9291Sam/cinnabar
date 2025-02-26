#pragma once

#include "util/util.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

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

        [[nodiscard]] u32 getVulkanVersion() const noexcept;

        template<class T>
            requires std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>
        [[nodiscard]] T getProcAddress(const char* function) const noexcept
        {
            return this->loader.getProcAddress<T>(function);
        }

        vk::Instance operator* () const noexcept;

    private:
        vk::detail::DynamicLoader loader;
        vk::UniqueInstance        instance;
#if CINNABAR_DEBUG_BUILD
        vk::UniqueDebugUtilsMessengerEXT debug_messenger;
#endif // CINNABAR_DEBUG_BUILD
        u32 vulkan_api_version;
    };
} // namespace gfx::render::vulkan