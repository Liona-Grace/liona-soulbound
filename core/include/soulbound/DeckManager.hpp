#pragma once
#include "soulbound/DeckStorage.hpp"
#include "soulbound/DeckValidator.hpp"

namespace soulbound {
class DeckManager {
public:
    DeckManager(CardDatabase cards, GameConfig config, std::filesystem::path directory);
    [[nodiscard]] std::vector<DeckDefinition> getAll() const;
    [[nodiscard]] std::optional<DeckDefinition> get(const std::string& id) const;
    void create(const DeckDefinition& deck) const;
    void update(const DeckDefinition& deck) const;
    [[nodiscard]] bool remove(const std::string& id) const;
    [[nodiscard]] DeckValidationResult validate(const DeckDefinition& deck,
        DeckValidationMode mode = DeckValidationMode::Storage) const;
private:
    CardDatabase cards_;
    GameConfig config_;
    DeckStorage storage_;
    void requireValid(const DeckDefinition& deck) const;
};
}
