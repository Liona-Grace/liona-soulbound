#pragma once
#include "soulbound/DeckDefinition.hpp"
#include <filesystem>
#include <optional>

namespace soulbound {
class DeckStorage {
public:
    explicit DeckStorage(std::filesystem::path directory);
    [[nodiscard]] std::vector<DeckDefinition> getAll() const;
    [[nodiscard]] std::optional<DeckDefinition> get(const std::string& id) const;
    // Writes a complete JSON file via temporary-file rename. Manager validates semantics.
    void save(const DeckDefinition& deck) const;
    [[nodiscard]] bool remove(const std::string& id) const;
private:
    std::filesystem::path directory_;
    [[nodiscard]] std::filesystem::path pathFor(const std::string& id) const;
};
}
