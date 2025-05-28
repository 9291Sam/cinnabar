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
#include "gfx/generators/voxel/generator.hpp"
#include "gfx/generators/voxel/material.hpp"
#include "gfx/generators/voxel/model.hpp"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include "util/events.hpp"
#include "util/logger.hpp"
#include "util/util.hpp"
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>
#include <exception>
#include <glm/ext/scalar_common.hpp>
#include <glm/fwd.hpp>
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
        std::mt19937                           gen {std::random_device {}()};
        std::uniform_real_distribution<f32>    dist {-16.0f, 16.0f};
        gfx::generators::voxel::WorldGenerator wg {12812389021980};

        for (int i = 0; i < 38; ++i)
        {
            triangles.push_back(this->triangle_renderer.createTriangle({dist(gen), dist(gen), dist(gen)}));
        }

        const i32 dim = 3; // 8

        for (i32 cX = -dim; cX <= dim; ++cX)
        {
            for (i32 cY = -dim; cY <= dim; ++cY)
            {
                for (i32 cZ = -dim; cZ <= dim; ++cZ)
                {
                    gfx::generators::voxel::AlignedChunkCoordinate aC {cX, cY, cZ};

                    gfx::generators::voxel::VoxelRenderer::VoxelChunk chunk = this->voxel_renderer.createVoxelChunk(aC);

                    std::vector<std::pair<gfx::generators::voxel::ChunkLocalPosition, gfx::generators::voxel::Voxel>>
                        newVoxels {};

                    if (glm::i32vec3 {cX, cY, cZ} == glm::i32vec3 {0, 0, 1})
                    {
                        gfx::generators::voxel::StaticVoxelModel cornelBox =
                            gfx::generators::voxel::StaticVoxelModel::createCornelBox();

                        const glm::u32vec3 extents = cornelBox.getExtent();
                        const auto         voxels  = cornelBox.getModel();

                        assert::critical(extents == glm::u32vec3 {64, 64, 64}, "no");

                        for (u32 x = 0; x < extents.x; ++x)
                        {
                            for (u32 y = 0; y < extents.y; ++y)
                            {
                                for (u32 z = 0; z < extents.z; ++z)
                                {
                                    const gfx::generators::voxel::Voxel v = voxels[x, y, z];

                                    if (v != gfx::generators::voxel::Voxel::NullAirEmpty)
                                    {
                                        newVoxels.push_back(
                                            {gfx::generators::voxel::ChunkLocalPosition {63 - x, y, z}, v});
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        const auto [brickMap, bricks, emissives] = wg.generateChunkPreDense(aC);
                        this->voxel_renderer.setVoxelChunkData(chunk, brickMap, bricks, emissives);
                    }

                    if (!newVoxels.empty())
                    {
                        this->voxel_renderer.setVoxelChunkData(chunk, newVoxels);
                    }

                    this->chunks.push_back(std::move(chunk));
                }
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
        const f32  deltaTime        = updateArgs.delta_time;
        const bool hasResizeOcurred = updateArgs.has_resize_ocurred;

        const gfx::core::Renderer* renderer = this->game->getRenderer();

        const vk::Extent2D frameBufferSize = renderer->getWindow()->getFramebufferSize();

        const f32 aspectRatio = static_cast<f32>(frameBufferSize.width) / static_cast<f32>(frameBufferSize.height);

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

        const f32 moveScale = 32.0f;

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

        const f32 rotateSpeedScale = 6.0f;

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
                .maybe_voxel_renderer {&this->voxel_renderer}}},
            .camera {this->camera}};
    }

    static constexpr f32 FovY = glm::radians(70.0f);

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
