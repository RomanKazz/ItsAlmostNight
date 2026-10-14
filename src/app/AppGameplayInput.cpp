#include "app/App.hpp"
#include "app/ActionModeEquipment.hpp"
#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingHotbarLayout.hpp"

#include "buildings/BuildingOrientation.hpp"
#include "buildings/RampPlacementDirection.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
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

} // namespace

void App::processGameplayInput(bool controlDown,
                               bool primaryMouseDown) {
    if (playerArrival_.active() &&
        acceptsGameplayInput(simulation_.snapshot().state)) {
        resetRunInputState();
        return;
    }
    if (acceptsGameplayInput(simulation_.snapshot().state)) {
        auto currentSnapshot = simulation_.snapshot();
        // The action-mode UI is authoritative. A building selection can
        // survive for a frame if a mode-change command missed the fixed-step
        // boundary; without healing that mismatch the simulation rejects all
        // melee attacks while the tools hotbar is already visible.
        if (actionModeUsesEquipment(actionMode_)) {
            pendingBuildingSelection_.reset();
            if (currentSnapshot.selectedBuilding) {
                pendingBuildingCancel_ = true;
                currentSnapshot.selectedBuilding.reset();
                currentSnapshot.buildingPreview.reset();
            }
        }
        // Reconcile the building hotbar with simulation selection after a
        // mode transition, including a frame before the next fixed tick.
        if (actionMode_ == ActionMode::Buildings && !currentSnapshot.coreId &&
            !currentSnapshot.activeChallengeCenter &&
            currentSnapshot.selectedBuilding != BuildingType::Core) {
            setFoundationBuildMode(false);
            buildingHotbarCategory_ = BuildingHotbarCategory::Base;
            lastBuildingSelection_ = BuildingType::Core;
            pendingBuildingCancel_ = false;
            pendingBuildingSelection_ = BuildingType::Core;
        }
        if (currentSnapshot.activeChallengeCenter) {
            foundationBuildMode_ = false;
            pendingBuildingCancel_ = true;
            platformFramePreview_.reset();
            wallPreview_.reset();
            rampPreview_.reset();
            foundationTerrainHit_.reset();
            clearModularPlacementDrag();
        }
        currentSnapshot.aimedBuilding =
            preciseBuildingAim(
                *renderer_, currentSnapshot,
                [this](EntityId id, bool open) {
                    return gateOpenAmount(id, open);
                },
                [this](const BuildingInstance& building) {
                    return flamethrowerVisualYaw(building);
                });
        if (foundationBuildMode_ ||
            currentSnapshot.selectedBuilding) {
            currentSnapshot.aimedBuilding.reset();
        }
        // Building mode keeps the previously equipped weapon in the
        // simulation, while ranged weapons intentionally suppress resource
        // aim. Use the axe for this aim test in every mode that can switch back
        // to gathering from resource hover.
        const bool resourceAutoSwitchMode =
            actionMode_ == ActionMode::Buildings ||
            (actionMode_ == ActionMode::Equipment &&
             isPlayerCombatWeapon(
                 currentSnapshot.selectedWeapon));
        const auto preciseResourceHit = preciseResourceAim(
            *renderer_, currentSnapshot,
            resourceAutoSwitchMode ? PlayerWeapon::Axe : currentSnapshot.selectedWeapon);
        currentSnapshot.aimedResource = preciseResourceHit
            ? std::optional<EntityId>{preciseResourceHit->id}
            : std::nullopt;

        currentSnapshot.aimedModularBuilding =
            preciseModularBuildingAim(
                *renderer_, currentSnapshot);
        if (buildingContextCardTarget_ &&
            (!primaryMouseDown ||
             (currentSnapshot.aimedBuilding != buildingContextCardTarget_ &&
              currentSnapshot.aimedModularBuilding != buildingContextCardTarget_))) {
            buildingContextCardTarget_.reset();
            buildingContextCardUpgradeCost_.reset();
            buildingContextCardStats_.reset();
        }
        currentSnapshot.aimedWorldLandmark =
            preciseWorldLandmarkAim(
                *renderer_, currentSnapshot);
        // Large landmark meshes may sit behind a smaller target while both
        // intersect the crosshair ray. Foreground gameplay targets own the
        // interaction and outline in that case.
        if (currentSnapshot.aimedWorldLandmark &&
            (currentSnapshot.aimedResource ||
             currentSnapshot.aimedBuilding ||
             currentSnapshot.aimedModularBuilding ||
             currentSnapshot.aimedEnemy ||
             currentSnapshot.aimedChest ||
             currentSnapshot.aimedLoot ||
             currentSnapshot.aimedChallengeColumn)) {
            currentSnapshot.aimedWorldLandmark.reset();
        }
        if (currentSnapshot.selectedBuilding) {
            currentSnapshot.aimedModularBuilding.reset();
        }
        if (coreDefenseMenuVisible_ &&
            !coreDefenseMenuClosing_) {
            const auto core = currentSnapshot.coreId
                ? std::ranges::find(
                      currentSnapshot.buildings,
                      *currentSnapshot.coreId,
                      &BuildingInstance::id)
                : currentSnapshot.buildings.end();
            double distance =
                std::numeric_limits<double>::infinity();
            if (core != currentSnapshot.buildings.end()) {
                const Vec3 center = buildingWorldPosition(*core);
                distance = std::hypot(
                    center.x - currentSnapshot.playerPosition.x,
                    center.z - currentSnapshot.playerPosition.z);
            }
            if (currentSnapshot.aimedBuilding ==
                currentSnapshot.coreId) {
                coreMenuLostAimSeconds_ = 0.0;
            } else {
                coreMenuLostAimSeconds_ +=
                    static_cast<double>(GetFrameTime());
            }
            if (distance > 4.5 ||
                coreMenuLostAimSeconds_ > 0.18) {
                coreDefenseMenuClosing_ = true;
            }
        }
        interactionResourceAim_ = currentSnapshot.aimedResource;
        input_.overrideAimedBuilding = true;
        input_.aimedBuildingOverride = currentSnapshot.aimedBuilding;
        input_.overrideAimedResource = true;
        input_.aimedResourceOverride =
            currentSnapshot.aimedResource;
        input_.aimedResourceHitPositionOverride =
            preciseResourceHit && currentSnapshot.aimedResource ==
                    preciseResourceHit->id
            ? std::optional<Vec3>{preciseResourceHit->position}
            : std::nullopt;
        input_.overrideAimedModularBuilding = true;
        input_.aimedModularBuildingOverride =
            currentSnapshot.aimedModularBuilding;
        input_.overrideAimedWorldLandmark = true;
        input_.aimedWorldLandmarkOverride =
            currentSnapshot.aimedWorldLandmark;
        const bool buildingManagementActive =
            !foundationBuildMode_ &&
            !currentSnapshot.selectedBuilding;
        const auto isOrdinaryBuilding = [&currentSnapshot](EntityId id) {
            return std::ranges::any_of(
                currentSnapshot.buildings,
                [id](const BuildingInstance& building) {
                    return building.id == id;
                });
        };
        const auto isModularBuilding = [&currentSnapshot](EntityId id) {
            return std::ranges::any_of(
                       currentSnapshot.platformFrames,
                       [id](const PlatformFrameInstance& frame) {
                           return frame.id == id;
                       }) ||
                std::ranges::any_of(
                       currentSnapshot.modularWalls,
                       [id](const WallInstance& wall) {
                           return wall.id == id;
                       }) ||
                std::ranges::any_of(
                       currentSnapshot.ramps,
                       [id](const RampInstance& ramp) {
                           return ramp.id == id;
                       });
        };
        const std::optional<EntityId> actionBuilding =
            buildingContextCardTarget_ &&
                    isOrdinaryBuilding(*buildingContextCardTarget_)
                ? buildingContextCardTarget_
                : buildingContextCardTarget_
                    ? std::nullopt
                    : currentSnapshot.aimedBuilding;
        const std::optional<EntityId> actionModularBuilding =
            buildingContextCardTarget_ &&
                    isModularBuilding(*buildingContextCardTarget_)
                ? buildingContextCardTarget_
                : buildingContextCardTarget_
                    ? std::nullopt
                    : currentSnapshot.aimedModularBuilding;
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
        input_.jumpHeld = keyDown(
            userSettings_.controls, ControlAction::Jump);

        const auto selectBuildingMode =
            [this, &currentSnapshot](BuildingType type) {
                selectActionMode(
                    ActionMode::Buildings, currentSnapshot);
                buildingHotbarCategory_ =
                    buildingHotbarCategory(type);
                lastBuildingSelection_ = type;
                pendingBuildingSelection_ = type;
            };
        const Vector2 rawMouseDelta = GetMouseDelta();
        const Vector2 mouseDelta = coreMenuPointerActive_
            ? Vector2{} : rawMouseDelta;
        const float wheel = GetMouseWheelMove();
        std::optional<EntityId> aimedDirectionalDefense;
        if (buildingManagementActive &&
            !currentSnapshot.selectedBuilding &&
            actionBuilding) {
            const auto aimed = std::ranges::find(
                currentSnapshot.buildings,
                *actionBuilding,
                &BuildingInstance::id);
            if (aimed != currentSnapshot.buildings.end() &&
                isDirectionalDefense(aimed->type)) {
                aimedDirectionalDefense = aimed->id;
            }
        }
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
            selectActionMode(
                actionMode_ == ActionMode::Equipment
                    ? ActionMode::Buildings
                    : ActionMode::Equipment,
                currentSnapshot);
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
            (!coreMenuPointerActive_ &&
             !placementConsumesRightClick &&
             keyPressed(
                 userSettings_.controls,
                 ControlAction::Dash));
        if (actionMode_ == ActionMode::Buildings &&
            buildingHotbarCategory_ ==
                BuildingHotbarCategory::Modular) {
            constexpr std::array<int, MaximumBuildingHotbarSlots>
                ConstructionKeys{
                    KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR,
                    KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO,
                };
            const ConstructionHotbarLayout layout =
                makeConstructionHotbarLayout(
                    currentSnapshot.unlockedBuildings,
                    currentSnapshot.coreMaxHealth > 0.0,
                    currentSnapshot.coreLevel,
                    currentSnapshot.sandboxMode ||
                        currentSnapshot.unlimitedResources);
            for (std::size_t keyIndex = 0;
                 keyIndex < layout.count; ++keyIndex) {
                if (IsKeyPressed(ConstructionKeys[keyIndex])) {
                    selectConstructionHotbarItem(
                        currentSnapshot, keyIndex);
                }
            }
            constexpr double BuildingWheelThreshold = 0.55;
            constexpr double BuildingWheelReleaseSeconds = 0.10;
            if (std::abs(wheel) > 0.01F) {
                buildingItemWheelReleaseRemaining_ =
                    BuildingWheelReleaseSeconds;
                if (buildingItemWheelArmed_) {
                    buildingItemWheelAccumulator_ = std::clamp(
                        buildingItemWheelAccumulator_ +
                            static_cast<double>(wheel),
                        -1.0, 1.0);
                    if (std::abs(buildingItemWheelAccumulator_) >=
                        BuildingWheelThreshold) {
                        selectNextBuildingItem(
                            currentSnapshot,
                            buildingItemWheelAccumulator_ > 0.0
                                ? -1 : 1);
                        buildingItemWheelAccumulator_ = 0.0;
                        buildingItemWheelArmed_ = false;
                    }
                }
            } else if (
                buildingItemWheelReleaseRemaining_ > 0.0 ||
                !buildingItemWheelArmed_) {
                buildingItemWheelReleaseRemaining_ = std::max(
                    0.0,
                    buildingItemWheelReleaseRemaining_ -
                        static_cast<double>(GetFrameTime()));
                if (buildingItemWheelReleaseRemaining_ <= 0.0) {
                    buildingItemWheelArmed_ = true;
                    buildingItemWheelAccumulator_ = 0.0;
                }
            }
        } else if (actionMode_ == ActionMode::Buildings) {
            constexpr std::array<int, MaximumBuildingHotbarSlots>
                BuildingKeys{
                    KEY_ONE, KEY_TWO, KEY_THREE,
                    KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO,
                };
            const BuildingHotbarLayout layout =
                makeBuildingHotbarLayout(
                    currentSnapshot.unlockedBuildings,
                    buildingHotbarCategory_,
                    currentSnapshot.coreMaxHealth > 0.0,
                    currentSnapshot.coreLevel,
                    currentSnapshot.sandboxMode ||
                        currentSnapshot.unlimitedResources);
            for (std::size_t keyIndex = 0;
                 keyIndex < layout.count; ++keyIndex) {
                if (IsKeyPressed(BuildingKeys[keyIndex])) {
                    const BuildingType type =
                        layout.types[keyIndex];
                    selectBuildingMode(type);
                }
            }
            constexpr double BuildingWheelThreshold = 0.55;
            constexpr double BuildingWheelReleaseSeconds = 0.10;
            if (std::abs(wheel) > 0.01F) {
                buildingItemWheelReleaseRemaining_ =
                    BuildingWheelReleaseSeconds;
                if (buildingItemWheelArmed_) {
                    buildingItemWheelAccumulator_ = std::clamp(
                        buildingItemWheelAccumulator_ +
                            static_cast<double>(wheel),
                        -1.0, 1.0);
                    if (std::abs(buildingItemWheelAccumulator_) >=
                        BuildingWheelThreshold) {
                        selectNextBuildingItem(
                            currentSnapshot,
                            buildingItemWheelAccumulator_ > 0.0
                                ? -1
                                : 1);
                        buildingItemWheelAccumulator_ = 0.0;
                        buildingItemWheelArmed_ = false;
                    }
                }
            } else if (
                buildingItemWheelReleaseRemaining_ > 0.0 ||
                !buildingItemWheelArmed_) {
                buildingItemWheelReleaseRemaining_ = std::max(
                    0.0,
                    buildingItemWheelReleaseRemaining_ -
                        static_cast<double>(GetFrameTime()));
                if (buildingItemWheelReleaseRemaining_ <= 0.0) {
                    buildingItemWheelArmed_ = true;
                    buildingItemWheelAccumulator_ = 0.0;
                }
            }
        } else {
            constexpr std::array<int, PlayerWeaponCount> EquipmentKeys{
                KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR,
                KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE,
            };
            const auto order = equipmentOrder(actionMode_);
            std::size_t visibleSlotIndex = 0;
            for (const PlayerWeapon weapon : order) {
                const std::size_t weaponIndex =
                    static_cast<std::size_t>(weapon);
                if (!currentSnapshot.unlockedWeapons[weaponIndex]) {
                    continue;
                }
                if (IsKeyPressed(
                        EquipmentKeys[visibleSlotIndex])) {
                    pendingWeaponSelection_ = weapon;
                    lastEquipmentSelection_ = weapon;
                }
                ++visibleSlotIndex;
            }
            const float hotbarWheel = wheel;
            if (std::abs(hotbarWheel) > 0.01F &&
                !aimedDirectionalDefense) {
                // Wheel up moves left, wheel down moves right, matching the
                // visible slot order in the current tools/weapons hotbar.
                selectNextActionModeItem(
                    currentSnapshot,
                    hotbarWheel > 0.0F ? -1 : 1);
            }
        }
        if (actionMode_ == ActionMode::Buildings && buildingHotbarCategory_ ==
                BuildingHotbarCategory::Modular &&
            keyPressed(userSettings_.controls,
                       ControlAction::UpgradeWeapon)) {
            selectNextBuildingItem(currentSnapshot);
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
        const bool startWavePressed = keyPressed(
            userSettings_.controls, ControlAction::StartWave);
        if (startWavePressed &&
            currentSnapshot.state == RunState::BuildPhase &&
            !currentSnapshot.coreId) {
            const std::string message =
                localizeText("Place Core first");
            if (statusMessageRemaining_ > 0.0 &&
                statusMessage_ == message) {
                statusMessageShakeRemaining_ = 0.22;
            }
            statusMessage_ = message;
            statusMessageRemaining_ = 1.8;
        } else {
            pendingStartWave_ = pendingStartWave_ || startWavePressed;
        }
        pendingUnlimitedResources_ =
            pendingUnlimitedResources_ || IsKeyPressed(KEY_O);
        if (keyPressed(
                userSettings_.controls,
                ControlAction::ToggleTool)) {
            selectNextActionModeItem(currentSnapshot);
        }
        if (buildingManagementActive &&
            keyPressed(userSettings_.controls, ControlAction::Copy)) {
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
                    if (copied->type == BuildingType::Core) {
                        const std::string message = localizeText(
                            "Core already placed");
                        if (statusMessageRemaining_ > 0.0 &&
                            statusMessage_ == message) {
                            statusMessageShakeRemaining_ = 0.22;
                        }
                        statusMessage_ = message;
                        statusMessageRemaining_ = 1.4;
                        invalidActionRemaining_ = 0.22;
                    } else {
                        selectBuildingMode(copied->type);
                    }
                    copiedPlacement = true;
                }
            }
            if (!copiedPlacement && actionModularBuilding) {
                const EntityId target =
                    *actionModularBuilding;
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
                        ActionMode::Buildings,
                        currentSnapshot);
                    buildingHotbarCategory_ =
                        BuildingHotbarCategory::Modular;
                    setFoundationBuildMode(true);
                    selectModularBuildPiece(
                        frame->storey == 0
                            ? ModularBuildPiece::Foundation
                            : ModularBuildPiece::
                                  FloorPlatform);
                } else if (
                    wall !=
                    currentSnapshot.modularWalls.end()) {
                    selectBuildingMode(BuildingType::Wall);
                } else if (
                    ramp != currentSnapshot.ramps.end()) {
                    selectActionMode(
                        ActionMode::Buildings,
                        currentSnapshot);
                    buildingHotbarCategory_ =
                        BuildingHotbarCategory::Modular;
                    setFoundationBuildMode(true);
                    selectModularBuildPiece(
                        ModularBuildPiece::Ramp);
                    modularRotation_ = ramp->rotation;
                }
            }
        }
        const bool aimingCore =
            currentSnapshot.aimedBuilding &&
            currentSnapshot.coreId &&
            currentSnapshot.aimedBuilding ==
                currentSnapshot.coreId;
        pendingWeaponUpgrade_ = pendingWeaponUpgrade_ ||
            (!aimingCore && actionMode_ == ActionMode::Equipment &&
             currentSnapshot.selectedWeapon == PlayerWeapon::Bomb &&
             keyPressed(userSettings_.controls,ControlAction::UpgradeWeapon));
        pendingRevealChest_ = pendingRevealChest_ ||
            keyPressed(userSettings_.controls,
                       ControlAction::RevealChest);
        pendingDefeatAllEnemies_ = pendingDefeatAllEnemies_ ||
            (controlDown && IsKeyPressed(KEY_K));
        pendingToggleInvulnerability_ =
            pendingToggleInvulnerability_ || IsKeyPressed(KEY_I);
        pendingGrantInsight_ = pendingGrantInsight_ ||
            (currentSnapshot.unlimitedResources &&
             controlDown && IsKeyPressed(KEY_J));
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
                      ControlAction::Upgrade)) {
            if (currentSnapshot.aimedLoot) {
                pendingChestReroll_ = RerollChestCommand{
                    *currentSnapshot.aimedLoot};
            } else if (buildingManagementActive && actionBuilding) {
                const auto building = std::find_if(
                    currentSnapshot.buildings.begin(),
                    currentSnapshot.buildings.end(),
                    [actionBuilding](const BuildingInstance& value) {
                        return value.id == *actionBuilding;
                    });
                if (building != currentSnapshot.buildings.end() &&
                    !BuildingSystem::usesGlobalBlueprint(building->type)) {
                    pendingBuildingUpgrade_ =
                        UpgradeBuildingCommand{
                            .buildingId = *actionBuilding,
                            .maximumAffordable = IsKeyDown(KEY_LEFT_SHIFT) ||
                                IsKeyDown(KEY_RIGHT_SHIFT),
                        };
                }
            }
        }
        if (aimingCore &&
            currentSnapshot.unlockedWeapons[
                static_cast<std::size_t>(PlayerWeapon::Bomb)] &&
            keyPressed(userSettings_.controls, ControlAction::UpgradeWeapon)) {
            pendingPurchaseBombBundle_ = true;
        }
        repairSweepActive_ =
            buildingManagementActive &&
            !aimingCore &&
            keyDown(userSettings_.controls, ControlAction::Repair);
        if (repairSweepActive_) {
            if (actionBuilding) {
                const auto target = std::find_if(
                    currentSnapshot.buildings.begin(),
                    currentSnapshot.buildings.end(),
                    [actionBuilding](
                        const BuildingInstance& building) {
                        return building.id == *actionBuilding;
                    });
                if (target != currentSnapshot.buildings.end() &&
                    target->health < target->maxHealth &&
                    repairSweepTarget_ != actionBuilding) {
                    pendingBuildingRepair_ =
                        RepairBuildingCommand{target->id};
                    repairSweepTarget_ = target->id;
                }
            } else if (actionModularBuilding) {
                const EntityId id = *actionModularBuilding;
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
        if (pendingBuildingRepair_) {
            contextualHammerRemaining_ = 1.15;
            contextualHammerSwingPending_ = true;
        }
        const bool canSweepRemove =
            buildingManagementActive;
        const auto aimedRemovalTarget =
            [&]() -> std::optional<EntityId> {
                if (actionBuilding) {
                    return actionBuilding;
                }
                return actionModularBuilding;
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
        if (keyPressed(userSettings_.controls,
                       ControlAction::Interact)) {
            const auto prop = currentSnapshot.aimedResource
                ? std::ranges::find(currentSnapshot.resourceNodes, *currentSnapshot.aimedResource, &ResourceNode::id)
                : currentSnapshot.resourceNodes.end();
            const bool aimedProp = !currentSnapshot.selectedBuilding && !foundationBuildMode_ &&
                prop != currentSnapshot.resourceNodes.end() && prop->active && isDestructibleProp(prop->type);
            if (currentSnapshot.aimedChest ||
                currentSnapshot.aimedLoot ||
                currentSnapshot.aimedChallengeColumn ||
                currentSnapshot.aimedWorldLandmark || aimedProp) {
                pendingInteract_ = true;
            } else if (!currentSnapshot.selectedBuilding &&
                       actionBuilding) {
                const auto gate = std::ranges::find(
                    currentSnapshot.buildings, *actionBuilding,
                    &BuildingInstance::id);
                if (gate != currentSnapshot.buildings.end() &&
                    gate->type == BuildingType::Gate) {
                    pendingGateToggle_ =
                        ToggleGateCommand{*actionBuilding};
                }
            }
        }
        if (IsKeyPressed(KEY_E) && buildingManagementActive &&
            currentSnapshot.aimedBuilding) {
            const auto producer = std::ranges::find(
                currentSnapshot.buildings, *currentSnapshot.aimedBuilding,
                &BuildingInstance::id);
            if (producer != currentSnapshot.buildings.end() &&
                (producer->type == BuildingType::CrystalMine ||
                 producer->type == BuildingType::LumberMill ||
                 producer->type == BuildingType::Quarry)) {
                pendingCollectProducer_ = CollectProducerCommand{producer->type};
            }
        }
        const bool rotateKeyPressed = IsKeyPressed(KEY_R);
        const bool reverseRotateKey =
            rotateKeyPressed &&
            (IsKeyDown(KEY_LEFT_SHIFT) ||
             IsKeyDown(KEY_RIGHT_SHIFT));
        if (foundationBuildMode_ ||
            currentSnapshot.selectedBuilding ||
            (buildingManagementActive && aimedDirectionalDefense)) {
            // The compact building catalog uses the wheel to move between
            // items. Rotation for a selected building remains available on R.
            const double rotationWheel =
                actionMode_ == ActionMode::Buildings
                    ? 0.0
                    : static_cast<double>(wheel);
            buildingRotationWheelAccumulator_ = std::clamp(
                buildingRotationWheelAccumulator_ +
                    rotationWheel,
                -1.0, 1.0);
            const bool wheelRotationReady =
                buildingRotationCooldownRemaining_ <= 0.0 &&
                std::abs(buildingRotationWheelAccumulator_) >= 1.0;
            if (rotateKeyPressed || wheelRotationReady) {
                const int direction = rotateKeyPressed
                    ? (reverseRotateKey ? -1 : 1)
                    : buildingRotationWheelAccumulator_ > 0.0
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
                } else if (
                    currentSnapshot.selectedBuilding &&
                    supportsManualBuildingRotation(
                        *currentSnapshot.selectedBuilding)) {
                    pendingBuildingRotation_ +=
                        direction;
                } else if (buildingManagementActive &&
                           aimedDirectionalDefense) {
                    pendingPlacedBuildingRotation_ =
                        RotatePlacedBuildingCommand{
                            *aimedDirectionalDefense,
                            direction};
                }
                buildingRotationWheelAccumulator_ = 0.0;
                buildingRotationCooldownRemaining_ = 0.16;
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
        placementDragPreviews_.clear();
        if (wallDragStart_ && placementDragType_ && placementDragSurface_) {
            const auto cells = placementGestureLine(*placementDragType_, *wallDragStart_,
                wallDragEnd_.value_or(*wallDragStart_), placementDragExtended_, placementDragAxis_);
            const auto rotation = alignsWithPlacementLine(*placementDragType_) && cells.size() > 1U && placementDragAxis_
                ? placementLineRotation(*placementDragAxis_)
                : currentSnapshot.buildingPreview ? currentSnapshot.buildingPreview->rotation : std::uint8_t{0};
            placementDragPreviews_ = simulation_.previewBuildingLine(*placementDragType_, cells,
                rotation, placementDragSurface_->height);
        }
        if (wallDragStart_ &&
            placementDragType_ &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            std::optional<PlacementError> rejected;
            for (const auto& preview : placementDragPreviews_) {
                if (!preview.placement.valid()) {
                    if (!rejected) rejected = preview.placement.error;
                    continue;
                }
                pendingWallPlacements_.push_back({
                    .type = preview.type, .gridPosition = preview.gridPosition, .rotation = preview.rotation,
                    .baseHeight = preview.baseHeight, .platformStorey = preview.platformStorey,
                    .lockHeight = true,
                    .foundationAnchor = preview.foundationPreview
                        ? std::optional<GridCoord>{preview.foundationPreview->anchor} : std::nullopt,
                });
            }
            if (rejected) {
                statusMessage_ = localizeText(std::string{placementErrorMessage(*rejected)});
                statusMessageRemaining_ = 1.8;
                statusMessageShakeRemaining_ = 0.22;
                invalidActionRemaining_ = 0.22;
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
            const bool placed = finishModularPlacementDrag();
            if (!placed) {
                if (modularPlacementFailureRepeatWindow_ > 0.0) {
                    invalidActionRemaining_ = 0.22;
                }
                modularPlacementFailureRepeatWindow_ = 0.8;
            } else {
                modularPlacementFailureRepeatWindow_ = 0.0;
            }
        }
        const bool mousePrimaryPressed =
            !coreMenuPointerActive_ &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        // A press always performs one immediate attack. Repeating starts only
        // after an intentional hold, so an ordinary click cannot queue a
        // second swing on the following frame.
        constexpr double AttackHoldRepeatDelaySeconds = 0.18;
        const bool primaryMouseHeldForRepeat =
            primaryMouseDown &&
            primaryAttackHoldSeconds_ >=
                AttackHoldRepeatDelaySeconds;
        const bool heldResourceGather =
            currentSnapshot.holdToGather &&
            actionMode_ == ActionMode::Equipment &&
            isPlayerTool(currentSnapshot.selectedWeapon) &&
            !currentSnapshot.buildingPreview &&
            !foundationBuildMode_ &&
            !buildingContextCardTarget_ &&
            primaryMouseHeldForRepeat;
        const bool heldWeaponAttack =
            actionMode_ == ActionMode::Equipment &&
            isPlayerCombatWeapon(
                currentSnapshot.selectedWeapon) &&
            !currentSnapshot.buildingPreview &&
            !foundationBuildMode_ &&
            !buildingContextCardTarget_ &&
            primaryMouseHeldForRepeat;
        const bool attackPressed =
            mousePrimaryPressed ||
            heldResourceGather ||
            heldWeaponAttack ||
            keyPressed(userSettings_.controls,
                       ControlAction::Attack);
        if (attackPressed) {
            const bool smartToolCanLeaveBuildMode =
                actionMode_ == ActionMode::Buildings;
            if (currentSnapshot.automaticToolSwitch &&
                (smartToolCanLeaveBuildMode ||
                 (actionMode_ == ActionMode::Equipment &&
                  !pendingBuildingSelection_ &&
                  !currentSnapshot.buildingPreview &&
                  !foundationBuildMode_)) &&
                currentSnapshot.aimedResource) {
                const auto resource = std::ranges::find_if(
                    currentSnapshot.resourceNodes,
                    [&currentSnapshot](const ResourceNode& node) {
                        return node.id ==
                               *currentSnapshot.aimedResource;
                    });
                if (resource != currentSnapshot.resourceNodes.end() &&
                    resource->active &&
                    isHarvestableResource(resource->type)) {
                    const PlayerWeapon desiredTool =
                        resource->type == ResourceType::Wood
                            ? PlayerWeapon::Axe
                            : PlayerWeapon::Pickaxe;
                    const bool desiredToolUnlocked =
                        currentSnapshot.unlimitedResources ||
                        currentSnapshot.unlockedWeapons[
                            static_cast<std::size_t>(desiredTool)];
                    if (desiredToolUnlocked) {
                        if (smartToolCanLeaveBuildMode) {
                            selectActionMode(
                                ActionMode::Equipment,
                                currentSnapshot);
                            currentSnapshot.selectedBuilding.reset();
                            currentSnapshot.buildingPreview.reset();
                        }
                        pendingWeaponSelection_ = desiredTool;
                        lastEquipmentSelection_ = desiredTool;
                        currentSnapshot.selectedWeapon = desiredTool;
                    }
                }
            }
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
            } else if (
                mousePrimaryPressed &&
                buildingManagementActive &&
                !pendingBuildingSelection_ &&
                !currentSnapshot.aimedEnemy &&
                !currentSnapshot.aimedResource &&
                !currentSnapshot.aimedChest &&
                !currentSnapshot.aimedLoot &&
                !currentSnapshot.aimedChallengeColumn &&
                !currentSnapshot.aimedWorldLandmark &&
                (currentSnapshot.aimedBuilding ||
                 currentSnapshot.aimedModularBuilding)) {
                const EntityId target =
                    currentSnapshot.aimedBuilding
                        ? *currentSnapshot.aimedBuilding
                        : *currentSnapshot
                               .aimedModularBuilding;
                buildingContextCardTarget_ = target;
            } else if (mousePrimaryPressed &&
                       buildingContextCardTarget_) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
            } else if (actionModeUsesEquipment(actionMode_) &&
                       !pendingBuildingSelection_ &&
                       currentSnapshot.selectedWeapon == PlayerWeapon::Bomb) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                if (mousePrimaryPressed) {
                    pendingBombThrow_ = true;
                    if (toolSwapRemaining_ <= 0.0 &&
                        toolSwingRemaining_ <= 0.0) {
                        toolSwingDuration_ = toolSwingDurationFor(
                            currentSnapshot, displayedToolVisual_);
                        toolSwingRemaining_ = toolSwingDuration_;
                    }
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
                        currentSnapshot.selectedWeapon ==
                            PlayerWeapon::Axe ||
                        currentSnapshot.selectedWeapon ==
                            PlayerWeapon::Club;
                    if (currentSnapshot.automaticToolSwitch &&
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
                        toolSwingDuration_ = toolSwingDurationFor(
                            currentSnapshot, displayedToolVisual_);
                        toolSwingRemaining_ =
                            toolSwingDuration_;
                        chooseHarvestSwingAnimation(displayedToolVisual_);
                        if (displayedToolVisual_ ==
                                FirstPersonToolVisual::Axe ||
                            displayedToolVisual_ ==
                                FirstPersonToolVisual::Pickaxe ||
                            displayedToolVisual_ ==
                                FirstPersonToolVisual::Club) {
                            audio_.playHarvestSwing();
                        }
                        toolSwingAttackPending_ = true;
                        toolSwingQueued_ = false;
                        toolQueuedSwingHasAttack_ = false;
                        toolQueuedResourceTarget_.reset();
                        toolSwingQueueRemaining_ = 0.0;
                    } else {
                        toolSwingQueued_ = true;
                        toolQueuedSwingHasAttack_ = true;
                        toolQueuedResourceTarget_ =
                            currentSnapshot.aimedEnemy
                                ? std::nullopt
                                : currentSnapshot.aimedResource;
                        toolSwingQueueRemaining_ = 0.75;
                    }
                }
            }
        }
    } else {
        interactionResourceAim_.reset();
        input_.moveForward = 0.0;
        input_.moveRight = 0.0;
        input_.sprint = false;
        input_.jumpHeld = false;
        pendingDash_ = false;
        repairSweepActive_ = false;
        repairSweepTarget_.reset();
    }
}

} // namespace ian
