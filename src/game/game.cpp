#include "game.hpp"
#include "gfx/core/renderer.hpp"
#include "gfx/core/window.hpp"
#include "gfx/frame_generator.hpp"
#include <atomic>

namespace game
{
    Game::Game::GameState::GameState()  = default;
    Game::Game::GameState::~GameState() = default;

    Game::Game(const gfx::core::Renderer* renderer_)
        : renderer {renderer_}
        , frame_generator {this->renderer}
        , maybe_game_state_construction_function {nullptr}
        , maybe_current_game_state {nullptr}
        , has_resize_ocurred {true}
    {}

    Game::~Game()
    {
        if (this->maybe_game_state_construction_function.load(std::memory_order_acquire) != nullptr)
        {
            log::warn("Destroying Game with new GameState enqueued!");
        }
    }

    const gfx::core::Renderer* Game::getRenderer() const
    {
        return this->renderer;
    }

    void Game::enterUpdateLoop()
    {
        while (!this->renderer->shouldWindowClose())
        {
            if (GameStateConstructionFunction maybeGameStateConstructionFunction =
                    this->maybe_game_state_construction_function.exchange(nullptr, std::memory_order_acq_rel);
                maybeGameStateConstructionFunction != nullptr)
            {
                if (this->maybe_current_game_state != nullptr)
                {
                    log::info("Destroying old GameState \"{}\"", this->maybe_current_game_state->identify());

                    this->renderer->getDevice()->getDevice().waitIdle();

                    this->maybe_current_game_state.reset();
                }

                this->has_resize_ocurred = true;

                log::info("Installing new GameState");

                this->maybe_current_game_state = maybeGameStateConstructionFunction(this);

                log::info("Installed new GameState \"{}\"", this->maybe_current_game_state->identify());
            }

            const float deltaTime = this->renderer->getWindow()->getDeltaTimeSeconds();

            assert::critical(
                this->maybe_current_game_state != nullptr, "Tried to iterate Game's update loop without a GameState!");

            GameStateUpdateResult newUpdate = this->maybe_current_game_state->update(
                GameStateUpdateArgs {.delta_time {deltaTime}, .has_resize_ocurred {this->has_resize_ocurred}});

            this->has_resize_ocurred = false;

            if (newUpdate.should_terminate)
            {
                break;
            }

            this->has_resize_ocurred = this->frame_generator.renderFrame(newUpdate.generators, newUpdate.camera);
        }
    }
} // namespace game