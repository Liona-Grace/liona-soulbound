#include "soulbound/Board.hpp"

#include <algorithm>

namespace soulbound {

Board::Board() = default;
Board::Board(std::size_t size) : zones_(size) {}

const std::vector<std::optional<CardInstance>>& Board::zones() const noexcept {
    return zones_;
}

bool Board::empty() const noexcept {
    return std::none_of(zones_.begin(), zones_.end(),
                        [](const auto& zone) { return zone.has_value(); });
}

} // namespace soulbound
