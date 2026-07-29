#include "combat/TrapSystem.hpp"

#include <algorithm>

namespace ian {

TrapSystem::TrapSystem() {
    traps_.reserve(64);
    activationBuffer_.reserve(64);
}

double TrapSystem::triggerRadius(std::uint8_t level) {
    return TriggerRadius +
           0.12 * static_cast<double>(level - 1);
}

double TrapSystem::slowPercent(std::uint8_t level) {
    const double multiplier =
        SlowMultiplier -
        0.03 * static_cast<double>(level - 1);
    return (1.0 - multiplier) * 100.0;
}

double TrapSystem::slowDuration(std::uint8_t level) {
    return SlowDuration +
           0.2 * static_cast<double>(level - 1);
}

double TrapSystem::cooldown(std::uint8_t level) {
    return Cooldown -
           0.15 * static_cast<double>(level - 1);
}

void TrapSystem::reset() {
    traps_.clear();
    activationBuffer_.clear();
}

void TrapSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    std::erase_if(traps_, [&buildings](const TrapRuntime& trap) {
        return std::none_of(buildings.begin(), buildings.end(),
                            [&trap](const BuildingInstance& building) {
                                return building.id == trap.buildingId &&
                                       building.type == BuildingType::SlowTrap;
                            });
    });
    for (const auto& building : buildings) {
        if (building.type != BuildingType::SlowTrap) {
            continue;
        }
        const bool exists =
            std::any_of(traps_.begin(), traps_.end(), [&building](const TrapRuntime& trap) {
                return trap.buildingId == building.id;
            });
        if (!exists) {
            traps_.push_back({.buildingId = building.id});
        }
    }
}

std::span<const TrapActivation> TrapSystem::tick(
    double deltaSeconds, const std::vector<BuildingInstance>& buildings, EnemySystem& enemies) {
    activationBuffer_.clear();
    for (auto& trap : traps_) {
        trap.cooldownRemaining = std::max(0.0, trap.cooldownRemaining - deltaSeconds);
        if (trap.cooldownRemaining > 0.0) {
            continue;
        }
        const auto building =
            std::find_if(buildings.begin(), buildings.end(), [&trap](const BuildingInstance& item) {
                return item.id == trap.buildingId;
            });
        if (building == buildings.end()) {
            continue;
        }

        const Vec3 position =
            buildingWorldPosition(*building);
        const double levelBonus = static_cast<double>(building->level - 1);
        const double radius = triggerRadius(building->level);
        const double multiplier =
            1.0 - slowPercent(building->level) / 100.0;
        const double duration = slowDuration(building->level);
        const auto affected = enemies.applySlowInRadius(
            position, radius, multiplier, duration);
        if (affected.empty()) {
            continue;
        }
        activationBuffer_.push_back({
            .trapId = trap.buildingId,
            .position = position,
            .affectedCount = static_cast<int>(affected.size()),
            .wearDamage =
                WearDamage / (1.0 + 0.12 * levelBonus),
        });
        trap.cooldownRemaining = cooldown(building->level);
    }
    return activationBuffer_;
}

} // namespace ian
