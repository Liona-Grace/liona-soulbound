#include "soulbound/DeckManager.hpp"
#include <stdexcept>
#include <utility>

namespace soulbound {
DeckManager::DeckManager(CardDatabase cards, GameConfig config, std::filesystem::path directory)
    : cards_(std::move(cards)), config_(config), storage_(std::move(directory)) {
    if (!config_.isValid()) throw std::invalid_argument("Invalid GameConfig");
}
DeckValidationResult DeckManager::validate(const DeckDefinition& deck, DeckValidationMode mode) const {
    return DeckValidator::validate(deck, cards_, config_, mode);
}
void DeckManager::requireValid(const DeckDefinition& deck) const {
    const auto result = validate(deck);
    if (!result.isValid()) throw std::invalid_argument("Invalid deck '" + deck.id + "': " + result.message);
}
std::vector<DeckDefinition> DeckManager::getAll() const {
    auto decks = storage_.getAll();
    for (const auto& deck : decks) requireValid(deck);
    return decks;
}
std::optional<DeckDefinition> DeckManager::get(const std::string& id) const {
    auto deck = storage_.get(id);
    if (deck) requireValid(*deck);
    return deck;
}
void DeckManager::create(const DeckDefinition& deck) const {
    requireValid(deck);
    if (storage_.get(deck.id)) throw std::invalid_argument("Deck already exists: " + deck.id);
    storage_.save(deck);
}
void DeckManager::update(const DeckDefinition& deck) const {
    requireValid(deck);
    if (!storage_.get(deck.id)) throw std::out_of_range("Deck not found: " + deck.id);
    storage_.save(deck);
}
bool DeckManager::remove(const std::string& id) const { return storage_.remove(id); }
}
