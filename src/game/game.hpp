#pragma once

#include "gfx/camera.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/frame_generator.hpp"
#include "util/threads.hpp"
#include <atomic>
#include <concepts>
namespace game
{
    class Game
    {
    public:
        struct GameStateUpdateResult
        {
            bool                                   should_terminate;
            gfx::FrameGenerator::FrameGenerateArgs generators;
            gfx::Camera                            camera;
        };

        struct GameStateUpdateArgs
        {
            float delta_time;
            bool  has_resize_ocurred;
        };

        struct GameState
        {
            GameState();
            virtual ~GameState() = 0;

            GameState(const GameState&)             = delete;
            GameState(GameState&&)                  = delete;
            GameState& operator= (const GameState&) = delete;
            GameState& operator= (GameState&&)      = delete;

            [[nodiscard]] virtual std::string_view identify() const = 0;

            virtual GameStateUpdateResult update(GameStateUpdateArgs) = 0;
        };

        using GameStateConstructionFunction = std::unique_ptr<GameState> (*)(Game*);
    public:
        explicit Game(const gfx::core::Renderer*);
        ~Game();

        Game(const Game&)             = delete;
        Game(Game&&)                  = delete;
        Game& operator= (const Game&) = delete;
        Game& operator= (Game&&)      = delete;

        const gfx::core::Renderer* getRenderer() const;

        template<class T>
            requires (std::derived_from<T, GameState> && std::constructible_from<T, Game*>)
        void enqueueInstallGameState() const
        {
            constexpr GameStateConstructionFunction MakeNewGameStateFunction = +[](Game* g)
            {
                return static_cast<std::unique_ptr<GameState>>(std::make_unique<T>(g));
            };

            const GameStateConstructionFunction maybeAlreadyEnqueuedGameState =
                this->maybe_game_state_construction_function.exchange(
                    MakeNewGameStateFunction, std::memory_order_acq_rel);

            if (maybeAlreadyEnqueuedGameState != nullptr)
            {
                log::warn("Multiple enqueue of new GameState!");
            }
        }

        void enterUpdateLoop();

    private:
        const gfx::core::Renderer* renderer;
        gfx::FrameGenerator        frame_generator;

        mutable std::atomic<GameStateConstructionFunction> maybe_game_state_construction_function;

        std::unique_ptr<GameState> maybe_current_game_state;
        bool                       has_resize_ocurred;
    };
} // namespace game