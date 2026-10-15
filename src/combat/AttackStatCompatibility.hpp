#pragma once
#include "buildings/BuildingSystem.hpp"
#include "combat/AttackStats.hpp"
#include <cstdint>
#include <initializer_list>

namespace ian {
using AttackStatMask = std::uint64_t;
static_assert(static_cast<std::size_t>(AttackStat::Count) <= 64);
[[nodiscard]] constexpr AttackStatMask attackStatMask(AttackStat stat) {
    return stat < AttackStat::Count ? AttackStatMask{1} << static_cast<std::size_t>(stat) : 0;
}
[[nodiscard]] constexpr AttackStatMask attackStatMask(std::initializer_list<AttackStat> stats) {
    AttackStatMask result{};
    for (auto stat : stats) result |= attackStatMask(stat);
    return result;
}
struct AttackStatCompatibility {
    BuildingType type;
    AttackStatMask stats;
};
inline constexpr auto PhysicalAttackStats = attackStatMask({
    AttackStat::Damage, AttackStat::AttackSpeed, AttackStat::Quantity,
    AttackStat::ProjectileSpeed, AttackStat::Duration, AttackStat::Size,
    AttackStat::Bounces, AttackStat::Pierce, AttackStat::CritChance,
    AttackStat::CritDamage, AttackStat::Knockback, AttackStat::Range,
    AttackStat::BounceRange});
// Weapon data, not UI logic. Only implemented, meaningful mechanics are listed.
inline constexpr std::array DefenseAttackCompatibility{
    AttackStatCompatibility{BuildingType::LaserTower, attackStatMask({
        AttackStat::Damage, AttackStat::AttackSpeed, AttackStat::Quantity, AttackStat::Bounces,
        AttackStat::BounceRange, AttackStat::CritChance, AttackStat::CritDamage, AttackStat::Knockback, AttackStat::Range})},
    AttackStatCompatibility{BuildingType::TeslaTower, attackStatMask({
        AttackStat::Damage, AttackStat::AttackSpeed, AttackStat::Quantity, AttackStat::Bounces,
        AttackStat::CritChance, AttackStat::CritDamage, AttackStat::Knockback, AttackStat::Range,
        AttackStat::BounceRange, AttackStat::BurstInterval})},
    AttackStatCompatibility{BuildingType::MissileLauncher,
        (PhysicalAttackStats & ~attackStatMask({AttackStat::Bounces, AttackStat::Pierce, AttackStat::BounceRange})) |
        attackStatMask({AttackStat::BurstInterval, AttackStat::Spread, AttackStat::Homing})},
    AttackStatCompatibility{BuildingType::GunTurret, PhysicalAttackStats |
        attackStatMask({AttackStat::BurstInterval, AttackStat::Spread, AttackStat::Homing})},
    AttackStatCompatibility{BuildingType::Turret, PhysicalAttackStats |
        attackStatMask({AttackStat::Spread, AttackStat::FanSpread, AttackStat::Homing})},
    AttackStatCompatibility{BuildingType::Cannon, PhysicalAttackStats},
    AttackStatCompatibility{BuildingType::Catapult, PhysicalAttackStats},
    AttackStatCompatibility{BuildingType::SpikeTrap, attackStatMask({
        AttackStat::Damage, AttackStat::AttackSpeed, AttackStat::Quantity,
        AttackStat::Duration, AttackStat::Size, AttackStat::CritChance,
        AttackStat::CritDamage, AttackStat::Knockback, AttackStat::Range})},
    AttackStatCompatibility{BuildingType::SlowTrap, attackStatMask({
        AttackStat::AttackSpeed, AttackStat::Duration, AttackStat::Size, AttackStat::Range})},
};
[[nodiscard]] constexpr bool supportsAttackStat(BuildingType type, AttackStat stat) {
    for (const auto& weapon : DefenseAttackCompatibility)
        if (weapon.type == type) return (weapon.stats & attackStatMask(stat)) != 0;
    return false;
}
[[nodiscard]] inline AttackModifiers compatibleAttackModifiers(BuildingType type, const AttackModifiers& source) {
    AttackModifiers result;
    for (std::size_t index = 0; index < result.values.size(); ++index)
        if (supportsAttackStat(type, static_cast<AttackStat>(index))) result.values[index] = source.values[index];
    return result;
}
} // namespace ian
