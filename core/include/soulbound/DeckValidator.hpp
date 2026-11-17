#pragma once
#include "soulbound/CardDatabase.hpp"
#include "soulbound/DeckDefinition.hpp"
#include "soulbound/GameConfig.hpp"
#include <string_view>

namespace soulbound {
enum class DeckValidationMode { Storage, Play };
enum class DeckValidationError {
    None, InvalidId, InvalidName, UnknownCard, InvalidCount,
    TooManyCards, IncompleteDeck
};
struct DeckValidationResult {
    DeckValidationError error = DeckValidationError::None;
    std::size_t totalCards = 0;
    std::string message;
    [[nodiscard]] bool isValid() const noexcept { return error == DeckValidationError::None; }
};
class DeckValidator {
public:
    // Deck IDs also serve as filenames: ASCII letters, digits, '-' and '_' only.
    [[nodiscard]] static bool isValidId(std::string_view id) noexcept;
    [[nodiscard]] static DeckValidationResult validate(const DeckDefinition& deck,
        const CardDatabase& cards, const GameConfig& config,
        DeckValidationMode mode = DeckValidationMode::Storage);
};
}
