#include "app/App.hpp"

#include "app/ActionModeEquipment.hpp"

#include <algorithm>
#include <array>

namespace ian {
using namespace app_detail;

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
