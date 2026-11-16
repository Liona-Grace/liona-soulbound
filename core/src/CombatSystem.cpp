#include "soulbound/CombatSystem.hpp"

#include <algorithm>

namespace soulbound {

CombatResult CombatSystem::resolve(
    const CardDefinition& attackerDefinition, CardInstance& attacker,
    const CardDefinition& defenderDefinition, CardInstance& defender) {
    CombatResult result;
    defender.currentHP -= attackerDefinition.attack;
    if (defender.currentHP <= 0) {
        result.damageToDefenderOwner = std::max(HitPoints{0}, -defender.currentHP);
    } else {
        attacker.currentHP -= defenderDefinition.attack;
        if (attacker.currentHP <= 0) {
            result.damageToAttackerOwner = std::max(HitPoints{0}, -attacker.currentHP);
        }
    }
    return result;
}

} // namespace soulbound
