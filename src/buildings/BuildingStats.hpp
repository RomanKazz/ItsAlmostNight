#pragma once

#include "buildings/BuildingSystem.hpp"

#include <optional>

namespace ian {

class GoldMineSystem;

struct BuildingStats {
    double maxHealth{};
    std::optional<double> attackDamage;
    std::optional<double> attackRange;
    std::optional<double> attacksPerSecond;
    std::optional<double> effectRadius;
    std::optional<double> goldPerCycle;
    std::optional<double> productionInterval;
    std::optional<double> slowPercent;
    std::optional<double> effectDuration;
    std::optional<double> cooldown;
    std::optional<double> storageCapacity;
};

struct BuildingStatComparison {
    std::optional<BuildingStats> previous;
    BuildingStats current;
    std::optional<BuildingStats> next;
};

[[nodiscard]] BuildingStats buildingStatsAtLevel(
    const BuildingInstance& building, std::uint8_t level,
    const GoldMineSystem& producers);

[[nodiscard]] BuildingStatComparison compareBuildingStats(
    const BuildingInstance& building,
    const GoldMineSystem& producers,
    std::uint8_t maximumLevel);

} // namespace ian
