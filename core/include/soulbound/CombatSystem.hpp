#pragma once

#include "soulbound/CardDefinition.hpp"
#include "soulbound/CardInstance.hpp"

namespace soulbound {

struct CombatResult {
    HitPoints damageToAttackerOwner = 0;
    HitPoints damageToDefenderOwner = 0;
};

class CombatSystem {
public:
    // Attacker strikes first. Only a surviving defender counterattacks.
    // Both strikes transfer excess damage to the owner of a destroyed unit.
    // Requires living units and validated, nonnegative attack values.
    [[nodiscard]] static CombatResult resolve(
        const CardDefinition& attackerDefinition, CardInstance& attacker,
        const CardDefinition& defenderDefinition, CardInstance& defender);
};

} // namespace soulbound
