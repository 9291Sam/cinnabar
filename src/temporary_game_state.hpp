#pragma once

#include "game/game.hpp"
#include "gfx/generators/imgui/imgui_renderer.hpp"
#include "gfx/generators/skybox/skybox_renderer.hpp"
#include "gfx/generators/triangle/triangle_renderer.hpp"
#include "gfx/voxel_world_manager.hpp"

struct TemporaryGameState : game::Game::GameState
{
    explicit TemporaryGameState(game::Game* game_);
    ~TemporaryGameState() override;

    TemporaryGameState(const TemporaryGameState&)             = delete;
    TemporaryGameState(TemporaryGameState&&)                  = delete;
    TemporaryGameState& operator= (const TemporaryGameState&) = delete;
    TemporaryGameState& operator= (TemporaryGameState&&)      = delete;

    [[nodiscard]] std::string_view                  identify() const override;
    [[nodiscard]] game::Game::GameStateUpdateResult update(game::Game::GameStateUpdateArgs updateArgs) override;

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