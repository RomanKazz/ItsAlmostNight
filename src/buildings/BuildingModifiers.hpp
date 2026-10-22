#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/AttackStats.hpp"

namespace ian {

// Old saves store the first enhancement as a flag; stacks take precedence.
[[nodiscard]] inline double buildingAnvilMultiplier(
    int stacks, bool enhanced = false) {
    return stacks > 0 ? 1.0 + 0.10 * stacks : enhanced ? 1.10 : 1.0;
}

[[nodiscard]] inline double buildingAnvilMultiplier(
    const BuildingInstance& building) {
    return buildingAnvilMultiplier(building.anvilStacks, building.anvilEnhanced);
}

// All defense types compose the same run, per-building and class sources.
// A weapon only supplies immutable base data and its attack pattern.
[[nodiscard]] inline FinalAttackStats resolveBuildingAttackStats(
    const AttackStats& base, const BuildingInstance& building,
    const AttackModifiers& run, BuildingType bonusType,
    const AttackModifiers& typeBonus, double highGroundDamage = 1.0,
    const AttackModifiers& upgradeBonus = {}) {
    AttackModifiers local;
    local[AttackStat::Damage].multiplier = buildingAnvilMultiplier(building) *
        (building.platformStorey > 0 ? highGroundDamage : 1.0);
    const std::array sources{run, local,
        building.type == bonusType ? typeBonus : AttackModifiers{}, upgradeBonus};
    return resolveAttackStats(base, sources);
}

} // namespace ian
