#include "combat/TowerSystem.hpp"
#include "buildings/BuildingOrientation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double BaseRange = 9.0;
constexpr double CrossbowBaseFireInterval = 1.25;
constexpr double SearchInterval = 0.25;
constexpr double BaseDamage = 1.0;
constexpr double TurnSpeed = 12.0;
constexpr double PitchSpeed = 8.0;
constexpr double AimTolerance = 0.08726646259971647;
constexpr double CrossbowPiercingRadius = 0.72;
// The gun turret has no pitch pivot. A small tolerance keeps it useful on
// rolling terrain, while preventing a turret on a modular floor from firing
// down through an entire storey.
constexpr double GunTurretMaximumSurfaceHeightDifference = 1.25;
constexpr std::size_t MaximumCrossbowTargets = 6;

struct PiercingCandidate {
    EntityId id;
    Vec3 hitPosition;
    double distanceAlongRay{};
};

bool isHitscanTower(BuildingType type) {
    return type == BuildingType::Turret ||
           type == BuildingType::GunTurret;
}

double attackRange(BuildingType type, std::uint8_t level) {
    const double base = type == BuildingType::GunTurret ? 10.0 : BaseRange;
    return base + 0.3 * static_cast<double>(level - 1);
}

double attackDamage(BuildingType type, std::uint8_t level) {
    const double base = type == BuildingType::GunTurret ? 0.42 : BaseDamage;
    return base + (type == BuildingType::GunTurret ? 0.08 : 0.2) *
        static_cast<double>(level - 1);
}

double fireInterval(BuildingType type, std::uint8_t level) {
    const double base = type == BuildingType::GunTurret
        ? 0.18 : CrossbowBaseFireInterval;
    const double levelRateBonus = type == BuildingType::GunTurret
        ? 0.05 : 0.03;
    return base / (1.0 + levelRateBonus *
        static_cast<double>(level - 1));
}

int piercingCount(BuildingType type, std::uint8_t level) {
    if (type != BuildingType::Turret) {
        return 0;
    }
    return 2 + (std::max<int>(1, level) - 1) / 2;
}

Vec3 towerPosition(const BuildingInstance& building) {
    Vec3 position = buildingWorldPosition(building);
    position.y += building.type == BuildingType::Turret ? 0.746495 : 1.4;
    return position;
}

Vec3 enemyAimPosition(const EnemyInstance& enemy) {
    const double height = [&enemy] {
        switch (enemy.type) {
        case EnemyType::Flying: return 1.72;
        case EnemyType::Boss: return 1.25;
        case EnemyType::Heavy: return 0.86;
        case EnemyType::Splitter: return 0.90;
        case EnemyType::Ranged: return 0.70;
        case EnemyType::Sapper: return 0.62;
        case EnemyType::Basic: return 0.56;
        case EnemyType::Fast: return 0.48;
        case EnemyType::Splitling: return 0.44;
        }
        return 0.56;
    }();
    return {
        enemy.position.x,
        enemy.worldSurfaceHeight + height,
        enemy.position.z,
    };
}

bool towerCanTarget(BuildingType towerType,
                    const EnemyInstance& enemy,
                    double towerSurfaceHeight) {
    if (towerType != BuildingType::GunTurret) {
        return true;
    }
    return enemy.type != EnemyType::Flying &&
           std::abs(enemy.worldSurfaceHeight - towerSurfaceHeight) <=
               GunTurretMaximumSurfaceHeightDifference;
}

bool withinRange(Vec3 origin, Vec3 target, double range) {
    const double deltaX = target.x - origin.x;
    const double deltaZ = target.z - origin.z;
    return (deltaX * deltaX) + (deltaZ * deltaZ) <= range * range;
}

std::size_t crossbowTargets(
    const EnemySystem& enemies, Vec3 origin, Vec3 aimPosition,
    double range, int maximumTargets,
    std::array<PiercingCandidate, MaximumCrossbowTargets>& result) {
    const double directionX = aimPosition.x - origin.x;
    const double directionY = aimPosition.y - origin.y;
    const double directionZ = aimPosition.z - origin.z;
    const double directionLength = std::sqrt(
        directionX * directionX + directionY * directionY +
        directionZ * directionZ);
    if (directionLength <= 1e-6) {
        return 0;
    }
    const Vec3 direction{
        directionX / directionLength,
        directionY / directionLength,
        directionZ / directionLength,
    };
    const std::size_t capacity = std::min<std::size_t>(
        result.size(), static_cast<std::size_t>(
            std::max(1, maximumTargets)));
    std::size_t count = 0;
    for (const EnemyInstance& enemy : enemies.enemies()) {
        if (!enemy.active || !towerCanTarget(
                BuildingType::Turret, enemy, origin.y)) {
            continue;
        }
        const Vec3 candidatePosition = enemyAimPosition(enemy);
        if (!withinRange(origin, candidatePosition, range)) {
            continue;
        }
        const double relativeX = candidatePosition.x - origin.x;
        const double relativeY = candidatePosition.y - origin.y;
        const double relativeZ = candidatePosition.z - origin.z;
        const double distanceAlongRay =
            relativeX * direction.x + relativeY * direction.y +
            relativeZ * direction.z;
        if (distanceAlongRay <= 0.0) {
            continue;
        }
        const double relativeLengthSquared =
            relativeX * relativeX + relativeY * relativeY +
            relativeZ * relativeZ;
        const double distanceFromRaySquared = std::max(
            0.0, relativeLengthSquared -
                distanceAlongRay * distanceAlongRay);
        if (distanceFromRaySquared >
            CrossbowPiercingRadius * CrossbowPiercingRadius) {
            continue;
        }
        const PiercingCandidate candidate{
            .id = enemy.id,
            .hitPosition = candidatePosition,
            .distanceAlongRay = distanceAlongRay,
        };
        const auto insertion = std::lower_bound(
            result.begin(), result.begin() +
                static_cast<std::ptrdiff_t>(count),
            candidate,
            [](const PiercingCandidate& left,
               const PiercingCandidate& right) {
                return left.distanceAlongRay < right.distanceAlongRay;
            });
        const std::size_t insertionIndex =
            static_cast<std::size_t>(insertion - result.begin());
        if (insertionIndex >= capacity) {
            continue;
        }
        const std::size_t shiftedEnd = std::min(count, capacity - 1);
        for (std::size_t index = shiftedEnd;
             index > insertionIndex; --index) {
            result[index] = result[index - 1];
        }
        result[insertionIndex] = candidate;
        count = std::min(count + 1, capacity);
    }
    return count;
}

} // namespace

TowerSystem::TowerSystem() {
    towers_.reserve(64);
    shotBuffer_.reserve(256);
}

double TowerSystem::attackRange(std::uint8_t level) {
    return BaseRange +
           0.3 * static_cast<double>(level - 1);
}

double TowerSystem::attackRange(BuildingType type, std::uint8_t level) {
    return ian::attackRange(type, level);
}

double TowerSystem::attackDamage(std::uint8_t level) {
    return BaseDamage +
           0.2 * static_cast<double>(level - 1);
}

double TowerSystem::attackDamage(BuildingType type, std::uint8_t level) {
    return ian::attackDamage(type, level);
}

double TowerSystem::fireInterval(std::uint8_t level) {
    return ian::fireInterval(BuildingType::Turret, level);
}

double TowerSystem::fireInterval(BuildingType type, std::uint8_t level) {
    return ian::fireInterval(type, level);
}

int TowerSystem::piercingCount(BuildingType type, std::uint8_t level) {
    return ian::piercingCount(type, level);
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
                                       isHitscanTower(building.type);
                            });
    });

    for (const auto& building : buildings) {
        if (!isHitscanTower(building.type)) {
            continue;
        }
        const double restYaw = buildingRotationYaw(
            building.type, building.rotation);
        const auto runtime = std::find_if(
            towers_.begin(), towers_.end(),
            [&building](const TowerRuntime& tower) {
                return tower.buildingId == building.id;
            });
        if (runtime == towers_.end()) {
            towers_.push_back({
                .buildingId = building.id,
                .type = building.type,
                .restYaw = restYaw,
                .baseYaw = restYaw,
                .yaw = restYaw,
            });
        } else if (
            std::abs(wrapBuildingAngle(
                runtime->restYaw - restYaw)) >
            0.0001) {
            runtime->restYaw = restYaw;
            runtime->targetId.reset();
            runtime->targetSearchCooldownRemaining = SearchInterval;
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
        const double range = attackRange(building->type, building->level) *
            rangeMultiplier_;
        const double towerBonus = building->anvilStacks > 0
            ? 1.0 + 0.10 * building->anvilStacks
            : building->anvilEnhanced ? 1.10 : 1.0;
        const double heightBonus = building->platformStorey > 0
            ? highGroundDamageMultiplier_ : 1.0;
        const double damage = attackDamage(building->type, building->level) *
            towerBonus * damageMultiplier_ * heightBonus;
        const double shotInterval = fireInterval(building->type, building->level) /
            fireRateMultiplier_;
        tower.baseYaw = smoothBuildingAngle(
            tower.baseYaw, tower.restYaw, deltaSeconds);

        if (tower.targetId) {
            const auto target = enemies.enemy(*tower.targetId);
            if (!target || !towerCanTarget(
                    building->type, *target, building->baseHeight) ||
                !withinRange(origin, target->position, range) ||
                !directionInsideDefenseArc(
                    origin, target->position, tower.restYaw,
                    building->level)) {
                tower.targetId.reset();
            }
        }

        if (!tower.targetId && tower.targetSearchCooldownRemaining <= 0.0) {
            Vec3 searchOrigin = origin;
            searchOrigin.y = building->baseHeight;
            tower.targetId = enemies.nearestEnemyInArc(
                searchOrigin, range, tower.restYaw,
                defenseAttackHalfAngleRadians(building->level),
                building->type != BuildingType::GunTurret,
                building->type == BuildingType::GunTurret
                    ? GunTurretMaximumSurfaceHeightDifference
                    : std::numeric_limits<double>::infinity());
            tower.targetSearchCooldownRemaining = SearchInterval;
        }
        if (!tower.targetId) {
            tower.yaw = smoothBuildingAngle(
                tower.yaw, tower.restYaw, deltaSeconds);
            tower.pitch += std::clamp(
                -tower.pitch, -PitchSpeed * deltaSeconds,
                PitchSpeed * deltaSeconds);
            continue;
        }

        const auto target = enemies.enemy(*tower.targetId);
        if (!target) {
            tower.targetId.reset();
            continue;
        }
        const Vec3 aimPosition = enemyAimPosition(*target);
        const double deltaX = aimPosition.x - origin.x;
        const double deltaY = aimPosition.y - origin.y;
        const double deltaZ = aimPosition.z - origin.z;
        const double desiredYaw = std::atan2(-deltaX, -deltaZ);
        const double desiredPitch = building->type == BuildingType::Turret
            ? std::atan2(deltaY, std::hypot(deltaX, deltaZ))
            : 0.0;
        const double yawDelta = wrapBuildingAngle(
            desiredYaw - tower.yaw);
        tower.yaw +=
            std::clamp(yawDelta, -TurnSpeed * deltaSeconds,
                       TurnSpeed * deltaSeconds);
        tower.yaw = wrapBuildingAngle(tower.yaw);
        const double pitchDelta = desiredPitch - tower.pitch;
        tower.pitch += std::clamp(
            pitchDelta, -PitchSpeed * deltaSeconds,
            PitchSpeed * deltaSeconds);
        if (std::abs(wrapBuildingAngle(
                desiredYaw - tower.yaw)) >
                AimTolerance ||
            std::abs(desiredPitch - tower.pitch) > AimTolerance ||
            tower.fireCooldownRemaining > 0.0) {
            continue;
        }
        std::array<PiercingCandidate, MaximumCrossbowTargets>
            piercingCandidates{};
        const std::size_t targetCount =
            building->type == BuildingType::Turret
            ? crossbowTargets(
                  enemies, origin, aimPosition, range,
                  1 + piercingCount(building->type, building->level),
                  piercingCandidates)
            : 1;
        if (building->type != BuildingType::Turret) {
            piercingCandidates.front() = {
                .id = target->id,
                .hitPosition = aimPosition,
            };
        }
        bool primaryTargetKilled = false;
        std::size_t successfulHits = 0;
        for (std::size_t index = 0; index < targetCount; ++index) {
            const PiercingCandidate& candidate =
                piercingCandidates[index];
            const auto damageResult = enemies.damage(
                candidate.id, damage);
            if (!damageResult) {
                continue;
            }
            ++successfulHits;
            primaryTargetKilled = primaryTargetKilled ||
                (candidate.id == target->id && damageResult->killed);
            shotBuffer_.push_back({
                .towerId = tower.buildingId,
                .targetId = candidate.id,
                .targetType = damageResult->type,
                .targetEliteAffixes = damageResult->eliteAffixes,
                .origin = origin,
                .hitPosition = candidate.hitPosition,
                .type = building->type,
                .damage = damageResult->damage,
                .muzzleIndex = tower.nextMuzzle,
                .secondaryImpact = index + 1 < targetCount,
                .killed = damageResult->killed,
            });
        }
        if (successfulHits == 0) {
            tower.targetId.reset();
            continue;
        }
        if (building->type == BuildingType::GunTurret) {
            tower.nextMuzzle = static_cast<std::uint8_t>(
                (tower.nextMuzzle + 1U) % 2U);
        }
        tower.fireCooldownRemaining = shotInterval;
        if (primaryTargetKilled) {
            tower.targetId.reset();
        }
    }
    return shotBuffer_;
}

const std::vector<TowerRuntime>& TowerSystem::towers() const {
    return towers_;
}

} // namespace ian
