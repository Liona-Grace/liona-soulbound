#pragma once

#include <string>

namespace soulbound {

struct CardDefinition {
    std::string id;
    std::string name;
    int attack = 0;
    int hp = 1;
};

} // namespace soulbound
