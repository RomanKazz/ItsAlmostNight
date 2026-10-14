#include "buildings/BuildingStats.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/BuildingOrientation.hpp"

#include "combat/CannonSystem.hpp"
#include "combat/CombatDisplayUnits.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "economy/CrystalMineSystem.hpp"

namespace ian {

BuildingStats buildingStatsAtLevel(
    const BuildingInstance& building, std::uint8_t level,
    const CrystalMineSystem& producers) {
    const double currentHealthMultiplier =
        1.0 + 0.15 * static_cast<double>(building.level - 1);
    const double baseMaxHealth =
        building.maxHealth / currentHealthMultiplier;
    BuildingStats stats{
        .maxHealth =
            baseMaxHealth *
            (1.0 + 0.15 * static_cast<double>(level - 1)),
    };
    if (building.type == BuildingType::Core) {
        stats.defenseBuildingLimit = static_cast<double>(
            buildingLimitForCoreLevel(
                BuildingType::Turret, level));
        stats.producerPerTypeLimit = static_cast<double>(
            buildingLimitForCoreLevel(
                BuildingType::CrystalMine, level));
        stats.woodCapacity = static_cast<double>(
            coreResourceCapacity(ResourceCapacityType::Wood, level));
        stats.stoneCapacity = static_cast<double>(
            coreResourceCapacity(ResourceCapacityType::Stone, level));
        stats.crystalCapacity = static_cast<double>(
            coreResourceCapacity(ResourceCapacityType::Crystal, level));
    } else if (building.type == BuildingType::Turret ||
               building.type == BuildingType::GunTurret || building.type == BuildingType::MissileLauncher || building.type == BuildingType::TeslaTower) {
        const double towerBonus = buildingAnvilMultiplier(building);
        stats.attackDamage = TowerSystem::attackDamage(
            building.type, level) *
            towerBonus * CombatDisplayScale;
        stats.attackRange = TowerSystem::attackRange(
            building.type, level);
        stats.attackArcDegrees = building.type == BuildingType::TeslaTower ? 360.0 : defenseAttackArcDegrees(level);
        stats.attacksPerSecond =
            1.0 / TowerSystem::fireInterval(building.type, level);
        if (building.type == BuildingType::MissileLauncher) stats.effectRadius = 1.6;
        if (building.type == BuildingType::Turret) {
            stats.piercingCount = static_cast<double>(
                TowerSystem::piercingCount(building.type, level));
        }
    } else if (building.type == BuildingType::Cannon ||
               building.type == BuildingType::Catapult) {
        const double towerBonus = buildingAnvilMultiplier(building);
        stats.attackDamage = CannonSystem::explosionDamage(
            building.type, level) *
            towerBonus * CombatDisplayScale;
        stats.attackRange = CannonSystem::attackRange(
            building.type, level);
        stats.attackArcDegrees = building.type == BuildingType::TeslaTower ? 360.0 : defenseAttackArcDegrees(level);
        stats.attacksPerSecond =
            1.0 / CannonSystem::fireInterval(building.type, level);
        stats.effectRadius = CannonSystem::explosionRadius(
            building.type, level);
    } else if (
        building.type == BuildingType::CrystalMine ||
        building.type == BuildingType::LumberMill ||
        building.type == BuildingType::Quarry) {
        stats.productionPerCycle = static_cast<double>(
            producers.productionAmount(level, building.type));
        stats.productionInterval =
            producers.productionInterval(building.type, level);
    } else if (building.type == BuildingType::SlowTrap) {
        stats.slowPercent = TrapSystem::slowPercent(level);
        stats.effectRadius = TrapSystem::triggerRadius(level);
        stats.effectDuration = TrapSystem::slowDuration(level);
        stats.cooldown = TrapSystem::cooldown(level);
    } else if (building.type == BuildingType::SpikeTrap) {
        stats.attackDamage = combatDisplayScalar(
            TrapSystem::spikeDamage(level));
        stats.effectRadius = TrapSystem::spikeTriggerRadius(level);
        stats.cooldown = TrapSystem::spikeCooldown(level);
    }
    return stats;
}

BuildingStatComparison compareBuildingStats(
    const BuildingInstance& building,
    const CrystalMineSystem& producers,
    std::uint8_t maximumLevel) {
    BuildingStatComparison comparison{
        .current = buildingStatsAtLevel(
            building, building.level, producers),
    };
    if (building.level > 1) {
        comparison.previous = buildingStatsAtLevel(
            building,
            static_cast<std::uint8_t>(building.level - 1),
            producers);
    }
    if (building.level < maximumLevel) {
        comparison.next = buildingStatsAtLevel(
            building,
            static_cast<std::uint8_t>(building.level + 1),
            producers);
    }
    return comparison;
}

} // namespace ian
