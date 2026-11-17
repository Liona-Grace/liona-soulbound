#include "soulbound/DeckValidator.hpp"
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace soulbound {
bool DeckValidator::isValidId(std::string_view id) noexcept {
    if (id.empty()) return false;
    for (const char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
DeckValidationResult DeckValidator::validate(const DeckDefinition& deck,
    const CardDatabase& cards, const GameConfig& config, DeckValidationMode mode) {
    if (!config.isValid()) throw std::invalid_argument("Invalid GameConfig");
    std::size_t total = 0;
    auto fail = [&](DeckValidationError error, std::string message) {
        return DeckValidationResult{error, total, std::move(message)};
    };
    if (!isValidId(deck.id)) return fail(DeckValidationError::InvalidId, "Invalid deck ID (use letters, digits, '-' or '_').");
    if (deck.name.find_first_not_of(" \t\r\n") == std::string::npos)
        return fail(DeckValidationError::InvalidName, "Deck name must not be blank.");
    std::unordered_map<std::string, std::size_t> copies;
    for (const auto& entry : deck.cards) {
        if (!cards.find(entry.cardId)) return fail(DeckValidationError::UnknownCard, "Unknown card: " + entry.cardId);
        if (entry.count <= 0) return fail(DeckValidationError::InvalidCount, "Card count must be positive: " + entry.cardId);
        // Subtract first, so neither narrowing nor summation can overflow.
        if (static_cast<std::uint64_t>(entry.count) > config.deckSize - total)
            return fail(DeckValidationError::TooManyCards, "Deck exceeds configured deckSize.");
        auto& count = copies[entry.cardId];
        if (static_cast<std::uint64_t>(entry.count) > config.maxCopiesPerCard - count)
            return fail(DeckValidationError::TooManyCopies, "Card exceeds maxCopiesPerCard ("
                + std::to_string(config.maxCopiesPerCard) + "): " + entry.cardId);
        count += static_cast<std::size_t>(entry.count);
        total += static_cast<std::size_t>(entry.count);
    }
    if (mode == DeckValidationMode::Play && total != config.deckSize)
        return fail(DeckValidationError::IncompleteDeck, "Deck must contain exactly " + std::to_string(config.deckSize) + " cards to play.");
    return {DeckValidationError::None, total, {}};
}
}
