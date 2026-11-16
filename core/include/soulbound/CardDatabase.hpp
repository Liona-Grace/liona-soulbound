#pragma once

#include "soulbound/CardDefinition.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace soulbound {

class CardDatabase {
public:
    // Rejects invalid stats, empty identifiers/names and duplicate identifiers.
    bool add(CardDefinition definition);
    [[nodiscard]] const CardDefinition* find(const std::string& id) const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::unordered_map<std::string, CardDefinition> definitions_;
};

} // namespace soulbound
