#include "app/App.hpp"

#include "app/ActionModeEquipment.hpp"

#include <algorithm>
#include <array>

namespace ian {
using namespace app_detail;

void App::suspendGameplayInput() {
    weaponLookYawOffset_ = weaponLookPitchOffset_ = 0.0;
    weaponLookYawVelocity_ = weaponLookPitchVelocity_ = 0.0;
    cameraInertiaInitialized_ = false;
    input_.moveForward = 0.0;
    input_.moveRight = 0.0;
    input_.sprint = false;
    pendingYaw_ = 0.0;
    pendingPitch_ = 0.0;
    pendingJump_ = false;
    pendingDash_ = false;
    pendingPickaxe_ = false;
    pendingRifleShot_ = false;
    pendingIceWandShot_ = false;
    pendingFireWandShot_ = false;
}

void App::resetRunInputState() {
    input_ = {};
    suspendGameplayInput();
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
    pendingPurchaseBombBundle_ = false;
    pendingBuildingSale_.reset();
    pendingModularBuildingRemoval_.reset();
    pendingWeaponSelection_.reset();
    pendingWeaponUpgrade_ = false;
    pendingBombThrow_ = false;
    pendingInteract_ = false;
    pendingRevealChest_ = false;
    pendingDefeatAllEnemies_ = false;
    pendingToggleInvulnerability_ = false;
    pendingGrantInsight_ = false;
    pendingDamageCore_ = false;
    pendingSpawnEnemy_ = false;
    pendingChainLightning_ = false;
    pendingGateToggle_.reset();
    pendingPlacedBuildingRotation_.reset();
    selectBuildingAfterCorePlacement_ = false;
    coreDefenseMenuVisible_ = false;
    coreDefenseMenuClosing_ = false;
    coreMenuPointerActive_ = false;
    coreDefenseMenuEntranceSeconds_ = 0.0;
    coreMenuLostAimSeconds_ = 0.0;
    coreDefenseMenuAnimatedLevel_ = 0;
    placementPreviewEntranceSeconds_ = 0.0;
    outgoingBuildingPreview_.reset();
    outgoingPreviewCenter_.reset();
    outgoingPlatformPreview_.reset();
    outgoingWallPreview_.reset();
    outgoingRampPreview_.reset();
    outgoingModularPreviewOrigin_.reset();
    outgoingPreviewRemaining_ = 0.0;
    runUpgradeChoiceWasVisible_ = false;
    runUpgradeChoiceInputDelayRemaining_ = 0.0;
    runUpgradeChoiceInputArmed_ = false;
    runUpgradeOverlayEntranceSeconds_ = 0.0;
    runUpgradeOverlayExitSeconds_ = 0.0;
    runUpgradePendingChoice_.reset();

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
    gateDoorVisuals_.clear();
}

void App::resetEquipmentActionMode(
    const SimulationSnapshot& snapshot) {
    foundationBuildMode_ = false;
    actionMode_ = ActionMode::Equipment;
    previousActionMode_ = ActionMode::Buildings;
    hotbarModeBlend_ = 0.0F;
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
        if (actionMode_ == ActionMode::Buildings &&
            mode == ActionMode::Equipment &&
            snapshot.buildingPreview) {
            outgoingBuildingPreview_ = snapshot.buildingPreview;
            outgoingPreviewCenter_ = placementPreviewCenter_;
            outgoingPreviewRemaining_ = 0.20;
        } else if (actionMode_ == ActionMode::Buildings &&
                   mode == ActionMode::Equipment &&
                   foundationBuildMode_) {
            outgoingPlatformPreview_ = platformFramePreview_;
            outgoingWallPreview_ = wallPreview_;
            outgoingRampPreview_ = rampPreview_;
            outgoingModularPreviewOrigin_ =
                modularPreviewVisualOrigin_;
            if (outgoingPlatformPreview_ || outgoingWallPreview_ ||
                outgoingRampPreview_) {
                outgoingPreviewRemaining_ = 0.20;
            }
        } else if (mode == ActionMode::Buildings) {
            outgoingBuildingPreview_.reset();
            outgoingPreviewCenter_.reset();
            outgoingPlatformPreview_.reset();
            outgoingWallPreview_.reset();
            outgoingRampPreview_.reset();
            outgoingModularPreviewOrigin_.reset();
            outgoingPreviewRemaining_ = 0.0;
        }
        previousActionMode_ = actionMode_;
        actionMode_ = mode;
        if (mode == ActionMode::Buildings &&
            toolViewModelInitialized_ &&
            displayedToolVisual_ != FirstPersonToolVisual::None) {
            toolSwapCandidateVisual_ = FirstPersonToolVisual::None;
            toolSwapDestinationVisual_ = FirstPersonToolVisual::None;
            toolSwapCandidateSeconds_ = 0.07;
            toolSwapDuration_ = std::clamp(
                static_cast<double>(activeToolTuning().swapDuration) *
                    ToolSwapDurationScale,
                0.30, 0.58);
            toolSwapRemaining_ = toolSwapDuration_;
        } else if (mode == ActionMode::Equipment &&
                   toolViewModelInitialized_ &&
                   displayedToolVisual_ ==
                       FirstPersonToolVisual::None &&
                   equipment) {
            const FirstPersonToolVisual visual =
                toolVisualForWeapon(*equipment);
            if (visual != FirstPersonToolVisual::None) {
                displayedToolVisual_ = visual;
                toolSwapCandidateVisual_ = visual;
                toolSwapDestinationVisual_ = visual;
                toolSwapCandidateSeconds_ = 0.07;
                toolSwapDuration_ = std::clamp(
                    static_cast<double>(
                        activeToolTuning().swapDuration) *
                        ToolSwapDurationScale,
                    0.30, 0.58);
                // Start the return partway above the hidden pose so the
                // weapon is visible as soon as the hotbars begin moving.
                toolSwapRemaining_ = toolSwapDuration_ * 0.34;
            }
        }
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
            lastBuildingSelection_ = BuildingType::Core;
            pendingBuildingSelection_ = BuildingType::Core;
        } else {
            const ConstructionHotbarLayout layout =
                makeConstructionHotbarLayout(
                    snapshot.unlockedBuildings, true, snapshot.coreLevel,
                    snapshot.sandboxMode || snapshot.unlimitedResources);
            selectConstructionHotbarItem(
                snapshot, layout.indexOf(lastBuildingSelection_).value_or(0U));
        }
        break;
    }
}

void App::selectNextBuildingItem(
    const SimulationSnapshot& snapshot,
    int direction) {
    const ConstructionHotbarLayout layout = makeConstructionHotbarLayout(
        snapshot.unlockedBuildings, snapshot.coreMaxHealth > 0.0,
        snapshot.coreLevel,
        snapshot.sandboxMode || snapshot.unlimitedResources);
    if (layout.count == 0U) return;
    std::optional<std::size_t> current;
    if (foundationBuildMode_) {
        current = layout.indexOf(modularBuildPiece_);
    } else {
        current = layout.indexOf(lastBuildingSelection_);
        if (!current && snapshot.selectedBuilding) {
            current = layout.indexOf(*snapshot.selectedBuilding);
        }
    }
    const std::size_t next = current
        ? (direction < 0 ? (*current + layout.count - 1U) % layout.count
                         : (*current + 1U) % layout.count)
        : (direction < 0 ? layout.count - 1U : 0U);
    selectConstructionHotbarItem(snapshot, next);
}

void App::selectConstructionHotbarItem(
    const SimulationSnapshot& snapshot,
    std::size_t index) {
    const ConstructionHotbarLayout layout =
        makeConstructionHotbarLayout(
            snapshot.unlockedBuildings,
            snapshot.coreMaxHealth > 0.0,
            snapshot.coreLevel,
            snapshot.sandboxMode ||
                snapshot.unlimitedResources);
    if (index >= layout.count) return;
    buildingHotbarCategory_ = BuildingHotbarCategory::Modular;
    const ConstructionHotbarItem& item = layout.items[index];
    if (item.modularPiece) {
        setFoundationBuildMode(true);
        selectModularBuildPiece(*item.modularPiece);
        return;
    }
    if (item.buildingType) {
        setFoundationBuildMode(false);
        pendingBuildingCancel_ = false;
        lastBuildingSelection_ = *item.buildingType;
        pendingBuildingSelection_ = *item.buildingType;
    }
}

void App::selectNextActionModeItem(
    const SimulationSnapshot& snapshot,
    int direction) {
    if (actionMode_ == ActionMode::Buildings) {
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
