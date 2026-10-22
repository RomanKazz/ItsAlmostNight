#include "enemies/EnemySystem.hpp"
#include "enemies/EnemyNavigation.hpp"

#include "core/Geometry.hpp"

#include "world/TerrainHeightfield.hpp"
#include "world/CollisionWorld.hpp"
#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

bool berserkerActive(const EnemyInstance& enemy) {
    return hasEliteAffix(
               enemy.eliteAffixes, EliteAffix::Berserker) &&
        enemy.maxHealth > 0.0 &&
        enemy.health / enemy.maxHealth <= 0.5;
}

constexpr double AttackRange = 0.15;
constexpr double PlayerAttackRange = 1.0;
constexpr double AttackInterval = 1.0;
constexpr double BuildingLookAhead = 2.5;
constexpr double SapperStructureSearchRadius = 6.0;
constexpr double SeparationRadius = 1.1;
constexpr double SeparationWeight = 0.65;
constexpr double BuildingGridCellSize = 2.0;
constexpr double BuildingGridMinimum = -192.0;
constexpr int BuildingGridSize = 192;
constexpr int BuildingGridCellCount =
    BuildingGridSize * BuildingGridSize;

void updateEnemySurface(
    EnemyInstance& enemy, const TerrainHeightfield* terrain,
    EnemyNavigationView navigation, double deltaSeconds) {
    if (terrain == nullptr) {
        return;
    }
    if (enemyIsFlying(enemy.type) || enemy.type == EnemyType::Boss) {
        // Flying visuals hover above the terrain. Keep their terrain base in
        // simulation state so defenses can aim at the same world-space
        // height that presentation uses.
        enemy.worldSurfaceHeight = terrain->getHeight(
            enemy.position.x, enemy.position.z);
        return;
    }
    if (navigation.collisionWorld == nullptr) return;
    const double terrainHeight = terrain->getHeight(
        enemy.position.x, enemy.position.z);
    const double currentHeight = terrainHeight + enemy.surfaceHeightOffset;
    // Support comes from the body footprint, independent of yaw or AI state.
    // The small margin starts a step before the collision sweep reaches the
    // edge and keeps partial support while fighting beside it. Upper floors
    // still require a ramp: they cannot be sampled through the step limit.
    constexpr double MaximumStepUp = 1.15;
    const double supportRadius = enemyCollisionCylinder(enemy).radius + .15;
    const auto surface = navigation.collisionWorld->enemySupportHeight(
        enemy.position.x, enemy.position.z, supportRadius,
        currentHeight + MaximumStepUp);
    const double targetOffset = surface
        ? std::max(0.0, *surface - terrainHeight)
        : 0.0;
    if (targetOffset >= enemy.surfaceHeightOffset) {
        // Short eased hop instead of a one-frame vertical snap. Ramp changes
        // remain continuous because their sampled target rises gradually.
        enemy.surfaceHeightOffset = std::min(
            targetOffset,
            enemy.surfaceHeightOffset + deltaSeconds * 6.5);
    } else {
        // Falling off destroyed/unsupported floors remains visible instead of
        // teleporting vertically to terrain.
        enemy.surfaceHeightOffset = std::max(
            targetOffset,
            enemy.surfaceHeightOffset - deltaSeconds * 7.0);
    }
    enemy.worldSurfaceHeight =
        terrainHeight + enemy.surfaceHeightOffset;
}

bool playerSharesAttackHeight(
    const EnemyInstance& enemy, Vec3 playerPosition) {
    const EnemyCapsule capsule =
        enemyPhysicalCapsule(enemy.type);
    const double enemyCenterHeight =
        enemy.position.y + enemy.worldSurfaceHeight;
    const double enemyExtent =
        capsule.segmentHalfHeight + capsule.radius;
    const double enemyMinimum =
        enemyCenterHeight - enemyExtent;
    const double enemyMaximum =
        enemyCenterHeight + enemyExtent;
    constexpr double PlayerBodyBelowEye = 1.70;
    constexpr double PlayerHeadAboveEye = 0.15;
    const double playerMinimum =
        playerPosition.y - PlayerBodyBelowEye;
    const double playerMaximum =
        playerPosition.y + PlayerHeadAboveEye;
    constexpr double VerticalAttackReach = 0.35;
    return enemyMaximum + VerticalAttackReach >=
               playerMinimum &&
        playerMaximum + VerticalAttackReach >=
               enemyMinimum;
}

Vec3 steerAroundWorldScenery(
    const EnemyInstance& enemy, Vec3 direction,
    EnemyNavigationView navigation) {
    if (enemyIsFlying(enemy.type) || navigation.collisionWorld == nullptr)
        return direction;
    const auto capsule = enemyPhysicalCapsule(enemy.type);
    return navigation.collisionWorld->scenerySteeringDirection(
        {enemy.position.x, enemy.position.y + enemy.worldSurfaceHeight +
            capsule.segmentHalfHeight + capsule.radius, enemy.position.z},
        direction, capsule.radius);
}

void moveEnemyHorizontally(
    EnemyInstance& enemy, Vec3 delta,
    EnemyNavigationView navigation) {
    if (enemyIsFlying(enemy.type) ||
        navigation.collisionWorld == nullptr) {
        enemy.position.x += delta.x;
        enemy.position.z += delta.z;
        return;
    }
    const EnemyCapsule capsule =
        enemyPhysicalCapsule(enemy.type);
    const double enemyTopHeight =
        enemy.position.y + enemy.worldSurfaceHeight +
        capsule.segmentHalfHeight + capsule.radius;
    constexpr double MaximumSlopeStep = 0.20;
    const Vec3 resolved =
        navigation.collisionWorld
            ->moveCircleAgainstRaisedSurfaces(
                {
                    enemy.position.x,
                    enemyTopHeight,
                    enemy.position.z,
                },
                delta, capsule.radius,
                enemy.worldSurfaceHeight +
                    MaximumSlopeStep);
    enemy.position.x = resolved.x;
    enemy.position.z = resolved.z;
}

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
        if (a.traversable != b.traversable) {
            return false;
        }
        if (a.minimumEnemySurfaceHeight !=
            b.minimumEnemySurfaceHeight) {
            return false;
        }
        if (a.maximumEnemySurfaceHeight !=
                b.maximumEnemySurfaceHeight ||
            a.attackSurfaceHeight != b.attackSurfaceHeight ||
            a.attackable != b.attackable) {
            return false;
        }
    }
    return true;
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
    case EnemyType::Bee:
    case EnemyType::Flying:
        return 0.24;
    case EnemyType::AirTank:
    case EnemyType::Heavy:
        return 0.08;
    case EnemyType::Boss:
        return 0.045;
    case EnemyType::Ranged:
        return 0.13;
    case EnemyType::Sapper:
        return 0.16;
    case EnemyType::Splitter:
        return 0.10;
    case EnemyType::Splitling:
        return 0.22;
    case EnemyType::Basic:
        return 0.14;
    }
    return 0.14;
}

double enemyRadius(const EnemyInstance& enemy) {
    return enemyCollisionCylinder(enemy).radius;
}

double attackRange(EnemyType type) {
    return type == EnemyType::Ranged ? 8.5 : AttackRange;
}

double playerAttackRange(EnemyType type) {
    return type == EnemyType::Ranged ? 9.0 : PlayerAttackRange;
}

double buildingRadius(BuildingType type);

double playerAggroRange(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 5.5;
    case EnemyType::Bee:
    case EnemyType::Flying:
        return 6.0;
    case EnemyType::Ranged:
        return 11.0;
    case EnemyType::Boss:
        return 5.0;
    case EnemyType::AirTank:
    case EnemyType::Heavy:
        return 4.0;
    case EnemyType::Sapper:
        return 3.5;
    case EnemyType::Splitter:
        return 4.0;
    case EnemyType::Splitling:
        return 5.0;
    case EnemyType::Basic:
        return 4.5;
    }
    return 4.5;
}

Vec3 structureProjectileAim(const EnemyStructureTarget& structure) {
    return {structure.position.x,
        structure.attackSurfaceHeight.value_or(structure.position.y) +
            (structure.buildingType == BuildingType::Core ? 1.05 : .70),
        structure.position.z};
}

double structureContactDistance(const EnemyInstance& enemy,
    const EnemyStructureTarget& structure, bool meleeApproach = false) {
    const double horizontal = std::max(0.0,
        std::hypot(structure.position.x - enemy.position.x,
                   structure.position.z - enemy.position.z) -
            structure.radius - enemyRadius(enemy));
    if (enemy.type != EnemyType::Ranged || meleeApproach) return horizontal;
    return std::hypot(horizontal,
        structureProjectileAim(structure).y - (enemy.worldSurfaceHeight + 1.02));
}

bool structureIsVerticallyReachable(
    const EnemyInstance& enemy,
    const EnemyStructureTarget& structure) {
    // Floors and ramps are traversal surfaces, never combat blockers. Their
    // supports can still disappear through structural dependency damage.
    if (structure.traversable || !structure.attackable) {
        return false;
    }
    if (enemy.type == EnemyType::Ranged) {
        // A physical shot can cross storeys; the melee navigation/support
        // limits do not describe its reach. Keep a finite vertical range.
        return std::abs(structureProjectileAim(structure).y -
            (enemy.worldSurfaceHeight + 1.02)) <= attackRange(enemy.type);
    }
    if (enemyIsFlying(enemy.type)) {
        return structure.buildingType ==
               BuildingType::Core;
    }
    if (enemy.worldSurfaceHeight + 1e-6 <
        structure.minimumEnemySurfaceHeight) {
        return false;
    }
    if (enemy.worldSurfaceHeight - 1e-6 >
        structure.maximumEnemySurfaceHeight) {
        return false;
    }
    const double maximumAttackHeight =
        maximumGroundStructureInteractionHeight(
            enemy.type,
            enemy.position.y + enemy.worldSurfaceHeight);
    return structure.position.y <= maximumAttackHeight;
}

struct StructureTargetSelection {
    const EnemyStructureTarget* target{};
    double contactDistance{};
};

StructureTargetSelection selectStructureTarget(
    const EnemyInstance& enemy,
    const BuildingQueryGrid& buildingGrid,
    double directionX, double directionZ, Vec3 combatGoal, bool lureApproach = false) {
    const EnemyStructureTarget* blocker = nullptr;
    double closestContactDistance = std::numeric_limits<double>::max();
    std::size_t greatestStructuralImpact = 0U;
    // Acquire actual close contacts on every side. Path headings can run
    // tangent to a wall; a touching melee enemy must turn and hit that wall
    // instead of orbiting it. Ranged units retain their firing distance.
    const double contactReach = lureApproach ? AttackRange : attackRange(enemy.type);
    {
        buildingGrid.forEachNearby(
            enemy.position,
            contactReach +
                enemyRadius(enemy) + 1.6,
            [&](const EnemyStructureTarget& building) {
                if (!structureIsVerticallyReachable(
                        enemy, building)) {
                    return;
                }
                const double offsetX =
                    building.position.x - enemy.position.x;
                const double offsetZ =
                    building.position.z - enemy.position.z;
                if (enemy.type != EnemyType::Ranged || lureApproach) {
                    const double gx = combatGoal.x-enemy.position.x, gz = combatGoal.z-enemy.position.z;
                    const double length = std::hypot(gx,gz);
                    if (length > 1e-9) {
                        const double projection = (offsetX*gx+offsetZ*gz)/length;
                        const double perpendicular = std::abs(offsetX*gz-offsetZ*gx)/length;
                        if (projection < 0 || perpendicular > building.radius+enemyRadius(enemy)) return;
                    }
                }
                const double contactDistance =
                    structureContactDistance(enemy, building, lureApproach);
                if (contactDistance <= contactReach + 1e-6 &&
                    contactDistance < closestContactDistance) {
                    blocker = &building;
                    closestContactDistance = contactDistance;
                }
            });
    }
    if (enemy.type == EnemyType::Sapper && !lureApproach) {
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
                                enemyRadius(enemy));
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
            building.radius + enemyRadius(enemy);
        if (perpendicularDistance > combinedRadius) {
            return;
        }

        const double contactDistance =
            structureContactDistance(enemy, building, lureApproach);
        const bool sapperPriority =
            !lureApproach && enemy.type == EnemyType::Sapper &&
            building.structuralImpact >
                greatestStructuralImpact;
        const bool equalSapperPriority = lureApproach ||
            (enemy.type != EnemyType::Sapper &&
             enemy.type != EnemyType::Ranged) ||
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

    return {blocker, closestContactDistance};
}

bool buildingIsInAttackRange(
    const EnemyInstance& enemy,
    const BuildingQueryGrid& buildingGrid) {
    bool found = false;
    const double searchRadius =
        attackRange(enemy.type) +
        enemyRadius(enemy) + 1.6;
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
        const double contactDistance = structureContactDistance(enemy, building);
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
    if (enemyIsFlying(enemy.type)) {
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
            enemyRadius(enemy);
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

double buildingRadius(BuildingType type) {
    // A collider wider than the platform makes the building push attackers
    // completely off their own support. Core used to reserve a 1.6m radius
    // on a floor with only a 1m half-width, causing endless climb/fall loops.
    const double footprint = buildingFootprintHalfExtent(type);
    return footprint == 1.0 ? .85 : .5;
}

} // namespace

bool enemyUsesForwardSurfaceProbe(EnemyState state) {
    return state == EnemyState::MoveToCore ||
           state == EnemyState::ChasePlayer;
}

EnemySystem::EnemySystem(
    std::array<EnemyDefinition, GameBalance::EnemyTypeCount> definitions)
    : definitions_(definitions) {
    enemies_.reserve(MaxEnemies);
    attackBuffer_.reserve(MaxEnemies);
    playerAttackBuffer_.reserve(MaxEnemies);
    projectiles_.reserve(256);
    magicEventBuffer_.reserve(256);
    areaDamageBuffer_.reserve(MaxEnemies);
    poisonHitBuffer_.reserve(MaxEnemies);
    statusTargetBuffer_.reserve(MaxEnemies);
    eliteSpawnEventBuffer_.reserve(64);
    eliteDeathEventBuffer_.reserve(64);
    bossActionEventBuffer_.reserve(16);
    decoyBeacons_.reserve(64);
    decoyNavigation_.reserve(64);
    structureBuffer_.reserve(256);
    incomingStructureBuffer_.reserve(256);
    structureNextBuffer_.reserve(256);
    structureGridHeads_.reserve(BuildingGridCellCount);
    collisionEnemyLinks_.reserve(MaxEnemies);
    areaTargetBuffer_.resize(SpatialHash::MaxEntries);
    pendingSplitBuffer_.reserve(MaxActiveEnemies);
    delayedSplitBuffer_.reserve(64);
}

void EnemySystem::reset() {
    // A new run has no enemy deaths to reward. Keeping old slots in the
    // Dead state made the coin fallback scanner interpret every enemy from
    // the previous run as a fresh kill on the first restarted tick.
    // clear() retains vector capacity, so restart stays allocation-free.
    enemies_.clear();
    decoyBeacons_.clear();
    decoyNavigation_.clear();
    firstIndex_ = nextIndex_;
    attackBuffer_.clear();
    playerAttackBuffer_.clear();
    projectiles_.clear();
    magicEventBuffer_.clear();
    nextProjectileIndex_ = 1U;
    performanceStats_.fullAiUpdates = 0U;
    performanceStats_.throttledAiMoves = 0U;
    areaDamageBuffer_.clear();
    poisonHitBuffer_.clear();
    statusTargetBuffer_.clear();
    splitEventBuffer_.clear();
    eliteSpawnEventBuffer_.clear();
    eliteDeathEventBuffer_.clear();
    bossActionEventBuffer_.clear();
    pendingSplitBuffer_.clear();
    delayedSplitBuffer_.clear();
    previousPlayerPosition_.reset();
    estimatedPlayerVelocity_ = {};
    structureBuffer_.clear();
    incomingStructureBuffer_.clear();
    structureNextBuffer_.clear();
    structureGridHeads_.clear();
    collisionEnemyLinks_.clear();
    activeCount_ = 0;
    overtime_ = {};
    spatialHash_.clear();
    performanceStats_ = {};
    profilingTick_ = false;
    spatialHashDirty_ = false;
    spatialRebuildsThisTick_ = 0U;
    spatialRebuildMillisecondsThisTick_ = 0.0;
    navigationCache_.reset();
    cachedNavigationRevision_ =
        std::numeric_limits<std::uint64_t>::max();
}

void EnemySystem::setOvertimeDifficulty(OvertimeDifficulty difficulty) {
    if (difficulty == overtime_) return;
    const auto valid = [](double value) {
        return std::isfinite(value) && value >= 1.0 && value <= std::exp2(40.0);
    };
    if (!valid(difficulty.health) || !valid(difficulty.damage) ||
        !valid(difficulty.speed) || !valid(difficulty.spawnRate)) return;
    const double healthRatio = difficulty.health / overtime_.health;
    const double damageRatio = difficulty.damage / overtime_.damage;
    const double speedRatio = difficulty.speed / overtime_.speed;
    for (EnemyInstance& enemy : enemies_) {
        if (!enemy.active) continue;
        enemy.maxHealth *= healthRatio;
        enemy.health *= healthRatio;
        enemy.damage *= damageRatio;
        enemy.speed *= speedRatio;
        // Preserve progress towards interrupting an already warned storm.
        enemy.bossStormInterruptDamage *= healthRatio;
        enemy.bossStormDamageTaken *= healthRatio;
    }
    for (EnemyProjectile& projectile : projectiles_) {
        if (projectile.active) projectile.damage *= damageRatio;
    }
    overtime_ = difficulty;
}

void EnemySystem::spawnWave(std::span<const Vec3> positions) {
    delayedSplitBuffer_.clear();
    previousPlayerPosition_.reset();
    estimatedPlayerVelocity_ = {};
    for (EnemyInstance& enemy : enemies_) {
        enemy.active = false;
        enemy.splitAnimationRemaining = 0.0;
    }
    activeCount_ = 0;
    for (const Vec3 position : positions) {
        appendEnemy({EnemyType::Basic, position});
    }
    rebuildSpatialIndex();
}

void EnemySystem::spawnWave(std::span<const EnemySpawn> spawns) {
    clearProjectiles();
    delayedSplitBuffer_.clear();
    previousPlayerPosition_.reset();
    estimatedPlayerVelocity_ = {};
    for (EnemyInstance& enemy : enemies_) {
        enemy.active = false;
        enemy.splitAnimationRemaining = 0.0;
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

void EnemySystem::constrainToArena(
    Vec3 center, double radius) {
    const double maximumDistance = std::max(0.5, radius - 0.55);
    bool moved = false;
    for (EnemyInstance& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }
        const double dx = enemy.position.x - center.x;
        const double dz = enemy.position.z - center.z;
        const double distance = std::hypot(dx, dz);
        if (distance <= maximumDistance || distance <= 1e-9) {
            continue;
        }
        enemy.position.x = center.x + dx / distance * maximumDistance;
        enemy.position.z = center.z + dz / distance * maximumDistance;
        enemy.knockbackVelocity.x = 0.0;
        enemy.knockbackVelocity.z = 0.0;
        moved = true;
    }
    for (EnemyProjectile& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }
        const double dx = projectile.position.x - center.x;
        const double dz = projectile.position.z - center.z;
        if (dx * dx + dz * dz > radius * radius) {
            projectile.active = false;
        }
    }
    if (moved) {
        rebuildSpatialIndex();
    }
}

bool EnemySystem::refreshStructureTargets(
    const std::vector<BuildingInstance>& buildings,
    std::span<const EnemyStructureTarget> additionalStructures) {
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
            .traversable = false,
            .minimumEnemySurfaceHeight =
                building.platformStorey >= 0
                    ? building.baseHeight - 0.05
                    : -std::numeric_limits<double>::infinity(),
            .maximumEnemySurfaceHeight =
                building.baseHeight +
                    (building.platformStorey >= 0 ? 1.0 : 1.6),
            // Projectiles aim at the building, not at buried foundation feet.
            .attackSurfaceHeight = building.baseHeight,
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

    return structureLayoutChanged;
}

void EnemySystem::updateEnemyTimers(
    EnemyInstance& enemy, double deltaSeconds) {
    const double eliteActionSpeed =
        berserkerActive(enemy) ? 1.35 : 1.0;
    enemy.attackCooldownRemaining =
        std::max(
            0.0,
            enemy.attackCooldownRemaining -
                deltaSeconds * eliteActionSpeed);
    enemy.spawnAnimationRemaining = std::max(
        0.0,
        enemy.spawnAnimationRemaining - deltaSeconds);
    enemy.portalArrivalRemaining=std::max(0.0,enemy.portalArrivalRemaining-deltaSeconds);
    enemy.ramCooldownRemaining =
        std::max(0.0, enemy.ramCooldownRemaining - deltaSeconds);
    enemy.bossAbilityCooldownRemaining = std::max(
        0.0,
        enemy.bossAbilityCooldownRemaining - deltaSeconds);
    enemy.aiUpdateRemaining = std::max(
        0.0, enemy.aiUpdateRemaining - deltaSeconds);
    enemy.markRemaining = std::max(0.0, enemy.markRemaining - deltaSeconds);
    enemy.slowRemaining = std::max(0.0, enemy.slowRemaining - deltaSeconds);
    if (enemy.slowRemaining <= 0.0) {
        enemy.movementMultiplier = 1.0;
    }
    for (EnemyStatusEffect& status : enemy.statusEffects) {
        status.immunityRemaining = std::max(
            0.0, status.immunityRemaining - deltaSeconds);
        if (status.type == StatusEffectType::Poison && status.remaining > 0.0) continue;
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
}

std::span<const EnemyAttack> EnemySystem::tick(
    double deltaSeconds, const std::vector<BuildingInstance>& buildings,
    const FlowField& flowField, std::optional<Vec3> playerPosition,
    std::span<const EnemyStructureTarget> additionalStructures,
    const TerrainHeightfield* terrain,
    EnemyNavigationView navigation,
    bool prioritizePlayerTarget) {
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
    magicEventBuffer_.clear();
    performanceStats_.fullAiUpdates = 0U;
    performanceStats_.throttledAiMoves = 0U;
    trackPlayerVelocity(deltaSeconds, playerPosition);
    updateProjectiles(
        deltaSeconds, buildings, playerPosition,
        additionalStructures, terrain);
    const auto core =
        std::find_if(buildings.begin(), buildings.end(), [](const BuildingInstance& building) {
            return building.type == BuildingType::Core;
        });
    if (core == buildings.end() &&
        (!prioritizePlayerTarget || !playerPosition)) {
        updatePendingSplits(deltaSeconds);
        if (spatialHashDirty_) {
            rebuildSpatialIndex();
        }
        finishTelemetry();
        return attackBuffer_;
    }
    const Vec3 coreWorldPosition =
        prioritizePlayerTarget && playerPosition
            ? *playerPosition
            : buildingWorldPosition(*core);
    std::shared_ptr<MultiLevelNavigation> multiLevelNavigation;
    if (navigation.revision != 0U &&
        navigationCache_ &&
        cachedNavigationRevision_ == navigation.revision) {
        multiLevelNavigation = navigationCache_;
    } else {
        multiLevelNavigation = makeMultiLevelNavigation(
            navigation, coreWorldPosition,
            core == buildings.end() ? -1 : core->platformStorey);
        if (navigation.revision != 0U) {
            navigationCache_ = multiLevelNavigation;
            cachedNavigationRevision_ = navigation.revision;
        }
    }

    const bool structureLayoutChanged = refreshStructureTargets(
        buildings, additionalStructures);

    if (spatialHashDirty_) {
        rebuildSpatialIndex();
    }
    updateDecoyBeacons(deltaSeconds, buildings, flowField, navigation, structureLayoutChanged);
    const BuildingQueryGrid buildingGrid(
        structureBuffer_, structureNextBuffer_,
        structureGridHeads_, structureLayoutChanged);

    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }

        updateEnemySurface(
            enemy, terrain, navigation, deltaSeconds);

        updateEnemyTimers(enemy, deltaSeconds);
        if (enemyHasStatus(enemy, StatusEffectType::Freeze)) {
            enemy.knockbackVelocity = {};
            continue;
        }
        enemy.steeringTime += deltaSeconds;
        const double waterMultiplier =
            terrain != nullptr && !enemyIsFlying(enemy.type)
                ? terrain->waterMovementMultiplier(
                      enemy.position.x, enemy.position.z)
                : 1.0;
        const double bossPhaseSpeed =
            enemy.type == EnemyType::Boss && enemy.bossPhase >= 3
                ? 1.22
                : 1.0;
        const double movementSpeed =
            enemy.speed * enemy.movementMultiplier * waterMultiplier *
            (berserkerActive(enemy) ? 1.28 : 1.0) *
            bossPhaseSpeed;

        if(enemy.arrivalPortal && enemy.portalArrivalRemaining>0 && enemy.type!=EnemyType::Boss) {
            enemy.yaw=enemy.portalExitYaw;
            enemy.state=EnemyState::MoveToCore;
            moveEnemyHorizontally(enemy,{std::sin(enemy.portalExitYaw)*movementSpeed*deltaSeconds,0,
                std::cos(enemy.portalExitYaw)*movementSpeed*deltaSeconds},navigation);
            continue;
        }
        if (advanceBossAbility(enemy, deltaSeconds, playerPosition, coreWorldPosition, terrain, navigation)) {
            continue;
        }
        const double knockbackSpeed = std::hypot(
            enemy.knockbackVelocity.x,
            enemy.knockbackVelocity.z);
        moveEnemyHorizontally(
            enemy,
            {
                enemy.knockbackVelocity.x * deltaSeconds,
                0.0,
                enemy.knockbackVelocity.z * deltaSeconds,
            },
            navigation);
        const double knockbackDecay = std::max(0.0, 1.0 - 5.0 * deltaSeconds);
        enemy.knockbackVelocity.x *= knockbackDecay;
        enemy.knockbackVelocity.z *= knockbackDecay;

        // Only an impulse stronger than locomotion owns movement. Weak hits
        // still displace the body, but cannot repeatedly switch off its chase.
        // Strong melee/blower impulses retain their full initial push.
        const double minimumKnockbackSpeed = std::max(0.05, movementSpeed);
        if (knockbackSpeed > minimumKnockbackSpeed) {
            continue;
        }

        if (enemy.type == EnemyType::AirTank) {
            tickAirTank(enemy, deltaSeconds, playerPosition, prioritizePlayerTarget);
            continue;
        }
        if(enemy.type==EnemyType::Bee) {
            tickBee(enemy,deltaSeconds,playerPosition);
            continue;
        }
        const double terrainHeight = terrain != nullptr
            ? terrain->getHeight(
                  enemy.position.x, enemy.position.z)
            : 0.0;
        const auto lureWaypoint = decoyWaypoint(enemy);
        const auto lureTarget = enemy.decoyTarget
            ? std::ranges::find(structureBuffer_, *enemy.decoyTarget, &EnemyStructureTarget::id)
            : structureBuffer_.end();
        const bool lured = lureWaypoint && lureTarget != structureBuffer_.end();
        const auto navigationWaypoint = lured ? lureWaypoint :
            prioritizePlayerTarget || enemyIsFlying(enemy.type)
                ? std::optional<Vec3>{}
                : multiLevelWaypoint(
                      *multiLevelNavigation, enemy,
                      terrainHeight, coreWorldPosition);
        const double aiInterval = lured ? 0.0 : navigationWaypoint
            ? 0.0
            : aiUpdateInterval(
                  enemy, playerPosition, coreWorldPosition);
        if (aiInterval > 0.0 &&
            enemy.aiUpdateRemaining > 0.0) {
            moveEnemyHorizontally(
                enemy,
                {
                    std::sin(enemy.yaw) *
                        movementSpeed * deltaSeconds,
                    0.0,
                    std::cos(enemy.yaw) *
                        movementSpeed * deltaSeconds,
                },
                navigation);
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
                const double phaseCooldownMultiplier =
                    enemy.bossPhase >= 3
                        ? 0.55
                        : enemy.bossPhase >= 2 ? 0.8 : 1.0;
                enemy.ramCooldownRemaining =
                    enemy.ramCooldown * phaseCooldownMultiplier;
            }
            continue;
        }

        // A navigation waypoint only describes the route to the core. It
        // must not suppress aggro: playerSharesAttackHeight below is the
        // authoritative same-storey check.
        if (playerPosition && !lured) {
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
            if ((prioritizePlayerTarget ||
                 playerDistanceSquared <= aggroRange * aggroRange) &&
                playerSharesAttackHeight(
                    enemy, *playerPosition) &&
                (prioritizePlayerTarget ||
                 !buildingIsInAttackRange(
                    enemy, buildingGrid)) &&
                (prioritizePlayerTarget ||
                 !buildingBlocksPathToPlayer(
                     enemy, buildingGrid,
                     *playerPosition))) {
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
                double approachDirectionX = directionX;
                double approachDirectionZ = directionZ;
                if (enemy.type == EnemyType::Basic &&
                    enemy.approachRole != EnemyApproachRole::Direct &&
                    playerDistance > PlayerAttackRange + 0.65) {
                    const double side =
                        enemy.approachRole == EnemyApproachRole::FlankLeft
                            ? -1.0
                            : 1.0;
                    const double flankWidth = std::clamp(
                        (playerDistance - PlayerAttackRange) * 0.58,
                        0.8, 2.5);
                    const double flankOffsetX =
                        -directionZ * side * flankWidth;
                    const double flankOffsetZ =
                        directionX * side * flankWidth;
                    const double flankX = playerOffsetX + flankOffsetX;
                    const double flankZ = playerOffsetZ + flankOffsetZ;
                    const double flankLength =
                        std::hypot(flankX, flankZ);
                    if (flankLength > 1e-9) {
                        approachDirectionX = flankX / flankLength;
                        approachDirectionZ = flankZ / flankLength;
                    }
                }
                if (playerDistance > playerAttackRange(enemy.type)) {
                    const auto heading = steerAroundWorldScenery(
                        enemy, {approachDirectionX, 0.0, approachDirectionZ}, navigation);
                    approachDirectionX = heading.x;
                    approachDirectionZ = heading.z;
                }
                double aimDirectionX = approachDirectionX;
                double aimDirectionZ = approachDirectionZ;
                if (enemy.type == EnemyType::Ranged) {
                    constexpr double ProjectileSpeed = 7.2;
                    const double leadSeconds = std::clamp(
                        playerDistance / ProjectileSpeed,
                        0.0, 0.72);
                    const double aimX = playerOffsetX +
                        estimatedPlayerVelocity_.x * leadSeconds;
                    const double aimZ = playerOffsetZ +
                        estimatedPlayerVelocity_.z * leadSeconds;
                    const double aimLength = std::hypot(aimX, aimZ);
                    if (aimLength > 1e-9) {
                        aimDirectionX = aimX / aimLength;
                        aimDirectionZ = aimZ / aimLength;
                    }
                }
                turnToward(
                    enemy,
                    std::atan2(aimDirectionX, aimDirectionZ),
                    aiTurnDeltaSeconds);
                enemy.target.reset();
                const double playerRange =
                    playerAttackRange(enemy.type);
                if (playerDistance <= playerRange) {
                    enemy.state = EnemyState::AttackPlayer;
                    if (enemy.attackCooldownRemaining <= 0.0) {
                        if (enemy.type == EnemyType::Ranged) {
                            const double originSurface = terrain
                                ? terrain->getHeight(
                                      enemy.position.x,
                                      enemy.position.z) +
                                      enemy.surfaceHeightOffset
                                : enemy.worldSurfaceHeight;
                            const Vec3 origin{
                                enemy.position.x,
                                originSurface + 1.02,
                                enemy.position.z};
                            constexpr double ProjectileSpeed = 7.2;
                            const double leadSeconds = std::clamp(
                                playerDistance / ProjectileSpeed,
                                0.0, 0.72);
                            const Vec3 target{
                                playerPosition->x +
                                    estimatedPlayerVelocity_.x *
                                        leadSeconds,
                                playerPosition->y - 0.62,
                                playerPosition->z +
                                    estimatedPlayerVelocity_.z *
                                        leadSeconds};
                            const double dx = target.x - origin.x;
                            const double dy = target.y - origin.y;
                            const double dz = target.z - origin.z;
                            const double length = std::max(
                                1e-6, std::sqrt(dx * dx + dy * dy + dz * dz));
                            projectiles_.push_back({
                                .id = {nextProjectileIndex_++, 1U},
                                .ownerId = enemy.id,
                                .targetId = std::nullopt,
                                .position = origin,
                                .targetPosition = target,
                                .previousPosition = origin,
                                .velocity = {
                                    dx / length * ProjectileSpeed,
                                    dy / length * ProjectileSpeed,
                                    dz / length * ProjectileSpeed},
                                .damage = enemy.damage,
                                .radius = 0.24,
                                .targetRadius = 0.42,
                                .lifetimeRemaining = 3.0,
                                .targetsPlayer = true,
                            });
                            magicEventBuffer_.push_back({enemy.id, {
                                origin.x + dx / length * 0.65,
                                origin.y + dy / length * 0.65,
                                origin.z + dz / length * 0.65}});
                        } else {
                            playerAttackBuffer_.push_back(
                                {enemy.id, enemy.damage});
                        }
                        enemy.attackCooldownRemaining =
                            attackInterval(enemy.type);
                    }
                    if (enemy.type == EnemyType::Ranged) {
                        constexpr double RetreatDistance = 4.5;
                        constexpr double PreferredDistance = 6.4;
                        constexpr double ApproachDistance = 7.6;
                        double moveX = 0.0;
                        double moveZ = 0.0;
                        if (playerDistance < RetreatDistance) {
                            moveX = -directionX;
                            moveZ = -directionZ;
                        } else if (playerDistance > ApproachDistance) {
                            moveX = directionX;
                            moveZ = directionZ;
                        } else {
                            const double strafeSide =
                                std::sin(enemy.steeringPhase) < 0.0
                                    ? -1.0
                                    : 1.0;
                            moveX = directionZ * strafeSide;
                            moveZ = -directionX * strafeSide;
                            const double radialCorrection = std::clamp(
                                (playerDistance - PreferredDistance) *
                                    0.38,
                                -0.45, 0.45);
                            moveX += directionX * radialCorrection;
                            moveZ += directionZ * radialCorrection;
                            const double moveLength =
                                std::hypot(moveX, moveZ);
                            if (moveLength > 1e-9) {
                                moveX /= moveLength;
                                moveZ /= moveLength;
                            }
                        }
                        const auto heading = steerAroundWorldScenery(
                            enemy, {moveX, 0.0, moveZ}, navigation);
                        moveEnemyHorizontally(
                            enemy,
                            {
                                heading.x * movementSpeed *
                                    deltaSeconds * 0.62,
                                0.0,
                                heading.z * movementSpeed *
                                    deltaSeconds * 0.62,
                            },
                            navigation);
                    }
                } else {
                    enemy.state = EnemyState::ChasePlayer;
                    const double movement = std::min(
                        movementSpeed * deltaSeconds,
                        playerDistance - playerRange);
                    moveEnemyHorizontally(
                        enemy,
                        {
                            approachDirectionX * movement,
                            0.0,
                            approachDirectionZ * movement,
                        },
                        navigation);
                }
                continue;
            }
        }

        const double coreX = lured ? lureTarget->position.x : coreWorldPosition.x;
        const double coreZ = lured ? lureTarget->position.z : coreWorldPosition.z;
        const double toCoreX = coreX - enemy.position.x;
        const double toCoreZ = coreZ - enemy.position.z;
        const double coreDistance = std::sqrt((toCoreX * toCoreX) + (toCoreZ * toCoreZ));
        if (coreDistance <= 1e-9) {
            continue;
        }
        double directionX = toCoreX / coreDistance;
        double directionZ = toCoreZ / coreDistance;
        const auto flowDirection =
            prioritizePlayerTarget || enemyIsFlying(enemy.type) ||
                    navigationWaypoint
                ? std::optional<Vec3>{}
                : flowField.directionAt(enemy.position);
        if (navigationWaypoint) {
            const double waypointX =
                navigationWaypoint->x - enemy.position.x;
            const double waypointZ =
                navigationWaypoint->z - enemy.position.z;
            const double waypointDistance =
                std::hypot(waypointX, waypointZ);
            if (waypointDistance > 1e-9) {
                directionX = waypointX / waypointDistance;
                directionZ = waypointZ / waypointDistance;
            }
        }
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
            const double separationWeight = navigationWaypoint
                ? SeparationWeight * 0.28
                : SeparationWeight;
            directionX +=
                (separationX / separationLength) * separationWeight;
            directionZ +=
                (separationZ / separationLength) * separationWeight;
            const double combinedLength =
                std::sqrt((directionX * directionX) + (directionZ * directionZ));
            directionX /= combinedLength;
            directionZ /= combinedLength;
        }
        const double wander = navigationWaypoint
            ? 0.0
            :
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
        const auto sceneryHeading = steerAroundWorldScenery(
            enemy, {directionX, 0.0, directionZ}, navigation);
        directionX = sceneryHeading.x;
        directionZ = sceneryHeading.z;
        turnToward(
            enemy, std::atan2(directionX, directionZ),
            aiTurnDeltaSeconds);
        if (!navigationWaypoint) {
            directionX = std::sin(enemy.yaw);
            directionZ = std::cos(enemy.yaw);
        }

        const StructureTargetSelection targetSelection =
            selectStructureTarget(
                enemy, buildingGrid, directionX, directionZ, {coreX,0,coreZ}, lured);
        const double lureContactDistance=lured
            ? std::max(0.0,coreDistance-lureTarget->radius-enemyRadius(enemy)) : 0;
        const bool approachBlocked=lured && !enemyIsFlying(enemy.type) && targetSelection.target &&
            targetSelection.contactDistance<=lureContactDistance;
        const EnemyStructureTarget* blocker = lured && !approachBlocked ? &*lureTarget : targetSelection.target;
        double closestContactDistance = lured && !approachBlocked ? lureContactDistance : targetSelection.contactDistance;

        if (blocker == nullptr) {
            enemy.state = EnemyState::MoveToCore;
            enemy.target.reset();
            moveEnemyHorizontally(
                enemy,
                {
                    directionX * movementSpeed * deltaSeconds,
                    0.0,
                    directionZ * movementSpeed * deltaSeconds,
                },
                navigation);
            continue;
        }

        if ((enemy.type == EnemyType::Sapper && blocker->structuralImpact > 0U) ||
            ((enemy.type != EnemyType::Ranged || lured) &&
             closestContactDistance <= AttackRange + .75)) {
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
                // Once a real blocker is close, close the gap toward its
                // body instead of orbiting a path waypoint beside the wall.
                directionX = offsetX / distance;
                directionZ = offsetZ / distance;
                closestContactDistance = std::max(
                    0.0,
                    distance - blocker->radius -
                        enemyRadius(enemy));
            }
        }

        const double enemyAttackRange = lured ? AttackRange :
            attackRange(enemy.type);
        if (closestContactDistance > enemyAttackRange + 1e-6) {
            const double movement =
                std::min(movementSpeed * deltaSeconds,
                         closestContactDistance -
                             enemyAttackRange);
            moveEnemyHorizontally(
                enemy,
                {
                    directionX * movement,
                    0.0,
                    directionZ * movement,
                },
                navigation);
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
            const double phaseWindupMultiplier =
                enemy.bossPhase >= 3
                    ? 0.72
                    : enemy.bossPhase >= 2 ? 0.86 : 1.0;
            enemy.ramWindupRemaining =
                enemy.ramWindup * phaseWindupMultiplier;
            continue;
        }
        enemy.state =
            blocker->buildingType == BuildingType::Core
                ? EnemyState::AttackCore
                : EnemyState::AttackBuilding;
        if (enemy.attackCooldownRemaining <= 0.0) {
            const double damage = blocker->buildingType
                ? buildingDamage(enemy, *blocker->buildingType)
                : (enemy.type == EnemyType::Sapper && blocker->modular
                       ? enemy.damage * 2.5
                       : enemy.damage);
            if (enemy.type == EnemyType::Ranged) {
                const double originSurface = terrain
                    ? terrain->getHeight(
                          enemy.position.x, enemy.position.z) +
                          enemy.surfaceHeightOffset
                    : enemy.worldSurfaceHeight;
                const Vec3 origin{
                    enemy.position.x, originSurface + 1.02,
                    enemy.position.z};
                const Vec3 target = structureProjectileAim(*blocker);
                const double dx = target.x - origin.x;
                const double dy = target.y - origin.y;
                const double dz = target.z - origin.z;
                const double length = std::max(
                    1e-6, std::sqrt(dx * dx + dy * dy + dz * dz));
                constexpr double ProjectileSpeed = 7.2;
                projectiles_.push_back({
                    .id = {nextProjectileIndex_++, 1U},
                    .ownerId = enemy.id,
                    .targetId = blocker->id,
                    .position = origin,
                    .targetPosition = target,
                    .previousPosition = origin,
                    .velocity = {
                        dx / length * ProjectileSpeed,
                        dy / length * ProjectileSpeed,
                        dz / length * ProjectileSpeed},
                    .damage = damage,
                    .radius = 0.24,
                    .targetRadius = std::clamp(
                        blocker->radius * 0.45, 0.32, 0.82),
                    .lifetimeRemaining = 3.0,
                    .targetsPlayer = false,
                });
                magicEventBuffer_.push_back({enemy.id, {
                    origin.x + dx / length * 0.65,
                    origin.y + dy / length * 0.65,
                    origin.z + dz / length * 0.65}});
            } else {
                attackBuffer_.push_back({
                    enemy.id, blocker->id, damage, false});
            }
            enemy.attackCooldownRemaining =
                attackInterval(enemy.type);
        }
    }

    const auto collisionStart = PerformanceClock::now();
    resolveEnemyCapsuleCollisions(
        enemies_, structureBuffer_, collisionEnemyLinks_,
        structureNextBuffer_, structureGridHeads_);
    for (EnemyInstance& enemy : enemies_) {
        if (enemy.active) {
            if(enemy.arrivalPortal && enemy.portalArrivalRemaining<=0)
                enemy.arrivalPortal.reset();
            updateEnemySurface(
                enemy, terrain, navigation, deltaSeconds);
        }
    }
    performanceStats_.collision.sample(
        performanceMilliseconds(collisionStart));
    updatePendingSplits(deltaSeconds);
    rebuildSpatialIndex();
    finishTelemetry();
    return attackBuffer_;
}

void EnemySystem::markEnemyDead(EnemyInstance& enemy) {
    if (!enemy.active) {
        return;
    }
    if(enemy.type==EnemyType::Boss) {
        for(auto& unit:enemies_) if(unit.active && unit.summonerId==enemy.id) {
            unit.active=false;unit.state=EnemyState::Dead;unit.health=0;
            if(activeCount_>0) --activeCount_;
        }
        std::erase_if(projectiles_,[&](const EnemyProjectile& projectile){return projectile.ownerId==enemy.id;});
        enemy.bossStrikesRemaining=0;
    }
    enemy.active = false;
    enemy.state = EnemyState::Dead;
    enemy.target.reset();
    if (enemy.eliteAffixes != 0U) {
        eliteDeathEventBuffer_.push_back({
            .id = enemy.id,
            .position = enemy.position,
            .affixes = enemy.eliteAffixes,
        });
    }
    if (activeCount_ > 0U) {
        --activeCount_;
    }
    spatialHashDirty_ = true;
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
    const auto slot = slotIndex(id);
    if (!slot || enemies_[*slot].id != id) {
        return nullptr;
    }
    return &enemies_[*slot];
}

const EnemyInstance* EnemySystem::findEnemy(EntityId id) const {
    const auto slot = slotIndex(id);
    if (!slot || enemies_[*slot].id != id) {
        return nullptr;
    }
    return &enemies_[*slot];
}

} // namespace ian
