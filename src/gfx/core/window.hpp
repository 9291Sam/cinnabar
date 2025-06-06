#pragma once

#include "glm/ext/vector_double2.hpp"
#include "util/util.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace gfx::core
{
    namespace recordables
    {
        class DebugMenu;
    } // namespace recordables

    /// Window managing class, also handles exposing a drawable surface and user input to the window
    class Window
    {
    public:
        struct Delta
        {
            f32 x;
            f32 y;
        };

        using GlfwKeyType = int;

        enum class Action : u8
        {
            PlayerMoveForward      = 0,
            PlayerMoveBackward     = 1,
            PlayerMoveLeft         = 2,
            PlayerMoveRight        = 3,
            PlayerMoveUp           = 4,
            PlayerMoveDown         = 5,
            PlayerSprint           = 6,
            ToggleConsole          = 7,
            ToggleCursorAttachment = 8,
            ResetPlayPosition      = 9,
            ReloadShaders          = 10,
            CloseWindow            = 11,
            MaxActionValue         = 12,
        };

        enum class InteractionMethod : u8
        {
            /// Only fires for the first time it's checked, no matter how long you hold the button down for. Useful for
            /// a toggle switch, i.e opening the developer console or opening an inventory menu
            SinglePress,
            /// Fires every frame, as long as the button is pressed Useful for movement keys
            EveryFrame,
        };

        struct ActionInformation
        {
            GlfwKeyType       key;
            InteractionMethod method;
        };
    public:

        Window(const std::map<Action, ActionInformation>&, vk::Extent2D windowSize, const char* name);
        ~Window();

        Window(const Window&)             = delete;
        Window(Window&&)                  = delete;
        Window& operator= (const Window&) = delete;
        Window& operator= (Window&&)      = delete;

        [[nodiscard]] bool         isActionActive(Action, bool ignoreCursorAttached = false) const;
        [[nodiscard]] Delta        getScreenSpaceMouseDelta() const;
        [[nodiscard]] Delta        getScrollDelta() const;
        [[nodiscard]] f32          getDeltaTimeSeconds() const;
        [[nodiscard]] vk::Extent2D getFramebufferSize() const;

        // Neither of these functions are threadsafe
        [[nodiscard]] vk::UniqueSurfaceKHR createSurface(vk::Instance);
        [[nodiscard]] bool                 shouldClose();

        [[nodiscard]] static std::span<const char*> getRequiredExtensions();

        void blockThisThreadWhileMinimized();
        void attachCursor() const;
        void detachCursor() const;
        void toggleCursor() const;
        void initializeImgui() const;
        void endFrame();
        // TODO: void updateKeyBindings();

    private:
        static void keypressCallback(GLFWwindow*, int, int, int, int);
        static void frameBufferResizeCallback(GLFWwindow*, int, int);
        static void mouseScrollCallback(GLFWwindow*, double, double);
        static void mouseButtonCallback(GLFWwindow*, int, int, int);
        static void windowFocusCallback(GLFWwindow*, int);
        static void errorCallback(int, const char*);

        friend recordables::DebugMenu;

        GLFWwindow*               window;
        std::atomic<vk::Extent2D> framebuffer_size;

        std::map<GlfwKeyType, std::vector<Action>> key_to_actions_map;
        std::map<Action, std::atomic<bool>>        action_active_map;
        std::map<Action, InteractionMethod>        action_interaction_map;

        std::chrono::time_point<std::chrono::steady_clock> last_frame_end_time;
        std::atomic<std::chrono::duration<f32>>            last_frame_duration;

        std::array<std::atomic<bool>, 8> mouse_buttons_pressed_state; // NOLINT
        mutable std::atomic<std::size_t> mouse_ignore_frames;
        std::atomic<Delta>               previous_mouse_position;
        std::atomic<Delta>               mouse_delta_pixels;
        std::atomic<Delta>               screen_space_mouse_delta;
        mutable std::atomic<bool>        is_cursor_attached;
        std::atomic<glm::dvec2>          previous_absolute_scroll_position;
        std::atomic<glm::dvec2>          absolute_scroll_position;
        std::atomic<Delta>               scroll_delta;
    };
} // namespace gfx::core
