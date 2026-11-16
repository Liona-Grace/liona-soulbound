#include "soulbound/CardDatabase.hpp"

#include <utility>

namespace soulbound {

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
