#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace soulbound {
struct DeckEntry {
    std::string cardId;
    std::int64_t count = 0;
    bool operator==(const DeckEntry&) const = default;
};
struct DeckDefinition {
    std::string id;
    std::string name;
    std::vector<DeckEntry> cards;
    bool operator==(const DeckDefinition&) const = default;
};
}
