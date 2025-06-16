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
        this->lights.push_back(
            this->voxel_world_manager.createVoxelLightUnique(gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {33.3, 23.2, 91.23, 8}, .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        this->lights.push_back(
            this->voxel_world_manager.createVoxelLightUnique(gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {133.3, 23.2, 91.23, 4},
                .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        this->lights.push_back(
            this->voxel_world_manager.createVoxelLightUnique(gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {133.3, 23.2, 91.23, 4},
                .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        this->lights.push_back(
            this->voxel_world_manager.createVoxelLightUnique(gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {133.3, 23.2, 91.23, 4},
                .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        this->lights.push_back(
            this->voxel_world_manager.createVoxelLightUnique(gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {133.3, 23.2, 91.23, 4},
                .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        this->lights.push_back(
            this->voxel_world_manager.createVoxelLightUnique(gfx::generators::voxel::GpuRaytracedLight {
                .position_and_half_intensity_distance {133.3, 23.2, 91.23, 4},
                .color_and_power {1.0, 1.0, 1.0, 42.0}}));

        this->sphere_entity = this->voxel_world_manager.createVoxelEntityUnique({}, glm::u8vec3 {16, 16, 16});
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
        // util::MultiTimer                                                                                  timer {};
        std::vector<std::pair<gfx::generators::voxel::ChunkLocalPosition, gfx::generators::voxel::Voxel>> newVoxels {};

        // Sphere properties
        const float time           = this->game->getRenderer()->getTimeAlive();
        const float sphereRadius   = 7.0f;
        const float sphereRadiusSq = sphereRadius * sphereRadius;
        const float orbitRadius    = 24.0f; // Kept within the 64x64x64 chunk bounds

        // Calculate the orbiting center of the sphere in chunk-local space
        const glm::vec3 sphereCenter = {
            31.5f + orbitRadius * std::cos(time * 0.5f),
            15.5f + 6.0f * std::sin(time * 0.4f), // Add some vertical motion
            31.5f + orbitRadius * std::sin(time * 0.5f)};

        // Define the voxel type for the sphere
        const auto sphereVoxelType = gfx::generators::voxel::Voxel::Jade; // Assuming this type exists

        // Determine the bounding box for the sphere, clamped to chunk boundaries [0, 63]
        const u32 startX = static_cast<u32>(std::max(0.0f, sphereCenter.x - sphereRadius));
        const u32 endX   = static_cast<u32>(std::min(63.0f, sphereCenter.x + sphereRadius));
        const u32 startY = static_cast<u32>(std::max(0.0f, sphereCenter.y - sphereRadius));
        const u32 endY   = static_cast<u32>(std::min(63.0f, sphereCenter.y + sphereRadius));
        const u32 startZ = static_cast<u32>(std::max(0.0f, sphereCenter.z - sphereRadius));
        const u32 endZ   = static_cast<u32>(std::min(63.0f, sphereCenter.z + sphereRadius));

        // Iterate through the bounding box and add sphere voxels
        for (u32 x = startX; x <= endX; ++x)
        {
            for (u32 y = startY; y <= endY; ++y)
            {
                for (u32 z = startZ; z <= endZ; ++z)
                {
                    // Calculate squared distance from current voxel to the sphere's center
                    const float dx = static_cast<float>(x) - sphereCenter.x;
                    const float dy = static_cast<float>(y) - sphereCenter.y;
                    const float dz = static_cast<float>(z) - sphereCenter.z;

                    // If inside the sphere, add the voxel.
                    // This may overwrite existing Cornell Box voxels, which is intended.
                    if ((dx * dx + dy * dy + dz * dz) <= sphereRadiusSq)
                    {
                        newVoxels.push_back(
                            {gfx::generators::voxel::ChunkLocalPosition {dx + 8, dy + 8, dz + 8}, sphereVoxelType});
                    }
                }
            }
        }

        this->voxel_world_manager.updateVoxelEntityData(this->sphere_entity, std::move(newVoxels));
        this->voxel_world_manager.updateVoxelEntityPosition(
            this->sphere_entity, gfx::WorldPosition {static_cast<glm::i32vec3>(sphereCenter - sphereRadius / 2.0f)});

        // timer.stamp("dump");
        const f32 height = 23.2f + (14.2f * std::sin(this->game->getRenderer()->getTimeAlive()));

        for (usize i = 0; i < this->lights.size(); ++i)
        {
            this->voxel_world_manager.updateVoxelLight(
                lights[i],
                gfx::generators::voxel::GpuRaytracedLight {
                    .position_and_half_intensity_distance {25 * i + 16.3, height, 200 * (i / 2) + 91.23, 8},
                    .color_and_power {1.0, 1.0, 1.0, 4.0}});
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
    gfx::VoxelWorldManager::UniqueVoxelEntity                            sphere_entity;

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
