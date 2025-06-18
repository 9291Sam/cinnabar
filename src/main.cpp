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
#include "gfx/generators/voxel/shared_data_structures.slang"
#include "gfx/generators/voxel/voxel_renderer.hpp"
#include "gfx/voxel_world_manager.hpp"
#include "util/events.hpp"
#include "util/logger.hpp"
#include "util/task_generator.hpp"
#include "util/timer.hpp"
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
        , voxel_world_manager {this->game->getRenderer(), 12812389021980}
    {
        this->lights.push_back(this->voxel_world_manager.createVoxelLightUnique(
            gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {33.3, 23.2, 91.23, 8}, .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        this->lights.push_back(this->voxel_world_manager.createVoxelLightUnique(
            gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {133.3, 23.2, 91.23, 4},
                .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        // const glm::u8vec3 size = glm::u8vec3 {4, 4, 4};

        for (int i = 0; i < 32; ++i)
        {
            this->sphere_entities.push_back(
                this->voxel_world_manager.createVoxelEntityUnique({}, glm::u8vec3 {16, 16, 16}));
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
        std::vector<std::pair<gfx::generators::voxel::ChunkLocalPosition, gfx::generators::voxel::Voxel>> newVoxels {};

        float iter = 0;
        for (const gfx::VoxelWorldManager::UniqueVoxelEntity& e : this->sphere_entities)
        {
            // Sphere properties
            const float time = this->game->getRenderer()->getTimeAlive() + static_cast<float>(iter);
            iter += (12.0f * glm::pi<f32>()) / this->sphere_entities.size();
            const float sphereRadius   = 7.0f;
            const float sphereRadiusSq = sphereRadius * sphereRadius;
            const float orbitRadius    = 64.0f;

            // Calculate the orbiting center of the sphere in world space
            const glm::vec3 worldSphereCenter = {
                32.5f + orbitRadius * std::cos(time * 0.5f),
                32.5f + 24.0f * std::sin(time * 0.33f),
                96.5f + orbitRadius * std::sin(time * 0.5f)};

            // Define the voxel type for the sphere
            const auto sphereVoxelType = gfx::generators::voxel::Voxel::Jade;

            // Entity volume is 16x16x16, so coordinates go from 0 to 15
            const int   entitySize   = 16;
            const float entityCenter = (entitySize - 1) * 0.5f; // 7.5f for centering

            // Calculate where the entity will be positioned (integer world coordinates)
            const glm::vec3    entityOffset   = glm::vec3(entityCenter);
            const glm::vec3    entityWorldPos = worldSphereCenter - entityOffset;
            const glm::i32vec3 entityIntPos   = static_cast<glm::i32vec3>(entityWorldPos);

            // Calculate the actual world position of the entity's origin after integer quantization
            const glm::vec3 actualEntityWorldPos = static_cast<glm::vec3>(entityIntPos);

            // Generate sphere voxels in entity-local space (16x16x16 volume)
            for (int x = 0; x < entitySize; ++x)
            {
                for (int y = 0; y < entitySize; ++y)
                {
                    for (int z = 0; z < entitySize; ++z)
                    {
                        // Convert entity-local coordinates to world coordinates
                        const glm::vec3 voxelWorldPos =
                            actualEntityWorldPos
                            + glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

                        // Calculate distance from voxel to the orbiting sphere center
                        const glm::vec3 diff       = voxelWorldPos - worldSphereCenter;
                        const float     distanceSq = glm::dot(diff, diff);

                        // If voxel is within sphere radius, add it
                        if (distanceSq <= sphereRadiusSq)
                        {
                            newVoxels.push_back(
                                {gfx::generators::voxel::ChunkLocalPosition {
                                     static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z)},
                                 sphereVoxelType});
                        }
                    }
                }
            }

            // Update the voxel data for the sphere entity
            this->voxel_world_manager.updateVoxelEntityData(e, std::move(newVoxels));

            this->voxel_world_manager.updateVoxelEntityPosition(
                e, gfx::WorldPosition {static_cast<glm::i32vec3>(entityWorldPos)});
        }
        // timer.stamp("dump");
        const f32 height = 23.2f + (14.2f * std::sin(this->game->getRenderer()->getTimeAlive()));

        for (usize i = 0; i < this->lights.size(); ++i)
        {
            this->voxel_world_manager.updateVoxelLight(
                lights[i],
                gfx::generators::voxel::GpuRaytracedLight {
                    .position_and_half_intensity_distance {25 * i + 16.3, height, 200 * (i / 2) + 91.23, 8},
                    .color_and_power {1.0, 1.0, 1.0, 16.0}});
        }

        util::TimestampStamper stamper;

        const f32  deltaTime        = updateArgs.delta_time;
        const bool hasResizeOcurred = updateArgs.has_resize_occurred;

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
            return game::Game::GameStateUpdateResult {
                .should_terminate {true}, .generators {}, .camera {}, .render_thread_profile {}};
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

        stamper.stamp("camera processing");

        this->voxel_world_manager.onFrameUpdate(camera);

        stamper.stamp("Voxel World Update");

        return game::Game::GameStateUpdateResult {
            .should_terminate {false},
            .generators {gfx::FrameGenerator::FrameGenerateArgs {
                .maybe_triangle_renderer {&this->triangle_renderer},
                .maybe_skybox_renderer {&this->skybox_renderer},
                .maybe_imgui_renderer {&this->imgui_renderer},
                .maybe_voxel_renderer {this->voxel_world_manager.getRenderer()}}},
            .camera {this->camera},
            .render_thread_profile {std::move(stamper)}};
    }

    static constexpr f32 FovY = glm::radians(70.0f);

    game::Game* game;

    gfx::generators::triangle::TriangleRenderer                        triangle_renderer;
    gfx::generators::skybox::SkyboxRenderer                            skybox_renderer;
    gfx::generators::imgui::ImguiRenderer                              imgui_renderer;
    gfx::VoxelWorldManager                                             voxel_world_manager;
    std::vector<gfx::generators::triangle::TriangleRenderer::Triangle> triangles;

    std::vector<gfx::generators::voxel::VoxelRenderer::VoxelChunk>       chunks;
    std::vector<gfx::generators::voxel::VoxelRenderer::UniqueVoxelLight> lights;
    std::vector<gfx::VoxelWorldManager::UniqueVoxelEntity>               sphere_entities;

    gfx::Camera camera {gfx::Camera::CameraDescriptor {.fov_y {FovY}}};
    usize       index_of_cornel_box = 0;
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
