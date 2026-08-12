#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
void Simulation::processBuildingCommands(const PlayerCommand& command) {
    if (command.selectBuilding) {
        selectedBuilding_ = command.selectBuilding;
    }
    if (command.cancelBuilding) {
        selectedBuilding_.reset();
    }
    if (selectedBuilding_ && command.rotateBuilding != 0) {
        const int rotation = static_cast<int>(buildingRotation_) + command.rotateBuilding;
        buildingRotation_ = static_cast<std::uint8_t>((rotation % 4 + 4) % 4);
    }

    if (selectedBuilding_) {
        const double playerFeetHeight =
            playerPosition_.y - gameplay_.eyeHeight;
        const auto standingSurface =
            collisionWorld_.modularSurfaceHeight(
                playerPosition_.x,
                playerPosition_.z,
                playerFeetHeight + 0.35);
        const bool standingOnFoundation =
            playerGrounded_ &&
            standingSurface &&
            std::abs(
                *standingSurface -
                playerFeetHeight) < 0.45;
        const double placementPlaneHeight =
            standingOnFoundation
                ? *standingSurface
                : terrain_.getHeight(
                      playerPosition_.x,
                      playerPosition_.z);
        const auto aimedPlatformSurface =
            foundations_.raycastPlatformSurface(
                playerPosition_,
                lookDirection(
                    playerYaw_, playerPitch_),
                gameplay_.maximumPlacementDistance +
                    modularStoreyHeight(
                        worldConfig_));
        const double horizontalAimDistance =
            aimedPlatformSurface
                ? std::max(
                      gameplay_
                          .minimumPlacementDistance,
                      std::hypot(
                          aimedPlatformSurface->x -
                              playerPosition_.x,
                          aimedPlatformSurface->z -
                              playerPosition_.z))
                : 0.0;
        const GridPosition gridPosition =
            aimedBuildingGridPosition(
                playerPosition_, playerYaw_,
                playerPitch_,
                aimedPlatformSurface
                    ? horizontalAimDistance
                    : gameplay_
                          .minimumPlacementDistance,
                aimedPlatformSurface
                    ? horizontalAimDistance
                    : gameplay_
                          .maximumPlacementDistance,
                *selectedBuilding_,
                aimedPlatformSurface
                    ? aimedPlatformSurface->y
                    : placementPlaneHeight);
        BuildingPlatformSurface surface =
            aimedPlatformSurface
                ? placementSurfaceWithPreferredHeight(
                      *selectedBuilding_,
                      gridPosition,
                      aimedPlatformSurface->y)
            : standingOnFoundation
                ? placementSurfaceWithPreferredHeight(
                      *selectedBuilding_,
                      gridPosition,
                      *standingSurface)
                : placementSurface(
                      *selectedBuilding_,
                      gridPosition);
        const bool needsAutomaticFoundation =
            surface.storey < 0 &&
            surface.height -
                    surface.foundationBottomHeight >
                0.025;
        const auto automaticFoundation =
            needsAutomaticFoundation
                ? automaticFoundationPlacement(
                      *selectedBuilding_,
                      gridPosition)
                : std::nullopt;
        if (automaticFoundation &&
            automaticFoundation->valid()) {
            surface.height =
                automaticFoundation->floorHeight;
            surface.foundationBottomHeight =
                std::min_element(
                    automaticFoundation
                        ->supports.begin(),
                    automaticFoundation
                        ->supports.end(),
                    [](const FoundationSupport& left,
                       const FoundationSupport& right) {
                        return left.bottom.y <
                               right.bottom.y;
                    })
                    ->bottom.y;
        }
        PlacementResult previewPlacement =
            validatePlacement(
                *selectedBuilding_,
                gridPosition, surface);
        if (automaticFoundation &&
            automaticFoundation->valid()) {
            previewPlacement.cost = addResourceCosts(
                previewPlacement.cost,
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::Foundation)]);
            if (previewPlacement.valid() &&
                !unlimitedResources_ &&
                !canAfford(
                    previewPlacement.cost,
                    wood_, stone_, crystals_)) {
                previewPlacement.error =
                    PlacementError::InsufficientResources;
            }
        }
        if (automaticFoundation &&
            !automaticFoundation->valid() &&
            previewPlacement.valid()) {
            previewPlacement.error =
                automaticFoundation->error ==
                        ModularPlacementError::
                            ResourceBlocked
                    ? PlacementError::ResourceBlocked
                    : PlacementError::WorldCollision;
        }
        buildingPreview_ = BuildingPreview{
            .type = *selectedBuilding_,
            .gridPosition = gridPosition,
            .rotation = buildingRotation_,
            .placement = previewPlacement,
            .baseHeight = surface.height,
            .platformStorey = surface.storey,
            .foundationBottomHeight =
                surface.foundationBottomHeight,
        };
    } else {
        buildingPreview_.reset();
    }

    if (command.placeBuilding) {
        const BuildingPlatformSurface naturalSurface =
            placementSurface(
                command.placeBuilding->type,
                command.placeBuilding->gridPosition);
        BuildingPlatformSurface surface =
            command.placeBuilding->lockHeight
                ? placementSurfaceWithPreferredHeight(
                      command.placeBuilding->type,
                      command.placeBuilding
                          ->gridPosition,
                      command.placeBuilding
                          ->baseHeight)
                : naturalSurface;
        const bool needsAutomaticFoundation =
            surface.storey < 0 &&
            surface.height -
                    surface.foundationBottomHeight >
                0.025;
        auto automaticFoundation =
            needsAutomaticFoundation
                ? automaticFoundationPlacement(
                      command.placeBuilding->type,
                      command.placeBuilding
                          ->gridPosition)
                : std::nullopt;
        if (automaticFoundation &&
            automaticFoundation->valid()) {
            surface.height =
                automaticFoundation->floorHeight;
            surface.foundationBottomHeight =
                std::min_element(
                    automaticFoundation
                        ->supports.begin(),
                    automaticFoundation
                        ->supports.end(),
                    [](const FoundationSupport& left,
                       const FoundationSupport& right) {
                        return left.bottom.y <
                               right.bottom.y;
                    })
                    ->bottom.y;
        }
        PlacementResult placement =
            validatePlacement(
                command.placeBuilding->type,
                command.placeBuilding
                    ->gridPosition,
                surface);
        if (automaticFoundation &&
            automaticFoundation->valid()) {
            placement.cost = addResourceCosts(
                placement.cost,
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::Foundation)]);
            if (placement.valid() &&
                !unlimitedResources_ &&
                !canAfford(
                    placement.cost,
                    wood_, stone_, crystals_)) {
                placement.error =
                    PlacementError::InsufficientResources;
            }
        }
        if (command.placeBuilding->lockHeight &&
            command.placeBuilding->platformStorey >= 0 &&
            (naturalSurface.storey !=
                 command.placeBuilding
                     ->platformStorey ||
             std::abs(
                 naturalSurface.height -
                 command.placeBuilding
                     ->baseHeight) > 0.05)) {
            placement.error =
                PlacementError::WorldCollision;
        }
        if (automaticFoundation &&
            !automaticFoundation->valid() &&
            placement.valid()) {
            placement.error =
                automaticFoundation->error ==
                        ModularPlacementError::
                            ResourceBlocked
                    ? PlacementError::ResourceBlocked
                    : PlacementError::WorldCollision;
        }
        if (placement.valid()) {
            std::optional<PlatformFrameInstance>
                createdFoundation;
            if (automaticFoundation) {
                createdFoundation =
                    foundations_.placePlatformFrame(
                        *automaticFoundation);
                if (!createdFoundation) {
                    placement.error =
                        PlacementError::WorldCollision;
                } else {
                    syncModularStructures();
                    raisePlayerOntoGroundFrame(
                        *createdFoundation);
                    surface = placementSurface(
                        command.placeBuilding->type,
                        command.placeBuilding
                            ->gridPosition);
                    placement = validatePlacement(
                        command.placeBuilding->type,
                        command.placeBuilding
                            ->gridPosition,
                        surface);
                }
            }
            if (!placement.valid()) {
                if (createdFoundation) {
                    static_cast<void>(
                        foundations_.remove(
                            createdFoundation->id));
                    syncModularStructures();
                }
            } else {
                const auto placed = buildings_.place(
                    command.placeBuilding->type,
                    command.placeBuilding->gridPosition,
                    command.placeBuilding->rotation,
                    unlimitedResources_
                        ? std::numeric_limits<int>::max()
                        : wood_,
                    unlimitedResources_
                        ? std::numeric_limits<int>::max()
                        : stone_,
                    unlimitedResources_
                        ? std::numeric_limits<int>::max()
                        : crystals_,
                    surface.height, surface.storey,
                    surface.foundationBottomHeight);
                if (placed) {
                    if (!unlimitedResources_) {
                        ResourceCost transactionCost =
                            placed->cost;
                        if (createdFoundation) {
                            transactionCost = addResourceCosts(
                                transactionCost,
                                modularBuildingCosts_[
                                    static_cast<std::size_t>(
                                        ModularBuildPiece::Foundation)]);
                        }
                        wood_ -= transactionCost.wood;
                        stone_ -= transactionCost.stone;
                        crystals_ -= transactionCost.crystals;
                    }
                    syncWorldStructures();
                    events_.push_back({
                        .type =
                            GameEventType::BuildingPlaced,
                        .entityId = placed->building.id,
                        .buildingType =
                            placed->building.type,
                        .position = buildingWorldPosition(
                            placed->building),
                    });
                    if (placed->building.type ==
                        BuildingType::Core) {
                        selectedBuilding_.reset();
                        buildingPreview_.reset();
                    }
                } else if (createdFoundation) {
                    static_cast<void>(
                        foundations_.remove(
                            createdFoundation->id));
                    syncModularStructures();
                }
            }
        }
        if (!placement.valid()) {
            Vec3 rejectedPosition =
                buildingWorldPosition(
                    command.placeBuilding->type,
                    command.placeBuilding
                        ->gridPosition);
            rejectedPosition.y = surface.height;
            events_.push_back({
                .type = GameEventType::BuildingRejected,
                .buildingType = command.placeBuilding->type,
                .placementError = placement.error,
                .position = rejectedPosition,
            });
        }
    }

    processBuildingActions(command);
}


} // namespace ian
