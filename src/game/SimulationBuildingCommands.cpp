#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "game/ModularCombat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

ResourceCost addCosts(ResourceCost left, ResourceCost right) {
    return {
        saturatingAdd(left.wood, right.wood),
        saturatingAdd(left.stone, right.stone),
        saturatingAdd(left.gold, right.gold),
    };
}

bool canPay(ResourceCost cost, int wood, int stone, int gold) {
    return wood >= cost.wood && stone >= cost.stone &&
           gold >= cost.gold;
}

} // namespace

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
                      gridPosition,
                      surface.height)
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
            previewPlacement.cost = addCosts(
                previewPlacement.cost,
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::Foundation)]);
            if (previewPlacement.valid() &&
                !unlimitedResources_ &&
                !canPay(
                    previewPlacement.cost,
                    wood_, stone_, gold_)) {
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
                          ->gridPosition,
                      surface.height)
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
            placement.cost = addCosts(
                placement.cost,
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::Foundation)]);
            if (placement.valid() &&
                !unlimitedResources_ &&
                !canPay(
                    placement.cost,
                    wood_, stone_, gold_)) {
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
                        : gold_,
                    surface.height, surface.storey,
                    surface.foundationBottomHeight);
                if (placed) {
                    if (!unlimitedResources_) {
                        ResourceCost transactionCost =
                            placed->cost;
                        if (createdFoundation) {
                            transactionCost = addCosts(
                                transactionCost,
                                modularBuildingCosts_[
                                    static_cast<std::size_t>(
                                        ModularBuildPiece::Foundation)]);
                        }
                        wood_ -= transactionCost.wood;
                        stone_ -= transactionCost.stone;
                        gold_ -= transactionCost.gold;
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
                        state_ = RunState::BuildPhase;
                        phaseTimeRemaining_ =
                            gameplay_
                                .firstBuildPhaseSeconds;
                        phaseDuration_ =
                            phaseTimeRemaining_;
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

    if (!selectedBuilding_ && command.upgradeBuilding) {
        const int availableWood =
            unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
        const int availableStone =
            unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
        const int availableGold =
            unlimitedResources_ ? std::numeric_limits<int>::max() : gold_;
        const UpgradeResult result =
            buildings_.upgrade(command.upgradeBuilding->buildingId, availableWood, availableStone,
                               availableGold);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ -= result.cost.wood;
                stone_ -= result.cost.stone;
                gold_ -= result.cost.gold;
            }
            syncWorldStructures();
            events_.push_back({
                .type = GameEventType::BuildingUpgraded,
                .entityId = result.building->id,
                .buildingType = result.building->type,
                .position =
                    buildingWorldPosition(*result.building),
            });
        } else {
            events_.push_back({
                .type = GameEventType::BuildingUpgradeRejected,
                .entityId = command.upgradeBuilding->buildingId,
                .upgradeError = result.error,
            });
        }
    }

    if (!selectedBuilding_ && command.repairBuilding) {
        const int availableWood =
            unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
        const int availableStone =
            unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
        const int availableGold =
            unlimitedResources_ ? std::numeric_limits<int>::max() : gold_;
        const RepairResult result =
            buildings_.repair(command.repairBuilding->buildingId, availableWood, availableStone,
                              availableGold);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ -= result.cost.wood;
                stone_ -= result.cost.stone;
                gold_ -= result.cost.gold;
            }
            goldMines_.syncBuildings(
                buildings_.buildings());
            events_.push_back({
                .type = GameEventType::BuildingRepaired,
                .entityId = result.building->id,
                .buildingType = result.building->type,
                .position =
                    buildingWorldPosition(*result.building),
                .amount = static_cast<int>(result.repairedHealth),
            });
        } else if (
            result.error == BuildingActionError::NotFound) {
            const ModularBuildingRepairResult
                modularResult = foundations_.repair(
                    command.repairBuilding->buildingId,
                    availableWood, availableStone);
            if (modularResult.valid()) {
                if (!unlimitedResources_) {
                    wood_ -= modularResult.cost.wood;
                    stone_ -= modularResult.cost.stone;
                }
                events_.push_back({
                    .type =
                        GameEventType::
                            ModularBuildingRepaired,
                    .entityId = modularResult.id,
                    .platformFrame =
                        modularResult.platformFrame,
                    .modularWall =
                        modularResult.wall,
                    .ramp = modularResult.ramp,
                    .position = modularBaseCenter(
                        modularResult, worldConfig_),
                    .amount = static_cast<int>(
                        modularResult.repairedHealth),
                });
            } else {
                events_.push_back({
                    .type =
                        GameEventType::
                            BuildingRepairRejected,
                    .entityId =
                        command.repairBuilding
                            ->buildingId,
                    .buildingActionError =
                        modularResult.error,
                });
            }
        } else {
            events_.push_back({
                .type = GameEventType::BuildingRepairRejected,
                .entityId = command.repairBuilding->buildingId,
                .buildingActionError = result.error,
            });
        }
    }

    if (!selectedBuilding_ && command.sellBuilding) {
        const SellResult result = buildings_.sell(command.sellBuilding->buildingId);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ = saturatingAdd(wood_, result.refund.wood);
                stone_ = saturatingAdd(stone_, result.refund.stone);
                gold_ = saturatingAdd(gold_, result.refund.gold);
            }
            aimedBuilding_.reset();
            syncWorldStructures();
            events_.push_back({
                .type = GameEventType::BuildingSold,
                .entityId = result.building->id,
                .buildingType = result.building->type,
                .position =
                    buildingWorldPosition(*result.building),
            });
        } else {
            events_.push_back({
                .type = GameEventType::BuildingSellRejected,
                .entityId = command.sellBuilding->buildingId,
                .buildingActionError = result.error,
            });
        }
    }

    if (!selectedBuilding_ &&
        command.removeModularBuilding) {
        const EntityId target =
            command.removeModularBuilding->buildingId;
        if (modularRemovalWouldDestroyCore(
                target)) {
            events_.push_back({
                .type =
                    GameEventType::
                        BuildingSellRejected,
                .entityId = target,
                .buildingType =
                    BuildingType::Core,
                .buildingActionError =
                    BuildingActionError::
                        Unsupported,
            });
        } else if (foundations_.remove(target)) {
            aimedModularBuilding_.reset();
            syncModularStructures();
            removeUnsupportedPlatformBuildings();
        }
    }

    if (!selectedBuilding_ && command.toggleGate) {
        const auto gate =
            std::find_if(buildings_.buildings().begin(), buildings_.buildings().end(),
                         [&command](const BuildingInstance& building) {
                             return building.id == command.toggleGate->gateId &&
                                    building.type == BuildingType::Gate;
                         });
        bool rejected = gate == buildings_.buildings().end();
        if (!rejected && gate->open) {
            const CollisionBox gateBox =
                buildingCollisionBox(
                    gate->type, gate->gridPosition,
                    gate->baseHeight);
            rejected = collisionWorld_.overlapsCircle(
                playerPosition_, CollisionWorld::PlayerRadius, gateBox);
        }
        if (rejected) {
            events_.push_back({
                .type = GameEventType::GateToggleRejected,
                .entityId = command.toggleGate->gateId,
            });
        } else {
            const auto toggled = buildings_.toggleGate(command.toggleGate->gateId);
            if (toggled) {
                syncWorldStructures();
                events_.push_back({
                    .type = GameEventType::GateToggled,
                    .entityId = toggled->id,
                    .buildingType = toggled->type,
                    .position =
                        buildingWorldPosition(*toggled),
                    .amount = toggled->open ? 1 : 0,
                });
            }
        }
    }
}


} // namespace ian
