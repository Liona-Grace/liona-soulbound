#pragma once

#include "soulbound/PlayerState.hpp"

#include <array>
#include <cstdint>

namespace soulbound {

enum class Phase { Main, Battle };
enum class GameStatus { NotStarted, Playing, Player1Won, Player2Won, Draw };

// Data only. Game exposes this state exclusively through a const reference.
struct GameState {
    std::array<PlayerState, 2> players;
    PlayerId activePlayer = 0;
    std::uint64_t turnNumber = 1;
    bool hasDrawnThisTurn = false;
    Phase phase = Phase::Main;
    GameStatus status = GameStatus::NotStarted;
};

} // namespace soulbound
