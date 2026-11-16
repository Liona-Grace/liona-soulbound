#pragma once

#include "soulbound/CardDatabase.hpp"
#include "soulbound/GameConfig.hpp"

#include <filesystem>

namespace soulbound {

class JsonLoader {
public:
    // Throws std::runtime_error on I/O, JSON syntax or schema errors.
    // Missing, unknown and duplicate fields are rejected.
    [[nodiscard]] static GameConfig loadGameConfig(const std::filesystem::path& path);
    [[nodiscard]] static CardDatabase loadCards(const std::filesystem::path& path);
};

} // namespace soulbound
