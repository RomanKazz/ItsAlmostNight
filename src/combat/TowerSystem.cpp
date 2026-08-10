#include "combat/TowerSystem.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr double BaseRange = 9.0;
constexpr double BaseFireInterval = 0.5;
constexpr double SearchInterval = 0.25;
constexpr double BaseDamage = 1.0;
constexpr double TurnSpeed = 12.0;
constexpr double AimTolerance = 0.08726646259971647;

Vec3 towerPosition(const BuildingInstance& building) {
    Vec3 position = buildingWorldPosition(building);
    position.y += 1.4;
    return position;
}

bool withinRange(Vec3 origin, Vec3 target, double range) {
    const double deltaX = target.x - origin.x;
    const double deltaZ = target.z - origin.z;
    return (deltaX * deltaX) + (deltaZ * deltaZ) <= range * range;
}

double wrapAngle(double angle) {
    constexpr double Pi = 3.14159265358979323846;
    constexpr double TwoPi = Pi * 2.0;
    while (angle > Pi) {
        angle -= TwoPi;
    }
    while (angle < -Pi) {
        angle += TwoPi;
    }
    return angle;
}

} // namespace

TowerSystem::TowerSystem() {
    towers_.reserve(64);
    shotBuffer_.reserve(64);
}

double TowerSystem::attackRange(std::uint8_t level) {
    return BaseRange +
           0.3 * static_cast<double>(level - 1);
}

double TowerSystem::attackDamage(std::uint8_t level) {
    return BaseDamage +
           0.2 * static_cast<double>(level - 1);
}

double TowerSystem::fireInterval(std::uint8_t level) {
    const double levelBonus = static_cast<double>(level - 1);
    return BaseFireInterval / (1.0 + 0.05 * levelBonus);
}

void TowerSystem::reset() {
    towers_.clear();
    shotBuffer_.clear();
}

void TowerSystem::setSkillModifiers(
    double damage, double range, double fireRate,
    double highGroundDamage) {
    damageMultiplier_ = std::max(0.05, damage);
    rangeMultiplier_ = std::max(0.05, range);
    fireRateMultiplier_ = std::max(0.05, fireRate);
    highGroundDamageMultiplier_ = std::max(1.0, highGroundDamage);
}

void TowerSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    std::erase_if(towers_, [&buildings](const TowerRuntime& tower) {
        return std::none_of(buildings.begin(), buildings.end(),
                            [&tower](const BuildingInstance& building) {
                                return building.id == tower.buildingId &&
                                       building.type == BuildingType::Turret;
                            });
    });

    for (const auto& building : buildings) {
        if (building.type != BuildingType::Turret) {
            continue;
        }
        const bool exists =
            std::any_of(towers_.begin(), towers_.end(), [&building](const TowerRuntime& tower) {
                return tower.buildingId == building.id;
            });
        if (!exists) {
            constexpr double QuarterTurn = 1.57079632679489661923;
            towers_.push_back({
                .buildingId = building.id,
                .yaw = static_cast<double>(building.rotation) * QuarterTurn,
            });
        }
    }
}

std::span<const TowerShot> TowerSystem::tick(double deltaSeconds,
                                             const std::vector<BuildingInstance>& buildings,
                                             EnemySystem& enemies) {
    shotBuffer_.clear();
    for (auto& tower : towers_) {
        const auto building =
            std::find_if(buildings.begin(), buildings.end(), [&tower](const BuildingInstance& item) {
                return item.id == tower.buildingId;
            });
        if (building == buildings.end()) {
            continue;
        }

        tower.fireCooldownRemaining = std::max(0.0, tower.fireCooldownRemaining - deltaSeconds);
        tower.targetSearchCooldownRemaining =
            std::max(0.0, tower.targetSearchCooldownRemaining - deltaSeconds);
        const Vec3 origin = towerPosition(*building);
        const double range = attackRange(building->level) *
            rangeMultiplier_;
        const double towerBonus = building->anvilStacks > 0
            ? 1.0 + 0.10 * building->anvilStacks
            : building->anvilEnhanced ? 1.10 : 1.0;
        const double heightBonus = building->platformStorey > 0
            ? highGroundDamageMultiplier_ : 1.0;
        const double damage = attackDamage(building->level) *
            towerBonus * damageMultiplier_ * heightBonus;
        const double shotInterval = fireInterval(building->level) /
            fireRateMultiplier_;

        if (tower.targetId) {
            const auto target = enemies.enemy(*tower.targetId);
            if (!target || !withinRange(origin, target->position, range)) {
                tower.targetId.reset();
            }
        }

        if (!tower.targetId && tower.targetSearchCooldownRemaining <= 0.0) {
            tower.targetId = enemies.nearestEnemy(origin, range);
            tower.targetSearchCooldownRemaining = SearchInterval;
        }
        if (!tower.targetId) {
            continue;
        }

        const auto target = enemies.enemy(*tower.targetId);
        if (!target) {
            tower.targetId.reset();
            continue;
        }
        const double deltaX = target->position.x - origin.x;
        const double deltaZ = target->position.z - origin.z;
        const double desiredYaw = std::atan2(-deltaX, -deltaZ);
        const double yawDelta = wrapAngle(desiredYaw - tower.yaw);
        tower.yaw +=
            std::clamp(yawDelta, -TurnSpeed * deltaSeconds,
                       TurnSpeed * deltaSeconds);
        tower.yaw = wrapAngle(tower.yaw);
        if (std::abs(wrapAngle(desiredYaw - tower.yaw)) >
                AimTolerance ||
            tower.fireCooldownRemaining > 0.0) {
            continue;
        }
        const auto damageResult = enemies.damage(target->id, damage);
        if (!damageResult) {
            tower.targetId.reset();
            continue;
        }

        shotBuffer_.push_back({
            .towerId = tower.buildingId,
            .targetId = target->id,
            .origin = origin,
            .hitPosition = target->position,
            .killed = damageResult->killed,
        });
        tower.fireCooldownRemaining = shotInterval;
        if (damageResult->killed) {
            tower.targetId.reset();
        }
    }
    return shotBuffer_;
}

const std::vector<TowerRuntime>& TowerSystem::towers() const {
    return towers_;
}

} // namespace ian
