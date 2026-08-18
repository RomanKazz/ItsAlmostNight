#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
void Simulation::raisePlayerOntoGroundFrame(
    const PlatformFrameInstance& frame) {
    if (frame.storey != 0) {
        return;
    }
    const double cellSize =
        worldConfig_.cellSize;
    const double minimumX =
        frame.anchor.x * cellSize;
    const double maximumX =
        (frame.anchor.x +
         PlatformFrameWidthCells) *
        cellSize;
    const double minimumZ =
        frame.anchor.z * cellSize;
    const double maximumZ =
        (frame.anchor.z +
         PlatformFrameWidthCells) *
        cellSize;
    const double playerFeet =
        playerPosition_.y -
        gameplay_.eyeHeight;
    if (playerPosition_.x < minimumX ||
        playerPosition_.x > maximumX ||
        playerPosition_.z < minimumZ ||
        playerPosition_.z > maximumZ ||
        playerFeet >= frame.floorHeight - 1e-6) {
        return;
    }
    playerPosition_.y =
        frame.floorHeight +
        gameplay_.eyeHeight;
    verticalVelocity_ = 0.0;
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.085;
    lastGroundSurfaceHeight_ = frame.floorHeight;
    playerGrounded_ = true;
}

bool Simulation::shouldAutoJumpGroundFrame(
    Vec3 movement) const {
    const double movementLength =
        std::hypot(movement.x, movement.z);
    if (movementLength <= 1e-9) {
        return false;
    }
    constexpr double MaximumStepUp = 0.65;
    const double maximumJumpRise =
        gameplay_.jumpSpeed *
            gameplay_.jumpSpeed /
        (2.0 * gameplay_.gravity);
    const double playerFeet =
        playerPosition_.y -
        gameplay_.eyeHeight;
    const Vec3 next{
        playerPosition_.x + movement.x,
        playerPosition_.y,
        playerPosition_.z + movement.z,
    };
    const double cellSize =
        worldConfig_.cellSize;
    const auto distanceSquared =
        [](Vec3 point, double minimumX,
           double maximumX, double minimumZ,
           double maximumZ) {
            const double closestX =
                std::clamp(
                    point.x, minimumX, maximumX);
            const double closestZ =
                std::clamp(
                    point.z, minimumZ, maximumZ);
            const double deltaX =
                point.x - closestX;
            const double deltaZ =
                point.z - closestZ;
            return deltaX * deltaX +
                   deltaZ * deltaZ;
        };
    const double radiusSquared =
        CollisionWorld::PlayerRadius *
        CollisionWorld::PlayerRadius;
    for (const PlatformFrameInstance& frame :
         foundations_.platformFrames()) {
        if (frame.storey != 0) {
            continue;
        }
        const double rise =
            frame.floorHeight - playerFeet;
        if (rise <= MaximumStepUp ||
            rise > maximumJumpRise + 0.10) {
            continue;
        }
        const double minimumX =
            frame.anchor.x * cellSize;
        const double maximumX =
            (frame.anchor.x +
             PlatformFrameWidthCells) *
            cellSize;
        const double minimumZ =
            frame.anchor.z * cellSize;
        const double maximumZ =
            (frame.anchor.z +
             PlatformFrameWidthCells) *
            cellSize;
        const double currentDistance =
            distanceSquared(
                playerPosition_, minimumX,
                maximumX, minimumZ, maximumZ);
        const double nextDistance =
            distanceSquared(
                next, minimumX, maximumX,
                minimumZ, maximumZ);
        if (currentDistance >
                radiusSquared &&
            nextDistance <= radiusSquared &&
            nextDistance < currentDistance) {
            return true;
        }
    }
    return false;
}

bool Simulation::modularRemovalWouldDestroyCore(
    EntityId id) const {
    const auto core = buildings_.core();
    if (!core || core->platformStorey < 0) {
        return false;
    }
    const auto frame = std::find_if(
        foundations_.platformFrames().begin(),
        foundations_.platformFrames().end(),
        [id](const PlatformFrameInstance& candidate) {
            return candidate.id == id;
        });
    if (frame ==
        foundations_.platformFrames().end()) {
        return false;
    }
    const int coreAnchorX =
        snapPlatformFrameAxis(
            core->gridPosition.x - 1);
    const int coreAnchorZ =
        snapPlatformFrameAxis(
            core->gridPosition.z - 1);
    return frame->anchor.x == coreAnchorX &&
           frame->anchor.z == coreAnchorZ &&
           frame->storey <= core->platformStorey;
}

void Simulation::syncBuildingRuntimeSystems() {
    towers_.syncBuildings(buildings_.buildings());
    cannons_.syncBuildings(buildings_.buildings());
    traps_.syncBuildings(buildings_.buildings());
    crystalMines_.syncBuildings(buildings_.buildings());
    // Capacity is derived from the current Core level. Keep inventory valid
    // after loading or any structural transaction involving the Core.
    clampResourcesToCapacity();
}

void Simulation::syncWorldStructures() {
    ++structuralRevision_;
    resources_.tick(
        0.0, buildings_.buildings(),
        map_.worldLimit, playerPosition_);
    if (resources_.consumeCollisionGeometryDirty()) {
        collisionWorld_.syncResourceCylinders(
            resources_.nodes(), treeCollisionAssets_,
            stoneCollisionAssets_);
    }
    collisionWorld_.syncBuildings(buildings_.buildings());
    syncBuildingRuntimeSystems();
    const auto core = buildings_.core();
    if (core) {
        flowField_.rebuild(core->gridPosition, buildings_.buildings());
        flowDebugVectors_ = flowField_.debugVectors();
    } else {
        flowField_.reset();
        flowDebugVectors_.clear();
    }
}

void Simulation::syncModularStructures() {
    ++structuralRevision_;
    if (modularPlacementBatchDepth_ > 0) {
        modularStructuresDirty_ = true;
        return;
    }
    collisionWorld_.syncModularBuildings({
        foundations_.platformFrames(),
        foundations_.walls(),
        foundations_.ramps(),
        worldConfig_.cellSize,
        platformCollisionAsset_.colliders,
        rampCollisionAsset_.colliders,
    });
}

void Simulation::beginModularPlacementBatch() {
    invalidateSnapshotCache();
    ++modularPlacementBatchDepth_;
}

void Simulation::endModularPlacementBatch() {
    invalidateSnapshotCache();
    if (modularPlacementBatchDepth_ <= 0) {
        return;
    }
    --modularPlacementBatchDepth_;
    if (modularPlacementBatchDepth_ == 0 &&
        modularStructuresDirty_) {
        modularStructuresDirty_ = false;
        syncModularStructures();
    }
}

void Simulation::removeUnsupportedPlatformBuildings() {
    std::vector<EntityId> unsupported;
    for (const BuildingInstance& building :
         buildings_.buildings()) {
        if (building.platformStorey < 0) {
            continue;
        }
        const bool twoByTwo =
            buildingFootprintHalfExtent(
                building.type) > 0.75;
        const int widthCells = twoByTwo ? 2 : 1;
        const int minimumX =
            twoByTwo
                ? building.gridPosition.x - 1
                : building.gridPosition.x;
        const int minimumZ =
            twoByTwo
                ? building.gridPosition.z - 1
                : building.gridPosition.z;
        const auto surface =
            foundations_.buildingSurface(
                minimumX, minimumZ, widthCells);
        if (!surface ||
            surface->storey !=
                building.platformStorey ||
            std::abs(
                surface->height -
                building.baseHeight) > 0.05) {
            unsupported.push_back(building.id);
        }
    }
    if (unsupported.empty()) {
        return;
    }
    for (EntityId id : unsupported) {
        const auto removed = buildings_.remove(id);
        if (!removed) {
            continue;
        }
        events_.push_back({
            .type = GameEventType::BuildingDestroyed,
            .entityId = removed->id,
            .buildingType = removed->type,
            .building = *removed,
            .position =
                buildingWorldPosition(*removed),
        });
    }
    aimedBuilding_.reset();
    syncWorldStructures();
}

} // namespace ian
