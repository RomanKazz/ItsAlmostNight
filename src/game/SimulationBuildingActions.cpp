#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "game/ModularCombat.hpp"

#include <algorithm>
#include <limits>

namespace ian {

void Simulation::processBuildingActions(
    const PlayerCommand& command) {
    if (!selectedBuilding_ && command.upgradeBuilding) {
        const int availableWood =
            unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
        const int availableStone =
            unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
        const int availableCurrency =
            unlimitedResources_ ? std::numeric_limits<int>::max() : crystals_;
        const UpgradeResult result =
            buildings_.upgrade(command.upgradeBuilding->buildingId, availableWood, availableStone,
                               availableCurrency);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ -= result.cost.wood;
                stone_ -= result.cost.stone;
                crystals_ -= result.cost.crystals;
            }
            syncBuildingRuntimeSystems();
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
        if (!unlimitedResources_ &&
            !skillTree_.hasEffect("unlock.hammer")) {
            events_.push_back({
                .type = GameEventType::BuildingRepairRejected,
                .entityId = command.repairBuilding->buildingId,
                .buildingActionError = BuildingActionError::Unsupported,
            });
            return;
        }
        const double cooldownRemaining =
            repairCooldownRemaining(
                command.repairBuilding->buildingId);
        if (cooldownRemaining > 0.0) {
            events_.push_back({
                .type = GameEventType::BuildingRepairRejected,
                .entityId = command.repairBuilding->buildingId,
                .buildingActionError =
                    BuildingActionError::Cooldown,
                .intensity = cooldownRemaining,
            });
            return;
        }
        const auto fortifyTarget = std::ranges::find(
            buildings_.buildings(), command.repairBuilding->buildingId,
            &BuildingInstance::id);
        if (fortifyTarget != buildings_.buildings().end() &&
            fortifyTarget->health >= fortifyTarget->maxHealth) {
            auto active = std::ranges::find(
                activeFortifications_, command.repairBuilding->buildingId,
                &ActiveFortification::id);
            if (active == activeFortifications_.end())
                activeFortifications_.push_back({command.repairBuilding->buildingId, 10.0});
            else
                active->remaining = 10.0;
            events_.push_back({.type = GameEventType::BuildingFortified,
                               .entityId = fortifyTarget->id,
                               .buildingType = fortifyTarget->type,
                               .position = buildingWorldPosition(*fortifyTarget),
                               .amount = 10});
            startRepairCooldown(fortifyTarget->id);
            return;
        }
        const int availableWood =
            unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
        const int availableStone =
            unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
        const int availableCurrency =
            unlimitedResources_ ? std::numeric_limits<int>::max() : crystals_;
        const RepairResult result =
            buildings_.repair(command.repairBuilding->buildingId, availableWood, availableStone,
                              availableCurrency);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ -= result.cost.wood;
                stone_ -= result.cost.stone;
                crystals_ -= result.cost.crystals;
            }
            crystalMines_.syncBuildings(
                buildings_.buildings());
            events_.push_back({
                .type = GameEventType::BuildingRepaired,
                .entityId = result.building->id,
                .buildingType = result.building->type,
                .position =
                    buildingWorldPosition(*result.building),
                .amount = static_cast<int>(result.repairedHealth),
            });
            startRepairCooldown(result.building->id);
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
                startRepairCooldown(modularResult.id);
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
                crystals_ = saturatingAdd(crystals_, result.refund.crystals);
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
