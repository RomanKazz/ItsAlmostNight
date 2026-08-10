#include "combat/TrapSystem.hpp"

#include <algorithm>

namespace ian {

TrapSystem::TrapSystem() {
    traps_.reserve(64);
    activationBuffer_.reserve(64);
    hitBuffer_.reserve(64);
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

double TrapSystem::spikeTriggerRadius(std::uint8_t level) {
    return 0.42 + 0.025 * static_cast<double>(level - 1);
}

double TrapSystem::spikeDamage(std::uint8_t level) {
    return 18.0 + 4.0 * static_cast<double>(level - 1);
}

double TrapSystem::spikeCooldown(std::uint8_t level) {
    return std::max(
        SpikeAnimationDuration,
        1.55 - 0.04 * static_cast<double>(level - 1));
}

void TrapSystem::reset() {
    traps_.clear();
    activationBuffer_.clear();
    hitBuffer_.clear();
}

void TrapSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    std::erase_if(traps_, [&buildings](const TrapRuntime& trap) {
        return std::none_of(buildings.begin(), buildings.end(),
                            [&trap](const BuildingInstance& building) {
                                return building.id == trap.buildingId &&
                                       (building.type == BuildingType::SlowTrap ||
                                        building.type == BuildingType::SpikeTrap);
                            });
    });
    for (const auto& building : buildings) {
        if (building.type != BuildingType::SlowTrap &&
            building.type != BuildingType::SpikeTrap) {
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
    hitBuffer_.clear();
    for (auto& trap : traps_) {
        trap.cooldownRemaining = std::max(0.0, trap.cooldownRemaining - deltaSeconds);
        trap.activationRemaining = std::max(
            0.0, trap.activationRemaining - deltaSeconds);
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
        int affectedCount = 0;
        if (building->type == BuildingType::SpikeTrap) {
            const auto hits = enemies.damageInRadius(
                position, spikeTriggerRadius(building->level),
                spikeDamage(building->level));
            affectedCount = static_cast<int>(hits.size());
            for (const EnemyDamageResult& hit : hits) {
                hitBuffer_.push_back({trap.buildingId, hit});
            }
            if (affectedCount == 0) {
                continue;
            }
            trap.activationRemaining = SpikeAnimationDuration;
            trap.cooldownRemaining = spikeCooldown(building->level);
        } else {
            const double multiplier =
                1.0 - slowPercent(building->level) / 100.0;
            const auto affected = enemies.applySlowInRadius(
                position, triggerRadius(building->level), multiplier,
                slowDuration(building->level));
            affectedCount = static_cast<int>(affected.size());
            if (affectedCount == 0) {
                continue;
            }
            trap.cooldownRemaining = cooldown(building->level);
        }
        activationBuffer_.push_back({
            .trapId = trap.buildingId,
            .position = position,
            .affectedCount = affectedCount,
            .wearDamage =
                WearDamage / (1.0 + 0.12 * levelBonus),
        });
    }
    return activationBuffer_;
}

const std::vector<TrapRuntime>& TrapSystem::traps() const {
    return traps_;
}

std::span<const TrapHit> TrapSystem::hits() const {
    return hitBuffer_;
}

} // namespace ian
