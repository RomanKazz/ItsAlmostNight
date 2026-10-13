#include "combat/TowerSystem.hpp"
#include "combat/DefenseTargeting.hpp"
#include "combat/DefenseRuntimeSupport.hpp"
#include "combat/DefenseAiming.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/BuildingOrientation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double BaseRange = 9.0;
constexpr double CrossbowBaseFireInterval = 1.0;
constexpr double SearchInterval = 0.25;
constexpr double BaseDamage = 1.2;
constexpr double GunBaseDamage = 1.08;
constexpr double TurnSpeed = 12.0;
constexpr double PitchSpeed = 8.0;
constexpr double AimTolerance = 0.08726646259971647;
constexpr double CrossbowPiercingRadius = 0.72;
constexpr double GunPiercingRadius = 0.42;
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
    const double base = type == BuildingType::GunTurret
        ? GunBaseDamage : BaseDamage;
    return base + (type == BuildingType::GunTurret ? 0.08 : 0.2) *
        static_cast<double>(level - 1);
}

double fireInterval(BuildingType type, std::uint8_t level) {
    const double base = type == BuildingType::GunTurret
        ? 0.38 : CrossbowBaseFireInterval;
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

bool withinRange(Vec3 origin, Vec3 target, double range) {
    const double deltaX = target.x - origin.x;
    const double deltaZ = target.z - origin.z;
    return (deltaX * deltaX) + (deltaZ * deltaZ) <= range * range;
}

std::size_t piercingTargets(
    const EnemySystem& enemies, BuildingType towerType,
    double towerSurfaceHeight, Vec3 origin, Vec3 aimPosition,
    double range, double piercingRadius, int maximumTargets,
    std::array<PiercingCandidate, MaximumCrossbowTargets>& result) {
    const double directionX = aimPosition.x - origin.x;
    const double directionY = towerType == BuildingType::GunTurret
        ? 0.0 : aimPosition.y - origin.y;
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
        if (!enemy.active || !hitscanDefenseCanTarget(
                towerType, enemy, towerSurfaceHeight)) {
            continue;
        }
        const Vec3 candidatePosition = enemyAimPosition(enemy);
        if (!withinRange(origin, candidatePosition, range)) {
            continue;
        }
        const double relativeX = candidatePosition.x - origin.x;
        const double relativeY = towerType == BuildingType::GunTurret
            ? 0.0 : candidatePosition.y - origin.y;
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
            piercingRadius * piercingRadius) {
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
    setTypeBonuses(BuildingType::Core, 1.0, 1.0);
    towers_.clear();
    shotBuffer_.clear();
}

void TowerSystem::setTypeBonuses(BuildingType type, double damage, double fireRate) {
    bonusType_ = type;
    typeDamageMultiplier_ = std::max(0.05, damage);
    typeFireRateMultiplier_ = std::max(0.05, fireRate);
}

void TowerSystem::setModifiers(
    double damage, double range, double fireRate,
    double highGroundDamage, int extraTargetCount,
    int gunPiercingCount) {
    damageMultiplier_ = std::max(0.05, damage);
    rangeMultiplier_ = std::max(0.05, range);
    fireRateMultiplier_ = std::max(0.05, fireRate);
    highGroundDamageMultiplier_ = std::max(1.0, highGroundDamage);
    extraTargetCount_ = std::clamp(extraTargetCount, 0, 3);
    gunPiercingCount_ = std::clamp(gunPiercingCount, 0, 3);
}

void TowerSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    defense_runtime::syncBuildings(towers_, buildings, SearchInterval,
        isHitscanTower,
        [](const BuildingInstance&, const TowerRuntime&) { return true; });
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

        tower.previousYaw = tower.yaw;
        tower.previousPitch = tower.pitch;
        tower.fireCooldownRemaining = std::max(0.0, tower.fireCooldownRemaining - deltaSeconds);
        tower.targetSearchCooldownRemaining =
            std::max(0.0, tower.targetSearchCooldownRemaining - deltaSeconds);
        const bool crossbow = building->type == BuildingType::Turret;
        const double aimResponse = crossbow ? 10.0 : 12.0;
        const double aimTurnSpeed = crossbow ? 4.0 : TurnSpeed;
        const double aimPitchSpeed = crossbow ? 2.8 : PitchSpeed;
        const Vec3 origin = towerPosition(*building);
        const double range = attackRange(building->type, building->level) *
            rangeMultiplier_;
        const double towerBonus = buildingAnvilMultiplier(*building);
        const double heightBonus = building->platformStorey > 0
            ? highGroundDamageMultiplier_ : 1.0;
        const double damage = attackDamage(building->type, building->level) *
            towerBonus * damageMultiplier_ * heightBonus *
            (building->type == bonusType_ ? typeDamageMultiplier_ : 1.0);
        const double shotInterval = fireInterval(building->type, building->level) /
            (fireRateMultiplier_ * (building->type == bonusType_ ? typeFireRateMultiplier_ : 1.0));
        tower.baseYaw = smoothBuildingAngle(
            tower.baseYaw, tower.restYaw, deltaSeconds);

        if (tower.targetId) {
            const auto target = enemies.enemy(*tower.targetId);
            if (!target || !hitscanDefenseCanTarget(
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
            defense_runtime::holdAimWithoutTarget(tower, deltaSeconds);
            continue;
        }

        const auto target = enemies.enemy(*tower.targetId);
        if (!target) {
            tower.targetId.reset();
            continue;
        }
        const Vec3 aimPosition = enemyAimPosition(*target);
        tower.manualTurnPending = false;
        const double deltaX = aimPosition.x - origin.x;
        const double deltaY = aimPosition.y - origin.y;
        const double deltaZ = aimPosition.z - origin.z;
        const double desiredYaw = std::atan2(-deltaX, -deltaZ);
        const double desiredPitch = building->type == BuildingType::Turret
            ? std::atan2(deltaY, std::hypot(deltaX, deltaZ))
            : 0.0;
        tower.yaw = advanceDefenseAim(
            tower.yaw, desiredYaw, tower.yawVelocity,
            deltaSeconds, aimResponse, aimTurnSpeed, true);
        tower.pitch = advanceDefenseAim(
            tower.pitch, desiredPitch, tower.pitchVelocity,
            deltaSeconds, 10.0, aimPitchSpeed, false);
        if (std::abs(wrapBuildingAngle(
                desiredYaw - tower.yaw)) >
                AimTolerance ||
            std::abs(desiredPitch - tower.pitch) > AimTolerance ||
            tower.fireCooldownRemaining > 0.0) {
            continue;
        }
        std::array<PiercingCandidate, MaximumCrossbowTargets>
            piercingCandidates{};
        const bool piercing = building->type == BuildingType::Turret ||
            (building->type == BuildingType::GunTurret &&
             gunPiercingCount_ > 0);
        const std::size_t targetCount = piercing
            ? piercingTargets(
                  enemies, building->type, building->baseHeight,
                  origin, aimPosition, range,
                  building->type == BuildingType::Turret
                      ? CrossbowPiercingRadius : GunPiercingRadius,
                  1 + (building->type == BuildingType::Turret
                      ? piercingCount(building->type, building->level)
                      : gunPiercingCount_),
                  piercingCandidates)
            : 1;
        if (!piercing) {
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
                .payloadDamage = damage,
                .muzzleIndex = tower.nextMuzzle,
                .secondaryImpact = index + 1 < targetCount,
                .killed = damageResult->killed,
            });
        }
        std::array<const EnemyInstance*, 3> extraTargets{};
        std::array<double, 3> extraDistances{
            range * range, range * range, range * range};
        for (const EnemyInstance& enemy : enemies.enemies()) {
            if (!enemy.active || !hitscanDefenseCanTarget(
                    building->type, enemy,
                    building->baseHeight) ||
                !directionInsideDefenseArc(
                    origin, enemy.position, tower.restYaw,
                    building->level) ||
                std::ranges::any_of(
                    std::span{piercingCandidates}.first(targetCount),
                    [&enemy](const PiercingCandidate& candidate) {
                        return candidate.id == enemy.id;
                    })) {
                continue;
            }
            const double extraDeltaX = enemy.position.x - origin.x;
            const double extraDeltaZ = enemy.position.z - origin.z;
            const double distanceSquared =
                extraDeltaX * extraDeltaX +
                extraDeltaZ * extraDeltaZ;
            const auto candidateDistances =
                std::span{extraDistances}.first(
                    static_cast<std::size_t>(extraTargetCount_));
            const auto insertion = std::ranges::find_if(
                candidateDistances,
                [distanceSquared](double candidate) {
                    return distanceSquared < candidate;
                });
            if (insertion == candidateDistances.end()) {
                continue;
            }
            const std::size_t insertionIndex =
                static_cast<std::size_t>(
                    insertion - candidateDistances.begin());
            for (std::size_t shift =
                     static_cast<std::size_t>(extraTargetCount_ - 1);
                 shift > insertionIndex; --shift) {
                extraDistances[shift] = extraDistances[shift - 1U];
                extraTargets[shift] = extraTargets[shift - 1U];
            }
            extraDistances[insertionIndex] = distanceSquared;
            extraTargets[insertionIndex] = &enemy;
        }
        for (int extraIndex = 0;
             extraIndex < extraTargetCount_; ++extraIndex) {
            const EnemyInstance* extraTarget = extraTargets[
                static_cast<std::size_t>(extraIndex)];
            if (!extraTarget) break;
            const double damageFraction = std::max(
                0.40, 0.65 - 0.10 *
                    static_cast<double>(extraIndex));
            const auto damageResult = enemies.damage(
                extraTarget->id, damage * damageFraction);
            if (!damageResult) continue;
            ++successfulHits;
            shotBuffer_.push_back({
                .towerId = tower.buildingId,
                .targetId = damageResult->id,
                .targetType = damageResult->type,
                .targetEliteAffixes =
                    damageResult->eliteAffixes,
                .origin = origin,
                .hitPosition = enemyAimPosition(*extraTarget),
                .type = building->type,
                .damage = damageResult->damage,
                .payloadDamage = damage * damageFraction,
                .muzzleIndex = static_cast<std::uint8_t>(
                    tower.nextMuzzle + 1U +
                    static_cast<std::uint8_t>(extraIndex)),
                .secondaryImpact = false,
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
