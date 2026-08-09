#include "enemies/EnemySystem.hpp"

#include "world/TerrainHeightfield.hpp"
#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <limits>

namespace ian {
namespace {

constexpr double EnemyRadius = 0.4;
constexpr double AttackRange = 0.55;
constexpr double PlayerAttackRange = 1.0;
constexpr double AttackInterval = 1.0;
constexpr double BuildingLookAhead = 2.5;
constexpr double SapperStructureSearchRadius = 6.0;
constexpr double SeparationRadius = 1.1;
constexpr double SeparationWeight = 0.65;
constexpr double Pi = 3.14159265358979323846;
constexpr double BuildingGridCellSize = 2.0;
constexpr double BuildingGridMinimum = -192.0;
constexpr int BuildingGridSize = 192;
constexpr int BuildingGridCellCount =
    BuildingGridSize * BuildingGridSize;

double aiUpdateInterval(
    const EnemyInstance& enemy,
    std::optional<Vec3> playerPosition,
    Vec3 corePosition) {
    if (enemy.state != EnemyState::MoveToCore &&
        enemy.state != EnemyState::ChasePlayer) {
        return 0.0;
    }
    double distanceSquared =
        (enemy.position.x - corePosition.x) *
            (enemy.position.x - corePosition.x) +
        (enemy.position.z - corePosition.z) *
            (enemy.position.z - corePosition.z);
    if (playerPosition) {
        const double playerDistanceSquared =
            (enemy.position.x - playerPosition->x) *
                (enemy.position.x - playerPosition->x) +
            (enemy.position.z - playerPosition->z) *
                (enemy.position.z - playerPosition->z);
        distanceSquared = std::min(
            distanceSquared, playerDistanceSquared);
    }
    constexpr double MediumDistance = 24.0;
    constexpr double FarDistance = 48.0;
    if (distanceSquared > FarDistance * FarDistance) {
        return 1.0 / 15.0;
    }
    if (distanceSquared > MediumDistance * MediumDistance) {
        return 1.0 / 30.0;
    }
    return 0.0;
}

std::size_t statusEffectIndex(StatusEffectType type) {
    return type == StatusEffectType::Freeze ? 0U : 1U;
}

class BuildingQueryGrid {
  public:
    explicit BuildingQueryGrid(
        std::span<const EnemyStructureTarget> structures,
        std::vector<int>& next,
        std::vector<int>& heads,
        bool rebuild)
        : structures_(structures),
          next_(next), heads_(heads) {
        if (!rebuild &&
            heads_.size() == BuildingGridCellCount &&
            next_.size() == structures.size()) {
            return;
        }
        next_.assign(structures.size(), -1);
        heads_.assign(BuildingGridCellCount, -1);
        for (std::size_t index = 0;
             index < structures.size(); ++index) {
            const Vec3 center =
                structures[index].position;
            const int bucket = cellIndex(
                coordinate(center.x),
                coordinate(center.z));
            next_[index] =
                heads_[static_cast<std::size_t>(bucket)];
            heads_[static_cast<std::size_t>(bucket)] =
                static_cast<int>(index);
        }
    }

    template <typename Callback>
    void forEachNearby(
        Vec3 position, double radius,
        Callback&& callback) const {
        const int minimumX = coordinate(
            position.x - radius);
        const int maximumX = coordinate(
            position.x + radius);
        const int minimumZ = coordinate(
            position.z - radius);
        const int maximumZ = coordinate(
            position.z + radius);
        for (int z = minimumZ; z <= maximumZ; ++z) {
            for (int x = minimumX; x <= maximumX; ++x) {
                int index = heads_[
                    static_cast<std::size_t>(
                        cellIndex(x, z))];
                while (index >= 0) {
                    callback(structures_[
                        static_cast<std::size_t>(index)]);
                    index = next_[
                        static_cast<std::size_t>(index)];
                }
            }
        }
    }

  private:
    [[nodiscard]] static int coordinate(double value) {
        if (!std::isfinite(value)) {
            return value < 0.0 ? 0 : BuildingGridSize - 1;
        }
        const double scaled =
            (value - BuildingGridMinimum) /
            BuildingGridCellSize;
        if (scaled <= 0.0) {
            return 0;
        }
        if (scaled >= static_cast<double>(BuildingGridSize - 1)) {
            return BuildingGridSize - 1;
        }
        return std::clamp(
            static_cast<int>(std::floor(scaled)),
            0, BuildingGridSize - 1);
    }

    [[nodiscard]] static int cellIndex(int x, int z) {
        return z * BuildingGridSize + x;
    }

    std::span<const EnemyStructureTarget> structures_;
    std::vector<int>& next_;
    std::vector<int>& heads_;
};

bool sameStructureLayout(
    std::span<const EnemyStructureTarget> left,
    std::span<const EnemyStructureTarget> right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const EnemyStructureTarget& a = left[index];
        const EnemyStructureTarget& b = right[index];
        if (a.id != b.id || a.position.x != b.position.x ||
            a.position.y != b.position.y ||
            a.position.z != b.position.z ||
            a.radius != b.radius ||
            a.buildingType != b.buildingType ||
            a.modular != b.modular ||
            a.structuralImpact != b.structuralImpact) {
            return false;
        }
    }
    return true;
}

double hashUnit(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return static_cast<double>(value) /
           static_cast<double>(
               std::numeric_limits<std::uint32_t>::max());
}

void turnToward(
    EnemyInstance& enemy, double targetYaw,
    double deltaSeconds) {
    const double difference = std::atan2(
        std::sin(targetYaw - enemy.yaw),
        std::cos(targetYaw - enemy.yaw));
    const double maximumTurn =
        enemy.turnRate * deltaSeconds;
    enemy.yaw += std::clamp(
        difference, -maximumTurn, maximumTurn);
    enemy.yaw = std::atan2(
        std::sin(enemy.yaw), std::cos(enemy.yaw));
}

double wanderStrength(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 0.2;
    case EnemyType::Flying:
        return 0.24;
    case EnemyType::Heavy:
        return 0.08;
    case EnemyType::Boss:
        return 0.045;
    case EnemyType::Ranged:
        return 0.13;
    case EnemyType::Sapper:
        return 0.16;
    case EnemyType::Basic:
        return 0.14;
    }
    return 0.14;
}

double enemyRadius(EnemyType type) {
    return type == EnemyType::Boss ? 1.0 : EnemyRadius;
}

double attackRange(EnemyType type) {
    return type == EnemyType::Ranged ? 4.5 : AttackRange;
}

double playerAttackRange(EnemyType type) {
    return type == EnemyType::Ranged ? 4.5 : PlayerAttackRange;
}

double buildingRadius(BuildingType type);

double playerAggroRange(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 5.5;
    case EnemyType::Flying:
        return 6.0;
    case EnemyType::Ranged:
        return 6.0;
    case EnemyType::Boss:
        return 5.0;
    case EnemyType::Heavy:
        return 4.0;
    case EnemyType::Sapper:
        return 3.5;
    case EnemyType::Basic:
        return 4.5;
    }
    return 4.5;
}

bool structureIsVerticallyReachable(
    const EnemyInstance& enemy,
    const EnemyStructureTarget& structure) {
    if (enemy.type == EnemyType::Flying) {
        return structure.buildingType ==
               BuildingType::Core;
    }
    const double maximumAttackHeight =
        maximumGroundStructureInteractionHeight(
            enemy.type, enemy.position.y);
    return structure.position.y <= maximumAttackHeight;
}

bool buildingIsInAttackRange(
    const EnemyInstance& enemy,
    const BuildingQueryGrid& buildingGrid) {
    bool found = false;
    const double searchRadius =
        attackRange(enemy.type) +
        enemyRadius(enemy.type) + 1.6;
    buildingGrid.forEachNearby(
        enemy.position, searchRadius,
        [&enemy, &found](
            const EnemyStructureTarget& building) {
        if (found) {
            return;
        }
        if (!structureIsVerticallyReachable(
                enemy, building)) {
            return;
        }
        const Vec3 center = building.position;
        const double offsetX = center.x - enemy.position.x;
        const double offsetZ = center.z - enemy.position.z;
        const double contactDistance =
            std::sqrt(offsetX * offsetX + offsetZ * offsetZ) -
            building.radius -
            enemyRadius(enemy.type);
        if (contactDistance <= attackRange(enemy.type)) {
            found = true;
        }
    });
    return found;
}

bool buildingBlocksPathToPlayer(
    const EnemyInstance& enemy,
    const BuildingQueryGrid& buildingGrid,
    Vec3 playerPosition) {
    if (enemy.type == EnemyType::Flying) {
        return false;
    }

    const double pathX = playerPosition.x - enemy.position.x;
    const double pathZ = playerPosition.z - enemy.position.z;
    const double pathLengthSquared =
        pathX * pathX + pathZ * pathZ;
    if (pathLengthSquared <= 1e-9) {
        return false;
    }

    bool blocked = false;
    const Vec3 midpoint{
        (enemy.position.x + playerPosition.x) * 0.5,
        enemy.position.y,
        (enemy.position.z + playerPosition.z) * 0.5,
    };
    const double searchRadius =
        std::sqrt(pathLengthSquared) * 0.5 + 1.6;
    buildingGrid.forEachNearby(
        midpoint, searchRadius,
        [&blocked, &enemy, pathX, pathZ,
         pathLengthSquared](
            const EnemyStructureTarget& building) {
        if (blocked) {
            return;
        }
        if (building.buildingType == BuildingType::Core) {
            return;
        }
        if (!structureIsVerticallyReachable(
                enemy, building)) {
            return;
        }
        const Vec3 center = building.position;
        const double offsetX = center.x - enemy.position.x;
        const double offsetZ = center.z - enemy.position.z;
        const double progress = std::clamp(
            (offsetX * pathX + offsetZ * pathZ) /
                pathLengthSquared,
            0.0, 1.0);
        if (progress <= 0.0 || progress >= 1.0) {
            return;
        }
        const double nearestX =
            enemy.position.x + pathX * progress;
        const double nearestZ =
            enemy.position.z + pathZ * progress;
        const double distanceX = center.x - nearestX;
        const double distanceZ = center.z - nearestZ;
        const double collisionRadius =
            building.radius +
            enemyRadius(enemy.type);
        if (distanceX * distanceX + distanceZ * distanceZ <=
            collisionRadius * collisionRadius) {
            blocked = true;
        }
    });
    return blocked;
}

double attackInterval(EnemyType type) {
    if (type == EnemyType::Ranged) {
        return 1.45;
    }
    if (type == EnemyType::Sapper) {
        return 1.2;
    }
    return AttackInterval;
}

double buildingDamage(const EnemyInstance& enemy,
                      BuildingType targetType) {
    if (enemy.type == EnemyType::Sapper &&
        (targetType == BuildingType::Wall ||
         targetType == BuildingType::Gate)) {
        return enemy.damage * 2.5;
    }
    return enemy.damage;
}

double knockbackMultiplier(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
        return 1.0;
    case EnemyType::Fast:
        return 1.15;
    case EnemyType::Heavy:
        return 0.3;
    case EnemyType::Boss:
        return 0.15;
    case EnemyType::Ranged:
        return 0.85;
    case EnemyType::Sapper:
        return 0.55;
    case EnemyType::Flying:
        return 0.7;
    }
    return 1.0;
}

double buildingRadius(BuildingType type) {
    if (type == BuildingType::Core) {
        return 1.6;
    }
    return buildingFootprintHalfExtent(type) == 1.0
               ? 1.1
               : 0.55;
}

double dot(Vec3 left, Vec3 right) {
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

Vec3 subtract(Vec3 left, Vec3 right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

std::optional<double> raySphereDistance(Vec3 origin, Vec3 direction, Vec3 center, double radius) {
    const Vec3 offset = subtract(origin, center);
    const double halfB = dot(offset, direction);
    const double c = dot(offset, offset) - (radius * radius);
    const double discriminant = (halfB * halfB) - c;
    if (discriminant < 0.0) {
        return std::nullopt;
    }

    const double root = std::sqrt(discriminant);
    const double nearDistance = -halfB - root;
    if (nearDistance >= 0.0) {
        return nearDistance;
    }
    const double farDistance = -halfB + root;
    return farDistance >= 0.0 ? std::optional<double>{farDistance} : std::nullopt;
}

} // namespace

EnemySystem::EnemySystem(
    std::array<EnemyDefinition, GameBalance::EnemyTypeCount> definitions)
    : definitions_(definitions) {
    enemies_.reserve(MaxEnemies);
    attackBuffer_.reserve(MaxEnemies);
    playerAttackBuffer_.reserve(MaxEnemies);
    areaDamageBuffer_.reserve(MaxEnemies);
    statusTargetBuffer_.reserve(MaxEnemies);
    structureBuffer_.reserve(256);
    incomingStructureBuffer_.reserve(256);
    structureNextBuffer_.reserve(256);
    structureGridHeads_.reserve(BuildingGridCellCount);
    collisionEnemyLinks_.reserve(MaxEnemies);
    areaTargetBuffer_.resize(SpatialHash::MaxEntries);
}

void EnemySystem::reset() {
    for (EnemyInstance& enemy : enemies_) {
        enemy.active = false;
        enemy.state = EnemyState::Dead;
        enemy.target.reset();
    }
    attackBuffer_.clear();
    playerAttackBuffer_.clear();
    performanceStats_.fullAiUpdates = 0U;
    performanceStats_.throttledAiMoves = 0U;
    areaDamageBuffer_.clear();
    statusTargetBuffer_.clear();
    structureBuffer_.clear();
    incomingStructureBuffer_.clear();
    structureNextBuffer_.clear();
    structureGridHeads_.clear();
    collisionEnemyLinks_.clear();
    activeCount_ = 0;
    spatialHash_.clear();
    performanceStats_ = {};
    profilingTick_ = false;
    spatialHashDirty_ = false;
    spatialRebuildsThisTick_ = 0U;
    spatialRebuildMillisecondsThisTick_ = 0.0;
}

void EnemySystem::spawnWave(std::span<const Vec3> positions) {
    for (EnemyInstance& enemy : enemies_) {
        enemy.active = false;
    }
    activeCount_ = 0;
    for (const Vec3 position : positions) {
        appendEnemy({EnemyType::Basic, position});
    }
    rebuildSpatialIndex();
}

void EnemySystem::spawnWave(std::span<const EnemySpawn> spawns) {
    for (EnemyInstance& enemy : enemies_) {
        enemy.active = false;
    }
    activeCount_ = 0;
    spawnGroup(spawns);
}

void EnemySystem::spawnGroup(std::span<const EnemySpawn> spawns) {
    for (const EnemySpawn& spawn : spawns) {
        appendEnemy(spawn);
    }
    rebuildSpatialIndex();
}

std::span<const EnemyAttack> EnemySystem::tick(
    double deltaSeconds, const std::vector<BuildingInstance>& buildings,
    const FlowField& flowField, std::optional<Vec3> playerPosition,
    std::span<const EnemyStructureTarget> additionalStructures,
    const TerrainHeightfield* terrain) {
    const auto tickStart = PerformanceClock::now();
    spatialRebuildsThisTick_ = 0U;
    spatialRebuildMillisecondsThisTick_ = 0.0;
    profilingTick_ = true;
    const auto finishTelemetry = [this, tickStart]() {
        profilingTick_ = false;
        performanceStats_.tick.sample(
            performanceMilliseconds(tickStart));
        performanceStats_.spatialRebuild.sample(
            spatialRebuildMillisecondsThisTick_);
        performanceStats_.activeEnemies = activeCount_;
        performanceStats_.spatialRebuilds =
            spatialRebuildsThisTick_;
    };
    attackBuffer_.clear();
    playerAttackBuffer_.clear();
    performanceStats_.fullAiUpdates = 0U;
    performanceStats_.throttledAiMoves = 0U;
    const auto core =
        std::find_if(buildings.begin(), buildings.end(), [](const BuildingInstance& building) {
            return building.type == BuildingType::Core;
        });
    if (core == buildings.end()) {
        finishTelemetry();
        return attackBuffer_;
    }
    const Vec3 coreWorldPosition =
        buildingWorldPosition(*core);

    incomingStructureBuffer_.clear();
    incomingStructureBuffer_.reserve(
        buildings.size() + additionalStructures.size());
    for (const BuildingInstance& building : buildings) {
        if (!buildingBlocksMovement(building)) {
            continue;
        }
        Vec3 attackPosition =
            buildingWorldPosition(building);
        if (building.platformStorey < 0) {
            attackPosition.y = std::min(
                building.baseHeight,
                building.foundationBottomHeight);
        }
        incomingStructureBuffer_.push_back({
            .id = building.id,
            .position = attackPosition,
            .radius = buildingRadius(building.type),
            .buildingType = building.type,
            .modular = false,
            .structuralImpact = 0U,
        });
    }
    incomingStructureBuffer_.insert(
        incomingStructureBuffer_.end(), additionalStructures.begin(),
        additionalStructures.end());

    const bool structureLayoutChanged = !sameStructureLayout(
        structureBuffer_, incomingStructureBuffer_);
    performanceStats_.structureGridRebuilds =
        structureLayoutChanged ? 1U : 0U;
    if (structureLayoutChanged) {
        structureBuffer_.swap(incomingStructureBuffer_);
    }

    if (spatialHashDirty_) {
        rebuildSpatialIndex();
    }
    const BuildingQueryGrid buildingGrid(
        structureBuffer_, structureNextBuffer_,
        structureGridHeads_, structureLayoutChanged);

    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }

        enemy.attackCooldownRemaining =
            std::max(0.0, enemy.attackCooldownRemaining - deltaSeconds);
        enemy.hitAnimationRemaining =
            std::max(
                0.0,
                enemy.hitAnimationRemaining - deltaSeconds);
        enemy.ramCooldownRemaining =
            std::max(0.0, enemy.ramCooldownRemaining - deltaSeconds);
        enemy.aiUpdateRemaining = std::max(
            0.0, enemy.aiUpdateRemaining - deltaSeconds);
        enemy.slowRemaining = std::max(0.0, enemy.slowRemaining - deltaSeconds);
        if (enemy.slowRemaining <= 0.0) {
            enemy.movementMultiplier = 1.0;
        }
        for (EnemyStatusEffect& status : enemy.statusEffects) {
            status.immunityRemaining = std::max(
                0.0, status.immunityRemaining - deltaSeconds);
            if (status.remaining > 0.0) {
                status.remaining = std::max(
                    0.0, status.remaining - deltaSeconds);
                status.visualParameter = 1.0;
                if (status.remaining <= 0.0) {
                    status.visualParameter = 0.92;
                }
            } else {
                status.visualParameter = std::max(
                    0.0, status.visualParameter - deltaSeconds * 4.5);
            }
        }
        if (enemyHasStatus(enemy, StatusEffectType::Freeze)) {
            enemy.knockbackVelocity = {};
            continue;
        }
        enemy.steeringTime += deltaSeconds;
        const double waterMultiplier =
            terrain != nullptr && enemy.type != EnemyType::Flying
                ? terrain->waterMovementMultiplier(
                      enemy.position.x, enemy.position.z)
                : 1.0;
        const double movementSpeed =
            enemy.speed * enemy.movementMultiplier * waterMultiplier;
        const double knockbackSpeed = std::hypot(
            enemy.knockbackVelocity.x,
            enemy.knockbackVelocity.z);
        enemy.position.x += enemy.knockbackVelocity.x * deltaSeconds;
        enemy.position.z += enemy.knockbackVelocity.z * deltaSeconds;
        const double knockbackDecay = std::max(0.0, 1.0 - 5.0 * deltaSeconds);
        enemy.knockbackVelocity.x *= knockbackDecay;
        enemy.knockbackVelocity.z *= knockbackDecay;

        // Knockback owns movement briefly. Otherwise chase movement in this
        // same tick can immediately cancel a melee impulse toward the player.
        constexpr double MinimumKnockbackSpeed = 0.05;
        if (knockbackSpeed > MinimumKnockbackSpeed) {
            continue;
        }

        const double aiInterval = aiUpdateInterval(
            enemy, playerPosition, coreWorldPosition);
        if (aiInterval > 0.0 &&
            enemy.aiUpdateRemaining > 0.0) {
            enemy.position.x +=
                std::sin(enemy.yaw) *
                movementSpeed * deltaSeconds;
            enemy.position.z +=
                std::cos(enemy.yaw) *
                movementSpeed * deltaSeconds;
            ++performanceStats_.throttledAiMoves;
            continue;
        }
        enemy.aiUpdateRemaining = aiInterval;
        ++performanceStats_.fullAiUpdates;
        const double aiTurnDeltaSeconds =
            std::max(deltaSeconds, aiInterval);

        if (enemy.state == EnemyState::BossRamWindup) {
            const auto target =
                std::find_if(structureBuffer_.begin(), structureBuffer_.end(),
                             [&enemy](const EnemyStructureTarget& building) {
                                 return enemy.target && building.id == *enemy.target;
                             });
            if (target == structureBuffer_.end()) {
                enemy.state = EnemyState::MoveToCore;
                enemy.target.reset();
                enemy.ramWindupRemaining = 0.0;
                continue;
            }
            enemy.ramWindupRemaining =
                std::max(0.0, enemy.ramWindupRemaining - deltaSeconds);
            if (enemy.ramWindupRemaining <= 0.0) {
                attackBuffer_.push_back({
                    enemy.id,
                    target->id,
                    enemy.damage * enemy.ramDamageMultiplier,
                    true,
                });
                enemy.state =
                    target->buildingType == BuildingType::Core
                                  ? EnemyState::AttackCore
                                  : EnemyState::AttackBuilding;
                enemy.attackCooldownRemaining = AttackInterval;
                enemy.ramCooldownRemaining = enemy.ramCooldown;
            }
            continue;
        }

        if (playerPosition) {
            const double playerOffsetX = playerPosition->x - enemy.position.x;
            const double playerOffsetZ = playerPosition->z - enemy.position.z;
            const double playerDistanceSquared =
                (playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ);
            const bool alreadyAggroed =
                enemy.state == EnemyState::ChasePlayer ||
                enemy.state == EnemyState::AttackPlayer;
            const double aggroRange =
                playerAggroRange(enemy.type) +
                (alreadyAggroed ? 1.5 : 0.0);
            if (playerDistanceSquared <= aggroRange * aggroRange &&
                !buildingIsInAttackRange(
                    enemy, buildingGrid) &&
                !buildingBlocksPathToPlayer(
                    enemy, buildingGrid,
                    *playerPosition)) {
                const double playerDistance =
                    std::sqrt(playerDistanceSquared);
                const double directionX =
                    playerDistance > 1e-9
                        ? playerOffsetX / playerDistance
                        : 0.0;
                const double directionZ =
                    playerDistance > 1e-9
                        ? playerOffsetZ / playerDistance
                        : 1.0;
                turnToward(
                    enemy,
                    std::atan2(directionX, directionZ),
                    aiTurnDeltaSeconds);
                enemy.target.reset();
                const double playerRange =
                    playerAttackRange(enemy.type);
                if (playerDistance <= playerRange) {
                    enemy.state = EnemyState::AttackPlayer;
                    if (enemy.attackCooldownRemaining <= 0.0) {
                        playerAttackBuffer_.push_back(
                            {enemy.id, enemy.damage});
                        enemy.attackCooldownRemaining =
                            attackInterval(enemy.type);
                    }
                } else {
                    enemy.state = EnemyState::ChasePlayer;
                    const double movement = std::min(
                        movementSpeed * deltaSeconds,
                        playerDistance - playerRange);
                    enemy.position.x +=
                        std::sin(enemy.yaw) * movement;
                    enemy.position.z +=
                        std::cos(enemy.yaw) * movement;
                }
                continue;
            }
        }

        const double coreX = static_cast<double>(core->gridPosition.x);
        const double coreZ = static_cast<double>(core->gridPosition.z);
        const double toCoreX = coreX - enemy.position.x;
        const double toCoreZ = coreZ - enemy.position.z;
        const double coreDistance = std::sqrt((toCoreX * toCoreX) + (toCoreZ * toCoreZ));
        if (coreDistance <= 1e-9) {
            continue;
        }
        double directionX = toCoreX / coreDistance;
        double directionZ = toCoreZ / coreDistance;
        const auto flowDirection =
            enemy.type == EnemyType::Flying
                ? std::optional<Vec3>{}
                : flowField.directionAt(enemy.position);
        if (flowDirection) {
            const double flowLength = std::sqrt((flowDirection->x * flowDirection->x) +
                                                (flowDirection->z * flowDirection->z));
            if (flowLength > 1e-9) {
                directionX = flowDirection->x / flowLength;
                directionZ = flowDirection->z / flowLength;
            }
        }

        double separationX = 0.0;
        double separationZ = 0.0;
        spatialHash_.forEachNearby(
            enemy.position, SeparationRadius, [&enemy, &separationX, &separationZ](
                                                  const SpatialEntry& neighbor) {
                if (neighbor.id == enemy.id) {
                    return;
                }
                double offsetX = enemy.position.x - neighbor.position.x;
                double offsetZ = enemy.position.z - neighbor.position.z;
                double distance =
                    std::sqrt((offsetX * offsetX) + (offsetZ * offsetZ));
                if (distance <= 1e-9) {
                    offsetX = enemy.id.index < neighbor.id.index ? -1.0 : 1.0;
                    offsetZ = 0.0;
                    distance = 1.0;
                }
                const double strength =
                    std::max(0.0, (SeparationRadius - distance) / SeparationRadius);
                separationX += (offsetX / distance) * strength;
                separationZ += (offsetZ / distance) * strength;
            });

        const double separationLength =
            std::sqrt((separationX * separationX) + (separationZ * separationZ));
        if (separationLength > 1e-9) {
            directionX += (separationX / separationLength) * SeparationWeight;
            directionZ += (separationZ / separationLength) * SeparationWeight;
            const double combinedLength =
                std::sqrt((directionX * directionX) + (directionZ * directionZ));
            directionX /= combinedLength;
            directionZ /= combinedLength;
        }
        const double wander =
            std::sin(
                enemy.steeringTime *
                    enemy.steeringFrequency +
                enemy.steeringPhase) *
                wanderStrength(enemy.type) +
            std::sin(
                enemy.steeringTime *
                    enemy.steeringFrequency * 0.43 +
                enemy.steeringPhase * 1.71) *
                wanderStrength(enemy.type) * 0.35;
        const double baseDirectionX = directionX;
        directionX += directionZ * wander;
        directionZ -= baseDirectionX * wander;
        const double steeredLength = std::sqrt(
            directionX * directionX +
            directionZ * directionZ);
        if (steeredLength > 1e-9) {
            directionX /= steeredLength;
            directionZ /= steeredLength;
        }
        turnToward(
            enemy, std::atan2(directionX, directionZ),
            aiTurnDeltaSeconds);
        directionX = std::sin(enemy.yaw);
        directionZ = std::cos(enemy.yaw);

        const EnemyStructureTarget* blocker = nullptr;
        double closestContactDistance = std::numeric_limits<double>::max();
        std::size_t greatestStructuralImpact = 0U;
        if (enemy.type == EnemyType::Sapper) {
            buildingGrid.forEachNearby(
                enemy.position,
                SapperStructureSearchRadius,
                [&](const EnemyStructureTarget& building) {
                    if (!structureIsVerticallyReachable(
                            enemy, building)) {
                        return;
                    }
                    if (building.structuralImpact == 0U) {
                        return;
                    }
                    const double offsetX =
                        building.position.x -
                        enemy.position.x;
                    const double offsetZ =
                        building.position.z -
                        enemy.position.z;
                    const double distance =
                        std::hypot(offsetX, offsetZ);
                    if (distance >
                        SapperStructureSearchRadius +
                            building.radius) {
                        return;
                    }
                    if (building.structuralImpact >
                            greatestStructuralImpact ||
                        (building.structuralImpact ==
                             greatestStructuralImpact &&
                         distance <
                             closestContactDistance)) {
                        blocker = &building;
                        greatestStructuralImpact =
                            building.structuralImpact;
                        closestContactDistance =
                            std::max(
                                0.0,
                                distance -
                                    building.radius -
                                    enemyRadius(
                                        enemy.type));
                    }
                });
        }
        buildingGrid.forEachNearby(
            enemy.position,
            BuildingLookAhead +
                attackRange(enemy.type) + 1.6,
            [&](const EnemyStructureTarget& building) {
            if (!structureIsVerticallyReachable(
                    enemy, building)) {
                return;
            }
            const Vec3 center = building.position;
            const double offsetX =
                center.x - enemy.position.x;
            const double offsetZ =
                center.z - enemy.position.z;
            const double projection = (offsetX * directionX) + (offsetZ * directionZ);
            const double lookAhead =
                std::max(BuildingLookAhead,
                         attackRange(enemy.type) + 0.75);
            if (projection < 0.0 ||
                projection > lookAhead + building.radius) {
                return;
            }

            const double perpendicularX = offsetX - directionX * projection;
            const double perpendicularZ = offsetZ - directionZ * projection;
            const double perpendicularDistance =
                std::sqrt((perpendicularX * perpendicularX) + (perpendicularZ * perpendicularZ));
            const double combinedRadius =
                building.radius + enemyRadius(enemy.type);
            if (perpendicularDistance > combinedRadius) {
                return;
            }

            const double contactDistance = std::max(0.0, projection - combinedRadius);
            const bool sapperPriority =
                enemy.type == EnemyType::Sapper &&
                building.structuralImpact >
                    greatestStructuralImpact;
            const bool equalSapperPriority =
                enemy.type != EnemyType::Sapper ||
                building.structuralImpact ==
                    greatestStructuralImpact;
            if (sapperPriority ||
                (equalSapperPriority &&
                 contactDistance <
                     closestContactDistance)) {
                blocker = &building;
                closestContactDistance = contactDistance;
                greatestStructuralImpact =
                    building.structuralImpact;
            }
        });

        if (blocker == nullptr) {
            enemy.state = EnemyState::MoveToCore;
            enemy.target.reset();
            enemy.position.x += directionX * movementSpeed * deltaSeconds;
            enemy.position.z += directionZ * movementSpeed * deltaSeconds;
            continue;
        }

        if (enemy.type == EnemyType::Sapper &&
            blocker->structuralImpact > 0U) {
            const double offsetX =
                blocker->position.x - enemy.position.x;
            const double offsetZ =
                blocker->position.z - enemy.position.z;
            const double distance =
                std::hypot(offsetX, offsetZ);
            if (distance > 1e-9) {
                turnToward(
                    enemy,
                    std::atan2(offsetX, offsetZ),
                    aiTurnDeltaSeconds);
                directionX = std::sin(enemy.yaw);
                directionZ = std::cos(enemy.yaw);
                closestContactDistance = std::max(
                    0.0,
                    distance - blocker->radius -
                        enemyRadius(enemy.type));
            }
        }

        const double enemyAttackRange =
            attackRange(enemy.type);
        if (closestContactDistance > enemyAttackRange) {
            const double movement =
                std::min(movementSpeed * deltaSeconds,
                         closestContactDistance -
                             enemyAttackRange);
            enemy.position.x += directionX * movement;
            enemy.position.z += directionZ * movement;
            enemy.state = EnemyState::MoveToCore;
            enemy.target.reset();
            continue;
        }

        enemy.target = blocker->id;
        const Vec3 blockerCenter = blocker->position;
        turnToward(
            enemy,
            std::atan2(
                blockerCenter.x - enemy.position.x,
                blockerCenter.z - enemy.position.z),
            aiTurnDeltaSeconds);
        if (enemy.type == EnemyType::Boss && enemy.ramCooldownRemaining <= 0.0) {
            enemy.state = EnemyState::BossRamWindup;
            enemy.ramWindupRemaining = enemy.ramWindup;
            continue;
        }
        enemy.state =
            blocker->buildingType == BuildingType::Core
                ? EnemyState::AttackCore
                : EnemyState::AttackBuilding;
        if (enemy.attackCooldownRemaining <= 0.0) {
            attackBuffer_.push_back({
                enemy.id, blocker->id,
                blocker->buildingType
                    ? buildingDamage(
                          enemy, *blocker->buildingType)
                    : (enemy.type == EnemyType::Sapper &&
                               blocker->modular
                           ? enemy.damage * 2.5
                           : enemy.damage),
                false});
            enemy.attackCooldownRemaining =
                attackInterval(enemy.type);
        }
    }

    const auto collisionStart = PerformanceClock::now();
    resolveEnemyCapsuleCollisions(
        enemies_, structureBuffer_, collisionEnemyLinks_,
        structureNextBuffer_, structureGridHeads_);
    performanceStats_.collision.sample(
        performanceMilliseconds(collisionStart));
    rebuildSpatialIndex();
    finishTelemetry();
    return attackBuffer_;
}

std::optional<EntityId> EnemySystem::raycast(Vec3 origin, Vec3 direction,
                                             double maxDistance) const {
    std::optional<EntityId> result;
    double closestDistance = std::numeric_limits<double>::max();
    for (const auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }

        const double radius =
            enemy.type == EnemyType::Boss ? 1.25
            : enemy.type == EnemyType::Flying ? 0.72
                                              : 0.65;
        const auto distance = raySphereDistance(origin, direction, enemy.position, radius);
        if (distance && *distance <= maxDistance && *distance < closestDistance) {
            result = enemy.id;
            closestDistance = *distance;
        }
    }
    return result;
}

std::optional<EnemyDamageResult> EnemySystem::damage(EntityId id, double amount) {
    EnemyInstance* enemy = findEnemy(id);
    if (enemy == nullptr || !enemy->active || amount <= 0.0) {
        return std::nullopt;
    }

    const double previousHealth = enemy->health;
    enemy->health = std::max(0.0, enemy->health - amount);
    enemy->hitAnimationRemaining = 0.22;
    const bool killed = enemy->health <= 0.0;
    if (killed) {
        enemy->active = false;
        --activeCount_;
        enemy->state = EnemyState::Dead;
        enemy->target.reset();
        spatialHashDirty_ = true;
    }
    return EnemyDamageResult{
        .id = enemy->id,
        .position = enemy->position,
        .damage = previousHealth - enemy->health,
        .remainingHealth = enemy->health,
        .killed = killed,
    };
}

std::optional<EntityId> EnemySystem::nearestEnemy(Vec3 position, double radius) const {
    std::optional<EntityId> nearest;
    double nearestDistanceSquared = radius * radius;
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        const EnemyInstance* enemy = findEnemy(entry.id);
        if (enemy == nullptr || !enemy->active) {
            return;
        }
        const double deltaX = entry.position.x - position.x;
        const double deltaZ = entry.position.z - position.z;
        const double distanceSquared = (deltaX * deltaX) + (deltaZ * deltaZ);
        if (distanceSquared < nearestDistanceSquared ||
            (distanceSquared == nearestDistanceSquared &&
             (!nearest || entry.id.index < nearest->index))) {
            nearest = entry.id;
            nearestDistanceSquared = distanceSquared;
        }
    });
    return nearest;
}

std::optional<EntityId> EnemySystem::densestEnemy(Vec3 position, double radius,
                                                  double clusterRadius) const {
    std::optional<EntityId> best;
    std::size_t bestCount = 0;
    double bestDistanceSquared = radius * radius;
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& candidate) {
        const EnemyInstance* candidateEnemy = findEnemy(candidate.id);
        if (candidateEnemy == nullptr || !candidateEnemy->active) {
            return;
        }
        std::size_t count = 0;
        spatialHash_.forEachNearby(candidate.position, clusterRadius,
                                   [this, &count](const SpatialEntry& entry) {
            const EnemyInstance* enemy = findEnemy(entry.id);
            if (enemy != nullptr && enemy->active) {
                ++count;
            }
        });
        const double deltaX = candidate.position.x - position.x;
        const double deltaZ = candidate.position.z - position.z;
        const double distanceSquared = (deltaX * deltaX) + (deltaZ * deltaZ);
        if (count > bestCount ||
            (count == bestCount &&
             (distanceSquared < bestDistanceSquared ||
              (distanceSquared == bestDistanceSquared &&
               (!best || candidate.id.index < best->index))))) {
            best = candidate.id;
            bestCount = count;
            bestDistanceSquared = distanceSquared;
        }
    });
    return best;
}

std::optional<EnemyInstance> EnemySystem::enemy(EntityId id) const {
    const EnemyInstance* instance = findEnemy(id);
    if (instance == nullptr || !instance->active) {
        return std::nullopt;
    }
    return *instance;
}

std::span<const EnemyDamageResult> EnemySystem::damageInRadius(Vec3 position, double radius,
                                                               double amount,
                                                               double knockbackStrength,
                                                               std::optional<Vec3> knockbackOrigin,
                                                               double maxTotalDamage,
                                                               std::optional<EntityId> excludedId) {
    areaDamageBuffer_.clear();
    if (radius <= 0.0 || amount <= 0.0) {
        return areaDamageBuffer_;
    }

    // Player attacks can land between enemy ticks. Refresh positions before
    // querying, then include capsule radius so enemies touching the impact
    // area are not missed just because their centers are outside it.
    rebuildSpatialIndex();
    const double queryRadius =
        radius + enemyCapsule(EnemyType::Boss).radius;
    std::size_t targetCount = 0;
    spatialHash_.forEachNearby(position, queryRadius,
                               [&](const SpatialEntry& entry) {
        const EnemyInstance* candidate = findEnemy(entry.id);
        if (candidate == nullptr || !candidate->active ||
            (excludedId && candidate->id == *excludedId)) {
            return;
        }
        const double hitRadius =
            radius + enemyCapsule(candidate->type).radius;
        const double deltaX = entry.position.x - position.x;
        const double deltaZ = entry.position.z - position.z;
        if (deltaX * deltaX + deltaZ * deltaZ <=
                hitRadius * hitRadius &&
            targetCount < areaTargetBuffer_.size()) {
            areaTargetBuffer_[targetCount++] = entry.id;
        }
    });
    const double damagePerTarget =
        maxTotalDamage > 0.0 && targetCount > 0U
            ? std::min(
                  amount,
                  maxTotalDamage /
                      static_cast<double>(targetCount))
            : amount;
    const Vec3 impulseOrigin =
        knockbackOrigin.value_or(position);
    for (std::size_t index = 0; index < targetCount; ++index) {
        EnemyInstance* enemy = findEnemy(areaTargetBuffer_[index]);
        if (enemy == nullptr || !enemy->active || amount <= 0.0) {
            continue;
        }
        const double previousHealth = enemy->health;
        enemy->health = std::max(
            0.0, enemy->health - damagePerTarget);
        enemy->hitAnimationRemaining = 0.22;
        const bool killed = enemy->health <= 0.0;
        if (killed) {
            enemy->active = false;
            --activeCount_;
            enemy->state = EnemyState::Dead;
            enemy->target.reset();
            spatialHashDirty_ = true;
        }
        areaDamageBuffer_.push_back({
            .id = enemy->id,
            .position = enemy->position,
            .damage = previousHealth - enemy->health,
            .remainingHealth = enemy->health,
            .killed = killed,
        });
        if (killed || knockbackStrength <= 0.0) {
            continue;
        }
        const double areaOffsetX =
            enemy->position.x - position.x;
        const double areaOffsetZ =
            enemy->position.z - position.z;
        const double radialDistance =
            std::sqrt(
                (areaOffsetX * areaOffsetX) +
                (areaOffsetZ * areaOffsetZ));
        double offsetX =
            enemy->position.x - impulseOrigin.x;
        double offsetZ =
            enemy->position.z - impulseOrigin.z;
        double directionDistance =
            std::sqrt((offsetX * offsetX) + (offsetZ * offsetZ));
        if (directionDistance <= 1e-9) {
            offsetX = (enemy->id.index % 2U) == 0U ? -1.0 : 1.0;
            offsetZ = 0.0;
            directionDistance = 1.0;
        }
        const double surfaceDistance = std::max(
            0.0,
            radialDistance - enemyCapsule(enemy->type).radius);
        const double falloff =
            std::max(0.0, 1.0 - surfaceDistance / radius);
        const double impulse =
            knockbackStrength * falloff * knockbackMultiplier(enemy->type);
        enemy->knockbackVelocity.x +=
            (offsetX / directionDistance) * impulse;
        enemy->knockbackVelocity.z +=
            (offsetZ / directionDistance) * impulse;
    }
    return areaDamageBuffer_;
}

std::span<const EntityId> EnemySystem::applySlowInRadius(Vec3 position, double radius,
                                                        double multiplier, double duration) {
    statusTargetBuffer_.clear();
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        EnemyInstance* enemy = findEnemy(entry.id);
        if (enemy == nullptr || !enemy->active) {
            return;
        }
        enemy->movementMultiplier =
            std::min(enemy->movementMultiplier, std::clamp(multiplier, 0.1, 1.0));
        enemy->slowRemaining = std::max(enemy->slowRemaining, duration);
        statusTargetBuffer_.push_back(enemy->id);
    });
    return statusTargetBuffer_;
}

bool EnemySystem::applyStatus(
    EntityId id, StatusEffectType requestedType,
    std::optional<EntityId> source, double duration,
    double intensity, StatusEffectRules rules) {
    EnemyInstance* enemy = findEnemy(id);
    if (enemy == nullptr || !enemy->active || duration <= 0.0) {
        return false;
    }

    const bool elite = enemy->type == EnemyType::Heavy;
    const bool boss = enemy->type == EnemyType::Boss;
    const StatusEffectType appliedType =
        requestedType == StatusEffectType::Freeze && boss
            ? StatusEffectType::Slow
            : requestedType;
    EnemyStatusEffect& status =
        enemy->statusEffects[statusEffectIndex(appliedType)];
    if (status.remaining > 0.0 || status.immunityRemaining > 0.0) {
        // A hit still refreshes the crack cue, but cannot extend an active
        // control effect. This is the immunity/diminishing-return window.
        status.visualParameter = std::max(status.visualParameter, 0.7);
        return false;
    }

    double effectiveDuration = duration;
    if (elite && appliedType == StatusEffectType::Freeze) {
        effectiveDuration *= std::clamp(
            rules.eliteDurationMultiplier, 0.0, 1.0);
    }
    double effectiveIntensity = std::clamp(intensity, 0.0, 1.0);
    if (boss && requestedType == StatusEffectType::Freeze) {
        effectiveIntensity = std::clamp(rules.bossSlowAmount, 0.0, 1.0);
    }
    status.type = appliedType;
    status.source = source;
    status.remaining = effectiveDuration;
    status.intensity = effectiveIntensity;
    status.immunityRemaining = effectiveDuration * std::clamp(
        rules.immunityWindowFraction, 0.0, 1.0);
    status.visualParameter = 1.0;

    if (appliedType == StatusEffectType::Freeze) {
        enemy->knockbackVelocity = {};
        enemy->state = EnemyState::ChasePlayer;
        enemy->target.reset();
    } else {
        const double movementMultiplier = std::clamp(
            1.0 - effectiveIntensity, 0.1, 1.0);
        enemy->movementMultiplier = std::min(
            enemy->movementMultiplier, movementMultiplier);
        enemy->slowRemaining = std::max(
            enemy->slowRemaining, effectiveDuration);
    }
    return true;
}

std::span<const EntityId> EnemySystem::applyStatusInRadius(
    Vec3 position, double radius, StatusEffectType type,
    std::optional<EntityId> source, double duration,
    double intensity, StatusEffectRules rules) {
    statusTargetBuffer_.clear();
    rebuildSpatialIndex();
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        EnemyInstance* enemy = findEnemy(entry.id);
        if (enemy == nullptr || !enemy->active) {
            return;
        }
        const double hitRadius = radius + enemyCapsule(enemy->type).radius;
        const double offsetX = enemy->position.x - position.x;
        const double offsetZ = enemy->position.z - position.z;
        if (offsetX * offsetX + offsetZ * offsetZ > hitRadius * hitRadius) {
            return;
        }
        if (applyStatus(enemy->id, type, source, duration, intensity, rules)) {
            statusTargetBuffer_.push_back(enemy->id);
        }
    });
    return statusTargetBuffer_;
}

std::size_t EnemySystem::defeatAll() {
    std::size_t defeated = 0;
    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }
        enemy.active = false;
        enemy.health = 0.0;
        enemy.state = EnemyState::Dead;
        enemy.target.reset();
        ++defeated;
    }
    activeCount_ = 0;
    rebuildSpatialIndex();
    return defeated;
}

std::size_t EnemySystem::activeCount() const {
    return activeCount_;
}

const std::vector<EnemyInstance>& EnemySystem::enemies() const {
    return enemies_;
}

std::span<const EnemyPlayerAttack> EnemySystem::playerAttacks() const {
    return playerAttackBuffer_;
}

const EnemyPerformanceStats& EnemySystem::performanceStats() const {
    return performanceStats_;
}

const EnemyStatusEffect& enemyStatusEffect(
    const EnemyInstance& enemy, StatusEffectType type) {
    return enemy.statusEffects[statusEffectIndex(type)];
}

bool enemyHasStatus(const EnemyInstance& enemy, StatusEffectType type) {
    return enemyStatusEffect(enemy, type).remaining > 0.0;
}

void EnemySystem::appendEnemy(const EnemySpawn& spawn) {
    if (activeCount() >= MaxActiveEnemies) {
        return;
    }
    const EnemyType type = spawn.type;
    const Vec3 position = spawn.position;
    const EnemyDefinition stats =
        definitions_[static_cast<std::size_t>(type)];
    const double healthMultiplier =
        std::max(0.01, spawn.healthMultiplier);
    const double damageMultiplier =
        std::max(0.01, spawn.damageMultiplier);
    const auto reusable =
        std::find_if(enemies_.begin(), enemies_.end(),
                     [](const EnemyInstance& enemy) { return !enemy.active; });
    if (reusable == enemies_.end() && enemies_.size() >= MaxEnemies) {
        return;
    }
    const EntityId id =
        reusable == enemies_.end()
            ? EntityId{nextIndex_++, 1}
            : EntityId{reusable->id.index, reusable->id.generation + 1};
    const double firstRandom =
        hashUnit(id.index * 0x9e3779b9U + id.generation);
    const double secondRandom =
        hashUnit(id.index * 0x85ebca6bU +
                 id.generation * 17U);
    const double thirdRandom =
        hashUnit(id.index * 0xc2b2ae35U +
                 id.generation * 31U);
    const double baseTurnRate =
        type == EnemyType::Boss
            ? 1.8
            : type == EnemyType::Heavy ? 2.35
                                       : 3.2;
    const EnemyInstance instance{
        .id = id,
        .type = type,
        .position = position,
        .health = stats.health * healthMultiplier,
        .maxHealth = stats.health * healthMultiplier,
        .speed = stats.speed,
        .damage = stats.damage * damageMultiplier,
        .attackCooldownRemaining = 0.0,
        .hitAnimationRemaining = 0.0,
        .ramWindup = stats.ramWindup,
        .ramDamageMultiplier = stats.ramDamageMultiplier,
        .ramCooldown = stats.ramCooldown,
        .ramWindupRemaining = 0.0,
        .ramCooldownRemaining = 0.0,
        .slowRemaining = 0.0,
        .movementMultiplier = 1.0,
        .knockbackVelocity = {},
        .yaw = 0.0,
        .steeringTime = 0.0,
        .steeringPhase = firstRandom * 2.0 * Pi,
        .steeringFrequency = 0.7 + secondRandom * 0.65,
        .turnRate = baseTurnRate *
                    (0.88 + thirdRandom * 0.24),
        .locomotionRate = 0.94 + secondRandom * 0.12,
        .state = EnemyState::Spawn,
        .target = std::nullopt,
        .active = true,
    };
    if (reusable == enemies_.end()) {
        enemies_.push_back(instance);
    } else {
        *reusable = instance;
    }
    ++activeCount_;
}

void EnemySystem::rebuildSpatialIndex() {
    const auto rebuildStart = PerformanceClock::now();
    spatialHash_.clear();
    for (const auto& enemy : enemies_) {
        if (enemy.active) {
            spatialHash_.insert(enemy.id, enemy.position);
        }
    }
    spatialHashDirty_ = false;
    if (profilingTick_) {
        ++spatialRebuildsThisTick_;
        spatialRebuildMillisecondsThisTick_ +=
            performanceMilliseconds(rebuildStart);
    }
}

EnemyInstance* EnemySystem::findEnemy(EntityId id) {
    if (id.index < FirstEnemyIndex) {
        return nullptr;
    }
    const std::size_t slot =
        static_cast<std::size_t>(id.index - FirstEnemyIndex);
    if (slot >= enemies_.size() || enemies_[slot].id != id) {
        return nullptr;
    }
    return &enemies_[slot];
}

const EnemyInstance* EnemySystem::findEnemy(EntityId id) const {
    if (id.index < FirstEnemyIndex) {
        return nullptr;
    }
    const std::size_t slot =
        static_cast<std::size_t>(id.index - FirstEnemyIndex);
    if (slot >= enemies_.size() || enemies_[slot].id != id) {
        return nullptr;
    }
    return &enemies_[slot];
}

} // namespace ian
