#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/AttackStats.hpp"

#include <optional>

namespace ian {

class CrystalMineSystem;

struct BuildingStats {
    double maxHealth{};
    std::optional<FinalAttackStats> finalAttack;
    std::optional<double> attackDamage;
    std::optional<double> pushStrength;
    std::optional<double> attackRange;
    std::optional<double> attackArcDegrees;
    std::optional<double> attacksPerSecond;
    std::optional<double> piercingCount;
    std::optional<double> effectRadius;
    std::optional<double> productionPerCycle;
    std::optional<double> productionInterval;
    std::optional<double> slowPercent;
    std::optional<double> effectDuration;
    std::optional<double> cooldown;
    std::optional<double> attractionTargets;
    std::optional<double> repairPerSecond;
    std::optional<double> repairInterval;
    std::optional<double> buildingLimit;
    std::optional<double> woodCapacity;
    std::optional<double> stoneCapacity;
    std::optional<double> crystalCapacity;
};

struct BuildingStatComparison {
    std::optional<BuildingStats> previous;
    BuildingStats current;
    std::optional<BuildingStats> next;
};

[[nodiscard]] BuildingStats buildingStatsAtLevel(
    const BuildingInstance& building, std::uint8_t level,
    const CrystalMineSystem& producers,
    double baseMaxHealth = 0.0);

[[nodiscard]] BuildingStatComparison compareBuildingStats(
    const BuildingInstance& building,
    const CrystalMineSystem& producers,
    std::uint8_t maximumLevel,
    double baseMaxHealth = 0.0);

} // namespace ian
