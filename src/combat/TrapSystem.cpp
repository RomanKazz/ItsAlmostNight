#include "combat/TrapSystem.hpp"
#include "enemies/EnemyCollision.hpp"
#include <cmath>

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
    setTypeBonuses(BuildingType::Core, 1.0, 1.0);
    traps_.clear();
    activationBuffer_.clear();
    hitBuffer_.clear();
}

void TrapSystem::setTypeBonuses(BuildingType type, double damage, double fireRate) {
    bonusType_ = type;
    typeDamageMultiplier_ = std::max(0.05, damage);
    typeFireRateMultiplier_ = std::max(0.05, fireRate);
}

void TrapSystem::setModifiers(
    double damage, double radius, double fireRate,
    double highGroundDamage, double reactiveShockDamage) {
    damageMultiplier_ = std::max(0.05, damage);
    radiusMultiplier_ = std::max(0.05, radius);
    fireRateMultiplier_ = std::max(0.05, fireRate);
    highGroundDamageMultiplier_ = std::max(1.0, highGroundDamage);
    reactiveShockDamage_ = std::max(0.0, reactiveShockDamage);
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
    query_.rebuild(enemies);
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
            const double heightBonus = building->platformStorey > 0
                ? highGroundDamageMultiplier_ : 1.0;
            const double radius = spikeTriggerRadius(building->level) * radiusMultiplier_;
            const double damage = spikeDamage(building->level) * damageMultiplier_ *
                heightBonus * (building->type == bonusType_ ? typeDamageMultiplier_ : 1.0);
            query_.nearby(enemies, position, radius * std::sqrt(2.0) + 4.0,
                [&](const EnemyInstance& enemy) {
                    const auto body = enemyCollisionCylinder(enemy);
                    const double bottom = body.center.y - body.halfHeight;
                    const double top = body.center.y + body.halfHeight;
                    if (bottom > position.y + .35 || top < position.y - .05) return;
                    const double dx = std::max(0.0, std::abs(body.center.x-position.x)-radius);
                    const double dz = std::max(0.0, std::abs(body.center.z-position.z)-radius);
                    if (dx*dx + dz*dz > body.radius*body.radius) return;
                    if (const auto hit = enemies.damage(enemy.id,damage)) {
                        ++affectedCount;
                        hitBuffer_.push_back({trap.buildingId,*hit,building->type});
                    }
                });
            if (affectedCount == 0) {
                continue;
            }
            trap.activationRemaining = SpikeAnimationDuration;
            trap.cooldownRemaining = spikeCooldown(building->level) /
                (fireRateMultiplier_ * (building->type == bonusType_ ? typeFireRateMultiplier_ : 1.0));
        } else {
            const double multiplier =
                1.0 - slowPercent(building->level) / 100.0;
            const auto affected = enemies.applySlowInRadius(
                position, triggerRadius(building->level) *
                    radiusMultiplier_, multiplier,
                slowDuration(building->level));
            affectedCount = static_cast<int>(affected.size());
            if (affectedCount == 0) {
                continue;
            }
            trap.cooldownRemaining = cooldown(building->level) /
                (fireRateMultiplier_ * (building->type == bonusType_ ? typeFireRateMultiplier_ : 1.0));
        }
        if (reactiveShockDamage_ > 0.0) {
            const double heightBonus = building->platformStorey > 0
                ? highGroundDamageMultiplier_ : 1.0;
            const auto shockHits = enemies.damageInRadius(
                position,
                std::max(
                    triggerRadius(building->level),
                    spikeTriggerRadius(building->level)) *
                    radiusMultiplier_ * 1.25,
                reactiveShockDamage_ * damageMultiplier_ *
                    heightBonus * (building->type == bonusType_ ? typeDamageMultiplier_ : 1.0),
                1.5);
            for (const EnemyDamageResult& hit : shockHits) {
                hitBuffer_.push_back({trap.buildingId, hit, building->type});
            }
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
