#pragma once

#include "soulbound/CardInstance.hpp"

#include <optional>
#include <vector>

namespace soulbound {

class Game;

class Board {
public:
    Board();
    explicit Board(std::size_t size);
    [[nodiscard]] const std::vector<std::optional<CardInstance>>& zones() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    friend class Game;
    std::vector<std::optional<CardInstance>> zones_;
};

} // namespace soulbound
