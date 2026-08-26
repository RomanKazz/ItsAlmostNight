#include "game/Simulation.hpp"

#include "buildings/CoreProgression.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ResourceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {

bool Simulation::buildingUnlocked(BuildingType type) const {
    if (sandboxMode_) return true;
    if (type == BuildingType::WoodStorage ||
        type == BuildingType::StoneStorage ||
        type == BuildingType::CrystalStorage) {
        return false;
    }
    if (unlimitedResources_) return true;
    if (type == BuildingType::Core) return true;
    const auto core = buildings_.core();
    const int coreLevel = core ? static_cast<int>(core->level) : 0;
    return coreLevel >= buildingRequiredCoreLevel(type);
}

PlacementResult Simulation::validatePlacement(
    BuildingType type, GridPosition position) const {
    return validatePlacement(
        type, position,
        placementSurface(type, position));
}

PlacementResult Simulation::validatePlacement(
    BuildingType type, GridPosition position,
    const BuildingPlatformSurface& surface) const {
    if (!buildingUnlocked(type)) {
        const bool corePlaced = buildings_.core().has_value();
        return {
            corePlaced ? PlacementError::CoreLevelRequired
                       : PlacementError::CoreRequired,
            buildings_.configuredCost(type),
        };
    }
    const int availableWood =
        unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
    const int availableStone =
        unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
    const int availableCurrency =
        unlimitedResources_ ? std::numeric_limits<int>::max() : crystals_;
    const bool twoByTwo =
        buildingFootprintHalfExtent(type) > 0.75;
    const int footprintWidth = twoByTwo ? 2 : 1;
    const int footprintMinimumX =
        twoByTwo ? position.x - 1 : position.x;
    const int footprintMinimumZ =
        twoByTwo ? position.z - 1 : position.z;
    const bool partialPlatformSupport =
        surface.storey < 0 &&
        foundations_.buildingFootprintIntersectsPlatform(
            footprintMinimumX, footprintMinimumZ,
            footprintWidth);
    const PlacementResult buildingValidation =
        buildings_.validate(
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
    BuildingType type, GridPosition position) const {
    const bool twoByTwo =
        buildingFootprintHalfExtent(type) > 0.75;
    const int widthCells = twoByTwo ? 2 : 1;
    const int minimumX = twoByTwo ? position.x - 1 : position.x;
    const int minimumZ = twoByTwo ? position.z - 1 : position.z;
    if (const auto surface = foundations_.buildingSurface(
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
    double preferredHeight) const {
    BuildingPlatformSurface surface =
        placementSurface(type, position);
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
    std::optional<double> preferredHeight) const {
    const bool twoByTwo =
        buildingFootprintHalfExtent(type) > 0.75;
    GridCoord anchor{
        twoByTwo ? position.x - 1 : position.x,
        0,
        twoByTwo ? position.z - 1 : position.z,
    };
    // A 2x2 building is centred on its grid position, so its exact lower
    // corner can be odd or even. Keep that anchor exact. A 1x1 building uses
    // the containing modular 2x2 frame so nearby small buildings can share it.
    if (!twoByTwo) {
        anchor.x = snapPlatformFrameAxis(anchor.x);
        anchor.z = snapPlatformFrameAxis(anchor.z);
    }
    const double cellSize = worldConfig_.cellSize;
    const Vec3 terrainHit{
        (anchor.x + 0.5) * cellSize,
        terrain_.getHeight(
            (anchor.x + 0.5) * cellSize,
            (anchor.z + 0.5) * cellSize),
        (anchor.z + 0.5) * cellSize,
    };
    // Adjacent ground frames define a shared construction plane. This keeps
    // separately clicked neighbouring buildings perfectly level.
    std::optional<double> adjacentFoundationHeight;
    for (const PlatformFrameInstance& frame :
         foundations_.platformFrames()) {
        if (frame.storey != 0 ||
            frame.supportState !=
                StructuralSupportState::Supported) {
            continue;
        }
        const int deltaX = std::abs(
            frame.anchor.x - anchor.x);
        const int deltaZ = std::abs(
            frame.anchor.z - anchor.z);
        if ((deltaX == PlatformFrameWidthCells &&
             deltaZ == 0) ||
            (deltaZ == PlatformFrameWidthCells &&
             deltaX == 0)) {
            adjacentFoundationHeight = frame.floorHeight;
            break;
        }
    }

    PlatformFramePlacement placement;
    const auto previewAtHeight =
        [this, anchor, terrainHit, twoByTwo](double height) {
            return twoByTwo
                ? foundations_
                      .previewAutomaticBuildingFoundationAtHeight(
                          anchor, height, playerPosition_)
                : previewFoundationAtHeight(
                      terrainHit, height);
        };
    if (preferredHeight) {
        placement = previewAtHeight(*preferredHeight);
    } else if (adjacentFoundationHeight) {
        placement = previewAtHeight(
            *adjacentFoundationHeight);
    } else {
        // Automatic building foundations are compact ground plinths, not a
        // full one-metre storey. Sample the complete 2x2 footprint and leave
        // only the pallet top visible above its highest terrain point.
        constexpr int SamplesPerAxis = 5;
        constexpr double CompactHeightStep = 0.05;
        const double minimumX = anchor.x * cellSize;
        const double minimumZ = anchor.z * cellSize;
        const double maximumX =
            (anchor.x + PlatformFrameWidthCells) * cellSize;
        const double maximumZ =
            (anchor.z + PlatformFrameWidthCells) * cellSize;
        double highestTerrain =
            -std::numeric_limits<double>::infinity();
        for (int z = 0; z < SamplesPerAxis; ++z) {
            for (int x = 0; x < SamplesPerAxis; ++x) {
                const double amountX =
                    static_cast<double>(x) /
                    static_cast<double>(SamplesPerAxis - 1);
                const double amountZ =
                    static_cast<double>(z) /
                    static_cast<double>(SamplesPerAxis - 1);
                highestTerrain = std::max(
                    highestTerrain,
                    terrain_.getHeight(
                        minimumX +
                            (maximumX - minimumX) * amountX,
                        minimumZ +
                            (maximumZ - minimumZ) * amountZ));
            }
        }
        const double compactHeight = std::ceil(
            (highestTerrain +
             worldConfig_.minimumGroundClearance) /
            CompactHeightStep) * CompactHeightStep;
        placement = previewAtHeight(compactHeight);
    }
    if (placement.error ==
            ModularPlacementError::InsufficientResources ||
        placement.error ==
            ModularPlacementError::CoreLevelRequired) {
        // Combined affordability is checked after adding building cost.
        placement.error = ModularPlacementError::None;
    }
    if (!placement.valid() || placement.storey != 0) {
        return placement;
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

PlacementResult
Simulation::previewPlacementWithOptionalHeight(
    BuildingType type, GridPosition position,
    std::optional<double> preferredHeight) const {
    BuildingPlatformSurface surface =
        preferredHeight
            ? placementSurfaceWithPreferredHeight(
                  type, position, *preferredHeight)
            : placementSurface(type, position);
    const bool needsAutomaticFoundation = surface.storey < 0;
    if (!needsAutomaticFoundation) {
        return validatePlacement(type, position, surface);
    }
    const auto automaticFoundation =
        automaticFoundationPlacement(
            type, position, preferredHeight);
    PlacementResult placement =
        validatePlacement(type, position, surface);
    if (!automaticFoundation || !automaticFoundation->valid()) {
        if (placement.valid() && automaticFoundation) {
            placement.error =
                automaticFoundation->error ==
                        ModularPlacementError::ResourceBlocked
                    ? PlacementError::ResourceBlocked
                    : PlacementError::WorldCollision;
        }
        return placement;
    }
    surface.height = automaticFoundation->floorHeight;
    surface.foundationBottomHeight =
        std::min_element(
            automaticFoundation->supports.begin(),
            automaticFoundation->supports.end(),
            [](const FoundationSupport& left,
               const FoundationSupport& right) {
                return left.bottom.y < right.bottom.y;
            })
            ->bottom.y;
    placement = validatePlacement(type, position, surface);
    if (foundationAddsPlacementCost(*automaticFoundation)) {
        const ResourceCost foundationCost =
            modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::Foundation)];
        placement.cost = {
            saturatingAdd(placement.cost.wood, foundationCost.wood),
            saturatingAdd(placement.cost.stone, foundationCost.stone),
            saturatingAdd(placement.cost.crystals, foundationCost.crystals),
        };
    }
    if (placement.valid() && !unlimitedResources_ &&
        (wood_ < placement.cost.wood ||
         stone_ < placement.cost.stone ||
         crystals_ < placement.cost.crystals)) {
        placement.error = PlacementError::InsufficientResources;
    }
    return placement;
}

BuildingPlatformSurface Simulation::previewPlacementSurface(
    BuildingType type, GridPosition position) const {
    return placementSurface(type, position);
}

BuildingPlatformSurface Simulation::previewPlacementSurface(
    BuildingType type, GridPosition position,
    double preferredHeight) const {
    return placementSurfaceWithPreferredHeight(
        type, position, preferredHeight);
}

} // namespace ian
