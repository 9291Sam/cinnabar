#pragma once

#include "gfx/camera.hpp"
#include "gfx/frame_generator.hpp"
namespace game
{
    class Game
    {
    public:
        struct GameStateUpdate
        {
            gfx::FrameGenerator::FrameGenerateArgs generators;
            gfx::Camera                            camera;
        };

        struct GameState
        {
            virtual ~GameState();

            virtual GameStateUpdate update(float deltaTime);
        };
    public:
        explicit Game(const core::Renderer*);
        ~Game() = default;

        Game(const Game&)             = delete;
        Game(Game&&)                  = delete;
        Game& operator= (const Game&) = delete;
        Game& operator= (Game&&)      = delete;

        void installGameState
    };
} // namespace game