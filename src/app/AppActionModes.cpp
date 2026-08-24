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

    interactionResourceAim_.reset();
    hoveredResource_.reset();
    hoveredBuilding_.reset();
    hoveredEnemy_.reset();
    hoverGraceRemaining_ = 0.0;
}

void App::selectActionMode(
    ActionMode mode,
    const SimulationSnapshot& snapshot) {
    if (actionMode_ == ActionMode::Tools &&
        isPlayerTool(snapshot.selectedWeapon)) {
        lastToolSelection_ = snapshot.selectedWeapon;
    } else if (actionMode_ == ActionMode::Weapons &&
               isPlayerCombatWeapon(snapshot.selectedWeapon)) {
        lastWeaponSelection_ = snapshot.selectedWeapon;
    }

    std::optional<PlayerWeapon> equipment;
    if (mode == ActionMode::Tools) {
        equipment = availableEquipment(
            lastToolSelection_, mode, snapshot);
    } else if (mode == ActionMode::Weapons) {
        equipment = availableEquipment(
            lastWeaponSelection_, mode, snapshot);
        if (!equipment) {
            statusMessage_ = "NO WEAPONS UNLOCKED";
            statusMessageRemaining_ = 1.4;
            invalidActionRemaining_ = 0.22;
            audio_.playUiError();
            return;
        }
    }

    if (mode != actionMode_) {
        previousActionMode_ = actionMode_;
        actionMode_ = mode;
    }

    switch (mode) {
    case ActionMode::Tools:
    case ActionMode::Weapons:
        setFoundationBuildMode(false);
        pendingBuildingCancel_ = true;
        pendingBuildingSelection_.reset();
        if (equipment) {
            pendingWeaponSelection_ = *equipment;
            if (mode == ActionMode::Tools) {
                lastToolSelection_ = *equipment;
            } else {
                lastWeaponSelection_ = *equipment;
            }
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
        pendingBuildingSelection_ =
            snapshot.coreMaxHealth <= 0.0
                ? BuildingType::Core
                : lastBuildingSelection_;
        break;
    case ActionMode::Modular:
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
        setFoundationBuildMode(true);
        break;
    }
}

void App::selectNextActionModeItem(
    const SimulationSnapshot& snapshot,
    int direction) {
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
    if (actionMode_ == ActionMode::Tools) {
        lastToolSelection_ = available[nextIndex];
    } else {
        lastWeaponSelection_ = available[nextIndex];
    }
}

} // namespace ian
