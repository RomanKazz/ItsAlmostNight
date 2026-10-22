#pragma once

#include "combat/AttackStatCompatibility.hpp"
#include "core/DeterministicRandom.hpp"
#include "progression/RunUpgrade.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace ian {

using BuildingUpgradePowers = std::array<
    std::array<double, static_cast<std::size_t>(AttackStat::Count)>,
    GameBalance::BuildingTypeCount>;

struct BuildingStatRoll {
    std::array<AttackStat, 2> stats{};
    std::uint8_t count{};
};

// Each weapon family rolls only stats its attack actually consumes. Utility
// buildings retain their authored level progression.
[[nodiscard]] inline std::span<const AttackStat> buildingStatUpgradePool(BuildingType type) {
    using S = AttackStat;
    static constexpr std::array gun{S::Damage, S::AttackSpeed, S::Quantity,
        S::Pierce, S::CritChance};
    static constexpr std::array turret{S::Damage, S::Range, S::Quantity,
        S::Pierce, S::CritChance, S::CritDamage};
    static constexpr std::array cannon{S::Damage, S::Size, S::Quantity,
        S::Knockback, S::CritDamage};
    static constexpr std::array catapult{S::Damage, S::Range, S::Size,
        S::Quantity, S::Duration};
    static constexpr std::array mortar{S::Damage, S::Range, S::Size,
        S::Quantity, S::CritDamage};
    static constexpr std::array flame{S::Damage, S::Size, S::Duration, S::Range};
    static constexpr std::array missile{S::Damage, S::Size, S::Quantity,
        S::ProjectileSpeed, S::CritChance};
    static constexpr std::array tesla{S::Damage, S::Bounces, S::Quantity, S::Range};
    static constexpr std::array laser{S::Damage, S::Duration, S::Pierce,
        S::CritChance, S::CritDamage};
    static constexpr std::array wind{S::Size, S::Range, S::Duration, S::Knockback};
    static constexpr std::array spikes{S::Damage, S::Size, S::CritChance, S::CritDamage};
    static constexpr std::array slow{S::Size, S::Duration, S::SlowStrength, S::Range};
    switch (type) {
    case BuildingType::GunTurret: return gun;
    case BuildingType::Turret: return turret;
    case BuildingType::Cannon: return cannon;
    case BuildingType::Catapult: return catapult;
    case BuildingType::Mortar: return mortar;
    case BuildingType::Flamethrower: return flame;
    case BuildingType::MissileLauncher: return missile;
    case BuildingType::TeslaTower: return tesla;
    case BuildingType::LaserTower: return laser;
    case BuildingType::WindBlower: return wind;
    case BuildingType::SpikeTrap: return spikes;
    case BuildingType::SlowTrap: return slow;
    default: return {};
    }
}

[[nodiscard]] constexpr double buildingStatUpgradeIncrement(AttackStat stat) {
    switch (stat) {
    case AttackStat::Damage: return 0.10;
    case AttackStat::AttackSpeed: return 0.07;
    case AttackStat::Range: return 0.4;
    case AttackStat::Quantity:
    case AttackStat::Bounces:
    case AttackStat::Pierce: return 1.0;
    case AttackStat::CritChance: return 0.075;
    case AttackStat::CritDamage: return 0.25;
    case AttackStat::ProjectileSpeed: return 0.20;
    case AttackStat::Size: return 0.10;
    case AttackStat::Duration: return 0.12;
    case AttackStat::Knockback: return 0.15;
    case AttackStat::SlowStrength: return 0.12;
    default: return 0.0;
    }
}

[[nodiscard]] constexpr bool buildingStatUpgradeIsFlat(AttackStat stat) {
    return stat == AttackStat::Range || stat == AttackStat::Quantity ||
        stat == AttackStat::Bounces || stat == AttackStat::Pierce ||
        stat == AttackStat::CritChance || stat == AttackStat::CritDamage;
}

// Cards compare the earned local bonus, not the weapon's built-in count.
// A Gun Turret with three baseline bullets therefore previews its first
// Projectile Count card as 0 -> 1.
[[nodiscard]] inline std::pair<double, double> buildingStatBonusPreview(
    const BuildingUpgradePowers& powers, BuildingType type, AttackStat stat,
    RunUpgradeRarity rarity) {
    const double before = powers[static_cast<std::size_t>(type)]
        [static_cast<std::size_t>(stat)] * buildingStatUpgradeIncrement(stat);
    return {before, before + buildingStatUpgradeIncrement(stat) *
        runUpgradeRarityMultiplier(rarity)};
}

[[nodiscard]] inline BuildingStatRoll rollBuildingStatUpgrade(
    BuildingType type, RunUpgradeRarity rarity, std::uint64_t seed) {
    const auto pool = buildingStatUpgradePool(type);
    BuildingStatRoll result;
    if (pool.empty()) return result;
    const bool twoStats = rarity != RunUpgradeRarity::Common ||
        (mixBits64(seed ^ 0x43b0d7e5ULL) & 1ULL) != 0;
    result.count = static_cast<std::uint8_t>(twoStats && pool.size() > 1 ? 2 : 1);
    const std::size_t first = mixBits64(seed ^ 0xa0761d6478bd642fULL) % pool.size();
    result.stats[0] = pool[first];
    if (result.count == 2) {
        const std::size_t other = mixBits64(seed ^ 0xe7037ed1a0b428dbULL) % (pool.size() - 1);
        result.stats[1] = pool[other >= first ? other + 1 : other];
    }
    return result;
}

inline void applyBuildingStatUpgrade(BuildingUpgradePowers& powers,
    BuildingType type, const BuildingStatRoll& roll, RunUpgradeRarity rarity) {
    const auto typeIndex = static_cast<std::size_t>(type);
    if (typeIndex >= powers.size()) return;
    const double rarityPower = runUpgradeRarityMultiplier(rarity);
    for (std::size_t i = 0; i < roll.count; ++i) {
        const auto stat = roll.stats[i];
        if (!supportsAttackStat(type, stat) ||
            buildingStatUpgradeIncrement(stat) <= 0.0) continue;
        powers[typeIndex][static_cast<std::size_t>(stat)] += rarityPower;
    }
}

[[nodiscard]] inline AttackModifiers buildingStatUpgradeModifiers(
    BuildingType type, const BuildingUpgradePowers& powers) {
    AttackModifiers result;
    const auto typeIndex = static_cast<std::size_t>(type);
    if (typeIndex >= powers.size()) return result;
    // Apply saved legacy powers too, even when a stat is no longer offered by
    // this building's new card pool.
    for (std::size_t index = 0; index < static_cast<std::size_t>(AttackStat::Count); ++index) {
        const auto stat = static_cast<AttackStat>(index);
        if (!supportsAttackStat(type, stat) || buildingStatUpgradeIncrement(stat) <= 0.0) continue;
        const double amount = powers[typeIndex][static_cast<std::size_t>(stat)] *
            buildingStatUpgradeIncrement(stat);
        if (buildingStatUpgradeIsFlat(stat)) result[stat].flat = amount;
        else result[stat].multiplier = 1.0 + amount;
    }
    return result;
}

} // namespace ian
