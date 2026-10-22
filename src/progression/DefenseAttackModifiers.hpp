#pragma once
#include "combat/AttackStats.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "progression/RunUpgrade.hpp"

namespace ian {
struct AttackUpgradeBinding {
    RunUpgradeEffect effect;
    AttackStat stat;
    double flatPerPower{};
    double percentPerPower{};
};
// Adding a stat card needs a definition/ID and one binding here, with no
// weapon-specific upgrade logic. Existing IDs 14/20/23 migrate in place.
inline constexpr std::array DefenseAttackUpgradeBindings{
    AttackUpgradeBinding{RunUpgradeEffect::DefenseDamage, AttackStat::Damage, 0.0, 0.12},
    AttackUpgradeBinding{RunUpgradeEffect::DefenseFireRate, AttackStat::AttackSpeed, 0.0, 0.10},
    AttackUpgradeBinding{RunUpgradeEffect::ProjectileCount, AttackStat::Quantity, 1.0, 0.0},
    AttackUpgradeBinding{RunUpgradeEffect::ProjectileSpeed, AttackStat::ProjectileSpeed, 0.0, 0.20},
    AttackUpgradeBinding{RunUpgradeEffect::AttackDuration, AttackStat::Duration, 0.0, 0.20},
    AttackUpgradeBinding{RunUpgradeEffect::AttackSize, AttackStat::Size, 0.0, 0.15},
    AttackUpgradeBinding{RunUpgradeEffect::CriticalChance, AttackStat::CritChance, 0.075, 0.0},
    AttackUpgradeBinding{RunUpgradeEffect::CriticalDamage, AttackStat::CritDamage, 0.0, 0.25},
    AttackUpgradeBinding{RunUpgradeEffect::AttackKnockback, AttackStat::Knockback, 0.0, 0.20},
};
// Consumers can filter stat cards without duplicating weapon rules or IDs.
[[nodiscard]] inline bool defenseUpgradeAppliesToAny(
    RunUpgradeEffect effect, std::span<const BuildingInstance> buildings) {
    for (const auto& binding : DefenseAttackUpgradeBindings) {
        if (binding.effect != effect) continue;
        return std::ranges::any_of(buildings, [&](const BuildingInstance& building) {
            return supportsAttackStat(building.type, binding.stat);
        });
    }
    return false;
}
[[nodiscard]] inline AttackModifiers defenseAttackCardModifiers(
    const std::array<double, RunUpgradeEffectCount>& powers) {
    AttackModifiers result;
    for (const auto& binding : DefenseAttackUpgradeBindings) {
        const double power = std::max(0.0, powers[runUpgradeIndex(binding.effect)]);
        auto& modifier = result[binding.stat];
        modifier.flat += binding.flatPerPower * power;
        modifier.multiplier *= 1.0 + binding.percentPerPower * power;
    }
    return result;
}
} // namespace ian
