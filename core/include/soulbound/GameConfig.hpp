#pragma once

#include <cstddef>

namespace soulbound {

struct GameConfig {
    int startingHP = 20;
    std::size_t startingHandSize = 4;
    std::size_t boardSize = 6;
    std::size_t deckSize = 30;

    [[nodiscard]] bool isValid() const noexcept {
        return startingHP > 0 && boardSize > 0 && deckSize > 0 && startingHandSize <= deckSize;
    }
};

} // namespace soulbound
