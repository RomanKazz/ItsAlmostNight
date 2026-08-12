#include "app/App.hpp"
#include "app/ActionModeEquipment.hpp"
#include "app/AppRenderSupport.hpp"

#include "buildings/RampPlacementDirection.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ian {
using namespace app_detail;

namespace {
constexpr double MouseSensitivity = 0.002;

bool keyPressed(const ControlSettings& settings,
                ControlAction action) {
    const int key = controlKey(settings, action);
    if (action == ControlAction::Dash && key == KEY_NULL) {
        return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    }
    return key != KEY_NULL &&
           IsKeyPressed(static_cast<KeyboardKey>(key));
}

bool keyDown(const ControlSettings& settings,
             ControlAction action) {
    const int key = controlKey(settings, action);
    return key != KEY_NULL &&
           IsKeyDown(static_cast<KeyboardKey>(key));
}

bool keyReleased(const ControlSettings& settings,
                 ControlAction action) {
    const int key = controlKey(settings, action);
    return key != KEY_NULL &&
           IsKeyReleased(static_cast<KeyboardKey>(key));
}

}

void App::processInput() {
    renderer_->processInput();
    const auto snapshot = simulation_.snapshot();
    const bool graphicsPanelVisible =
        renderer_->graphicsPanelVisible();
    if (pendingControlRebind_) {
        const int pressedKey = GetKeyPressed();
        if (pressedKey == KEY_ESCAPE) {
            pendingControlRebind_.reset();
        } else if (pressedKey != KEY_NULL) {
            setControlKey(
                userSettings_.controls, *pendingControlRebind_,
                pressedKey);
            pendingControlRebind_.reset();
        }
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
        return;
    }
    if (graphicsPanelVisible != graphicsPanelWasVisible_) {
        if (graphicsPanelVisible) {
            EnableCursor();
        } else if (snapshot.state != RunState::MainMenu &&
                   snapshot.state != RunState::Paused) {
            DisableCursor();
        }
        graphicsPanelWasVisible_ = graphicsPanelVisible;
    }
    if (skillTree_.isOpen()) {
        if (keyPressed(userSettings_.controls, ControlAction::Skills) ||
            IsKeyPressed(KEY_ESCAPE)) {
            setSkillTreeVisible(false);
            audio_.playUiConfirm();
        }
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
        return;
    }
    if (!graphicsPanelVisible &&
        (keyPressed(userSettings_.controls, ControlAction::Skills) ||
         pendingOpenSkillTreeFromUi_)) {
        pendingOpenSkillTreeFromUi_ = false;
        setSkillTreeVisible(true);
        audio_.playUiConfirm();
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
        pendingYaw_ = 0.0;
        pendingPitch_ = 0.0;
        return;
    }
    if (graphicsPanelVisible) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            renderer_->setGraphicsPanelVisible(false);
            audio_.playUiConfirm();
        }
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
        pendingYaw_ = 0.0;
        pendingPitch_ = 0.0;
        return;
    }
    if (pendingResumeFromUi_) {
        pendingResumeFromUi_ = false;
        if (snapshot.state == RunState::Paused) {
            simulation_.togglePause();
            fixedStep_.reset();
            DisableCursor();
            audio_.playUiConfirm();
        }
    }
    if (pendingReturnToMenuFromUi_) {
        pendingReturnToMenuFromUi_ = false;
        simulation_.returnToMainMenu();
        fixedStep_.reset();
        EnableCursor();
        audio_.playUiConfirm();
        return;
    }
    if (snapshot.state != RunState::MainMenu &&
        IsKeyPressed(KEY_B)) {
        enemySpawnMenuVisible_ = !enemySpawnMenuVisible_;
        if (enemySpawnMenuVisible_) {
            EnableCursor();
        } else if (snapshot.state != RunState::Paused) {
            DisableCursor();
        }
    }
    const bool controlDown =
        IsKeyDown(KEY_LEFT_CONTROL) ||
        IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shiftDown =
        IsKeyDown(KEY_LEFT_SHIFT) ||
        IsKeyDown(KEY_RIGHT_SHIFT);
    if (snapshot.state != RunState::MainMenu &&
        controlDown &&
        IsKeyPressed(KEY_F10)) {
        simulation_.setStructuralCollapseEnabled(
            !simulation_.structuralCollapseEnabled());
    }
    if (snapshot.state != RunState::MainMenu &&
        controlDown &&
        IsKeyPressed(KEY_F11)) {
        static_cast<void>(
            simulation_.clearModularBuildings());
    }
    if (enemySpawnMenuVisible_) {
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
        pendingYaw_ = 0.0;
        pendingPitch_ = 0.0;
        return;
    }
    if (snapshot.state == RunState::MainMenu &&
        (IsKeyPressed(KEY_ENTER) || pendingStartFromUi_)) {
        pendingStartFromUi_ = false;
        simulation_.startRun();
        const auto startedSnapshot = simulation_.snapshot();
        rebuildTerrainGraphics();
        worldRevealOrigin_ = {
            static_cast<float>(
                startedSnapshot.playerPosition.x),
            static_cast<float>(
                startedSnapshot.playerPosition.z),
        };
        worldRevealElapsed_ = 0.0;
        audio_.playUiConfirm();
        fixedStep_.reset();
        statusMessage_.clear();
        statusMessageRemaining_ = 0.0;
        effects_.clear();
        arrowVisuals_.clear();
        floatingDamageNumbers_.clear();
        resourceGainVisuals_.clear();
        productionVisuals_.clear();
        destroyedResourceVisuals_.clear();
        soldBuildingVisuals_.clear();
        destroyedEnemyVisuals_.clear();
        enemyDrawInstances_.clear();
        destroyedEnemyDrawInstances_.clear();
        pendingSoldBuildingVisual_.reset();
        pendingSoldModularVisual_.reset();
        pendingSoldWallConnections_ = 0U;
        queuedBuildingSales_.clear();
        queuedModularBuildingRemovals_.clear();
        removalDragActive_ = false;
        removalDragTargets_.clear();
        grassClearAreas_.clear();
        buildingRotationWheelAccumulator_ = 0.0;
        buildingRotationCooldownRemaining_ = 0.0;
        cameraShakeRemaining_ = 0.0;
        cameraShakeStrength_ = 0.0;
        cameraLookYawLag_ = 0.0;
        cameraLookPitchLag_ = 0.0;
        cameraStrafeLean_ = 0.0;
        cameraImpulseOffset_ = {};
        landingResponseRemaining_ = 0.0;
        cameraInertiaInitialized_ = false;
        groundCameraSmoothingInitialized_ = false;
        groundCameraWasGrounded_ = false;
        cameraBobPositionInitialized_ = false;
        damageIndicators_.clear();
        woodHudBounceRemaining_ = 0.0;
        stoneHudBounceRemaining_ = 0.0;
        coinHudBounceRemaining_ = 0.0;
        playerDamageFlashRemaining_ = 0.0;
        recentlyDamagedBuilding_.reset();
        damagedBuildingHealthBarRemaining_ = 0.0;
        hitStopRemaining_ = 0.0;
        toolContactHoldRemaining_ = 0.0;
        crosshairHitRemaining_ = 0.0;
        invalidActionRemaining_ = 0.0;
        buildingHoverSeconds_ = 0.0;
        buildingContextCardTarget_.reset();
        buildingContextCardUpgradeCost_.reset();
        buildingContextCardStats_.reset();
        buildingImpactVisuals_.clear();
        placementPreviewCenter_.reset();
        placementPreviewGrid_.reset();
        placementPreviewType_.reset();
        placementSnapPulseRemaining_ = 0.0;
        weaponRecoilRemaining_ = 0.0;
        weaponRecoilStrength_ = 0.0F;
        cameraFov_ = 75.0F;
        cancelledPlacementPreview_.reset();
        buildingShotRecoilVisuals_.clear();
        buildingStatsUpgradeEntity_.reset();
        buildingStatsUpgradeRemaining_ = 0.0;
        wallDragStart_.reset();
        wallDragEnd_.reset();
        placementDragType_.reset();
        placementDragSurface_.reset();
        placementDragAxis_.reset();
        placementDragCandidateEnd_.reset();
        placementDragCandidateFrames_ = 0;
        placementDragLookMovement_ = 0.0;
        placementDragExtended_ = false;
        pendingWallPlacements_.clear();
        clearModularPlacementDrag();
        modularSnapHit_.reset();
        modularSnapMarker_.reset();
        modularEdgeHoverFrame_.reset();
        modularEdgeExtensionAnchor_.reset();
        rampSocketFrame_.reset();
        rampSocketRotation_.reset();
        rampSocketLostGraceRemaining_ = 0.0;
        rampSocketManualOverrideRemaining_ = 0.0;
        modularPreviewAnchor_.reset();
        buildModePieVisible_ = false;
        buildModePieDirection_ = {};
        buildModePieChoice_.reset();
        actionMode_ = ActionMode::Tools;
        previousActionMode_ = ActionMode::Weapons;
        lastToolSelection_ = PlayerWeapon::BareHands;
        lastWeaponSelection_ = PlayerWeapon::Club;
        foundationBuildMode_ = false;
        DisableCursor();
    }
    if (keyPressed(userSettings_.controls, ControlAction::Pause) ||
        IsKeyPressed(KEY_ESCAPE)) {
        simulation_.togglePause();
        fixedStep_.reset();
        if (simulation_.snapshot().state == RunState::Paused) {
            EnableCursor();
        } else if (simulation_.snapshot().state != RunState::MainMenu) {
            DisableCursor();
        }
    }
    if (snapshot.state != RunState::MainMenu &&
        (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) &&
        keyPressed(userSettings_.controls, ControlAction::Map)) {
        minimapHidden_ = !minimapHidden_;
    }
    if (snapshot.state != RunState::MainMenu &&
        (automaticRunRestartPending_ ||
         keyPressed(userSettings_.controls, ControlAction::Restart))) {
        automaticRunRestartPending_ = false;
        simulation_.restartRun();
        const auto restartedSnapshot = simulation_.snapshot();
        rebuildTerrainGraphics();
        worldRevealOrigin_ = {
            static_cast<float>(
                restartedSnapshot.playerPosition.x),
            static_cast<float>(
                restartedSnapshot.playerPosition.z),
        };
        worldRevealElapsed_ = 0.0;
        fixedStep_.reset();
        statusMessage_.clear();
        statusMessageRemaining_ = 0.0;
        effects_.clear();
        arrowVisuals_.clear();
        floatingDamageNumbers_.clear();
        resourceGainVisuals_.clear();
        productionVisuals_.clear();
        destroyedResourceVisuals_.clear();
        soldBuildingVisuals_.clear();
        destroyedEnemyVisuals_.clear();
        enemyDrawInstances_.clear();
        destroyedEnemyDrawInstances_.clear();
        pendingSoldBuildingVisual_.reset();
        pendingSoldModularVisual_.reset();
        pendingSoldWallConnections_ = 0U;
        queuedBuildingSales_.clear();
        queuedModularBuildingRemovals_.clear();
        removalDragActive_ = false;
        removalDragTargets_.clear();
        grassClearAreas_.clear();
        buildingRotationWheelAccumulator_ = 0.0;
        buildingRotationCooldownRemaining_ = 0.0;
        cameraShakeRemaining_ = 0.0;
        cameraShakeStrength_ = 0.0;
        cameraLookYawLag_ = 0.0;
        cameraLookPitchLag_ = 0.0;
        cameraStrafeLean_ = 0.0;
        cameraImpulseOffset_ = {};
        landingResponseRemaining_ = 0.0;
        cameraInertiaInitialized_ = false;
        groundCameraSmoothingInitialized_ = false;
        groundCameraWasGrounded_ = false;
        cameraBobPositionInitialized_ = false;
        damageIndicators_.clear();
        woodHudBounceRemaining_ = 0.0;
        stoneHudBounceRemaining_ = 0.0;
        coinHudBounceRemaining_ = 0.0;
        playerDamageFlashRemaining_ = 0.0;
        recentlyDamagedBuilding_.reset();
        damagedBuildingHealthBarRemaining_ = 0.0;
        hitStopRemaining_ = 0.0;
        toolContactHoldRemaining_ = 0.0;
        crosshairHitRemaining_ = 0.0;
        invalidActionRemaining_ = 0.0;
        buildingHoverSeconds_ = 0.0;
        buildingContextCardTarget_.reset();
        buildingContextCardUpgradeCost_.reset();
        buildingContextCardStats_.reset();
        buildingImpactVisuals_.clear();
        placementPreviewCenter_.reset();
        placementPreviewGrid_.reset();
        placementPreviewType_.reset();
        placementSnapPulseRemaining_ = 0.0;
        weaponRecoilRemaining_ = 0.0;
        weaponRecoilStrength_ = 0.0F;
        cameraFov_ = 75.0F;
        cancelledPlacementPreview_.reset();
        buildingShotRecoilVisuals_.clear();
        buildingStatsUpgradeEntity_.reset();
        buildingStatsUpgradeRemaining_ = 0.0;
        wallDragStart_.reset();
        wallDragEnd_.reset();
        placementDragType_.reset();
        placementDragSurface_.reset();
        placementDragAxis_.reset();
        placementDragCandidateEnd_.reset();
        placementDragCandidateFrames_ = 0;
        placementDragLookMovement_ = 0.0;
        placementDragExtended_ = false;
        pendingWallPlacements_.clear();
        clearModularPlacementDrag();
        modularSnapHit_.reset();
        modularSnapMarker_.reset();
        modularEdgeHoverFrame_.reset();
        modularEdgeExtensionAnchor_.reset();
        rampSocketFrame_.reset();
        rampSocketRotation_.reset();
        rampSocketLostGraceRemaining_ = 0.0;
        rampSocketManualOverrideRemaining_ = 0.0;
        modularPreviewAnchor_.reset();
        buildModePieVisible_ = false;
        buildModePieDirection_ = {};
        buildModePieChoice_.reset();
        actionMode_ = ActionMode::Tools;
        previousActionMode_ = ActionMode::Weapons;
        lastToolSelection_ = PlayerWeapon::BareHands;
        lastWeaponSelection_ = PlayerWeapon::Club;
        foundationBuildMode_ = false;
    }
    if (snapshot.state != RunState::MainMenu) {
        if (shiftDown &&
            IsKeyPressed(KEY_F10)) {
            performanceOverlayVisible_ =
                !performanceOverlayVisible_;
        }
        if (shiftDown && IsKeyPressed(KEY_F9)) {
            objectiveDebugMenuVisible_ =
                !objectiveDebugMenuVisible_;
        }
        if (controlDown &&
            IsKeyPressed(KEY_F6)) {
            showTerrainWireframe_ =
                !showTerrainWireframe_;
        }
        if (controlDown &&
            IsKeyPressed(KEY_F7)) {
            simulation_.regenerateTerrain(
                simulation_.terrain().seed());
            rebuildTerrainGraphics();
        }
        if (controlDown &&
            IsKeyPressed(KEY_F9)) {
            simulation_.regenerateTerrain(
                simulation_.terrain().seed() +
                0x9e3779b9U);
            rebuildTerrainGraphics();
        }
        if (IsKeyPressed(KEY_T)) {
            slowMotion_ = !slowMotion_;
            fixedStep_.reset();
        }
        if (controlDown &&
            IsKeyPressed(KEY_F8)) {
            showColliders_ = !showColliders_;
        }
        if (IsKeyPressed(KEY_H)) {
            showFlowField_ = !showFlowField_;
        }
        if (IsKeyPressed(KEY_L)) {
            showSpatialHash_ = !showSpatialHash_;
        }
        if (IsKeyPressed(KEY_J)) {
            hideBottomHud_ = !hideBottomHud_;
        }
        if (IsKeyPressed(KEY_Y)) {
            environment_.toggleFrozen();
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            environment_.adjustTime(-0.025F);
        }
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
            environment_.adjustTime(0.025F);
        }
        if (IsKeyPressed(KEY_APOSTROPHE)) {
            environment_.cycleProfile();
        }
        if (IsKeyPressed(KEY_BACKSLASH)) {
            environment_.useAutomaticTime();
        }
    }

    if (acceptsGameplayInput(simulation_.snapshot().state)) {
        auto currentSnapshot = simulation_.snapshot();
        currentSnapshot.aimedBuilding =
            preciseBuildingAim(
                *renderer_, currentSnapshot);
        currentSnapshot.aimedResource =
            preciseResourceAim(
                *renderer_, currentSnapshot);
        currentSnapshot.aimedModularBuilding =
            preciseModularBuildingAim(
                *renderer_, currentSnapshot);
        interactionResourceAim_ = currentSnapshot.aimedResource;
        input_.overrideAimedBuilding = true;
        input_.aimedBuildingOverride =
            currentSnapshot.aimedBuilding;
        input_.overrideAimedResource = true;
        input_.aimedResourceOverride =
            currentSnapshot.aimedResource;
        input_.overrideAimedModularBuilding = true;
        input_.aimedModularBuildingOverride =
            currentSnapshot.aimedModularBuilding;
        const std::optional<EntityId> actionBuilding =
            buildingContextCardTarget_
                ? buildingContextCardTarget_
                : currentSnapshot.aimedBuilding;
        if (wallDragStart_ &&
            (!currentSnapshot.selectedBuilding ||
             currentSnapshot.selectedBuilding !=
                 placementDragType_)) {
            wallDragStart_.reset();
            wallDragEnd_.reset();
            placementDragType_.reset();
            placementDragSurface_.reset();
            placementDragAxis_.reset();
            placementDragCandidateEnd_.reset();
            placementDragCandidateFrames_ = 0;
            placementDragLookMovement_ = 0.0;
            placementDragExtended_ = false;
        }
        input_.moveForward =
            static_cast<double>(
                keyDown(userSettings_.controls,
                        ControlAction::MoveForward)) -
            static_cast<double>(
                keyDown(userSettings_.controls,
                        ControlAction::MoveBackward));
        input_.moveRight =
            static_cast<double>(
                keyDown(userSettings_.controls,
                        ControlAction::MoveRight)) -
            static_cast<double>(
                keyDown(userSettings_.controls,
                        ControlAction::MoveLeft));
        input_.sprint = keyDown(
            userSettings_.controls, ControlAction::Sprint);

        const auto selectBuildingMode =
            [this, &currentSnapshot](BuildingType type) {
                selectActionMode(
                    ActionMode::Buildings, currentSnapshot);
                lastBuildingSelection_ = type;
                pendingBuildingSelection_ = type;
            };
        const Vector2 mouseDelta = GetMouseDelta();
        if (wallDragStart_) {
            placementDragLookMovement_ +=
                static_cast<double>(Vector2Length(mouseDelta));
        }
        if (modularDragPiece_) {
            modularDragLookMovement_ +=
                static_cast<double>(
                    Vector2Length(mouseDelta));
        }
        if (keyPressed(userSettings_.controls,
                       ControlAction::BuildMode)) {
            buildModePieVisible_ = true;
            buildModePieDirection_ = {};
            buildModePieChoice_.reset();
        }
        if (buildModePieVisible_) {
            constexpr float MaximumRadius = 112.0F;
            constexpr float DeadZoneRadius = 28.0F;
            buildModePieDirection_.x += mouseDelta.x;
            buildModePieDirection_.y += mouseDelta.y;
            const float directionLength =
                Vector2Length(buildModePieDirection_);
            if (directionLength > MaximumRadius) {
                buildModePieDirection_ =
                    Vector2Scale(
                        buildModePieDirection_,
                        MaximumRadius /
                            directionLength);
            }
            if (Vector2Length(
                    buildModePieDirection_) >=
                    DeadZoneRadius) {
                const float absoluteX = std::abs(
                    buildModePieDirection_.x);
                const float absoluteY = std::abs(
                    buildModePieDirection_.y);
                if (absoluteX > absoluteY) {
                    buildModePieChoice_ =
                        buildModePieDirection_.x < 0.0F
                            ? ActionMode::Tools
                            : ActionMode::Buildings;
                } else {
                    buildModePieChoice_ =
                        buildModePieDirection_.y < 0.0F
                            ? ActionMode::Weapons
                            : ActionMode::Modular;
                }
            } else {
                buildModePieChoice_.reset();
                if (keyDown(
                        userSettings_.controls,
                        ControlAction::MoveLeft)) {
                    buildModePieChoice_ = ActionMode::Tools;
                } else if (keyDown(
                               userSettings_.controls,
                               ControlAction::MoveForward)) {
                    buildModePieChoice_ = ActionMode::Weapons;
                } else if (keyDown(
                               userSettings_.controls,
                               ControlAction::MoveRight)) {
                    buildModePieChoice_ = ActionMode::Buildings;
                } else if (keyDown(
                               userSettings_.controls,
                               ControlAction::MoveBackward)) {
                    buildModePieChoice_ = ActionMode::Modular;
                }
            }

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

            if (keyReleased(userSettings_.controls,
                            ControlAction::BuildMode)) {
                selectActionMode(
                    buildModePieChoice_.value_or(
                        previousActionMode_),
                    currentSnapshot);
                buildModePieVisible_ = false;
                buildModePieDirection_ = {};
                buildModePieChoice_.reset();
            }
            return;
        }
        const double sensitivity =
            MouseSensitivity * static_cast<double>(
                                   userSettings_.controls.mouseSensitivity);
        pendingYaw_ += static_cast<double>(mouseDelta.x) * sensitivity;
        const double pitchDirection =
            userSettings_.controls.invertMouseY ? 1.0 : -1.0;
        pendingPitch_ += static_cast<double>(mouseDelta.y) *
                         sensitivity * pitchDirection;
        pendingJump_ = pendingJump_ || keyPressed(
            userSettings_.controls, ControlAction::Jump);
        const bool placementConsumesRightClick =
            foundationBuildMode_ ||
            currentSnapshot.buildingPreview.has_value();
        pendingDash_ = pendingDash_ ||
            (!placementConsumesRightClick &&
             keyPressed(
                 userSettings_.controls,
                 ControlAction::Dash));
        if (actionMode_ == ActionMode::Modular) {
            if (IsKeyPressed(KEY_ONE)) {
                selectModularBuildPiece(
                    ModularBuildPiece::Foundation);
            }
            if (IsKeyPressed(KEY_TWO)) {
                selectModularBuildPiece(
                    ModularBuildPiece::FloorPlatform);
            }
            if (IsKeyPressed(KEY_THREE)) {
                selectModularBuildPiece(
                    ModularBuildPiece::Wall);
            }
            if (IsKeyPressed(KEY_FOUR)) {
                selectModularBuildPiece(
                    ModularBuildPiece::Ramp);
            }
        } else if (actionMode_ == ActionMode::Buildings) {
            const bool shiftHeld =
                IsKeyDown(KEY_LEFT_SHIFT) ||
                IsKeyDown(KEY_RIGHT_SHIFT);
            if (shiftHeld) {
                if (IsKeyPressed(KEY_ONE)) {
                    selectBuildingMode(BuildingType::WoodStorage);
                }
                if (IsKeyPressed(KEY_TWO)) {
                    selectBuildingMode(BuildingType::StoneStorage);
                }
                if (IsKeyPressed(KEY_THREE)) {
                    selectBuildingMode(BuildingType::CrystalStorage);
                }
            } else {
                if (IsKeyPressed(KEY_ONE)) {
                    selectBuildingMode(BuildingType::Core);
                }
                if (IsKeyPressed(KEY_TWO)) {
                    selectBuildingMode(BuildingType::Wall);
                }
                if (IsKeyPressed(KEY_THREE)) {
                    selectBuildingMode(BuildingType::Turret);
                }
                if (IsKeyPressed(KEY_FOUR)) {
                    selectBuildingMode(BuildingType::GoldMine);
                }
                if (IsKeyPressed(KEY_FIVE)) {
                    selectBuildingMode(BuildingType::Cannon);
                }
                if (IsKeyPressed(KEY_SIX)) {
                    selectBuildingMode(BuildingType::SlowTrap);
                }
                if (IsKeyPressed(KEY_SEVEN)) {
                    selectBuildingMode(BuildingType::Gate);
                }
                if (IsKeyPressed(KEY_EIGHT)) {
                    selectBuildingMode(BuildingType::LumberMill);
                }
                if (IsKeyPressed(KEY_NINE)) {
                    selectBuildingMode(BuildingType::Quarry);
                }
                if (IsKeyPressed(KEY_ZERO)) {
                    selectBuildingMode(BuildingType::SpikeTrap);
                }
            }
        } else {
            constexpr std::array<int, PlayerWeaponCount> EquipmentKeys{
                KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR,
                KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT,
            };
            const auto order = equipmentOrder(actionMode_);
            std::size_t visibleSlot = 0;
            for (const PlayerWeapon weapon : order) {
                const std::size_t weaponIndex =
                    static_cast<std::size_t>(weapon);
                if (!currentSnapshot.unlockedWeapons[weaponIndex]) {
                    continue;
                }
                if (IsKeyPressed(EquipmentKeys[visibleSlot])) {
                    pendingWeaponSelection_ = weapon;
                    if (actionMode_ == ActionMode::Tools) {
                        lastToolSelection_ = weapon;
                    } else {
                        lastWeaponSelection_ = weapon;
                    }
                }
                ++visibleSlot;
            }
        }
        if (foundationBuildMode_ &&
            keyPressed(userSettings_.controls,
                       ControlAction::UpgradeWeapon)) {
            switch (modularBuildPiece_) {
            case ModularBuildPiece::Foundation:
                selectModularBuildPiece(
                    ModularBuildPiece::FloorPlatform);
                break;
            case ModularBuildPiece::FloorPlatform:
                selectModularBuildPiece(
                    ModularBuildPiece::Wall);
                break;
            case ModularBuildPiece::Wall:
                selectModularBuildPiece(
                    ModularBuildPiece::Ramp);
                break;
            case ModularBuildPiece::Ramp:
                selectModularBuildPiece(
                    ModularBuildPiece::Foundation);
                break;
            }
        }
        const bool cancelBuildingPressed =
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        pendingBuildingCancel_ =
            pendingBuildingCancel_ || cancelBuildingPressed;
        if (cancelBuildingPressed &&
            foundationBuildMode_) {
            selectActionMode(
                previousActionMode_, currentSnapshot);
        }
        if (cancelBuildingPressed &&
            currentSnapshot.buildingPreview) {
            const auto& preview =
                *currentSnapshot.buildingPreview;
            const Vec3 center = buildingWorldPosition(
                preview.type, preview.gridPosition);
            const Vector2 visualCenter =
                repelInvalidPreview(
                    placementPreviewCenter_.value_or(
                        Vector2{
                            static_cast<float>(center.x),
                            static_cast<float>(center.z),
                        }),
                    preview,
                    currentSnapshot.playerPosition);
            constexpr double Duration = 0.24;
            cancelledPlacementPreview_ = {
                .type = preview.type,
                .yaw =
                    static_cast<float>(
                        placementRotationYaw_),
                .center = visualCenter,
                .remaining = Duration,
                .duration = Duration,
            };
            selectActionMode(
                previousActionMode_, currentSnapshot);
        }
        pendingStartWave_ = pendingStartWave_ || keyPressed(
            userSettings_.controls, ControlAction::StartWave);
        pendingUnlimitedResources_ =
            pendingUnlimitedResources_ || IsKeyPressed(KEY_O);
        if (keyPressed(
                userSettings_.controls,
                ControlAction::ToggleTool)) {
            selectNextActionModeItem(currentSnapshot);
        }
        if (keyPressed(userSettings_.controls, ControlAction::Copy)) {
            bool copiedPlacement = false;
            if (actionBuilding) {
                const auto copied = std::find_if(
                    currentSnapshot.buildings.begin(),
                    currentSnapshot.buildings.end(),
                    [&actionBuilding](
                        const BuildingInstance& building) {
                        return building.id ==
                               *actionBuilding;
                    });
                if (copied !=
                    currentSnapshot.buildings.end()) {
                    selectBuildingMode(copied->type);
                    copiedPlacement = true;
                }
            }
            if (!copiedPlacement &&
                currentSnapshot.aimedModularBuilding) {
                const EntityId target =
                    *currentSnapshot
                         .aimedModularBuilding;
                const auto frame = std::find_if(
                    currentSnapshot.platformFrames.begin(),
                    currentSnapshot.platformFrames.end(),
                    [target](
                        const PlatformFrameInstance&
                            candidate) {
                        return candidate.id == target;
                    });
                const auto wall = std::find_if(
                    currentSnapshot.modularWalls.begin(),
                    currentSnapshot.modularWalls.end(),
                    [target](
                        const WallInstance& candidate) {
                        return candidate.id == target;
                    });
                const auto ramp = std::find_if(
                    currentSnapshot.ramps.begin(),
                    currentSnapshot.ramps.end(),
                    [target](
                        const RampInstance& candidate) {
                        return candidate.id == target;
                    });
                if (frame !=
                    currentSnapshot.platformFrames.end()) {
                    selectActionMode(
                        ActionMode::Modular,
                        currentSnapshot);
                    selectModularBuildPiece(
                        frame->storey == 0
                            ? ModularBuildPiece::Foundation
                            : ModularBuildPiece::
                                  FloorPlatform);
                } else if (
                    wall !=
                    currentSnapshot.modularWalls.end()) {
                    selectActionMode(
                        ActionMode::Modular,
                        currentSnapshot);
                    selectModularBuildPiece(
                        ModularBuildPiece::Wall);
                    modularRotation_ = wall->rotation;
                } else if (
                    ramp != currentSnapshot.ramps.end()) {
                    selectActionMode(
                        ActionMode::Modular,
                        currentSnapshot);
                    selectModularBuildPiece(
                        ModularBuildPiece::Ramp);
                    modularRotation_ = ramp->rotation;
                }
            }
        }
        pendingWeaponUpgrade_ =
            pendingWeaponUpgrade_ ||
            (actionMode_ == ActionMode::Weapons &&
             keyPressed(userSettings_.controls,
                        ControlAction::UpgradeWeapon));
        pendingBombThrow_ = pendingBombThrow_ || keyPressed(
            userSettings_.controls, ControlAction::Bomb);
        pendingDefeatAllEnemies_ =
            pendingDefeatAllEnemies_ || IsKeyPressed(KEY_K);
        pendingToggleInvulnerability_ =
            pendingToggleInvulnerability_ || IsKeyPressed(KEY_I);
        pendingDamageCore_ = pendingDamageCore_ ||
            (controlDown && IsKeyPressed(KEY_M));
        if (IsKeyPressed(KEY_Z)) {
            switch (debugSpawnType_) {
            case EnemyType::Basic:
                debugSpawnType_ = EnemyType::Fast;
                break;
            case EnemyType::Fast:
                debugSpawnType_ = EnemyType::Heavy;
                break;
            case EnemyType::Heavy:
                debugSpawnType_ = EnemyType::Ranged;
                break;
            case EnemyType::Ranged:
                debugSpawnType_ = EnemyType::Sapper;
                break;
            case EnemyType::Sapper:
                debugSpawnType_ = EnemyType::Flying;
                break;
            case EnemyType::Flying:
                debugSpawnType_ = EnemyType::Splitter;
                break;
            case EnemyType::Splitter:
                debugSpawnType_ = EnemyType::Splitling;
                break;
            case EnemyType::Splitling:
                debugSpawnType_ = EnemyType::Boss;
                break;
            case EnemyType::Boss:
                debugSpawnType_ = EnemyType::Basic;
                break;
            }
        }
        if (keyPressed(userSettings_.controls,
                      ControlAction::Upgrade) &&
            !currentSnapshot.selectedBuilding) {
            if (actionBuilding) {
                pendingBuildingUpgrade_ =
                    UpgradeBuildingCommand{*actionBuilding};
            } else if (currentSnapshot.coreId) {
                pendingBuildingUpgrade_ = UpgradeBuildingCommand{*currentSnapshot.coreId};
            }
        }
        repairSweepActive_ =
            !currentSnapshot.selectedBuilding &&
            keyDown(userSettings_.controls, ControlAction::Repair);
        if (repairSweepActive_) {
            if (currentSnapshot.aimedBuilding) {
                const auto target = std::find_if(
                    currentSnapshot.buildings.begin(),
                    currentSnapshot.buildings.end(),
                    [&currentSnapshot](
                        const BuildingInstance& building) {
                        return building.id ==
                               *currentSnapshot.aimedBuilding;
                    });
                if (target != currentSnapshot.buildings.end() &&
                    target->health < target->maxHealth &&
                    repairSweepTarget_ !=
                        currentSnapshot.aimedBuilding) {
                    pendingBuildingRepair_ =
                        RepairBuildingCommand{target->id};
                    repairSweepTarget_ = target->id;
                }
            } else if (
                currentSnapshot.aimedModularBuilding) {
                const EntityId id =
                    *currentSnapshot
                         .aimedModularBuilding;
                const auto queueRepair =
                    [this, id](
                        const auto& instances) {
                        const auto target =
                            std::find_if(
                                instances.begin(),
                                instances.end(),
                                [id](const auto& instance) {
                                    return instance.id == id;
                                });
                        if (target != instances.end() &&
                            target->health <
                                target->maxHealth &&
                            repairSweepTarget_ != id) {
                            pendingBuildingRepair_ =
                                RepairBuildingCommand{id};
                            repairSweepTarget_ = id;
                            return true;
                        }
                        return false;
                    };
                if (!queueRepair(
                        currentSnapshot.platformFrames) &&
                    !queueRepair(
                        currentSnapshot.modularWalls)) {
                    static_cast<void>(queueRepair(
                        currentSnapshot.ramps));
                }
            } else {
                repairSweepTarget_.reset();
            }
        } else {
            repairSweepTarget_.reset();
        }
        const bool canSweepRemove =
            !foundationBuildMode_ &&
            !currentSnapshot.selectedBuilding;
        const auto aimedRemovalTarget =
            [&]() -> std::optional<EntityId> {
                if (currentSnapshot.aimedBuilding) {
                    return currentSnapshot.aimedBuilding;
                }
                return currentSnapshot
                    .aimedModularBuilding;
            };
        if (canSweepRemove &&
            keyPressed(userSettings_.controls, ControlAction::Sell) &&
            aimedRemovalTarget()) {
            removalDragActive_ = true;
            removalDragTargets_.clear();
        }
        if (removalDragActive_ &&
            keyDown(userSettings_.controls, ControlAction::Sell)) {
            if (const auto target =
                    aimedRemovalTarget();
                target &&
                std::find(
                    removalDragTargets_.begin(),
                    removalDragTargets_.end(),
                    *target) ==
                    removalDragTargets_.end()) {
                removalDragTargets_.push_back(
                    *target);
            }
        }
        if (removalDragActive_ &&
            keyReleased(userSettings_.controls,
                        ControlAction::Sell)) {
            for (const EntityId target :
                 removalDragTargets_) {
                const bool ordinary =
                    std::any_of(
                        currentSnapshot.buildings.begin(),
                        currentSnapshot.buildings.end(),
                        [target](
                            const BuildingInstance&
                                building) {
                            return building.id == target;
                        });
                if (ordinary) {
                    queuedBuildingSales_.push_back(
                        SellBuildingCommand{target});
                } else {
                    queuedModularBuildingRemovals_
                        .push_back(
                            RemoveModularBuildingCommand{
                                target});
                }
            }
            removalDragActive_ = false;
            removalDragTargets_.clear();
            buildingContextCardTarget_.reset();
            buildingContextCardUpgradeCost_.reset();
            buildingContextCardStats_.reset();
        }
        if (!currentSnapshot.selectedBuilding &&
            keyPressed(userSettings_.controls,
                       ControlAction::Interact)) {
            if (currentSnapshot.aimedChest ||
                currentSnapshot.aimedLoot) {
                pendingInteract_ = true;
            } else if (actionBuilding) {
                pendingGateToggle_ =
                    ToggleGateCommand{*actionBuilding};
            }
        }
        const float wheel = GetMouseWheelMove();
        if (foundationBuildMode_ ||
            currentSnapshot.selectedBuilding) {
            buildingRotationWheelAccumulator_ = std::clamp(
                buildingRotationWheelAccumulator_ +
                    static_cast<double>(wheel),
                -1.0, 1.0);
            if (buildingRotationCooldownRemaining_ <= 0.0 &&
                std::abs(buildingRotationWheelAccumulator_) >= 1.0) {
                const int direction =
                    buildingRotationWheelAccumulator_ > 0.0
                        ? 1
                        : -1;
                if (foundationBuildMode_) {
                    const Rotation currentRotation =
                        modularBuildPiece_ ==
                                    ModularBuildPiece::Ramp &&
                                rampSocketRotation_
                            ? *rampSocketRotation_
                            : modularRotation_;
                    const int rotation =
                        (static_cast<int>(
                             currentRotation) +
                         (direction > 0 ? 1 : 3)) %
                        4;
                    modularRotation_ =
                        static_cast<Rotation>(rotation);
                    if (modularBuildPiece_ ==
                            ModularBuildPiece::Ramp &&
                        rampSocketFrame_) {
                        rampSocketRotation_ =
                            modularRotation_;
                        rampSocketManualOverrideRemaining_ =
                            0.75;
                        rampSocketLostGraceRemaining_ =
                            std::max(
                                rampSocketLostGraceRemaining_,
                                RampSocketLostGraceSeconds);
                    }
                } else {
                    pendingBuildingRotation_ +=
                        direction;
                }
                buildingRotationWheelAccumulator_ = 0.0;
                buildingRotationCooldownRemaining_ = 0.2;
            }
        } else {
            buildingRotationWheelAccumulator_ = 0.0;
        }
        if (pendingBuildingCancel_) {
            wallDragStart_.reset();
            wallDragEnd_.reset();
            placementDragType_.reset();
            placementDragSurface_.reset();
            placementDragAxis_.reset();
            placementDragCandidateEnd_.reset();
            placementDragCandidateFrames_ = 0;
            placementDragLookMovement_ = 0.0;
            placementDragExtended_ = false;
            placementDragCandidateEnd_.reset();
            placementDragCandidateFrames_ = 0;
            placementDragLookMovement_ = 0.0;
            placementDragExtended_ = false;
            pendingWallPlacements_.clear();
        }
        if (wallDragStart_ &&
            placementDragType_ &&
            currentSnapshot.selectedBuilding ==
                placementDragType_) {
            std::optional<GridPosition> aimedEnd;
            if (placementDragSurface_) {
                aimedEnd =
                    aimedBuildingGridPosition(
                        currentSnapshot.playerPosition,
                        currentSnapshot.playerYaw,
                        currentSnapshot.playerPitch,
                        MinimumPlacementDistance,
                        MaximumPlacementDistance,
                        *placementDragType_,
                        placementDragSurface_->height);
            } else if (
                currentSnapshot.buildingPreview &&
                currentSnapshot.buildingPreview->type ==
                    *placementDragType_) {
                aimedEnd =
                    currentSnapshot.buildingPreview
                        ->gridPosition;
            }
            if (aimedEnd) {
                const bool movedCell =
                    *aimedEnd != *wallDragStart_;
                if (!movedCell) {
                    wallDragEnd_ = wallDragStart_;
                    placementDragCandidateEnd_.reset();
                    placementDragCandidateFrames_ = 0;
                } else if (!placementDragExtended_) {
                    constexpr double MinimumDragPixels = 4.0;
                    constexpr int ConfirmationFrames = 2;
                    if (placementDragLookMovement_ < MinimumDragPixels) {
                        wallDragEnd_ = wallDragStart_;
                        placementDragCandidateEnd_.reset();
                        placementDragCandidateFrames_ = 0;
                    } else {
                        if (placementDragCandidateEnd_ == aimedEnd) {
                            ++placementDragCandidateFrames_;
                        } else {
                            placementDragCandidateEnd_ = aimedEnd;
                            placementDragCandidateFrames_ = 1;
                        }
                        if (placementDragCandidateFrames_ >=
                            ConfirmationFrames) {
                            placementDragExtended_ = true;
                            wallDragEnd_ = aimedEnd;
                        } else {
                            wallDragEnd_ = wallDragStart_;
                        }
                    }
                } else {
                    wallDragEnd_ = aimedEnd;
                }
            }
            constexpr double AxisSwitchMarginCells = 1.0;
            placementDragAxis_ =
                stabilizePlacementLineAxis(
                    wallDragEnd_->x -
                        wallDragStart_->x,
                    wallDragEnd_->z -
                        wallDragStart_->z,
                    placementDragAxis_,
                    AxisSwitchMarginCells);
        }
        if (wallDragStart_ &&
            placementDragType_ &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            const BuildingType dragType =
                *placementDragType_;
            const auto cells = placementGestureLine(
                dragType, *wallDragStart_,
                wallDragEnd_.value_or(*wallDragStart_),
                placementDragExtended_,
                placementDragAxis_);
            const std::size_t count = cells.size();
            std::uint8_t dragRotation =
                static_cast<std::uint8_t>(
                    currentSnapshot.buildingPreview
                        ? currentSnapshot
                              .buildingPreview->rotation
                        : 0);
            if (dragType == BuildingType::Wall &&
                cells.size() > 1U &&
                placementDragAxis_) {
                dragRotation =
                    *placementDragAxis_ ==
                            PlacementLineAxis::X
                        ? 0U
                        : 1U;
            }
            const ResourceCost dragCost =
                currentSnapshot.buildingCosts[
                    static_cast<std::size_t>(dragType)];
            int queuedCount = 0;
            for (std::size_t index = 0;
                 index < count; ++index) {
                const BuildingPlatformSurface surface =
                    placementDragSurface_
                        ? simulation_
                              .previewPlacementSurface(
                                  dragType,
                                  cells[index],
                                  placementDragSurface_
                                      ->height)
                        : simulation_
                              .previewPlacementSurface(
                                  dragType,
                                  cells[index]);
                const PlacementResult placement =
                    simulation_.previewPlacement(
                        dragType, cells[index],
                        surface.height);
                if (!placement.valid()) {
                    continue;
                }
                const int nextCount = queuedCount + 1;
                if (!currentSnapshot.unlimitedResources &&
                    (currentSnapshot.wood <
                         dragCost.wood * nextCount ||
                     currentSnapshot.stone <
                         dragCost.stone * nextCount ||
                     currentSnapshot.gold <
                         dragCost.gold * nextCount)) {
                    continue;
                }
                pendingWallPlacements_.push_back({
                    .type = dragType,
                    .gridPosition = cells[index],
                    .rotation = dragRotation,
                    .baseHeight = surface.height,
                    .platformStorey =
                        surface.storey,
                    .lockHeight = true,
                });
                queuedCount = nextCount;
            }
            wallDragStart_.reset();
            wallDragEnd_.reset();
            placementDragType_.reset();
            placementDragSurface_.reset();
            placementDragAxis_.reset();
        }
        if (foundationBuildMode_) {
            updateModularPlacementPreview(
                currentSnapshot);
        } else {
            platformFramePreview_.reset();
            wallPreview_.reset();
            rampPreview_.reset();
            foundationTerrainHit_.reset();
            clearModularPlacementDrag();
            modularSnapHit_.reset();
            modularSnapMarker_.reset();
            modularPreviewAnchor_.reset();
        }
        if (foundationBuildMode_ &&
            modularDragPiece_ &&
            IsMouseButtonReleased(
                MOUSE_BUTTON_LEFT)) {
            static_cast<void>(
                finishModularPlacementDrag());
        }
        const bool mousePrimaryPressed =
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const bool heldResourceGather =
            currentSnapshot.holdToGather &&
            currentSnapshot.aimedResource &&
            !currentSnapshot.buildingPreview &&
            !foundationBuildMode_ &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const bool attackPressed =
            mousePrimaryPressed ||
            heldResourceGather ||
            keyPressed(userSettings_.controls,
                       ControlAction::Attack);
        if (attackPressed) {
            if (mousePrimaryPressed && foundationBuildMode_) {
                if (foundationTerrainHit_) {
                    switch (modularBuildPiece_) {
                    case ModularBuildPiece::Foundation:
                    case ModularBuildPiece::FloorPlatform:
                        if (platformFramePreview_ &&
                            platformFramePreview_->valid()) {
                            beginModularPlacementDrag();
                        }
                        break;
                    case ModularBuildPiece::Wall:
                        if (wallPreview_ &&
                            wallPreview_->valid()) {
                            beginModularPlacementDrag();
                        }
                        break;
                    case ModularBuildPiece::Ramp:
                        if (rampPreview_ &&
                            rampPreview_->valid()) {
                            beginModularPlacementDrag();
                        }
                        break;
                    }
                }
            } else if (
                mousePrimaryPressed &&
                currentSnapshot.buildingPreview &&
                currentSnapshot.buildingPreview
                    ->placement.valid() &&
                currentSnapshot.buildingPreview->type !=
                    BuildingType::Core) {
                wallDragStart_ =
                    currentSnapshot
                        .buildingPreview->gridPosition;
                wallDragEnd_ = wallDragStart_;
                placementDragType_ =
                    currentSnapshot.buildingPreview->type;
                placementDragAxis_.reset();
                placementDragCandidateEnd_.reset();
                placementDragCandidateFrames_ = 0;
                placementDragLookMovement_ = 0.0;
                placementDragExtended_ = false;
                placementDragSurface_ =
                    BuildingPlatformSurface{
                        .height =
                            currentSnapshot
                                .buildingPreview
                                ->baseHeight,
                        .foundationBottomHeight =
                            currentSnapshot
                                .buildingPreview
                                ->foundationBottomHeight,
                        .storey =
                            currentSnapshot
                                .buildingPreview
                                ->platformStorey,
                    };
            } else if (
                mousePrimaryPressed &&
                currentSnapshot.buildingPreview &&
                currentSnapshot.buildingPreview
                    ->placement.valid()) {
                pendingBuildingPlacement_ = PlaceBuildingCommand{
                    .type = currentSnapshot.buildingPreview->type,
                    .gridPosition = currentSnapshot.buildingPreview->gridPosition,
                    .rotation = currentSnapshot.buildingPreview->rotation,
                    .baseHeight =
                        currentSnapshot
                            .buildingPreview->baseHeight,
                    .platformStorey =
                        currentSnapshot
                            .buildingPreview
                            ->platformStorey,
                    .lockHeight = true,
                };
            } else if (mousePrimaryPressed &&
                !pendingBuildingSelection_ &&
                !currentSnapshot.aimedEnemy &&
                currentSnapshot.aimedBuilding) {
                if (buildingContextCardTarget_ ==
                    currentSnapshot.aimedBuilding) {
                    buildingContextCardTarget_.reset();
                    buildingContextCardUpgradeCost_.reset();
                    buildingContextCardStats_.reset();
                } else {
                    buildingContextCardTarget_ =
                        currentSnapshot.aimedBuilding;
                    buildingContextCardUpgradeCost_ =
                        currentSnapshot
                            .aimedBuildingUpgradeCost;
                    buildingContextCardStats_ =
                        currentSnapshot.aimedBuildingStats;
                }
            } else if (actionModeUsesEquipment(actionMode_) &&
                       !pendingBuildingSelection_ &&
                       currentSnapshot.selectedWeapon == PlayerWeapon::Rifle) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                pendingRifleShot_ = true;
            } else if (actionModeUsesEquipment(actionMode_) &&
                       !pendingBuildingSelection_ &&
                       currentSnapshot.selectedWeapon == PlayerWeapon::IceWand) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                pendingIceWandShot_ = true;
            } else if (actionModeUsesEquipment(actionMode_) &&
                       !pendingBuildingSelection_ &&
                       currentSnapshot.selectedWeapon == PlayerWeapon::FireWand) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                pendingFireWandShot_ = true;
            } else if (actionModeUsesEquipment(actionMode_) &&
                       !pendingBuildingSelection_) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                constexpr double ToolInputBufferSeconds = 0.14;
                if (currentSnapshot.pickaxeCooldownRemaining <=
                    ToolInputBufferSeconds) {
                    toolSwingUsesAxe_ =
                        displayedToolVisual_ ==
                            FirstPersonToolVisual::Axe ||
                        displayedToolVisual_ ==
                            FirstPersonToolVisual::Club;
                    if (currentSnapshot.automaticToolSwitch &&
                        currentSnapshot.selectedWeapon !=
                            PlayerWeapon::BareHands &&
                        currentSnapshot.aimedResource) {
                        const auto resource = std::find_if(
                            currentSnapshot.resourceNodes.begin(),
                            currentSnapshot.resourceNodes.end(),
                            [&currentSnapshot](const ResourceNode& node) {
                                return node.id ==
                                       *currentSnapshot.aimedResource;
                            });
                        if (resource !=
                                currentSnapshot.resourceNodes.end() &&
                            isHarvestableResource(resource->type)) {
                            const PlayerWeapon desiredTool =
                                resource->type == ResourceType::Wood
                                    ? PlayerWeapon::Axe
                                    : PlayerWeapon::Pickaxe;
                            const bool desiredToolUnlocked =
                                currentSnapshot.unlimitedResources ||
                                currentSnapshot.unlockedWeapons[
                                    static_cast<std::size_t>(
                                        desiredTool)];
                            // Keep the animation on the held tool when the
                            // ideal tool is unavailable. The simulation then
                            // applies the intended inefficient-tool damage
                            // (for example, an axe can still mine stone).
                            if (desiredToolUnlocked) {
                                toolSwingUsesAxe_ =
                                    desiredTool == PlayerWeapon::Axe;
                            }
                        }
                    }
                    if (toolSwingUsesAxe_ ==
                            (displayedToolVisual_ ==
                                 FirstPersonToolVisual::Axe ||
                             displayedToolVisual_ ==
                                 FirstPersonToolVisual::Club) &&
                        toolSwapRemaining_ <= 0.0 &&
                        toolSwingRemaining_ <= 0.0) {
                        toolSwingDuration_ =
                            toolTuning_.swingDuration;
                        toolSwingRemaining_ =
                            toolSwingDuration_;
                        toolSwingAttackPending_ = true;
                        toolSwingQueued_ = false;
                        toolQueuedSwingHasAttack_ = false;
                        toolQueuedResourceTarget_.reset();
                        toolSwingQueueRemaining_ = 0.0;
                    } else {
                        toolSwingQueued_ = true;
                        toolQueuedSwingHasAttack_ = true;
                        toolQueuedResourceTarget_ =
                            currentSnapshot.aimedResource;
                        toolSwingQueueRemaining_ = 0.75;
                    }
                }
            }
        }
    } else {
        interactionResourceAim_.reset();
        buildModePieVisible_ = false;
        buildModePieDirection_ = {};
        buildModePieChoice_.reset();
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
        pendingDash_ = false;
        repairSweepActive_ = false;
        repairSweepTarget_.reset();
    }
}

} // namespace ian
