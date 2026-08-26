#include "app/App.hpp"

#include "app/ActionModeEquipment.hpp"

#include <algorithm>
#include <array>

namespace ian {
using namespace app_detail;

void App::resetRunInputState() {
    input_ = {};
    pendingYaw_ = 0.0;
    pendingPitch_ = 0.0;
    pendingJump_ = false;
    pendingDash_ = false;
    pendingPickaxe_ = false;
    pendingRifleShot_ = false;
    pendingIceWandShot_ = false;
    pendingFireWandShot_ = false;
    pendingBuildingSelection_.reset();
    pendingBuildingCancel_ = false;
    pendingBuildingPlacement_.reset();
    pendingBuildingRotation_ = 0;
    buildingItemWheelAccumulator_ = 0.0;
    buildingItemWheelReleaseRemaining_ = 0.0;
    buildingItemWheelArmed_ = true;
    pendingStartWave_ = false;
    pendingUnlimitedResources_ = false;
    pendingBuildingUpgrade_.reset();
    pendingBuildingBlueprintUpgrade_.reset();
    pendingBuildingRepair_.reset();
    pendingRepairAllBuildings_ = false;
    pendingPurchaseBombBundle_ = false;
    pendingBuildingSale_.reset();
    pendingModularBuildingRemoval_.reset();
    pendingWeaponSelection_.reset();
    pendingWeaponUpgrade_ = false;
    pendingBombThrow_ = false;
    pendingInteract_ = false;
    pendingChestReroll_.reset();
    pendingRevealChest_ = false;
    pendingDefeatAllEnemies_ = false;
    pendingToggleInvulnerability_ = false;
    pendingDamageCore_ = false;
    pendingSpawnEnemy_ = false;
    pendingChainLightning_ = false;
    pendingGateToggle_.reset();
    pendingPlacedBuildingRotation_.reset();
    coreDefenseMenuVisible_ = false;
    runUpgradeChoiceWasVisible_ = false;
    runUpgradeChoiceInputDelayRemaining_ = 0.0;
    runUpgradeChoiceInputArmed_ = false;

    primaryAttackHoldSeconds_ = 0.0;
    toolSwingUsesAxe_ = false;
    toolSwingQueued_ = false;
    toolQueuedSwingHasAttack_ = false;
    toolQueuedResourceTarget_.reset();
    toolSwingAttackPending_ = false;
    toolSwingQueueRemaining_ = 0.0;
    toolSwingRemaining_ = 0.0;
    toolContactHoldRemaining_ = 0.0;
    displayedToolVisual_ = FirstPersonToolVisual::None;
    toolSwapCandidateVisual_ = FirstPersonToolVisual::None;
    toolSwapDestinationVisual_ = FirstPersonToolVisual::None;
    toolViewModelInitialized_ = false;
    toolSwapCandidateSeconds_ = 0.0;
    toolSwapRemaining_ = 0.0;
    contextualHammerRemaining_ = 0.0;
    contextualHammerSwingPending_ = false;

    interactionResourceAim_.reset();
    buildingHotbarCategory_ = BuildingHotbarCategory::Base;
    hoveredResource_.reset();
    hoveredBuilding_.reset();
    hoveredEnemy_.reset();
    hoverGraceRemaining_ = 0.0;
}

void App::resetEquipmentActionMode(
    const SimulationSnapshot& snapshot) {
    foundationBuildMode_ = false;
    actionMode_ = ActionMode::Equipment;
    previousActionMode_ = ActionMode::Buildings;
    lastEquipmentSelection_ = snapshot.selectedWeapon;
}

void App::selectActionMode(
    ActionMode mode,
    const SimulationSnapshot& snapshot) {
    if (actionMode_ == ActionMode::Equipment) {
        lastEquipmentSelection_ = snapshot.selectedWeapon;
    }

    std::optional<PlayerWeapon> equipment;
    if (mode == ActionMode::Equipment) {
        equipment = availableEquipment(
            lastEquipmentSelection_, mode, snapshot);
    }

    if (mode != actionMode_) {
        previousActionMode_ = actionMode_;
        actionMode_ = mode;
        buildingItemWheelAccumulator_ = 0.0;
        buildingItemWheelReleaseRemaining_ = 0.0;
        buildingItemWheelArmed_ = true;
    }

    switch (mode) {
    case ActionMode::Equipment:
        setFoundationBuildMode(false);
        pendingBuildingCancel_ = true;
        pendingBuildingSelection_.reset();
        if (equipment) {
            pendingWeaponSelection_ = *equipment;
            lastEquipmentSelection_ = *equipment;
        }
        break;
    case ActionMode::Buildings:
        pendingWeaponSelection_.reset();
        pendingPickaxe_ = false;
        pendingRifleShot_ = false;
        pendingIceWandShot_ = false;
        pendingFireWandShot_ = false;
        toolSwingAttackPending_ = false;
        toolSwingQueued_ = false;
        toolQueuedSwingHasAttack_ = false;
        toolQueuedResourceTarget_.reset();
        toolSwingQueueRemaining_ = 0.0;
        setFoundationBuildMode(false);
        pendingBuildingCancel_ = false;
        if (snapshot.coreMaxHealth <= 0.0) {
            buildingHotbarCategory_ = BuildingHotbarCategory::Base;
            pendingBuildingSelection_ = BuildingType::Core;
        } else {
            buildingHotbarCategory_ =
                buildingHotbarCategory(lastBuildingSelection_);
            const BuildingHotbarLayout layout =
                makeBuildingHotbarLayout(
                    snapshot.unlockedBuildings,
                    buildingHotbarCategory_, true,
                    snapshot.coreLevel,
                    snapshot.sandboxMode);
            if (layout.indexOf(lastBuildingSelection_)) {
                pendingBuildingSelection_ = lastBuildingSelection_;
            } else if (layout.count > 0U) {
                lastBuildingSelection_ = layout.types[0];
                pendingBuildingSelection_ = lastBuildingSelection_;
            } else {
                pendingBuildingSelection_.reset();
            }
        }
        break;
    }
}

void App::selectNextBuildingCategory(
    const SimulationSnapshot& snapshot,
    int direction) {
    if (snapshot.coreMaxHealth <= 0.0) {
        buildingHotbarCategory_ = BuildingHotbarCategory::Base;
        lastBuildingSelection_ = BuildingType::Core;
        pendingBuildingSelection_ = BuildingType::Core;
        return;
    }
    const int step = direction < 0 ? -1 : 1;
    int category = static_cast<int>(buildingHotbarCategory_);
    for (std::size_t attempt = 0;
         attempt < BuildingHotbarCategoryCount; ++attempt) {
        category =
            (category + step +
             static_cast<int>(BuildingHotbarCategoryCount)) %
            static_cast<int>(BuildingHotbarCategoryCount);
        const auto candidate =
            static_cast<BuildingHotbarCategory>(category);
        if (candidate == BuildingHotbarCategory::Modular) {
            if (!snapshot.sandboxMode && snapshot.coreLevel < 2) {
                continue;
            }
            buildingHotbarCategory_ = candidate;
            pendingBuildingSelection_.reset();
            setFoundationBuildMode(true);
            return;
        }
        const BuildingHotbarLayout layout =
            makeBuildingHotbarLayout(
                snapshot.unlockedBuildings, candidate, true,
                snapshot.coreLevel,
                snapshot.sandboxMode);
        if (layout.count == 0U) continue;
        setFoundationBuildMode(false);
        buildingHotbarCategory_ = candidate;
        lastBuildingSelection_ = layout.types[0];
        pendingBuildingSelection_ = lastBuildingSelection_;
        return;
    }
}

void App::selectNextBuildingItem(
    const SimulationSnapshot& snapshot,
    int direction) {
    if (buildingHotbarCategory_ ==
        BuildingHotbarCategory::Modular) {
        const int count = static_cast<int>(ModularBuildPieceCount);
        const int current = static_cast<int>(modularBuildPiece_);
        const int step = direction < 0 ? -1 : 1;
        selectModularBuildPiece(static_cast<ModularBuildPiece>(
            (current + step + count) % count));
        return;
    }
    const BuildingHotbarLayout layout =
        makeBuildingHotbarLayout(
            snapshot.unlockedBuildings,
            buildingHotbarCategory_,
            snapshot.coreMaxHealth > 0.0,
            snapshot.coreLevel,
            snapshot.sandboxMode);
    if (layout.count == 0U) return;

    const BuildingType current =
        layout.indexOf(lastBuildingSelection_)
            ? lastBuildingSelection_
            : snapshot.selectedBuilding.value_or(
                  layout.types[0]);
    const std::size_t currentIndex =
        layout.indexOf(current).value_or(0U);
    const std::size_t nextIndex = direction < 0
        ? (currentIndex + layout.count - 1U) % layout.count
        : (currentIndex + 1U) % layout.count;
    lastBuildingSelection_ = layout.types[nextIndex];
    pendingBuildingSelection_ = lastBuildingSelection_;
}

void App::selectNextActionModeItem(
    const SimulationSnapshot& snapshot,
    int direction) {
    if (actionMode_ == ActionMode::Buildings) {
        selectNextBuildingCategory(snapshot, direction);
        return;
    }
    const auto order = equipmentOrder(actionMode_);
    if (order.empty()) {
        return;
    }
    std::array<PlayerWeapon, PlayerWeaponCount> available{};
    std::size_t availableCount = 0U;
    for (const PlayerWeapon weapon : order) {
        if (snapshot.unlockedWeapons[
                static_cast<std::size_t>(weapon)]) {
            available[availableCount++] = weapon;
        }
    }
    if (availableCount == 0U) {
        return;
    }
    const auto availableItems = std::span{
        available.data(), availableCount};
    const auto current = std::ranges::find(
        availableItems, snapshot.selectedWeapon);
    std::size_t nextIndex = 0U;
    if (current != availableItems.end()) {
        const std::size_t currentIndex = static_cast<std::size_t>(
            std::distance(availableItems.begin(), current));
        nextIndex = direction < 0
            ? (currentIndex + availableCount - 1U) % availableCount
            : (currentIndex + 1U) % availableCount;
    } else if (direction < 0) {
        nextIndex = availableCount - 1U;
    }
    pendingWeaponSelection_ = available[nextIndex];
    lastEquipmentSelection_ = available[nextIndex];
}

} // namespace ian
