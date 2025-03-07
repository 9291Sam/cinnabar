
#include <glm/ext/scalar_common.hpp>
#include <random>
//

#include "game/game.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/device.hpp"
#include "gfx/core/vulkan/instance.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/core/window.hpp"
#include "gfx/frame_generator.hpp"
#include "gfx/generators/skybox/skybox_renderer.hpp"
#include "gfx/generators/triangle/triangle_renderer.hpp"
#include "util/logger.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <cstddef>
#include <glm/trigonometric.hpp>
#include <iterator>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

struct TemporaryGameState : game::Game::GameState
{
    explicit TemporaryGameState(game::Game* game_)
        : game {game_}
        , triangle_renderer {this->game->getRenderer()}
        , skybox_renderer {this->game->getRenderer()}
    {
        std::mt19937                          gen {std::random_device {}()};
        std::uniform_real_distribution<float> dist {-32.0f, 32.0f};

        for (int i = 0; i < 38; ++i)
        {
            triangles.push_back(this->triangle_renderer.createTriangle(
                {.translation {glm::vec4 {dist(gen), dist(gen), dist(gen), 1.0f}}, .scale {10.0f, 10.0f, 10.0f}}));
        }
    }
    ~TemporaryGameState() override
    {
        for (auto& t : triangles)
        {
            this->triangle_renderer.destroyTriangle(std::move(t));
        }
    }

    TemporaryGameState(const TemporaryGameState&)             = delete;
    TemporaryGameState(TemporaryGameState&&)                  = delete;
    TemporaryGameState& operator= (const TemporaryGameState&) = delete;
    TemporaryGameState& operator= (TemporaryGameState&&)      = delete;

    [[nodiscard]] std::string_view identify() const override
    {
        using namespace std::literals;

        return "Temporary Game State"sv;
    }

    game::Game::GameStateUpdateResult update(game::Game::GameStateUpdateArgs updateArgs) override
    {
        const float deltaTime        = updateArgs.delta_time;
        const bool  hasResizeOcurred = updateArgs.has_resize_ocurred;

        const gfx::core::Renderer* renderer = this->game->getRenderer();

        const vk::Extent2D frameBufferSize = renderer->getWindow()->getFramebufferSize();

        const float aspectRatio =
            static_cast<float>(frameBufferSize.width) / static_cast<float>(frameBufferSize.height);

        if (hasResizeOcurred)
        {
            camera = gfx::Camera {gfx::Camera::CameraDescriptor {
                .aspect_ratio {aspectRatio},
                .position {camera.getPosition()},
                .fov_y {FovY},
                .pitch {camera.getPitch()},
                .yaw {camera.getYaw()}}};
        }

        if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::ToggleCursorAttachment))
        {
            renderer->getWindow()->toggleCursor();
        }

        if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::CloseWindow))
        {
            return game::Game::GameStateUpdateResult {.should_terminate {true}, .generators {}, .camera {}};
        }

        glm::vec3 previousPosition = camera.getPosition();

        glm::vec3 newPosition = previousPosition;

        const float moveScale = 32.0f;

        // Adjust the new position based on input for movement directions
        if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveForward))
        {
            newPosition += camera.getForwardVector() * deltaTime * moveScale;
        }
        else if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveBackward))
        {
            newPosition -= camera.getForwardVector() * deltaTime * moveScale;
        }

        if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveLeft))
        {
            newPosition -= camera.getRightVector() * deltaTime * moveScale;
        }
        else if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveRight))
        {
            newPosition += camera.getRightVector() * deltaTime * moveScale;
        }

        if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveUp))
        {
            newPosition += gfx::Transform::UpVector * deltaTime * moveScale;
        }
        else if (renderer->getWindow()->isActionActive(gfx::core::Window::Action::PlayerMoveDown))
        {
            newPosition -= gfx::Transform::UpVector * deltaTime * moveScale;
        }

        camera.setPosition(newPosition);

        const float rotateSpeedScale = 6.0f;

        auto getMouseDeltaRadians = [&]
        {
            // each value from -1.0 -> 1.0 representing how much it moved
            // on the screen
            const auto [nDeltaX, nDeltaY] = renderer->getWindow()->getScreenSpaceMouseDelta();

            const auto deltaRadiansX = (nDeltaX / 2) * aspectRatio * FovY;
            const auto deltaRadiansY = (nDeltaY / 2) * FovY;

            return gfx::core::Window::Delta {.x {deltaRadiansX}, .y {deltaRadiansY}};
        };

        auto [xDelta, yDelta] = getMouseDeltaRadians();

        camera.addYaw(xDelta * rotateSpeedScale);
        camera.addPitch(yDelta * rotateSpeedScale);

        return game::Game::GameStateUpdateResult {
            .should_terminate {false},
            .generators {gfx::FrameGenerator::FrameGenerateArgs {
                .maybe_triangle_renderer {&this->triangle_renderer}, .maybe_skybox_render {&this->skybox_renderer}}},
            .camera {this->camera}};
    }

    static constexpr float FovY = glm::radians(70.0f);

    game::Game* game;

    gfx::generators::triangle::TriangleRenderer                        triangle_renderer;
    gfx::generators::skybox::SkyboxRenderer                            skybox_renderer;
    std::vector<gfx::generators::triangle::TriangleRenderer::Triangle> triangles;

    gfx::Camera camera {gfx::Camera::CameraDescriptor {.fov_y {FovY}}};
};

int main()
{
    util::GlobalLoggerContext loggerContext {};

    CPPTRACE_TRYZ
    {
        log::info(
            "Cinnabar has started v{}.{}.{}.{}{}",
            CINNABAR_VERSION_MAJOR,
            CINNABAR_VERSION_MINOR,
            CINNABAR_VERSION_PATCH,
            CINNABAR_VERSION_TWEAK,
            CINNABAR_DEBUG_BUILD ? " Debug Build" : "");

        gfx::core::Renderer renderer {};
        game::Game          game {&renderer};

        game.enqueueInstallGameState<TemporaryGameState>();

        game.enterUpdateLoop();

        renderer.getDevice()->getDevice().waitIdle();
    }
    CPPTRACE_CATCHZ(const std::exception& e)
    {
        log::critical(
            "Cinnabar has crashed! | {} {}\n{}",
            e.what(),
            typeid(e).name(),
            cpptrace::from_current_exception().to_string(true));
    }
    CPPTRACE_CATCH_ALT(...)
    {
        log::critical(
            "Cinnabar has crashed! | Unknown Exception type thrown!\n{}",
            cpptrace::from_current_exception().to_string(true));
    }

    log::info("Cinnabar has shutdown successfully!");
}
