#include "buildings/BuildingStats.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/RepairStation.hpp"
#include "buildings/DecoyBeacon.hpp"
#include "buildings/BuildingOrientation.hpp"

#include "combat/CannonSystem.hpp"
#include "combat/CombatDisplayUnits.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "economy/CrystalMineSystem.hpp"

#include <cstddef>

namespace ian {

BuildingStats buildingStatsAtLevel(
    const BuildingInstance& building, std::uint8_t level,
    const CrystalMineSystem& producers,
    double baseMaxHealth) {
    if (baseMaxHealth <= 0.0) {
        static const auto Defaults = GameBalance::defaults().buildings;
        baseMaxHealth = Defaults[static_cast<std::size_t>(
            building.type)].maxHealth;
    }
    const double currentBaseHealth = buildingMaxHealthAtLevel(
        baseMaxHealth, building.level);
    const double healthBonus = currentBaseHealth > 0.0
        ? building.maxHealth / currentBaseHealth : 1.0;
    BuildingStats stats{
        .maxHealth = buildingMaxHealthAtLevel(
            baseMaxHealth, level) * healthBonus,
    };
    if (building.type == BuildingType::Core) {
        stats.woodCapacity = static_cast<double>(
            coreResourceCapacity(ResourceCapacityType::Wood, level));
        stats.stoneCapacity = static_cast<double>(
            coreResourceCapacity(ResourceCapacityType::Stone, level));
        stats.crystalCapacity = static_cast<double>(
            coreResourceCapacity(ResourceCapacityType::Crystal, level));
    } else if (building.type == BuildingType::WindBlower) {
        const auto attack = TowerSystem::baseStats(building.type, level);
        stats.attackRange = attack.range;
        stats.attackArcDegrees = defenseAttackArcDegrees(building.type, level);
        stats.cooldown = attack.attackInterval;
        stats.pushStrength = attack.knockback;
    } else if (building.type == BuildingType::Turret ||
               building.type == BuildingType::GunTurret ||
               building.type == BuildingType::Flamethrower || building.type == BuildingType::MissileLauncher || building.type == BuildingType::TeslaTower || building.type == BuildingType::LaserTower) {
        const double towerBonus = buildingAnvilMultiplier(building);
        stats.attackDamage = TowerSystem::attackDamage(
            building.type, level) *
            towerBonus * CombatDisplayScale;
        stats.attackRange = TowerSystem::attackRange(
            building.type, level);
        stats.attackArcDegrees = defenseAttackArcDegrees(building.type, level);
        stats.attacksPerSecond =
            1.0 / TowerSystem::fireInterval(building.type, level);
        if (building.type == BuildingType::MissileLauncher) stats.effectRadius = 1.6;
        if (building.type == BuildingType::Flamethrower) {
            stats.effectDuration = TowerSystem::baseStats(building.type, level).duration;
        }
        if (building.type == BuildingType::Turret) {
            stats.piercingCount = static_cast<double>(
                TowerSystem::piercingCount(building.type, level));
        }
    } else if (building.type == BuildingType::Cannon ||
               building.type == BuildingType::Catapult || building.type == BuildingType::Mortar) {
        const double towerBonus = buildingAnvilMultiplier(building);
        stats.attackDamage = CannonSystem::explosionDamage(
            building.type, level) *
            towerBonus * CombatDisplayScale;
        stats.attackRange = CannonSystem::attackRange(
            building.type, level);
        stats.attackArcDegrees = defenseAttackArcDegrees(building.type, level);
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
    } else if (building.type == BuildingType::DecoyBeacon) {
        stats.effectRadius = decoyBeaconRadius(level);
        stats.effectDuration = DecoyBeaconDuration;
        stats.cooldown = DecoyBeaconInterval;
        stats.attractionTargets = decoyBeaconTargets(level);
    } else if (building.type == BuildingType::RepairStation) {
        stats.effectRadius = repairStationRadius(level);
        stats.repairPerSecond = repairStationRate(level);
        stats.repairInterval = RepairStationInterval;
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
    std::uint8_t maximumLevel,
    double baseMaxHealth) {
    BuildingStatComparison comparison{
        .current = buildingStatsAtLevel(
            building, building.level, producers, baseMaxHealth),
    };
    if (building.level > 1) {
        comparison.previous = buildingStatsAtLevel(
            building,
            static_cast<std::uint8_t>(building.level - 1),
            producers, baseMaxHealth);
    }
    if (building.level < maximumLevel) {
        comparison.next = buildingStatsAtLevel(
            building,
            static_cast<std::uint8_t>(building.level + 1),
            producers, baseMaxHealth);
    }
    return comparison;
}

} // namespace ian
