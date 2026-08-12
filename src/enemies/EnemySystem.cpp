#include "enemies/EnemySystem.hpp"

#include "core/Geometry.hpp"

#include "world/TerrainHeightfield.hpp"
#include "world/CollisionWorld.hpp"
#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <limits>
#include <map>
#include <queue>
#include <tuple>
#include <utility>

namespace ian {
namespace {

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

struct TraversalEdge {
    int to{-1};
    const RampInstance* ramp{};
};

struct RampEndpoints {
    Vec3 lowInside;
    Vec3 highInside;
    Vec3 lowConnection;
    Vec3 highConnection;
};

RampEndpoints rampEndpoints(
    const RampInstance& ramp, double cellSize) {
    const bool alongZ =
        ramp.rotation == Rotation::Deg0 ||
        ramp.rotation == Rotation::Deg180;
    const double minimumX = ramp.anchor.x * cellSize;
    const double maximumX =
        (ramp.anchor.x +
         (alongZ ? ModularRampWidthCells
                 : ModularRampRunCells)) * cellSize;
    const double minimumZ = ramp.anchor.z * cellSize;
    const double maximumZ =
        (ramp.anchor.z +
         (alongZ ? ModularRampRunCells
                 : ModularRampWidthCells)) * cellSize;
    const double inset = std::min(0.18, cellSize * 0.18);
    const double connection = std::min(0.12, cellSize * 0.12);
    const double centerX = (minimumX + maximumX) * 0.5;
    const double centerZ = (minimumZ + maximumZ) * 0.5;
    switch (ramp.rotation) {
    case Rotation::Deg0:
        return {
            {centerX, ramp.bottomHeight, minimumZ + inset},
            {centerX, ramp.topHeight, maximumZ - inset},
            {centerX, ramp.bottomHeight, minimumZ - connection},
            {centerX, ramp.topHeight, maximumZ + connection},
        };
    case Rotation::Deg90:
        return {
            {maximumX - inset, ramp.bottomHeight, centerZ},
            {minimumX + inset, ramp.topHeight, centerZ},
            {maximumX + connection, ramp.bottomHeight, centerZ},
            {minimumX - connection, ramp.topHeight, centerZ},
        };
    case Rotation::Deg180:
        return {
            {centerX, ramp.bottomHeight, maximumZ - inset},
            {centerX, ramp.topHeight, minimumZ + inset},
            {centerX, ramp.bottomHeight, maximumZ + connection},
            {centerX, ramp.topHeight, minimumZ - connection},
        };
    case Rotation::Deg270:
        return {
            {minimumX + inset, ramp.bottomHeight, centerZ},
            {maximumX - inset, ramp.topHeight, centerZ},
            {minimumX - connection, ramp.bottomHeight, centerZ},
            {maximumX + connection, ramp.topHeight, centerZ},
        };
    }
    return {};
}

class MultiLevelNavigation {
  public:
    MultiLevelNavigation(
        EnemyNavigationView view, Vec3 corePosition,
        int coreStorey)
        : view_(view), groundNode_(
              static_cast<int>(view.platformFrames.size())) {
        if (coreStorey < 0 || view.platformFrames.empty()) {
            return;
        }
        edges_.resize(view.platformFrames.size() + 1U);
        for (std::size_t index = 0;
             index < view.platformFrames.size(); ++index) {
            const PlatformFrameInstance& frame =
                view.platformFrames[index];
            if (frame.supportState !=
                StructuralSupportState::Supported) {
                continue;
            }
            frameIndex_.emplace(
                std::tuple{
                    frame.storey,
                    frame.anchor.x,
                    frame.anchor.z},
                static_cast<int>(index));
            maximumStorey_ = std::max(
                maximumStorey_, frame.storey);
        }
        for (std::size_t left = 0;
             left < view.platformFrames.size(); ++left) {
            const PlatformFrameInstance& a =
                view.platformFrames[left];
            if (a.supportState !=
                StructuralSupportState::Supported) {
                continue;
            }
            if (a.storey == 0) {
                connect(groundNode_, static_cast<int>(left), nullptr);
            }
            constexpr std::array<GridCoord, 2> ForwardNeighbours{{
                {PlatformFrameWidthCells, 0, 0},
                {0, 0, PlatformFrameWidthCells},
            }};
            for (const GridCoord offset : ForwardNeighbours) {
                const auto neighbour = frameIndex_.find(
                    std::tuple{
                        a.storey,
                        a.anchor.x + offset.x,
                        a.anchor.z + offset.z});
                if (neighbour == frameIndex_.end()) {
                    continue;
                }
                const PlatformFrameInstance& b =
                    view.platformFrames[
                        static_cast<std::size_t>(neighbour->second)];
                if (std::abs(a.floorHeight - b.floorHeight) <= 1e-4) {
                    connect(
                        static_cast<int>(left),
                        neighbour->second, nullptr);
                }
            }
        }
        goalNode_ = frameAt(
            corePosition, coreStorey, corePosition.y);
        if (goalNode_ < 0) {
            return;
        }
        for (const RampInstance& ramp : view.ramps) {
            if (ramp.supportState !=
                StructuralSupportState::Supported) {
                continue;
            }
            const RampEndpoints endpoints =
                rampEndpoints(ramp, view.cellSize);
            const int low = frameAt(
                endpoints.lowConnection,
                ramp.targetStorey - 1,
                ramp.bottomHeight);
            const int high = frameAt(
                endpoints.highConnection,
                ramp.targetStorey,
                ramp.topHeight);
            if (low >= 0 && high >= 0) {
                connect(low, high, &ramp);
            }
        }
        buildRoutes();
    }

    [[nodiscard]] std::optional<Vec3> waypoint(
        const EnemyInstance& enemy, double terrainHeight,
        Vec3 corePosition) const {
        if (!valid()) {
            return std::nullopt;
        }
        const double surfaceHeight =
            terrainHeight + enemy.surfaceHeightOffset;
        for (const RampInstance& ramp : view_.ramps) {
            const RampEndpoints endpoints =
                rampEndpoints(ramp, view_.cellSize);
            if (!insideRamp(enemy.position, ramp) ||
                std::abs(surfaceHeight - ramp.bottomHeight) >
                    std::abs(ramp.topHeight - ramp.bottomHeight) + 0.6) {
                continue;
            }
            const int high = frameAt(
                endpoints.highConnection,
                ramp.targetStorey, ramp.topHeight);
            if (high >= 0 &&
                distance_[static_cast<std::size_t>(high)] >= 0) {
                return endpoints.highInside;
            }
        }

        int current = groundNode_;
        for (int storey = 0;
             storey <= maximumStorey_; ++storey) {
            const int frame = frameAt(
                enemy.position, storey, surfaceHeight);
            if (frame >= 0) {
                current = frame;
                break;
            }
        }
        if (current == goalNode_) {
            return corePosition;
        }
        if (current < 0 ||
            current >= static_cast<int>(next_.size()) ||
            !next_[static_cast<std::size_t>(current)]) {
            return std::nullopt;
        }
        const TraversalEdge edge =
            *next_[static_cast<std::size_t>(current)];
        if (edge.ramp) {
            return rampEndpoints(*edge.ramp, view_.cellSize).lowInside;
        }
        if (edge.to == groundNode_) {
            return std::nullopt;
        }
        return frameCenter(
            view_.platformFrames[static_cast<std::size_t>(edge.to)]);
    }

    [[nodiscard]] bool valid() const {
        return goalNode_ >= 0 &&
            groundNode_ < static_cast<int>(distance_.size()) &&
            distance_[static_cast<std::size_t>(groundNode_)] >= 0;
    }

  private:
    [[nodiscard]] Vec3 frameCenter(
        const PlatformFrameInstance& frame) const {
        return {
            (frame.anchor.x + PlatformFrameWidthCells * 0.5) *
                view_.cellSize,
            frame.floorHeight,
            (frame.anchor.z + PlatformFrameWidthCells * 0.5) *
                view_.cellSize,
        };
    }

    [[nodiscard]] bool contains(
        const PlatformFrameInstance& frame,
        Vec3 point, double margin = 0.0) const {
        const double minimumX = frame.anchor.x * view_.cellSize;
        const double maximumX =
            (frame.anchor.x + PlatformFrameWidthCells) * view_.cellSize;
        const double minimumZ = frame.anchor.z * view_.cellSize;
        const double maximumZ =
            (frame.anchor.z + PlatformFrameWidthCells) * view_.cellSize;
        return point.x >= minimumX - margin &&
            point.x <= maximumX + margin &&
            point.z >= minimumZ - margin &&
            point.z <= maximumZ + margin;
    }

    [[nodiscard]] int frameAt(
        Vec3 point, int storey, double height) const {
        const int cellX = static_cast<int>(
            std::floor(point.x / view_.cellSize));
        const int cellZ = static_cast<int>(
            std::floor(point.z / view_.cellSize));
        const auto candidate = frameIndex_.find(
            std::tuple{
                storey,
                snapPlatformFrameAxis(cellX),
                snapPlatformFrameAxis(cellZ)});
        if (candidate == frameIndex_.end()) {
            return -1;
        }
        const PlatformFrameInstance& frame =
            view_.platformFrames[
                static_cast<std::size_t>(candidate->second)];
        return std::abs(frame.floorHeight - height) < 0.65 &&
                contains(frame, point, 0.18)
            ? candidate->second
            : -1;
    }

    [[nodiscard]] bool insideRamp(
        Vec3 point, const RampInstance& ramp) const {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const double minimumX = ramp.anchor.x * view_.cellSize;
        const double maximumX =
            (ramp.anchor.x +
             (alongZ ? ModularRampWidthCells
                     : ModularRampRunCells)) * view_.cellSize;
        const double minimumZ = ramp.anchor.z * view_.cellSize;
        const double maximumZ =
            (ramp.anchor.z +
             (alongZ ? ModularRampRunCells
                     : ModularRampWidthCells)) * view_.cellSize;
        return point.x >= minimumX - 0.08 &&
            point.x <= maximumX + 0.08 &&
            point.z >= minimumZ - 0.08 &&
            point.z <= maximumZ + 0.08;
    }

    void connect(int left, int right, const RampInstance* ramp) {
        edges_[static_cast<std::size_t>(left)].push_back({right, ramp});
        edges_[static_cast<std::size_t>(right)].push_back({left, ramp});
    }

    void buildRoutes() {
        distance_.assign(edges_.size(), -1);
        next_.assign(edges_.size(), std::nullopt);
        std::queue<int> pending;
        distance_[static_cast<std::size_t>(goalNode_)] = 0;
        pending.push(goalNode_);
        while (!pending.empty()) {
            const int node = pending.front();
            pending.pop();
            for (const TraversalEdge& edge :
                 edges_[static_cast<std::size_t>(node)]) {
                if (distance_[static_cast<std::size_t>(edge.to)] >= 0) {
                    continue;
                }
                distance_[static_cast<std::size_t>(edge.to)] =
                    distance_[static_cast<std::size_t>(node)] + 1;
                next_[static_cast<std::size_t>(edge.to)] =
                    TraversalEdge{node, edge.ramp};
                pending.push(edge.to);
            }
        }
    }

    EnemyNavigationView view_;
    int groundNode_{};
    int goalNode_{-1};
    std::vector<std::vector<TraversalEdge>> edges_;
    std::vector<int> distance_;
    std::vector<std::optional<TraversalEdge>> next_;
    std::map<std::tuple<int, int, int>, int> frameIndex_;
    int maximumStorey_{};
};

std::optional<double> lockedAttackSurfaceHeight(
    const EnemyInstance& enemy,
    std::span<const EnemyStructureTarget> structures) {
    if (!enemy.target ||
        (enemy.state != EnemyState::AttackCore &&
         enemy.state != EnemyState::AttackBuilding &&
         enemy.state != EnemyState::BossRamWindup)) {
        return std::nullopt;
    }
    const auto target = std::ranges::find(
        structures, *enemy.target, &EnemyStructureTarget::id);
    if (target == structures.end() ||
        !target->attackSurfaceHeight) {
        return std::nullopt;
    }
    // Only stabilize a platform the navigation system has already reached;
    // horizontal proximity alone must never lift a ground enemy upstairs.
    constexpr double SurfaceLockTolerance = 0.5;
    if (enemy.worldSurfaceHeight + SurfaceLockTolerance <
        *target->attackSurfaceHeight) {
        return std::nullopt;
    }
    return target->attackSurfaceHeight;
}

void updateEnemySurface(
    EnemyInstance& enemy, const TerrainHeightfield* terrain,
    EnemyNavigationView navigation, double deltaSeconds,
    std::optional<double> lockedSurfaceHeight = std::nullopt) {
    if (enemy.type == EnemyType::Flying || terrain == nullptr ||
        navigation.collisionWorld == nullptr) {
        return;
    }
    const double terrainHeight = terrain->getHeight(
        enemy.position.x, enemy.position.z);
    if (lockedSurfaceHeight) {
        enemy.surfaceHeightOffset = std::max(
            0.0, *lockedSurfaceHeight - terrainHeight);
        enemy.worldSurfaceHeight =
            terrainHeight + enemy.surfaceHeightOffset;
        return;
    }
    const double currentHeight =
        terrainHeight + enemy.surfaceHeightOffset;
    // Ground-storey frames can sit above nearby uneven terrain. Enemies hop
    // onto them, while higher floors still require a ramp.
    constexpr double MaximumStepUp = 1.15;
    auto surface =
        navigation.collisionWorld->modularSurfaceHeight(
            enemy.position.x, enemy.position.z,
            currentHeight + MaximumStepUp);
    if (enemyUsesForwardSurfaceProbe(enemy.state)) {
        // Begin the hop before the capsule reaches a platform edge. Sampling
        // ahead also lets a ramp user finish the last step onto an upper
        // floor while its centre is still on the slope. Attack states are
        // intentionally excluded: their yaw follows a target while the
        // capsule is stationary, so probing along it would pull the enemy
        // between unrelated surface heights.
        const double probeDistance =
            enemyPhysicalCapsule(enemy.type).radius + 0.20;
        const double probeX = enemy.position.x +
            std::sin(enemy.yaw) * probeDistance;
        const double probeZ = enemy.position.z +
            std::cos(enemy.yaw) * probeDistance;
        const auto aheadSurface =
            navigation.collisionWorld->modularSurfaceHeight(
                probeX, probeZ,
                currentHeight + MaximumStepUp);
        if (aheadSurface &&
            (!surface || *aheadSurface > *surface)) {
            surface = aheadSurface;
        }
    }
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

void moveEnemyHorizontally(
    EnemyInstance& enemy, Vec3 delta,
    EnemyNavigationView navigation) {
    if (enemy.type == EnemyType::Flying ||
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
    case EnemyType::Splitter:
        return 0.10;
    case EnemyType::Splitling:
        return 0.22;
    case EnemyType::Basic:
        return 0.14;
    }
    return 0.14;
}

double enemyRadius(EnemyType type) {
    return enemyPhysicalCapsule(type).radius;
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
    case EnemyType::Splitter:
        return 4.0;
    case EnemyType::Splitling:
        return 5.0;
    case EnemyType::Basic:
        return 4.5;
    }
    return 4.5;
}

bool structureIsVerticallyReachable(
    const EnemyInstance& enemy,
    const EnemyStructureTarget& structure) {
    // Floors and ramps are traversal surfaces, never combat blockers. Their
    // supports can still disappear through structural dependency damage.
    if (structure.traversable || !structure.attackable) {
        return false;
    }
    if (enemy.type == EnemyType::Flying) {
        return structure.buildingType ==
               BuildingType::Core;
    }
    if (enemy.worldSurfaceHeight + 1e-6 <
        structure.minimumEnemySurfaceHeight) {
        return false;
    }
    const double maximumAttackHeight =
        maximumGroundStructureInteractionHeight(
            enemy.type,
            enemy.position.y + enemy.surfaceHeightOffset);
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

double buildingRadius(BuildingType type) {
    if (type == BuildingType::Core) {
        return 1.6;
    }
    return buildingFootprintHalfExtent(type) == 1.0
               ? 1.1
               : 0.55;
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
    areaDamageBuffer_.reserve(MaxEnemies);
    statusTargetBuffer_.reserve(MaxEnemies);
    structureBuffer_.reserve(256);
    incomingStructureBuffer_.reserve(256);
    structureNextBuffer_.reserve(256);
    structureGridHeads_.reserve(BuildingGridCellCount);
    collisionEnemyLinks_.reserve(MaxEnemies);
    areaTargetBuffer_.resize(SpatialHash::MaxEntries);
    pendingSplitBuffer_.reserve(MaxActiveEnemies);
}

void EnemySystem::reset() {
    // A new run has no enemy deaths to reward. Keeping old slots in the
    // Dead state made the coin fallback scanner interpret every enemy from
    // the previous run as a fresh kill on the first restarted tick.
    // clear() retains vector capacity, so restart stays allocation-free.
    enemies_.clear();
    attackBuffer_.clear();
    playerAttackBuffer_.clear();
    performanceStats_.fullAiUpdates = 0U;
    performanceStats_.throttledAiMoves = 0U;
    areaDamageBuffer_.clear();
    statusTargetBuffer_.clear();
    splitEventBuffer_.clear();
    pendingSplitBuffer_.clear();
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
    navigationCache_.reset();
    cachedNavigationRevision_ =
        std::numeric_limits<std::uint64_t>::max();
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
    const TerrainHeightfield* terrain,
    EnemyNavigationView navigation) {
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
    std::shared_ptr<MultiLevelNavigation> multiLevelNavigation;
    if (navigation.revision != 0U &&
        navigationCache_ &&
        cachedNavigationRevision_ == navigation.revision) {
        multiLevelNavigation =
            std::static_pointer_cast<MultiLevelNavigation>(
                navigationCache_);
    } else {
        multiLevelNavigation =
            std::make_shared<MultiLevelNavigation>(
                navigation, coreWorldPosition,
                core->platformStorey);
        if (navigation.revision != 0U) {
            navigationCache_ = multiLevelNavigation;
            cachedNavigationRevision_ = navigation.revision;
        }
    }

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
                    ? building.baseHeight - 0.45
                    : -std::numeric_limits<double>::infinity(),
            .attackSurfaceHeight =
                building.platformStorey >= 0
                    ? std::optional<double>{building.baseHeight}
                    : std::nullopt,
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

        updateEnemySurface(
            enemy, terrain, navigation, deltaSeconds,
            lockedAttackSurfaceHeight(
                enemy, structureBuffer_));

        enemy.attackCooldownRemaining =
            std::max(0.0, enemy.attackCooldownRemaining - deltaSeconds);
        enemy.hitAnimationRemaining =
            std::max(
                0.0,
                enemy.hitAnimationRemaining - deltaSeconds);
        enemy.spawnAnimationRemaining = std::max(
            0.0,
            enemy.spawnAnimationRemaining - deltaSeconds);
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

        // Knockback owns movement briefly. Otherwise chase movement in this
        // same tick can immediately cancel a melee impulse toward the player.
        constexpr double MinimumKnockbackSpeed = 0.05;
        if (knockbackSpeed > MinimumKnockbackSpeed) {
            continue;
        }

        const double terrainHeight = terrain != nullptr
            ? terrain->getHeight(
                  enemy.position.x, enemy.position.z)
            : 0.0;
        const auto navigationWaypoint =
            enemy.type == EnemyType::Flying
                ? std::optional<Vec3>{}
                : multiLevelNavigation->waypoint(
                      enemy, terrainHeight,
                      coreWorldPosition);
        const double aiInterval = navigationWaypoint
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
                enemy.ramCooldownRemaining = enemy.ramCooldown;
            }
            continue;
        }

        // A navigation waypoint only describes the route to the core. It
        // must not suppress aggro: playerSharesAttackHeight below is the
        // authoritative same-storey check.
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
                playerSharesAttackHeight(
                    enemy, *playerPosition) &&
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
                    moveEnemyHorizontally(
                        enemy,
                        {
                            std::sin(enemy.yaw) * movement,
                            0.0,
                            std::cos(enemy.yaw) * movement,
                        },
                        navigation);
                }
                continue;
            }
        }

        const double coreX = coreWorldPosition.x;
        const double coreZ = coreWorldPosition.z;
        const double toCoreX = coreX - enemy.position.x;
        const double toCoreZ = coreZ - enemy.position.z;
        const double coreDistance = std::sqrt((toCoreX * toCoreX) + (toCoreZ * toCoreZ));
        if (coreDistance <= 1e-9) {
            continue;
        }
        double directionX = toCoreX / coreDistance;
        double directionZ = toCoreZ / coreDistance;
        const auto flowDirection =
            enemy.type == EnemyType::Flying || navigationWaypoint
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
    for (EnemyInstance& enemy : enemies_) {
        if (enemy.active) {
            updateEnemySurface(
                enemy, terrain, navigation, deltaSeconds,
                lockedAttackSurfaceHeight(
                    enemy, structureBuffer_));
        }
    }
    performanceStats_.collision.sample(
        performanceMilliseconds(collisionStart));
    rebuildSpatialIndex();
    finishTelemetry();
    return attackBuffer_;
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
    const auto reusable = std::find_if(
        enemies_.begin(), enemies_.end(),
        [this](const EnemyInstance& enemy) {
            return !enemy.active &&
                std::ranges::none_of(
                    splitEventBuffer_,
                    [&enemy](const EnemySplitResult& split) {
                        return split.parentId == enemy.id;
                    });
        });
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
        .spawnAnimationRemaining =
            type == EnemyType::Splitling ? 0.38 : 0.0,
        .ramWindup = stats.ramWindup,
        .ramDamageMultiplier = stats.ramDamageMultiplier,
        .ramCooldown = stats.ramCooldown,
        .ramWindupRemaining = 0.0,
        .ramCooldownRemaining = 0.0,
        .slowRemaining = 0.0,
        .movementMultiplier = 1.0,
        .knockbackVelocity = spawn.initialKnockbackVelocity,
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

void EnemySystem::spawnSplitlings(
    EntityId parentId, Vec3 position,
    double healthMultiplier, double damageMultiplier) {
    constexpr int ChildCount = 3;
    constexpr double SpawnRadius = 0.42;
    constexpr double LaunchSpeed = 3.8;
    constexpr double TwoPi = 6.28318530717958647692;
    auto split = std::ranges::find(
        splitEventBuffer_, parentId,
        &EnemySplitResult::parentId);
    if (split == splitEventBuffer_.end()) {
        splitEventBuffer_.push_back({
            .parentId = parentId,
            .position = position,
            .childCount = 0,
        });
        split = std::prev(splitEventBuffer_.end());
    }
    const double phase = hashUnit(
        parentId.index * 0x9e3779b9U + parentId.generation) * TwoPi;
    for (int child = 0; child < ChildCount; ++child) {
        if (activeCount_ >= MaxActiveEnemies) {
            break;
        }
        const double angle = phase + TwoPi *
            static_cast<double>(child) /
            static_cast<double>(ChildCount);
        const Vec3 direction{std::cos(angle), 0.0, std::sin(angle)};
        const std::size_t before = activeCount_;
        appendEnemy({
            .type = EnemyType::Splitling,
            .position = {
                position.x + direction.x * SpawnRadius,
                0.55,
                position.z + direction.z * SpawnRadius,
            },
            .healthMultiplier = healthMultiplier,
            .damageMultiplier = damageMultiplier,
            .initialKnockbackVelocity = {
                direction.x * LaunchSpeed,
                0.0,
                direction.z * LaunchSpeed,
            },
        });
        if (activeCount_ > before) {
            ++split->childCount;
        }
    }
    spatialHashDirty_ = true;
}

void EnemySystem::markEnemyDead(EnemyInstance& enemy) {
    if (!enemy.active) {
        return;
    }
    enemy.active = false;
    enemy.state = EnemyState::Dead;
    enemy.target.reset();
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
