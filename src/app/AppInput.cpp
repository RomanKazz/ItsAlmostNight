#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"

#include "buildings/RampPlacementDirection.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ian {
using namespace app_detail;

namespace {
constexpr double MouseSensitivity = 0.002;
}

void App::processInput() {
    renderer_->processInput();
    const auto snapshot = simulation_.snapshot();
    const bool graphicsPanelVisible =
        renderer_->graphicsPanelVisible();
    if (graphicsPanelVisible != graphicsPanelWasVisible_) {
        if (graphicsPanelVisible) {
            EnableCursor();
        } else if (snapshot.state != RunState::MainMenu &&
                   snapshot.state != RunState::Paused) {
            DisableCursor();
        }
        graphicsPanelWasVisible_ = graphicsPanelVisible;
    }
    if (graphicsPanelVisible) {
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
        pendingYaw_ = 0.0;
        pendingPitch_ = 0.0;
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
        audio_.playUiConfirm();
        fixedStep_.reset();
        statusMessage_.clear();
        statusMessageRemaining_ = 0.0;
        effects_.clear();
        arrowVisuals_.clear();
        productionVisuals_.clear();
        soldBuildingVisuals_.clear();
        destroyedEnemyVisuals_.clear();
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
        cameraBobPositionInitialized_ = false;
        damageIndicators_.clear();
        playerDamageFlashRemaining_ = 0.0;
        recentlyDamagedBuilding_.reset();
        damagedBuildingHealthBarRemaining_ = 0.0;
        hitStopRemaining_ = 0.0;
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
        DisableCursor();
    }
    if (IsKeyPressed(KEY_P)) {
        simulation_.togglePause();
        fixedStep_.reset();
        if (simulation_.snapshot().state == RunState::Paused) {
            EnableCursor();
        } else if (simulation_.snapshot().state != RunState::MainMenu) {
            DisableCursor();
        }
    }
    if (snapshot.state != RunState::MainMenu && IsKeyPressed(KEY_R)) {
        simulation_.restartRun();
        fixedStep_.reset();
        statusMessage_.clear();
        statusMessageRemaining_ = 0.0;
        effects_.clear();
        arrowVisuals_.clear();
        productionVisuals_.clear();
        soldBuildingVisuals_.clear();
        destroyedEnemyVisuals_.clear();
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
        cameraBobPositionInitialized_ = false;
        damageIndicators_.clear();
        playerDamageFlashRemaining_ = 0.0;
        recentlyDamagedBuilding_.reset();
        damagedBuildingHealthBarRemaining_ = 0.0;
        hitStopRemaining_ = 0.0;
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
    }
    if (snapshot.state != RunState::MainMenu) {
        if (controlDown &&
            IsKeyPressed(KEY_F6)) {
            showTerrainWireframe_ =
                !showTerrainWireframe_;
        }
        if (controlDown &&
            IsKeyPressed(KEY_F7)) {
            simulation_.regenerateTerrain(
                simulation_.terrain().seed());
            renderer_->rebuildTerrain(
                simulation_.terrain());
        }
        if (controlDown &&
            IsKeyPressed(KEY_F9)) {
            simulation_.regenerateTerrain(
                simulation_.terrain().seed() +
                0x9e3779b9U);
            renderer_->rebuildTerrain(
                simulation_.terrain());
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
        input_.overrideAimedBuilding = true;
        input_.aimedBuildingOverride =
            currentSnapshot.aimedBuilding;
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
        }
        input_.moveForward =
            static_cast<double>(IsKeyDown(KEY_W)) - static_cast<double>(IsKeyDown(KEY_S));
        input_.moveRight =
            static_cast<double>(IsKeyDown(KEY_D)) - static_cast<double>(IsKeyDown(KEY_A));
        input_.sprint = IsKeyDown(KEY_LEFT_SHIFT);

        const auto selectBuildingMode =
            [this](BuildingType type) {
                lastBuildingSelection_ = type;
                setFoundationBuildMode(false);
                pendingBuildingCancel_ = false;
                pendingBuildingSelection_ = type;
            };
        const Vector2 mouseDelta = GetMouseDelta();
        if (modularDragPiece_) {
            modularDragLookMovement_ +=
                static_cast<double>(
                    Vector2Length(mouseDelta));
        }
        if (IsKeyPressed(KEY_TAB)) {
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
                buildModePieChoice_ =
                    buildModePieDirection_.x < 0.0F
                        ? BuildModePieChoice::Buildings
                        : BuildModePieChoice::Foundations;
            } else {
                buildModePieChoice_.reset();
            }

            input_.moveForward = 0.0;
            input_.moveRight = 0.0;
            input_.sprint = false;
            pendingYaw_ = 0.0;
            pendingPitch_ = 0.0;
            pendingJump_ = false;
            pendingPickaxe_ = false;
            pendingRifleShot_ = false;

            if (IsKeyReleased(KEY_TAB)) {
                if (buildModePieChoice_ ==
                    BuildModePieChoice::Buildings) {
                    selectBuildingMode(
                        lastBuildingSelection_);
                } else if (
                    buildModePieChoice_ ==
                    BuildModePieChoice::Foundations) {
                    setFoundationBuildMode(true);
                }
                buildModePieVisible_ = false;
                buildModePieDirection_ = {};
                buildModePieChoice_.reset();
            }
            return;
        }
        pendingYaw_ += static_cast<double>(mouseDelta.x) * MouseSensitivity;
        pendingPitch_ -= static_cast<double>(mouseDelta.y) * MouseSensitivity;
        pendingJump_ = pendingJump_ || IsKeyPressed(KEY_SPACE);
        if (foundationBuildMode_) {
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
                selectBuildingMode(
                    BuildingType::LumberMill);
            }
            if (IsKeyPressed(KEY_NINE)) {
                selectBuildingMode(BuildingType::Quarry);
            }
        }
        if (IsKeyPressed(KEY_ZERO)) {
            setFoundationBuildMode(
                !foundationBuildMode_);
        }
        if (foundationBuildMode_ &&
            IsKeyPressed(KEY_V)) {
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
            setFoundationBuildMode(false);
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
        }
        pendingStartWave_ = pendingStartWave_ || IsKeyPressed(KEY_N);
        pendingUnlimitedResources_ =
            pendingUnlimitedResources_ || IsKeyPressed(KEY_O);
        pendingWeaponToggle_ =
            pendingWeaponToggle_ || IsKeyPressed(KEY_C);
        if (IsKeyPressed(KEY_Q)) {
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
                    setFoundationBuildMode(true);
                    selectModularBuildPiece(
                        frame->storey == 0
                            ? ModularBuildPiece::Foundation
                            : ModularBuildPiece::
                                  FloorPlatform);
                } else if (
                    wall !=
                    currentSnapshot.modularWalls.end()) {
                    setFoundationBuildMode(true);
                    selectModularBuildPiece(
                        ModularBuildPiece::Wall);
                    modularRotation_ = wall->rotation;
                } else if (
                    ramp != currentSnapshot.ramps.end()) {
                    setFoundationBuildMode(true);
                    selectModularBuildPiece(
                        ModularBuildPiece::Ramp);
                    modularRotation_ = ramp->rotation;
                }
            }
        }
        pendingWeaponUpgrade_ =
            pendingWeaponUpgrade_ ||
            (!foundationBuildMode_ &&
             IsKeyPressed(KEY_V));
        pendingBombThrow_ = pendingBombThrow_ || IsKeyPressed(KEY_G);
        pendingDefeatAllEnemies_ =
            pendingDefeatAllEnemies_ || IsKeyPressed(KEY_K);
        pendingToggleInvulnerability_ =
            pendingToggleInvulnerability_ || IsKeyPressed(KEY_I);
        pendingDamageCore_ = pendingDamageCore_ || IsKeyPressed(KEY_M);
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
                debugSpawnType_ = EnemyType::Boss;
                break;
            case EnemyType::Boss:
                debugSpawnType_ = EnemyType::Basic;
                break;
            }
        }
        if (IsKeyPressed(KEY_U) &&
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
            IsKeyDown(KEY_F);
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
            IsKeyPressed(KEY_X) &&
            aimedRemovalTarget()) {
            removalDragActive_ = true;
            removalDragTargets_.clear();
        }
        if (removalDragActive_ &&
            IsKeyDown(KEY_X)) {
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
            IsKeyReleased(KEY_X)) {
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
            IsKeyPressed(KEY_E) && actionBuilding) {
            pendingGateToggle_ =
                ToggleGateCommand{*actionBuilding};
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
            pendingWallPlacements_.clear();
        }
        if (wallDragStart_ &&
            currentSnapshot.buildingPreview &&
            placementDragType_ ==
                currentSnapshot.buildingPreview->type) {
            wallDragEnd_ =
                currentSnapshot.buildingPreview->gridPosition;
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
            const auto cells = placementLine(
                dragType, *wallDragStart_,
                wallDragEnd_.value_or(*wallDragStart_),
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
                pendingWallPlacements_.push_back({
                    .type = dragType,
                    .gridPosition = cells[index],
                    .rotation = dragRotation,
                    .baseHeight = surface.height,
                    .platformStorey =
                        surface.storey,
                    .lockHeight = true,
                });
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
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (foundationBuildMode_) {
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
            } else if (currentSnapshot.buildingPreview &&
                currentSnapshot.buildingPreview->type !=
                    BuildingType::Core) {
                wallDragStart_ =
                    currentSnapshot
                        .buildingPreview->gridPosition;
                wallDragEnd_ = wallDragStart_;
                placementDragType_ =
                    currentSnapshot.buildingPreview->type;
                placementDragAxis_.reset();
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
            } else if (currentSnapshot.buildingPreview) {
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
            } else if (
                !pendingBuildingSelection_ &&
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
            } else if (!pendingBuildingSelection_ &&
                       currentSnapshot.selectedWeapon == PlayerWeapon::Rifle) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                pendingRifleShot_ = true;
            } else if (!pendingBuildingSelection_) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                pendingPickaxe_ = true;
            }
        }
    } else {
        buildModePieVisible_ = false;
        buildModePieDirection_ = {};
        buildModePieChoice_.reset();
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
        repairSweepActive_ = false;
        repairSweepTarget_.reset();
    }
}

} // namespace ian
