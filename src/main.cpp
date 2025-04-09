

#include "game/game.hpp"
#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/vulkan/descriptor_manager.hpp"
#include "gfx/core/vulkan/device.hpp"
#include "gfx/core/vulkan/pipeline_manager.hpp"
#include "gfx/core/vulkan/swapchain.hpp"
#include "gfx/core/window.hpp"
#include "gfx/frame_generator.hpp"
#include "gfx/generators/imgui/imgui_renderer.hpp"
#include "gfx/generators/skybox/skybox_renderer.hpp"
#include "gfx/generators/triangle/triangle_renderer.hpp"
#include "gfx/generators/voxel/data_structures.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include "slang_compiler.hpp"
#include "util/events.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <cstddef>
#include <exception>
#include <glm/ext/scalar_common.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <slang.h>
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
        , imgui_renderer {this->game->getRenderer()}
        , voxel_renderer {this->game->getRenderer()}
    {
        std::mt19937                          gen {std::random_device {}()};
        std::uniform_real_distribution<float> dist {-16.0f, 16.0f};

        for (int i = 0; i < 38; ++i)
        {
            triangles.push_back(this->triangle_renderer.createTriangle({dist(gen), dist(gen), dist(gen)}));
        }

        for (i32 cX = -2; cX <= 2; ++cX)
        {
            for (i32 cZ = -2; cZ <= 2; ++cZ)
            {
                std::vector<std::pair<gfx::generators::voxel::ChunkLocalPosition, gfx::generators::voxel::Voxel>>
                                                               newVoxels {};
                gfx::generators::voxel::AlignedChunkCoordinate aC {cX, -1, cZ};

                gfx::generators::voxel::VoxelRenderer::VoxelChunk chunk = this->voxel_renderer.createVoxelChunk(
                    gfx::generators::voxel::WorldPosition::assemble(aC, {}).asVector());

                for (u8 x = 0; x < 64; ++x)
                {
                    for (u8 z = 0; z < 64; ++z)
                    {
                        newVoxels.push_back(
                            {gfx::generators::voxel::ChunkLocalPosition {glm::u8vec3 {x, 0, z}},
                             gfx::generators::voxel::getRandomVoxel(
                                 gfx::generators::voxel::gpu_hashU32(static_cast<u32>((cX * 3933) + (102023 * cZ))))});
                    }
                }

                if (cX != -2)
                {
                    for (u8 x = 24; x < 40; ++x)
                    {
                        for (u8 y = 0; y < 22; ++y)
                        {
                            for (u8 z = 24; z < 40; ++z)
                            {
                                newVoxels.push_back(
                                    {gfx::generators::voxel::ChunkLocalPosition {glm::u8vec3 {x, y, z}},
                                     gfx::generators::voxel::Voxel::Cobalt});
                            }
                        }
                    }
                }

                this->voxel_renderer.setVoxelChunkData(chunk, newVoxels);

                this->chunks.push_back(std::move(chunk));
            }
        }
    }
    ~TemporaryGameState() override
    {
        for (auto& t : triangles)
        {
            this->triangle_renderer.destroyTriangle(std::move(t));
        }

        for (gfx::generators::voxel::VoxelRenderer::VoxelChunk& c : this->chunks)
        {
            this->voxel_renderer.destroyVoxelChunk(std::move(c));
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
                .maybe_triangle_renderer {&this->triangle_renderer},
                .maybe_skybox_renderer {&this->skybox_renderer},
                .maybe_imgui_renderer {&this->imgui_renderer},
                // .maybe_voxel_renderer {nullptr}
                .maybe_voxel_renderer {&this->voxel_renderer}}},
            .camera {this->camera}};
    }

    static constexpr float FovY = glm::radians(70.0f);

    game::Game* game;

    gfx::generators::triangle::TriangleRenderer                        triangle_renderer;
    gfx::generators::skybox::SkyboxRenderer                            skybox_renderer;
    gfx::generators::imgui::ImguiRenderer                              imgui_renderer;
    gfx::generators::voxel::VoxelRenderer                              voxel_renderer;
    std::vector<gfx::generators::triangle::TriangleRenderer::Triangle> triangles;

    std::vector<gfx::generators::voxel::VoxelRenderer::VoxelChunk> chunks;

    gfx::Camera camera {gfx::Camera::CameraDescriptor {.fov_y {FovY}}};
};

int main()
{
    util::GlobalLoggerContext loggerContext {};
    util::GlobalEventContext  eventContent {};

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
            "Cinnabar has crashed! | Unknown Exception type thrown!\n{} {}",
            cpptrace::from_current_exception().to_string(true),
            slang::getLastInternalErrorMessage());

        throw;
    }

    log::info("Cinnabar has shutdown successfully!");
}
