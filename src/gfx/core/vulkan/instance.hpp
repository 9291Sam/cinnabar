#pragma once

#include "util/util.hpp"
#include <vulkan/vulkan.hpp>

namespace gfx::core::vulkan
{
    /// Wrapper around a vulkan instance and the things required to be able to
    /// load an instance (loading a dynamic library from the OS)
    ///
    /// Also automatically loads in the debug layers on debug builds
    class Instance
    {
    public:
        Instance();
        ~Instance() = default;

        Instance(const Instance&)             = delete;
        Instance(Instance&&)                  = delete;
        Instance& operator= (const Instance&) = delete;
        Instance& operator= (Instance&&)      = delete;

        /// Lowers to the appropriate symbol that loads a function pointer from a dynamic library
        /// this is `dlsym` or `GetProcAddress`
        template<class T>
            requires std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>
        [[nodiscard]] T getProcAddress(const char* function) const noexcept
        {
            return this->loader.getProcAddress<T>(function);
        }

        vk::Instance operator* () const noexcept;

        /// Returns a version in the format described by
        /// https://docs.vulkan.org/spec/latest/chapters/extensions.html#VK_MAKE_API_VERSION
        [[nodiscard]] u32 getVulkanVersion() const noexcept;

    private:
        vk::detail::DynamicLoader loader;
        vk::UniqueInstance        instance;
#if CINNABAR_DEBUG_BUILD
        vk::UniqueDebugUtilsMessengerEXT debug_messenger;
#endif // CINNABAR_DEBUG_BUILD
        u32 vulkan_api_version;
    };
} // namespace gfx::core::vulkan