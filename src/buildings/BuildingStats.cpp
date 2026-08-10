#include "buildings/BuildingStats.hpp"

#include "combat/CannonSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "economy/GoldMineSystem.hpp"

namespace ian {

BuildingStats buildingStatsAtLevel(
    const BuildingInstance& building, std::uint8_t level,
    const GoldMineSystem& producers) {
    const double currentHealthMultiplier =
        1.0 + 0.15 * static_cast<double>(building.level - 1);
    const double baseMaxHealth =
        building.maxHealth / currentHealthMultiplier;
    BuildingStats stats{
        .maxHealth =
            baseMaxHealth *
            (1.0 + 0.15 * static_cast<double>(level - 1)),
    };
    if (building.type == BuildingType::Turret) {
        const double towerBonus = building.anvilStacks > 0
            ? 1.0 + 0.10 * building.anvilStacks
            : building.anvilEnhanced ? 1.10 : 1.0;
        stats.attackDamage = TowerSystem::attackDamage(level) *
            towerBonus;
        stats.attackRange = TowerSystem::attackRange(level);
        stats.attacksPerSecond =
            1.0 / TowerSystem::fireInterval(level);
    } else if (building.type == BuildingType::Cannon) {
        const double towerBonus = building.anvilStacks > 0
            ? 1.0 + 0.10 * building.anvilStacks
            : building.anvilEnhanced ? 1.10 : 1.0;
        stats.attackDamage = CannonSystem::explosionDamage(level) *
            towerBonus;
        stats.attackRange = CannonSystem::attackRange(level);
        stats.attacksPerSecond =
            1.0 / CannonSystem::fireInterval(level);
        stats.effectRadius = CannonSystem::explosionRadius(level);
    } else if (
        building.type == BuildingType::GoldMine ||
        building.type == BuildingType::LumberMill ||
        building.type == BuildingType::Quarry) {
        stats.goldPerCycle = static_cast<double>(
            producers.productionAmount(level, building.type));
        stats.productionInterval =
            producers.productionInterval(building.type);
    } else if (building.type == BuildingType::SlowTrap) {
        stats.slowPercent = TrapSystem::slowPercent(level);
        stats.effectRadius = TrapSystem::triggerRadius(level);
        stats.effectDuration = TrapSystem::slowDuration(level);
        stats.cooldown = TrapSystem::cooldown(level);
    } else if (building.type == BuildingType::SpikeTrap) {
        stats.attackDamage = TrapSystem::spikeDamage(level);
        stats.effectRadius = TrapSystem::spikeTriggerRadius(level);
        stats.cooldown = TrapSystem::spikeCooldown(level);
    }
    return stats;
}

BuildingStatComparison compareBuildingStats(
    const BuildingInstance& building,
    const GoldMineSystem& producers,
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
