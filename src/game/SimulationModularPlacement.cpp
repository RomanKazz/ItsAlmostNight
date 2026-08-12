#include "game/Simulation.hpp"

#include "game/ResourceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ian {
namespace {

constexpr double ResourcePlacementClearance = 0.08;

bool resourceOverlapsBox(
    std::span<const ResourceNode> nodes,
    const CollisionBox& box) {
    return std::any_of(
        nodes.begin(), nodes.end(),
        [&box](const ResourceNode& node) {
            if (!node.active) {
                return false;
            }
            const double distanceX = std::max(
                0.0,
                std::max(
                    box.minX - node.position.x,
                    node.position.x - box.maxX));
            const double distanceY = std::max(
                0.0,
                std::max(
                    box.minimumBlockingEyeY -
                        node.position.y,
                    node.position.y -
                        box.maximumBlockingEyeY));
            const double distanceZ = std::max(
                0.0,
                std::max(
                    box.minZ - node.position.z,
                    node.position.z - box.maxZ));
            const double required =
                node.radius + ResourcePlacementClearance;
            return distanceX * distanceX +
                       distanceY * distanceY +
                       distanceZ * distanceZ <
                   required * required;
        });
}

CollisionBox platformFloorCollisionBox(
    const PlatformFramePlacement& placement,
    double cellSize) {
    constexpr double PlatformFloorThickness = 0.18;
    return {
        placement.anchor.x * cellSize,
        (placement.anchor.x + PlatformFrameWidthCells) *
            cellSize,
        placement.anchor.z * cellSize,
        (placement.anchor.z + PlatformFrameWidthCells) *
            cellSize,
        placement.floorHeight,
        placement.floorHeight - PlatformFloorThickness,
    };
}

} // namespace

const TerrainHeightfield& Simulation::terrain() const {
    return terrain_;
}

const EnemyPerformanceStats& Simulation::enemyPerformanceStats() const {
    return enemies_.performanceStats();
}

void Simulation::regenerateTerrain(std::uint32_t seed) {
    invalidateSnapshotCache();
    ++structuralRevision_;
    terrain_.generate(seed);
    collisionWorld_.syncPondLilySurfaces(
        generatePondLilyPlacements(terrain_));
    resources_ = ResourceSystem(
        scatterResources(
            map_.resources, map_.worldLimit,
            terrain_, map_.obstacles),
        [this](double x, double z) {
            return terrain_.getHeight(x, z);
        },
        [this](double x, double z, double radius) {
            return resourceGroundPositionIsSafe(x, z, radius);
        });
    resources_.setWoodYieldMultiplier(woodYieldMultiplier_);
    if (resources_.consumeCollisionGeometryDirty()) {
        collisionWorld_.syncResourceCylinders(
            resources_.nodes(), treeCollisionAssets_);
    }
    lootChests_.reset(
        terrain_.seed(), map_.worldLimit, terrain_,
        resources_.nodes(), playerPosition_);
    lootChests_.setGoldCostMultiplier(
        chestOpeningCostMultiplier_);
    foundations_.reset();
    syncModularStructures();
    syncWorldStructures();
    playerHorizontalVelocity_ = {};
    playerPosition_.y =
        terrain_.getHeight(
            playerPosition_.x,
            playerPosition_.z) +
        gameplay_.eyeHeight;
    verticalVelocity_ = 0.0;
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.0;
    lastGroundSurfaceHeight_ =
        playerPosition_.y - gameplay_.eyeHeight;
    playerGrounded_ = true;
}

PlatformFramePlacement Simulation::previewFoundation(
    Vec3 terrainHit) const {
    PlatformFramePlacement placement =
        foundations_.previewFoundation(
            terrainHit, playerPosition_);
    const double cellSize = worldConfig_.cellSize;
    if (placement.valid() &&
        rectangleHasDeepWater(
            placement.anchor.x * cellSize,
            (placement.anchor.x + PlatformFrameWidthCells) *
                cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + PlatformFrameWidthCells) *
                cellSize)) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() &&
        collisionWorld_.overlapsRampBox(
            platformFloorCollisionBox(
                placement, cellSize))) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() &&
        resourceOverlapsRectangle(
            resources_.nodes(),
            placement.anchor.x * cellSize,
            (placement.anchor.x + PlatformFrameWidthCells) *
                cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + PlatformFrameWidthCells) *
                cellSize)) {
        placement.error =
            ModularPlacementError::ResourceBlocked;
    }
    if (placement.valid() &&
        lootChestOverlapsRectangle(
            lootChests_.chests(),
            placement.anchor.x * cellSize,
            (placement.anchor.x + PlatformFrameWidthCells) *
                cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + PlatformFrameWidthCells) *
                cellSize)) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::Foundation)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

PlatformFramePlacement Simulation::previewFoundationAtHeight(
    Vec3 terrainHit, double floorHeight) const {
    PlatformFramePlacement placement =
        foundations_.previewFoundationAtHeight(
            terrainHit, floorHeight,
            playerPosition_);
    const double cellSize = worldConfig_.cellSize;
    if (placement.valid() &&
        rectangleHasDeepWater(
            placement.anchor.x * cellSize,
            (placement.anchor.x + PlatformFrameWidthCells) *
                cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + PlatformFrameWidthCells) *
                cellSize)) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() &&
        collisionWorld_.overlapsRampBox(
            platformFloorCollisionBox(
                placement, cellSize))) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() &&
        resourceOverlapsRectangle(
            resources_.nodes(),
            placement.anchor.x * cellSize,
            (placement.anchor.x + PlatformFrameWidthCells) *
                cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + PlatformFrameWidthCells) *
                cellSize)) {
        placement.error =
            ModularPlacementError::ResourceBlocked;
    }
    if (placement.valid() &&
        lootChestOverlapsRectangle(
            lootChests_.chests(),
            placement.anchor.x * cellSize,
            (placement.anchor.x + PlatformFrameWidthCells) *
                cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + PlatformFrameWidthCells) *
                cellSize)) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::Foundation)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<PlatformFrameInstance> Simulation::placeFoundation(
    Vec3 terrainHit) {
    invalidateSnapshotCache();
    const PlatformFramePlacement preview =
        previewFoundation(terrainHit);
    auto placed = foundations_.placePlatformFrame(preview);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[static_cast<std::size_t>(
                    ModularBuildPiece::Foundation)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
        raisePlayerOntoGroundFrame(*placed);
        events_.push_back({.type = GameEventType::ModularBuildingPlaced,
                           .entityId = placed->id,
                           .platformFrame = *placed,
                           .amount = static_cast<int>(ModularBuildPiece::Foundation)});
        processInsightEvent(events_.back());
    }
    return placed;
}

std::optional<PlatformFrameInstance>
Simulation::placeFoundationAtHeight(
    Vec3 terrainHit, double floorHeight) {
    invalidateSnapshotCache();
    const PlatformFramePlacement preview =
        previewFoundationAtHeight(terrainHit, floorHeight);
    auto placed = foundations_.placePlatformFrame(preview);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[static_cast<std::size_t>(
                    ModularBuildPiece::Foundation)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
        raisePlayerOntoGroundFrame(*placed);
        events_.push_back({.type = GameEventType::ModularBuildingPlaced,
                           .entityId = placed->id,
                           .platformFrame = *placed,
                           .amount = static_cast<int>(ModularBuildPiece::Foundation)});
        processInsightEvent(events_.back());
    }
    return placed;
}

PlatformFramePlacement Simulation::previewFloorPlatform(
    GridCoord anchor, int storey, double floorHeight) const {
    PlatformFramePlacement placement =
        foundations_.previewFloorPlatform(
            anchor, storey, floorHeight,
            playerPosition_);
    const double cellSize = worldConfig_.cellSize;
    const CollisionBox floorBox =
        platformFloorCollisionBox(placement, cellSize);
    if (placement.valid() &&
        collisionWorld_.overlapsRampBox(floorBox)) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() &&
        resourceOverlapsBox(resources_.nodes(), floorBox)) {
        placement.error =
            ModularPlacementError::ResourceBlocked;
    }
    if (placement.valid() &&
        lootChestOverlapsRectangle(
            lootChests_.chests(),
            floorBox.minX, floorBox.maxX,
            floorBox.minZ, floorBox.maxZ)) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::FloorPlatform)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<PlatformFrameInstance>
Simulation::placeFloorPlatform(
    GridCoord anchor, int storey, double floorHeight) {
    invalidateSnapshotCache();
    const PlatformFramePlacement placement =
        previewFloorPlatform(anchor, storey, floorHeight);
    auto placed = foundations_.placePlatformFrame(placement);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[static_cast<std::size_t>(
                    ModularBuildPiece::FloorPlatform)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
        events_.push_back({.type = GameEventType::ModularBuildingPlaced,
                           .entityId = placed->id,
                           .platformFrame = *placed,
                           .amount = static_cast<int>(ModularBuildPiece::FloorPlatform)});
        processInsightEvent(events_.back());
    }
    return placed;
}

WallPlacement Simulation::previewWall(
    Vec3 terrainHit, Rotation rotation) const {
    WallPlacement placement = foundations_.previewWall(
        terrainHit, playerPosition_, rotation);
    const double cellSize = worldConfig_.cellSize;
    if (placement.valid() &&
        resourceOverlapsRectangle(
            resources_.nodes(),
            placement.anchor.x * cellSize,
            (placement.anchor.x + 1) * cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + 1) * cellSize)) {
        placement.error =
            ModularPlacementError::ResourceBlocked;
    }
    if (placement.valid() &&
        lootChestOverlapsRectangle(
            lootChests_.chests(),
            placement.anchor.x * cellSize,
            (placement.anchor.x + 1) * cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + 1) * cellSize)) {
        placement.error = ModularPlacementError::Occupied;
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::Wall)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<WallInstance> Simulation::placeWall(
    Vec3 terrainHit, Rotation rotation) {
    invalidateSnapshotCache();
    const WallPlacement preview = previewWall(terrainHit, rotation);
    auto placed = foundations_.placeWall(preview);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[static_cast<std::size_t>(
                    ModularBuildPiece::Wall)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
        events_.push_back({.type = GameEventType::ModularBuildingPlaced,
                           .entityId = placed->id,
                           .modularWall = *placed,
                           .amount = static_cast<int>(ModularBuildPiece::Wall)});
        processInsightEvent(events_.back());
    }
    return placed;
}

RampPlacement Simulation::previewRamp(
    Vec3 terrainHit, Rotation rotation) const {
    RampPlacement placement = foundations_.previewRamp(
        terrainHit, playerPosition_, rotation);
    const double cellSize = worldConfig_.cellSize;
    if (placement.valid()) {
        const auto rampBoxes = rampCollisionBoxes(
            placement.anchor,
            placement.rotation,
            placement.bottomHeight,
            placement.topHeight,
            cellSize);
        const bool blockedByWorld = std::any_of(
            rampBoxes.begin(), rampBoxes.end(),
            [this](const CollisionBox& box) {
                return collisionWorld_.overlapsBox(box);
            });
        const bool blockedByBuilding = std::any_of(
            buildings_.buildings().begin(),
            buildings_.buildings().end(),
            [&rampBoxes](const BuildingInstance& building) {
                const CollisionBox buildingBox =
                    buildingCollisionBox(
                        building.type,
                        building.gridPosition,
                        building.baseHeight);
                return std::any_of(
                    rampBoxes.begin(), rampBoxes.end(),
                    [&buildingBox](const CollisionBox& rampBox) {
                        return collisionBoxesOverlap(
                            rampBox, buildingBox);
                    });
            });
        const bool blockedByResource = std::any_of(
            rampBoxes.begin(), rampBoxes.end(),
            [this](const CollisionBox& box) {
                return resourceOverlapsBox(
                    resources_.nodes(), box);
            });
        const bool blockedByChest = std::any_of(
            rampBoxes.begin(), rampBoxes.end(),
            [this](const CollisionBox& box) {
                return lootChestOverlapsRectangle(
                    lootChests_.chests(),
                    box.minX, box.maxX,
                    box.minZ, box.maxZ);
            });
        const bool blockedByWater = std::any_of(
            rampBoxes.begin(), rampBoxes.end(),
            [this](const CollisionBox& box) {
                return rectangleHasDeepWater(
                    box.minX, box.maxX,
                    box.minZ, box.maxZ);
            });
        if (blockedByWorld || blockedByWater ||
            blockedByBuilding || blockedByChest) {
            placement.error = ModularPlacementError::Occupied;
        } else if (blockedByResource) {
            placement.error =
                ModularPlacementError::ResourceBlocked;
        }
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::Ramp)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<RampInstance> Simulation::placeRamp(
    Vec3 terrainHit, Rotation rotation) {
    invalidateSnapshotCache();
    const RampPlacement preview = previewRamp(terrainHit, rotation);
    auto placed = foundations_.placeRamp(preview);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[static_cast<std::size_t>(
                    ModularBuildPiece::Ramp)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
        events_.push_back({.type = GameEventType::ModularBuildingPlaced,
                           .entityId = placed->id,
                           .ramp = *placed,
                           .amount = static_cast<int>(ModularBuildPiece::Ramp)});
        processInsightEvent(events_.back());
    }
    return placed;
}

void Simulation::setStructuralCollapseEnabled(bool enabled) {
    invalidateSnapshotCache();
    ++structuralRevision_;
    foundations_.setStructuralCollapseEnabled(enabled);
}

bool Simulation::structuralCollapseEnabled() const {
    return foundations_.structuralCollapseEnabled();
}

std::vector<EntityId> Simulation::structuralCollapseRisk(
    std::span<const EntityId> supports) const {
    std::vector<EntityId> result =
        foundations_.structuralGraph().collapseRiskIds(supports);
    std::vector<EntityId> affectedFrames = result;
    affectedFrames.insert(
        affectedFrames.end(), supports.begin(), supports.end());
    for (const BuildingInstance& building : buildings_.buildings()) {
        if (building.platformStorey < 0) {
            continue;
        }
        const bool twoByTwo =
            buildingFootprintHalfExtent(building.type) > 0.75;
        const int widthCells = twoByTwo ? 2 : 1;
        const int minimumX = twoByTwo
            ? building.gridPosition.x - 1
            : building.gridPosition.x;
        const int minimumZ = twoByTwo
            ? building.gridPosition.z - 1
            : building.gridPosition.z;
        const bool losesPlatform = std::ranges::any_of(
            foundations_.platformFrames(),
            [&](const PlatformFrameInstance& frame) {
                return std::ranges::find(
                           affectedFrames, frame.id) !=
                       affectedFrames.end() &&
                       frame.storey == building.platformStorey &&
                       std::abs(frame.floorHeight -
                                building.baseHeight) <= 0.05 &&
                       minimumX >= frame.anchor.x &&
                       minimumZ >= frame.anchor.z &&
                       minimumX + widthCells <=
                           frame.anchor.x +
                               PlatformFrameWidthCells &&
                       minimumZ + widthCells <=
                           frame.anchor.z +
                               PlatformFrameWidthCells;
            });
        if (losesPlatform) {
            result.push_back(building.id);
        }
    }
    return result;
}

std::size_t Simulation::clearModularBuildings() {
    invalidateSnapshotCache();
    aimedModularBuilding_.reset();
    const std::size_t removed = foundations_.clear();
    syncModularStructures();
    removeUnsupportedPlatformBuildings();
    return removed;
}

} // namespace ian
