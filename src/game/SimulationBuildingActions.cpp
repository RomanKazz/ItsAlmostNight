#include "game/Simulation.hpp"

#include "buildings/BuildingOrientation.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ModularCombat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

ResourceCost salvageSaleBonus(ResourceCost cost,
                              ResourceCost standardRefund,
                              double stacks) {
    if (stacks <= 0) return {};
    const double fraction = std::min(
        0.90, 0.30 * static_cast<double>(stacks));
    const auto bonus = [fraction](int paid, int refunded) {
        return std::min(
            std::max(0, paid - refunded),
            static_cast<int>(std::lround(
                static_cast<double>(paid) * fraction)));
    };
    return {
        bonus(cost.wood, standardRefund.wood),
        bonus(cost.stone, standardRefund.stone),
        bonus(cost.crystals, standardRefund.crystals),
    };
}

} // namespace

void Simulation::processBuildingActions(
    const PlayerCommand& command) {
    if (command.collectProducer) {
        const BuildingType type = command.collectProducer->type;
        if (type == BuildingType::LumberMill ||
            type == BuildingType::Quarry ||
            type == BuildingType::CrystalMine) {
            const ResourceType resource = type == BuildingType::LumberMill
                ? ResourceType::Wood
                : type == BuildingType::Quarry
                    ? ResourceType::Stone : ResourceType::Crystal;
            const ResourceCapacityType capacityType =
                type == BuildingType::LumberMill
                    ? ResourceCapacityType::Wood
                    : type == BuildingType::Quarry
                        ? ResourceCapacityType::Stone
                        : ResourceCapacityType::Crystal;
            int space = unlimitedResources_
                ? std::numeric_limits<int>::max()
                : std::max(0, resourceCapacity(capacityType) -
                    (resource == ResourceType::Wood ? wood_ :
                     resource == ResourceType::Stone ? stone_ : crystals_));
            for (const CrystalMineRuntime& producer : crystalMines_.mines()) {
                if (producer.buildingType != type || space <= 0) continue;
                const auto building = std::ranges::find(
                    buildings_.buildings(), producer.buildingId,
                    &BuildingInstance::id);
                if (building == buildings_.buildings().end()) continue;
                const int amount = crystalMines_.collect(building->id, space);
                if (amount <= 0) continue;
                space -= amount;
                if (resource == ResourceType::Wood) addWood(amount);
                else if (resource == ResourceType::Stone) addStone(amount);
                else addCrystals(amount);
                events_.push_back({
                    .type = resource == ResourceType::Crystal
                        ? GameEventType::CrystalProduced
                        : GameEventType::ResourceGranted,
                    .entityId = building->id,
                    .resourceType = resource,
                    .buildingType = type,
                    .position = buildingWorldPosition(*building),
                    .amount = amount,
                });
            }
        }
    }

    if (!selectedBuilding_ && command.upgradeBuilding) {
        std::optional<BuildingInstance> upgraded;
        UpgradeError error = UpgradeError::None;
        const int attempts = command.upgradeBuilding->maximumAffordable
            ? MaxStructureLevel - 1 : 1;
        for (int attempt = 0; attempt < attempts; ++attempt) {
            const int availableWood = unlimitedResources_
                ? std::numeric_limits<int>::max() : wood_;
            const int availableStone = unlimitedResources_
                ? std::numeric_limits<int>::max() : stone_;
            const int availableCrystals = unlimitedResources_
                ? std::numeric_limits<int>::max() : crystals_;
            const UpgradeResult result = buildings_.upgrade(
                command.upgradeBuilding->buildingId,
                availableWood, availableStone, availableCrystals);
            if (!result.valid() || !result.building) {
                error = result.error;
                break;
            }
            if (!unlimitedResources_) {
                wood_ -= result.cost.wood;
                stone_ -= result.cost.stone;
                crystals_ -= result.cost.crystals;
            }
            upgraded = result.building;
            if (upgraded->type == BuildingType::Core) {
                coreHealthBaseLevel_ = upgraded->level;
                coreBuildingBaseLevel_ = upgraded->level;
                coreStorageBaseLevel_ = std::min<int>(upgraded->level, MaxBuildingLevel);
            }
        }
        if (upgraded) {
            // A bulk upgrade produces one effect/sound and one runtime refresh.
            syncBuildingRuntimeSystems();
            events_.push_back({
                .type = GameEventType::BuildingUpgraded,
                .entityId = upgraded->id,
                .buildingType = upgraded->type,
                .position = buildingWorldPosition(*upgraded),
            });
        } else {
            events_.push_back({
                .type = GameEventType::BuildingUpgradeRejected,
                .entityId = command.upgradeBuilding->buildingId,
                .upgradeError = error,
            });
        }
    }

    if (!selectedBuilding_ && command.repairBuilding) {
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
            fortifyTarget->type != BuildingType::Core &&
            fortifyTarget->health >= fortifyTarget->maxHealth) {
            auto active = std::ranges::find(
                activeFortifications_, command.repairBuilding->buildingId,
                &ActiveFortification::id);
            if (active == activeFortifications_.end())
                activeFortifications_.push_back({command.repairBuilding->buildingId,
                    classLevelBonuses().fortificationDuration});
            else
                active->remaining = classLevelBonuses().fortificationDuration;
            events_.push_back({.type = GameEventType::BuildingFortified,
                               .entityId = fortifyTarget->id,
                               .buildingType = fortifyTarget->type,
                               .position = buildingWorldPosition(*fortifyTarget),
                               .amount = static_cast<int>(std::lround(classLevelBonuses().fortificationDuration))});
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
            triggerAnvilShockwave(
                result.building->id,
                buildingWorldPosition(*result.building));
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
                triggerAnvilShockwave(
                    modularResult.id,
                    modularBaseCenter(modularResult, worldConfig_));
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
                const ResourceCost bonus = salvageSaleBonus(
                    buildings_.configuredCost(result.building->type),
                    result.refund,
                    runUpgradePower(RunUpgradeEffect::Salvager));
                wood_ = saturatingAdd(
                    wood_, saturatingAdd(result.refund.wood, bonus.wood));
                stone_ = saturatingAdd(
                    stone_, saturatingAdd(result.refund.stone, bonus.stone));
                crystals_ = saturatingAdd(
                    crystals_, saturatingAdd(
                        result.refund.crystals, bonus.crystals));
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
        ResourceCost modularCost{};
        const auto frame = std::ranges::find(
            foundations_.platformFrames(), target,
            &PlatformFrameInstance::id);
        if (frame != foundations_.platformFrames().end()) {
            modularCost = modularBuildingCosts_[static_cast<std::size_t>(
                frame->storey == 0
                    ? ModularBuildPiece::Foundation
                    : ModularBuildPiece::FloorPlatform)];
        } else if (std::ranges::any_of(
                       foundations_.walls(),
                       [target](const WallInstance& wall) {
                           return wall.id == target;
                       })) {
            modularCost = modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::Wall)];
        } else if (std::ranges::any_of(
                       foundations_.ramps(),
                       [target](const RampInstance& ramp) {
                           return ramp.id == target;
                       })) {
            modularCost = modularBuildingCosts_[static_cast<std::size_t>(
                ModularBuildPiece::Ramp)];
        }
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
            if (!unlimitedResources_) {
                const ResourceCost bonus = salvageSaleBonus(
                    modularCost, {}, runUpgradePower(RunUpgradeEffect::Salvager));
                wood_ = saturatingAdd(wood_, bonus.wood);
                stone_ = saturatingAdd(stone_, bonus.stone);
                crystals_ = saturatingAdd(crystals_, bonus.crystals);
            }
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
                                    (building.type == BuildingType::Gate || building.type == BuildingType::Wall);
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
            const auto toggled = gate->type == BuildingType::Wall
                ? buildings_.convertWallToGate(command.toggleGate->gateId)
                : buildings_.toggleGate(command.toggleGate->gateId);
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

void Simulation::triggerAnvilShockwave(
    EntityId sourceId, Vec3 position) {
    const int stacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Anvil)];
    if (stacks <= 0) return;
    const double radius = std::min(
        9.0, 4.8 + static_cast<double>(stacks - 1) * 0.7);
    const double strength = std::min(
        15.0, 8.0 + static_cast<double>(stacks - 1) * 1.25);
    const auto affected = enemies_.knockbackInRadius(
        position, radius, strength);
    events_.push_back({
        .type = GameEventType::AnvilRepairShockwave,
        .entityId = sourceId,
        .position = position,
        .amount = static_cast<int>(affected.size()),
        .intensity = radius,
    });
}

} // namespace ian
