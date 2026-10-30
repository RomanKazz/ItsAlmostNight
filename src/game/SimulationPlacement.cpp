#include "game/Simulation.hpp"
#include "buildings/FoundationPlacementError.hpp"

#include "buildings/CoreProgression.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ResourceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {

bool Simulation::ownsRunBuilding(BuildingType type) const {
    if (type == BuildingType::Wall || type == BuildingType::Gate) return true;
    if (std::ranges::find(RunEconomyBuildingTypes, type) != RunEconomyBuildingTypes.end()) return true;
    const auto effect = buildingRunUpgrade(type);
    if (!effect) return true;
    const auto* playerClass = playerClassDefinition(playerClass_);
    const BuildingType starting = playerClass ? playerClass->startingBuilding : BuildingType::GunTurret;
    return type == starting || runUpgradeStacks_[runUpgradeIndex(*effect)] > 0;
}

std::size_t Simulation::runBuildingArsenalCount() const {
    return static_cast<std::size_t>(std::count_if(RunBuildingTypes.begin(), RunBuildingTypes.end(),
        [this](BuildingType type) { return runCombatBuilding(type) && type != BuildingType::Wall && ownsRunBuilding(type); }));
}

bool Simulation::buildingUnlocked(BuildingType type) const {
    if (isRemovedStorageBuilding(type)) return false;
    if (sandboxMode_ || unlimitedResources_) return true;
    if (type == BuildingType::Core) return true;
    return buildings_.core().has_value() && ownsRunBuilding(type);
}

PlacementResult Simulation::validatePlacement(
    BuildingType type, GridPosition position) const {
    return validatePlacement(
        type, position,
        placementSurface(type, position));
}

PlacementResult Simulation::validatePlacement(
    BuildingType type, GridPosition position,
    const BuildingPlatformSurface& surface, const FoundationSystem* plannedFoundations,
    const BuildingSystem* plannedBuildings, std::optional<ResourceCost> available) const {
    const auto& foundations = plannedFoundations ? *plannedFoundations : foundations_;
    const auto& buildings = plannedBuildings ? *plannedBuildings : buildings_;
    const ResourceCost budget = available.value_or(ResourceCost{wood_, stone_, crystals_});
    if (!buildingUnlocked(type)) {
        const bool corePlaced = buildings.core().has_value();
        return {
            corePlaced ? PlacementError::BlueprintRequired
                       : PlacementError::CoreRequired,
            buildings.configuredCost(type),
        };
    }
    const int availableWood =
        unlimitedResources_ ? std::numeric_limits<int>::max() : budget.wood;
    const int availableStone =
        unlimitedResources_ ? std::numeric_limits<int>::max() : budget.stone;
    const int availableCurrency =
        unlimitedResources_ ? std::numeric_limits<int>::max() : budget.crystals;
    const bool twoByTwo =
        buildingFootprintHalfExtent(type) > 0.75;
    const int footprintWidth = twoByTwo ? 2 : 1;
    const int footprintMinimumX =
        twoByTwo ? position.x - 1 : position.x;
    const int footprintMinimumZ =
        twoByTwo ? position.z - 1 : position.z;
    const bool partialPlatformSupport =
        surface.storey < 0 &&
        foundations.buildingFootprintIntersectsPlatform(
            footprintMinimumX, footprintMinimumZ,
            footprintWidth);
    const PlacementResult buildingValidation =
        buildings.validate(
            type, position, availableWood,
            availableStone, availableCurrency,
            surface.height);
    if (!buildingValidation.valid()) {
        return buildingValidation;
    }
    if (partialPlatformSupport) {
        return {
            PlacementError::WorldCollision,
            buildingValidation.cost,
        };
    }
    if (surface.storey < 0 &&
        surface.height - surface.foundationBottomHeight >
            worldConfig_.maximumFoundationHeightDifference +
                1e-6) {
        return {
            PlacementError::WorldCollision,
            buildingValidation.cost,
        };
    }

    const double cellSize = worldConfig_.cellSize;
    if (rectangleHasDeepWater(
            footprintMinimumX * cellSize,
            (footprintMinimumX + footprintWidth) * cellSize,
            footprintMinimumZ * cellSize,
            (footprintMinimumZ + footprintWidth) * cellSize)) {
        return {
            PlacementError::WorldCollision,
            buildingValidation.cost,
        };
    }
    if (resourceOverlapsRectangle(
            resources_.nodes(),
            footprintMinimumX * cellSize,
            (footprintMinimumX + footprintWidth) * cellSize,
            footprintMinimumZ * cellSize,
            (footprintMinimumZ + footprintWidth) * cellSize)) {
        return {
            PlacementError::ResourceBlocked,
            buildingValidation.cost,
        };
    }
    const double minimumX = footprintMinimumX * cellSize;
    const double maximumX =
        (footprintMinimumX + footprintWidth) * cellSize;
    const double minimumZ = footprintMinimumZ * cellSize;
    const double maximumZ =
        (footprintMinimumZ + footprintWidth) * cellSize;
    if (lootChestOverlapsRectangle(
            lootChests_.chests(),
            minimumX, maximumX,
            minimumZ, maximumZ)) {
        return {
            PlacementError::WorldCollision,
            buildingValidation.cost,
        };
    }

    const Vec3 center = buildingWorldPosition(type, position);
    const double deltaX = center.x - playerPosition_.x;
    const double deltaZ = center.z - playerPosition_.z;
    const double distance =
        std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    if (distance > gameplay_.maximumPlacementDistance + 0.75) {
        return {PlacementError::OutOfRange, buildingValidation.cost};
    }

    const CollisionBox candidate =
        buildingCollisionBox(type, position, surface.height);
    if (collisionWorld_.overlapsBox(candidate)) {
        return {PlacementError::WorldCollision, buildingValidation.cost};
    }
    return buildingValidation;
}

bool Simulation::rectangleHasDeepWater(
    double minimumX, double maximumX,
    double minimumZ, double maximumZ) const {
    constexpr int SamplesPerAxis = 5;
    for (int z = 0; z < SamplesPerAxis; ++z) {
        for (int x = 0; x < SamplesPerAxis; ++x) {
            const double amountX =
                static_cast<double>(x) /
                static_cast<double>(SamplesPerAxis - 1);
            const double amountZ =
                static_cast<double>(z) /
                static_cast<double>(SamplesPerAxis - 1);
            const double sampleX =
                minimumX + (maximumX - minimumX) * amountX;
            const double sampleZ =
                minimumZ + (maximumZ - minimumZ) * amountZ;
            if (terrain_.waterDepth(sampleX, sampleZ) >
                worldConfig_.pondBuildDepthLimit) {
                return true;
            }
        }
    }
    return false;
}

BuildingPlatformSurface Simulation::placementSurface(
    BuildingType type, GridPosition position, const FoundationSystem* plannedFoundations) const {
    const auto& foundations = plannedFoundations ? *plannedFoundations : foundations_;
    const bool twoByTwo =
        buildingFootprintHalfExtent(type) > 0.75;
    const int widthCells = twoByTwo ? 2 : 1;
    const int minimumX = twoByTwo ? position.x - 1 : position.x;
    const int minimumZ = twoByTwo ? position.z - 1 : position.z;
    if (const auto surface = foundations.buildingSurface(
            minimumX, minimumZ, widthCells)) {
        return *surface;
    }
    const Vec3 center = buildingWorldPosition(type, position);
    const double halfExtent = buildingFootprintHalfExtent(type);
    const double inset = std::min(0.05, halfExtent * 0.1);
    const double sampleHalfExtent =
        std::max(0.0, halfExtent - inset);
    double highestTerrain =
        -std::numeric_limits<double>::infinity();
    double lowestTerrain =
        std::numeric_limits<double>::infinity();
    constexpr int TerrainSamplesPerRadius = 4;
    for (int zIndex = -TerrainSamplesPerRadius;
         zIndex <= TerrainSamplesPerRadius; ++zIndex) {
        for (int xIndex = -TerrainSamplesPerRadius;
             xIndex <= TerrainSamplesPerRadius; ++xIndex) {
            const double sampleX =
                center.x +
                static_cast<double>(xIndex) /
                    static_cast<double>(TerrainSamplesPerRadius) *
                    sampleHalfExtent;
            const double sampleZ =
                center.z +
                static_cast<double>(zIndex) /
                    static_cast<double>(TerrainSamplesPerRadius) *
                    sampleHalfExtent;
            const double terrainHeight =
                terrain_.getHeight(sampleX, sampleZ);
            highestTerrain = std::max(highestTerrain, terrainHeight);
            lowestTerrain = std::min(lowestTerrain, terrainHeight);
        }
    }
    const double verticalCell = worldConfig_.verticalGridStep;
    constexpr double SnapEpsilon = 1e-6;
    const double snappedTop =
        std::ceil((highestTerrain - SnapEpsilon) / verticalCell) *
        verticalCell;
    const double snappedBottom =
        std::floor((lowestTerrain + SnapEpsilon) / verticalCell) *
        verticalCell;
    return {
        .height = snappedTop,
        .foundationBottomHeight =
            std::min(snappedBottom, snappedTop),
        .storey = -1,
    };
}

BuildingPlatformSurface
Simulation::placementSurfaceWithPreferredHeight(
    BuildingType type, GridPosition position,
    double preferredHeight, const FoundationSystem* plannedFoundations) const {
    const auto& foundations = plannedFoundations ? *plannedFoundations : foundations_;
    const bool twoByTwo =
        buildingFootprintHalfExtent(type) > 0.75;
    const int widthCells = twoByTwo ? 2 : 1;
    const int minimumX = twoByTwo
        ? position.x - 1
        : position.x;
    const int minimumZ = twoByTwo
        ? position.z - 1
        : position.z;
    if (const auto preferredSurface =
            foundations.buildingSurface(
                minimumX, minimumZ, widthCells,
                preferredHeight)) {
        return *preferredSurface;
    }
    BuildingPlatformSurface surface =
        placementSurface(type, position, &foundations);
    if (surface.storey >= 0) {
        return surface;
    }
    // A preferred construction plane comes from an already rendered preview
    // or a snapped platform. Keep it exact: silently replacing it with a
    // higher terrain plane makes the placed building jump after the click.
    surface.height = preferredHeight;
    surface.foundationBottomHeight =
        std::min(surface.foundationBottomHeight, preferredHeight);
    return surface;
}

std::optional<PlatformFramePlacement>
Simulation::automaticFoundationPlacement(
    BuildingType type, GridPosition position,
    std::optional<double> preferredHeight, const FoundationSystem* plannedFoundations,
    std::optional<GridCoord> foundationAnchor) const {
    const auto& foundations = plannedFoundations ? *plannedFoundations : foundations_;
    const bool twoByTwo =
        buildingFootprintHalfExtent(type) > 0.75;
    GridCoord anchor{
        twoByTwo ? position.x - 1 : position.x,
        0,
        twoByTwo ? position.z - 1 : position.z,
    };
    // Preserve exact anchors on the one-cell grid. Compact buildings can
    // choose any of the four 2x2 supports containing their occupied cell.
    const auto previewAtAnchor = [this, preferredHeight, twoByTwo, &foundations](GridCoord candidate) {
        PlatformFramePlacement placement;
        if (preferredHeight) {
            placement = foundations.previewAutomaticBuildingFoundationAtHeight(
                candidate, *preferredHeight, playerPosition_);
        } else {
            placement = foundations.previewAutomaticBuildingFoundation(
                candidate, playerPosition_, GroundPlatformHeightStep);
        }
        if (!twoByTwo) placement = validateFoundationAtHeight(placement);
        if (placement.error == ModularPlacementError::InsufficientResources ||
            placement.error == ModularPlacementError::CoreLevelRequired) {
            // Combined affordability is checked after adding building cost.
            placement.error = ModularPlacementError::None;
        }
        return placement;
    };
    if (foundationAnchor) {
        // A drag locks one 2x2 lattice. Do not search the opposite side of
        // the row when its chosen support is blocked.
        const int width = twoByTwo ? 2 : 1;
        const int minimumX = twoByTwo ? position.x - 1 : position.x;
        const int minimumZ = twoByTwo ? position.z - 1 : position.z;
        if (foundationAnchor->x > minimumX || foundationAnchor->z > minimumZ ||
            foundationAnchor->x + PlatformFrameWidthCells < minimumX + width ||
            foundationAnchor->z + PlatformFrameWidthCells < minimumZ + width) {
            return PlatformFramePlacement{.error = ModularPlacementError::Occupied, .anchor = *foundationAnchor};
        }
        auto exact = foundations.previewAutomaticBuildingFoundationAtHeight(
            *foundationAnchor, preferredHeight.value_or(placementSurface(type, position, &foundations).height),
            playerPosition_);
        exact = validateFoundationAtHeight(exact);
        if (exact.error == ModularPlacementError::InsufficientResources ||
            exact.error == ModularPlacementError::CoreLevelRequired) exact.error = ModularPlacementError::None;
        return exact;
    }
    const auto placement = previewAtAnchor(anchor);
    if (placement.valid() || twoByTwo) return placement;
    constexpr std::array<GridCoord, 3> AlternativeAnchors{{
        {-1, 0, 0}, {0, 0, -1}, {-1, 0, -1},
    }};
    for (const auto offset : AlternativeAnchors) {
        const auto alternative = previewAtAnchor(
            {anchor.x + offset.x, 0, anchor.z + offset.z});
        if (alternative.valid()) return alternative;
    }
    return placement;
}

bool Simulation::foundationAddsPlacementCost(
    const PlatformFramePlacement& placement) const {
    // The compact pallet top is part of every building. Charge the separate
    // modular-foundation cost only when terrain needs visibly extended legs.
    constexpr double IncludedSupportAllowance = 0.20;
    return std::any_of(
        placement.supports.begin(), placement.supports.end(),
        [this](const FoundationSupport& support) {
            return support.length >
                worldConfig_.minimumGroundClearance +
                    IncludedSupportAllowance;
        });
}

PlacementResult Simulation::previewPlacement(
    BuildingType type, GridPosition position) const {
    return previewPlacementWithOptionalHeight(
        type, position, std::nullopt);
}

PlacementResult Simulation::previewPlacement(
    BuildingType type, GridPosition position,
    double preferredHeight) const {
    return previewPlacementWithOptionalHeight(
        type, position, preferredHeight);
}

BuildingPreview Simulation::previewBuildingPlacement(
    BuildingType type, GridPosition position, std::uint8_t rotation,
    std::optional<double> preferredHeight, const FoundationSystem& foundations,
    const BuildingSystem& buildings, ResourceCost available,
    std::optional<GridCoord> foundationAnchor) const {
    auto surface = preferredHeight
        ? placementSurfaceWithPreferredHeight(type, position, *preferredHeight, &foundations)
        : placementSurface(type, position, &foundations);
    const auto foundation = surface.storey < 0
        ? automaticFoundationPlacement(type, position, preferredHeight, &foundations, foundationAnchor)
        : std::nullopt;
    if (foundation && foundation->hasGeometry()) {
        surface.height = foundation->floorHeight;
        surface.foundationBottomHeight = std::ranges::min_element(foundation->supports,
            {}, [](const FoundationSupport& support) { return support.bottom.y; })->bottom.y;
    }
    auto placement = validatePlacement(type, position, surface, &foundations, &buildings, available);
    if (foundation && !foundation->valid() && placement.valid()) {
        placement.error = foundationPlacementError(foundation->error);
    }
    if (foundation && foundation->valid() && foundationAddsPlacementCost(*foundation)) {
        const auto cost = modularBuildingCosts_[static_cast<std::size_t>(ModularBuildPiece::Foundation)];
        placement.cost = {saturatingAdd(placement.cost.wood, cost.wood),
            saturatingAdd(placement.cost.stone, cost.stone),
            saturatingAdd(placement.cost.crystals, cost.crystals)};
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(placement.cost, available.wood, available.stone, available.crystals)) {
        placement.error = PlacementError::InsufficientResources;
    }
    return {.type = type, .gridPosition = position, .rotation = rotation, .placement = placement,
        .baseHeight = surface.height, .platformStorey = surface.storey,
        .foundationBottomHeight = surface.foundationBottomHeight, .foundationPreview = foundation};
}

PlacementResult Simulation::previewPlacementWithOptionalHeight(
    BuildingType type, GridPosition position, std::optional<double> preferredHeight) const {
    return previewBuildingPlacement(type, position, 0, preferredHeight,
        foundations_, buildings_, {wood_, stone_, crystals_}).placement;
}

std::vector<BuildingPreview> Simulation::previewBuildingLine(
    BuildingType type, std::span<const GridPosition> cells,
    std::uint8_t rotation, double preferredHeight) const {
    // Stage only construction state, never copy or tick the game world.
    // Later cells reuse earlier 2x2 supports and consume the remaining budget/slots.
    if (cells.empty()) return {};
    if (cells.size() == 1U) {
        return {previewBuildingPlacement(type, cells.front(), rotation, preferredHeight,
            foundations_, buildings_, {wood_, stone_, crystals_})};
    }
    const bool compact = buildingFootprintHalfExtent(type) < 0.75;
    const bool alongX = std::abs(cells.back().x - cells.front().x) >=
                        std::abs(cells.back().z - cells.front().z);
    std::optional<GridCoord> initialOrigin;
    const auto plan = [&](std::optional<GridCoord> origin) {
        auto foundations = foundations_;
        auto buildings = buildings_;
        ResourceCost available{wood_, stone_, crystals_};
        std::vector<BuildingPreview> result;
        result.reserve(cells.size());
        std::optional<GridCoord> lineOrigin = origin;
        for (const auto cell : cells) {
            std::optional<GridCoord> alignedAnchor;
            if (compact && lineOrigin) {
                alignedAnchor = *lineOrigin;
                const auto alignedAxis = [](int cellAxis, int originAxis) {
                    return originAxis + static_cast<int>(std::floor(
                        static_cast<double>(cellAxis - originAxis) / PlatformFrameWidthCells)) * PlatformFrameWidthCells;
                };
                if (alongX) alignedAnchor->x = alignedAxis(cell.x, lineOrigin->x);
                else alignedAnchor->z = alignedAxis(cell.z, lineOrigin->z);
            }
            auto preview = previewBuildingPlacement(type, cell, rotation, preferredHeight,
                foundations, buildings, available, alignedAnchor);
            if (compact && !lineOrigin && preview.placement.valid()) {
                if (preview.foundationPreview) lineOrigin = preview.foundationPreview->anchor;
                else for (const auto& frame : foundations.platformFrames()) {
                    if (frame.storey == preview.platformStorey && std::abs(frame.floorHeight - preview.baseHeight) < 0.05 &&
                        frame.anchor.x <= cell.x && frame.anchor.x + PlatformFrameWidthCells > cell.x &&
                        frame.anchor.z <= cell.z && frame.anchor.z + PlatformFrameWidthCells > cell.z) {
                        lineOrigin = frame.anchor;
                        break;
                    }
                }
            }
            if (preview.placement.valid()) {
                std::optional<PlatformFrameInstance> created;
                if (preview.foundationPreview) {
                    created = foundations.placePlatformFrame(*preview.foundationPreview);
                    if (!created) preview.placement.error = PlacementError::WorldCollision;
                }
                const auto surface = placementSurfaceWithPreferredHeight(type, cell,
                    preview.baseHeight, &foundations);
                if (preview.placement.valid()) {
                    const auto placed = buildings.place(type, cell, rotation,
                        unlimitedResources_ ? std::numeric_limits<int>::max() : available.wood,
                        unlimitedResources_ ? std::numeric_limits<int>::max() : available.stone,
                        unlimitedResources_ ? std::numeric_limits<int>::max() : available.crystals,
                        surface.height, surface.storey, surface.foundationBottomHeight);
                    if (!placed) preview.placement.error = PlacementError::WorldCollision;
                }
                if (preview.placement.valid() && !unlimitedResources_) {
                    available.wood -= preview.placement.cost.wood;
                    available.stone -= preview.placement.cost.stone;
                    available.crystals -= preview.placement.cost.crystals;
                } else if (!preview.placement.valid() && created) {
                    static_cast<void>(foundations.remove(created->id));
                }
            }
            result.push_back(std::move(preview));
        }
        if (!initialOrigin) initialOrigin = lineOrigin;
        return result;
    };
    auto result = plan(std::nullopt);
    if (!compact || std::ranges::all_of(result, [](const BuildingPreview& preview) {
            return preview.placement.valid();
        })) return result;
    const GridCoord original = initialOrigin.value_or(GridCoord{cells.front().x, 0, cells.front().z});

    // Compare the four consistent 2x2 lattices containing this row. Moving
    // its foundations by one cell can close a gap against an existing pallet.
    // Select one plan for the entire drag, rather than zigzagging each support.
    const auto quality = [](const std::vector<BuildingPreview>& previews) {
        int valid = 0;
        int foundations = 0;
        for (const auto& preview : previews) {
            if (!preview.placement.valid()) continue;
            ++valid;
            if (preview.foundationPreview) ++foundations;
        }
        return std::pair{valid, -foundations};
    };
    for (int phase = 1; phase < 4; ++phase) {
        auto origin = original;
        if (phase & 1) {
            if (alongX) ++origin.x;
            else ++origin.z;
        }
        if (phase & 2) {
            if (alongX) origin.z = original.z == cells.front().z ? cells.front().z - 1 : cells.front().z;
            else origin.x = original.x == cells.front().x ? cells.front().x - 1 : cells.front().x;
        }
        auto alternative = plan(origin);
        if (quality(alternative) > quality(result)) result = std::move(alternative);
    }
    return result;
}

BuildingPlatformSurface Simulation::previewPlacementSurface(
    BuildingType type, GridPosition position) const {
    return placementSurface(type, position);
}

std::optional<PlatformFramePlacement> Simulation::previewBuildingFoundation(
    BuildingType type, GridPosition position, std::optional<double> preferredHeight) const {
    const auto surface = preferredHeight ? placementSurfaceWithPreferredHeight(type, position, *preferredHeight)
        : placementSurface(type, position);
    if (surface.storey >= 0) return std::nullopt;
    return automaticFoundationPlacement(type, position, preferredHeight);
}

BuildingPlatformSurface Simulation::previewPlacementSurface(
    BuildingType type, GridPosition position,
    double preferredHeight) const {
    return placementSurfaceWithPreferredHeight(
        type, position, preferredHeight);
}

} // namespace ian
