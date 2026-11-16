#pragma once

#include "soulbound/Board.hpp"

#include <vector>

namespace soulbound {

struct PlayerState {
    HitPoints hp = 0;
    std::vector<CardInstance> deck;
    std::vector<CardInstance> hand;
    std::vector<CardInstance> tomb;
    Board board;
};

} // namespace soulbound
