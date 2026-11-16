#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace soulbound {

using InstanceId = std::uint64_t;
using PlayerId = std::size_t;
using HitPoints = std::int64_t;

struct CardInstance {
    InstanceId id = 0;
    std::string definitionId;
    PlayerId owner = 0;
    HitPoints currentHP = 0;
    bool hasAttacked = false;
};

} // namespace soulbound
