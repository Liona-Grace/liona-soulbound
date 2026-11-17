#include "soulbound/CardDatabase.hpp"
#include "soulbound/JsonLoader.hpp"

#include <algorithm>
#include <utility>

namespace soulbound {

CardDatabase CardDatabase::fromFile(const std::filesystem::path& path) {
    return JsonLoader::loadCards(path);
}

std::vector<CardDefinition> CardDatabase::getAll() const {
    std::vector<CardDefinition> cards;
    cards.reserve(definitions_.size());
    for (const auto& [id, definition] : definitions_) cards.push_back(definition);
    std::sort(cards.begin(), cards.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return cards;
}

bool CardDatabase::add(CardDefinition definition) {
    if (definition.id.empty() || definition.name.empty() ||
        definition.attack < 0 || definition.hp <= 0) {
        return false;
    }
    const auto id = definition.id;
    return definitions_.emplace(id, std::move(definition)).second;
}

const CardDefinition* CardDatabase::find(const std::string& id) const {
    const auto it = definitions_.find(id);
    return it == definitions_.end() ? nullptr : &it->second;
}

std::size_t CardDatabase::size() const noexcept {
    return definitions_.size();
}

} // namespace soulbound
