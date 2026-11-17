#pragma once

#include "soulbound/CardDefinition.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace soulbound {

class CardDatabase {
public:
    [[nodiscard]] static CardDatabase fromFile(const std::filesystem::path& path);
    // Sorted by ID; returned values are independent of this database.
    [[nodiscard]] std::vector<CardDefinition> getAll() const;
    // Rejects invalid stats, empty identifiers/names and duplicate identifiers.
    bool add(CardDefinition definition);
    [[nodiscard]] const CardDefinition* find(const std::string& id) const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::unordered_map<std::string, CardDefinition> definitions_;
};

} // namespace soulbound
