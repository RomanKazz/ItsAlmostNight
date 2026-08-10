#include "combat/CannonSystem.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr double BaseRange = 12.0;
constexpr double ClusterRadius = 3.0;
constexpr double BaseFireInterval = 3.2;
constexpr double SearchInterval = 0.5;
constexpr double TurnSpeed = 12.0;
constexpr double PitchSpeed = 8.0;
constexpr double AimTolerance = 0.08726646259971647;
constexpr double MinimumPitch = -0.2617993877991494;
constexpr double MaximumPitch = 0.9599310885968813;
constexpr double ProjectileSpeed = 12.0;
constexpr double Gravity = 9.8;
constexpr double BaseExplosionRadius = 2.5;
constexpr double BaseExplosionDamage = 2.5;
constexpr double BaseExplosionImpulse = 3.5;
constexpr std::size_t MaxProjectiles = 300;

Vec3 cannonPosition(const BuildingInstance& building) {
    Vec3 position = buildingWorldPosition(building);
    position.y += 1.5;
    return position;
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

CannonSystem::CannonSystem() {
    cannons_.reserve(64);
    projectiles_.reserve(MaxProjectiles);
    explosionBuffer_.reserve(32);
    shotBuffer_.reserve(32);
}

double CannonSystem::attackRange(std::uint8_t level) {
    return BaseRange +
           0.35 * static_cast<double>(level - 1);
}

double CannonSystem::fireInterval(std::uint8_t level) {
    const double levelBonus = static_cast<double>(level - 1);
    return BaseFireInterval / (1.0 + 0.05 * levelBonus);
}

double CannonSystem::explosionRadius(std::uint8_t level) {
    return BaseExplosionRadius +
           0.12 * static_cast<double>(level - 1);
}

double CannonSystem::explosionDamage(std::uint8_t level) {
    return BaseExplosionDamage +
           0.35 * static_cast<double>(level - 1);
}

void CannonSystem::reset() {
    cannons_.clear();
    projectiles_.clear();
    explosionBuffer_.clear();
    shotBuffer_.clear();
}

void CannonSystem::clearProjectiles() {
    for (auto& projectile : projectiles_) {
        projectile.active = false;
    }
}

void CannonSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    std::erase_if(cannons_, [&buildings](const CannonRuntime& cannon) {
        return std::none_of(buildings.begin(), buildings.end(),
                            [&cannon](const BuildingInstance& building) {
                                return building.id == cannon.buildingId &&
                                       building.type == BuildingType::Cannon;
                            });
    });
    for (const auto& building : buildings) {
        if (building.type != BuildingType::Cannon) {
            continue;
        }
        const bool exists =
            std::any_of(cannons_.begin(), cannons_.end(), [&building](const CannonRuntime& cannon) {
                return cannon.buildingId == building.id;
            });
        if (!exists) {
            constexpr double QuarterTurn = 1.57079632679489661923;
            cannons_.push_back({
                .buildingId = building.id,
                .yaw = static_cast<double>(building.rotation) * QuarterTurn,
            });
        }
    }
}

std::span<const CannonExplosion> CannonSystem::tick(
    double deltaSeconds, const std::vector<BuildingInstance>& buildings, EnemySystem& enemies) {
    explosionBuffer_.clear();
    shotBuffer_.clear();
    for (auto& cannon : cannons_) {
        const auto building =
            std::find_if(buildings.begin(), buildings.end(), [&cannon](const BuildingInstance& item) {
                return item.id == cannon.buildingId;
            });
        if (building == buildings.end()) {
            continue;
        }

        cannon.fireCooldownRemaining = std::max(0.0, cannon.fireCooldownRemaining - deltaSeconds);
        cannon.targetSearchCooldownRemaining =
            std::max(0.0, cannon.targetSearchCooldownRemaining - deltaSeconds);
        const Vec3 origin = cannonPosition(*building);
        const double range = attackRange(building->level);
        const double shotInterval = fireInterval(building->level);
        if (cannon.targetId) {
            const auto target = enemies.enemy(*cannon.targetId);
            if (!target) {
                cannon.targetId.reset();
            } else {
                const double deltaX = target->position.x - origin.x;
                const double deltaZ = target->position.z - origin.z;
                if ((deltaX * deltaX) + (deltaZ * deltaZ) > range * range) {
                    cannon.targetId.reset();
                }
            }
        }
        if (!cannon.targetId && cannon.targetSearchCooldownRemaining <= 0.0) {
            cannon.targetId = enemies.densestEnemy(origin, range, ClusterRadius);
            cannon.targetSearchCooldownRemaining = SearchInterval;
        }

        bool aimedAtTarget = false;
        if (cannon.targetId) {
            const auto target = enemies.enemy(*cannon.targetId);
            if (target) {
                const double deltaX = target->position.x - origin.x;
                const double deltaZ = target->position.z - origin.z;
                const double desiredYaw = std::atan2(-deltaX, -deltaZ);
                const double yawDelta = wrapAngle(desiredYaw - cannon.yaw);
                cannon.yaw +=
                    std::clamp(yawDelta, -TurnSpeed * deltaSeconds,
                               TurnSpeed * deltaSeconds);
                cannon.yaw = wrapAngle(cannon.yaw);

                const double horizontalDistance =
                    std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
                const double flightTime =
                    std::max(0.2, horizontalDistance / ProjectileSpeed);
                const double verticalSpeed =
                    (target->position.y - origin.y +
                     0.5 * Gravity * flightTime * flightTime) /
                    flightTime;
                const double desiredPitch = std::clamp(
                    std::atan2(verticalSpeed, ProjectileSpeed),
                    MinimumPitch, MaximumPitch);
                const double pitchDelta = desiredPitch - cannon.pitch;
                cannon.pitch +=
                    std::clamp(pitchDelta, -PitchSpeed * deltaSeconds,
                               PitchSpeed * deltaSeconds);

                aimedAtTarget =
                    std::abs(wrapAngle(desiredYaw - cannon.yaw)) <=
                        AimTolerance &&
                    std::abs(desiredPitch - cannon.pitch) <= AimTolerance;
            } else {
                cannon.targetId.reset();
            }
        }
        if (aimedAtTarget && cannon.fireCooldownRemaining <= 0.0) {
            const auto target = enemies.enemy(*cannon.targetId);
            if (target) {
                launch(*building, target->position);
                cannon.fireCooldownRemaining = shotInterval;
            }
        }
    }

    for (auto& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }
        projectile.fuseRemaining -= deltaSeconds;
        projectile.velocity.y -= Gravity * deltaSeconds;
        projectile.position.x += projectile.velocity.x * deltaSeconds;
        projectile.position.y += projectile.velocity.y * deltaSeconds;
        projectile.position.z += projectile.velocity.z * deltaSeconds;
        if (projectile.fuseRemaining <= 0.0) {
            projectile.position = projectile.targetPosition;
            explode(projectile, enemies);
        }
    }
    return explosionBuffer_;
}

const std::vector<CannonProjectile>& CannonSystem::projectiles() const {
    return projectiles_;
}

const std::vector<CannonRuntime>& CannonSystem::cannons() const {
    return cannons_;
}

std::span<const CannonShot> CannonSystem::shots() const {
    return shotBuffer_;
}

void CannonSystem::launch(const BuildingInstance& cannon, Vec3 targetPosition) {
    CannonProjectile* projectile = nullptr;
    for (auto& candidate : projectiles_) {
        if (!candidate.active) {
            projectile = &candidate;
            ++projectile->id.generation;
            break;
        }
    }
    if (projectile == nullptr) {
        if (projectiles_.size() >= MaxProjectiles) {
            return;
        }
        projectiles_.push_back({});
        projectile = &projectiles_.back();
        projectile->id = {nextProjectileIndex_++, 1};
    }

    const Vec3 origin = cannonPosition(cannon);
    const double deltaX = targetPosition.x - origin.x;
    const double deltaZ = targetPosition.z - origin.z;
    const double horizontalDistance = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    const double flightTime = std::max(0.2, horizontalDistance / ProjectileSpeed);
    projectile->cannonId = cannon.id;
    projectile->position = origin;
    projectile->targetPosition = targetPosition;
    projectile->velocity = {
        deltaX / flightTime,
        (targetPosition.y - origin.y + 0.5 * Gravity * flightTime * flightTime) / flightTime,
        deltaZ / flightTime,
    };
    projectile->fuseRemaining = flightTime;
    const double levelBonus = static_cast<double>(cannon.level - 1);
    projectile->explosionRadius = explosionRadius(cannon.level);
    const double towerBonus = cannon.anvilStacks > 0
        ? 1.0 + 0.10 * cannon.anvilStacks
        : cannon.anvilEnhanced ? 1.10 : 1.0;
    projectile->explosionDamage = explosionDamage(cannon.level) *
        towerBonus;
    projectile->explosionImpulse =
        BaseExplosionImpulse + 0.35 * levelBonus;
    projectile->active = true;
    shotBuffer_.push_back({
        .cannonId = cannon.id,
        .projectileId = projectile->id,
        .position = origin,
    });
}

void CannonSystem::explode(CannonProjectile& projectile, EnemySystem& enemies) {
    const auto damage = enemies.damageInRadius(projectile.position, projectile.explosionRadius,
                                               projectile.explosionDamage,
                                               projectile.explosionImpulse);
    int killedCount = 0;
    for (const auto& result : damage) {
        if (result.killed) {
            ++killedCount;
        }
    }
    explosionBuffer_.push_back({
        .projectileId = projectile.id,
        .position = projectile.position,
        .radius = projectile.explosionRadius,
        .hitCount = static_cast<int>(damage.size()),
        .killedCount = killedCount,
    });
    projectile.active = false;
}

} // namespace ian
