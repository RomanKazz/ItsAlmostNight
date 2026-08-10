#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "graphics/WorldTransforms.hpp"

#include "presentation/PresentationTimeline.hpp"
#include "ui/UiLabels.hpp"
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

void App::update() {
    const double frameSeconds = static_cast<double>(GetFrameTime());
    const auto& progressionSnapshot = simulation_.snapshot();
    skillTree_.setUnlimitedPoints(progressionSnapshot.unlimitedResources);
    skillTree_.setInsightProgress(progressionSnapshot.currentInsight,
                                  progressionSnapshot.requiredInsight);
    insightPulseRemaining_ = std::max(0.0, insightPulseRemaining_ - frameSeconds);
    insightGainRemaining_ = std::max(0.0, insightGainRemaining_ - frameSeconds);
    insightPointSequenceRemaining_ = std::max(
        0.0, insightPointSequenceRemaining_ - frameSeconds);
    objectivePulseRemaining_ = std::max(
        0.0, objectivePulseRemaining_ - frameSeconds);
    for (const ObjectiveStatus& objective :
         progressionSnapshot.objectives) {
        auto [progress, inserted] =
            objectiveProgressCache_.try_emplace(
                objective.definition.id, objective.progress);
        if (!inserted &&
            objective.progress > progress->second + 1e-6) {
            objectivePulseId_ = objective.definition.id;
            objectivePulseRemaining_ = objectivePulseDuration_;
        }
        progress->second = objective.progress;
    }
    if (displayedInsight_ < 0.0) displayedInsight_ = progressionSnapshot.currentInsight;
    const double insightPointProgress =
        presentation::timelineProgress(
            insightPointSequenceRemaining_,
            insightPointSequenceDuration_);
    if (pendingInsightPointNotification_ > 0 &&
        insightPointProgress >= 0.35) {
        statusMessage_ = pendingInsightPointNotification_ == 1
            ? "SKILL POINT ACQUIRED — PRESS K"
            : std::to_string(pendingInsightPointNotification_) +
                  " SKILL POINTS ACQUIRED — PRESS K";
        statusMessageRemaining_ = 3.0;
        pendingInsightPointNotification_ = 0;
    }
    if (insightPointSequenceRemaining_ > 0.0) {
        const double progress = insightPointProgress;
        if (progress < 0.35) {
            displayedInsight_ = insightAnimationBefore_ +
                (insightAnimationRequirement_ - insightAnimationBefore_) * (progress / 0.35);
        } else if (progress < 0.55) {
            displayedInsight_ = insightAnimationRequirement_;
        } else {
            displayedInsight_ = insightAnimationAfter_ * ((progress - 0.55) / 0.45);
        }
    } else {
        displayedInsight_ += (progressionSnapshot.currentInsight - displayedInsight_) *
            (1.0 - std::exp(-10.0 * frameSeconds));
    }
    if (const auto purchase = skillTree_.update(static_cast<float>(frameSeconds))) {
        const PlayerWeapon weaponBeforePurchase =
            simulation_.snapshot().selectedWeapon;
        if (simulation_.purchaseSkill(*purchase) == SkillPurchaseError::None) {
            audio_.playUiConfirm();
            const auto& purchasedSnapshot = simulation_.snapshot();
            if (purchasedSnapshot.selectedWeapon !=
                weaponBeforePurchase) {
                if (isPlayerTool(
                        purchasedSnapshot.selectedWeapon)) {
                    lastToolSelection_ =
                        purchasedSnapshot.selectedWeapon;
                } else {
                    lastWeaponSelection_ =
                        purchasedSnapshot.selectedWeapon;
                }
                selectActionMode(
                    isPlayerTool(purchasedSnapshot.selectedWeapon)
                        ? ActionMode::Tools
                        : ActionMode::Weapons,
                    purchasedSnapshot);
            }
        }
    }
    const auto& hotbarSnapshot = simulation_.snapshot();
    if (playerSpawnDropActive_ &&
        hotbarSnapshot.state != RunState::Paused) {
        constexpr double SpawnGravity = 14.0;
        playerSpawnDropVelocity_ -= SpawnGravity * frameSeconds;
        playerSpawnDropHeight_ +=
            playerSpawnDropVelocity_ * frameSeconds;
        if (playerSpawnDropHeight_ <= 0.0) {
            playerSpawnDropHeight_ = 0.0;
            playerSpawnDropVelocity_ = 0.0;
            playerSpawnDropActive_ = false;
            Vec3 landingPosition = hotbarSnapshot.playerPosition;
            landingPosition.y = simulation_.terrain().getHeight(
                                    landingPosition.x,
                                    landingPosition.z) +
                0.04;
            addEffect(PresentationEffectType::LandingDust,
                      landingPosition, 0.62, 1.0F);
            landingResponseDuration_ = 0.26;
            landingResponseRemaining_ = landingResponseDuration_;
            landingResponseStrength_ = 0.72;
            addCameraShake(0.16, 0.045);
            addCameraImpulse({0.0, -0.018, 0.0});
        }
    }
    if (hotbarSnapshot.state != RunState::MainMenu &&
        hotbarSnapshot.state != RunState::Paused) {
        worldRevealElapsed_ = std::min(
            1.7,
            worldRevealElapsed_ + frameSeconds);
    }
    const float hotbarBlend =
        1.0F - std::exp(
                   -14.0F *
                   static_cast<float>(frameSeconds));
    float buildingTarget = static_cast<float>(
        hotbarSnapshot.selectedBuilding
            ? static_cast<std::size_t>(
                  *hotbarSnapshot.selectedBuilding)
            : static_cast<std::size_t>(
                  lastBuildingSelection_));
    buildHotbarSelectionPosition_ +=
        (buildingTarget -
         buildHotbarSelectionPosition_) *
        hotbarBlend;
    foundationHotbarSelectionPosition_ +=
        (static_cast<float>(modularBuildPiece_) -
         foundationHotbarSelectionPosition_) *
        hotbarBlend;
    const float weaponTarget = static_cast<float>(
        actionMode_ == ActionMode::Tools
            ? playerWeaponVisibleHotbarIndex(
                  hotbarSnapshot.selectedWeapon,
                  hotbarSnapshot.unlockedWeapons,
                  PlayerToolHotbarOrder)
            : playerWeaponVisibleHotbarIndex(
                  hotbarSnapshot.selectedWeapon,
                  hotbarSnapshot.unlockedWeapons,
                  PlayerCombatHotbarOrder));
    weaponHotbarSelectionPosition_ +=
        (weaponTarget - weaponHotbarSelectionPosition_) *
        hotbarBlend;
    const float buildAlphaTarget =
        actionMode_ == ActionMode::Buildings
            ? 1.0F
            : 0.0F;
    const float foundationAlphaTarget =
        actionMode_ == ActionMode::Modular ? 1.0F : 0.0F;
    const float weaponAlphaTarget =
        actionMode_ == ActionMode::Tools ||
                actionMode_ == ActionMode::Weapons
            ? 1.0F
            : 0.0F;
    buildHotbarSelectionAlpha_ +=
        (buildAlphaTarget - buildHotbarSelectionAlpha_) *
        hotbarBlend;
    foundationHotbarSelectionAlpha_ +=
        (foundationAlphaTarget -
         foundationHotbarSelectionAlpha_) *
        hotbarBlend;
    weaponHotbarSelectionAlpha_ +=
        (weaponAlphaTarget - weaponHotbarSelectionAlpha_) *
        hotbarBlend;
    const bool minimapHeld =
        hotbarSnapshot.state != RunState::MainMenu &&
        !skillTree_.isOpen() &&
        !renderer_->graphicsPanelVisible() &&
        !IsKeyDown(KEY_LEFT_CONTROL) &&
        !IsKeyDown(KEY_RIGHT_CONTROL) &&
        controlKey(userSettings_.controls, ControlAction::Map) != KEY_NULL &&
        IsKeyDown(static_cast<KeyboardKey>(controlKey(
            userSettings_.controls, ControlAction::Map)));
    const float minimapTarget = minimapHeld ? 1.0F : 0.0F;
    const float minimapBlend =
        1.0F - std::exp(
                   -18.0F * static_cast<float>(frameSeconds));
    minimapExpansion_ +=
        (minimapTarget - minimapExpansion_) * minimapBlend;
    if (!minimapHeld && minimapExpansion_ < 0.001F) {
        minimapExpansion_ = 0.0F;
    }
    const bool hitStopActive = hitStopRemaining_ > 0.0;
    hitStopRemaining_ =
        std::max(0.0, hitStopRemaining_ - frameSeconds);
    const bool toolContactHoldActive =
        toolContactHoldRemaining_ > 0.0;
    toolContactHoldRemaining_ = std::max(
        0.0,
        toolContactHoldRemaining_ - frameSeconds);
    crosshairHitRemaining_ =
        std::max(0.0, crosshairHitRemaining_ - frameSeconds);
    invalidActionRemaining_ =
        std::max(0.0, invalidActionRemaining_ - frameSeconds);
    placementSnapPulseRemaining_ = std::max(
        0.0, placementSnapPulseRemaining_ - frameSeconds);
    weaponRecoilRemaining_ = std::max(
        0.0, weaponRecoilRemaining_ - frameSeconds);
    iceWandRecoilRemaining_ = std::max(
        0.0, iceWandRecoilRemaining_ - frameSeconds);
    iceImpactFlashRemaining_ = std::max(
        0.0, iceImpactFlashRemaining_ - frameSeconds);
    const double previousToolSwingRemaining =
        toolSwingRemaining_;
    toolSwingRemaining_ = std::max(
        0.0,
        toolSwingRemaining_ -
            (hitStopActive || toolContactHoldActive
                 ? 0.0
                 : frameSeconds));
    const double toolContactProgress = std::clamp(
        static_cast<double>(toolTuning_.hitProgress),
        0.25, 0.65);
    const double toolContactRemaining =
        toolSwingDuration_ * (1.0 - toolContactProgress);
    if (toolSwingAttackPending_ &&
        previousToolSwingRemaining > toolContactRemaining &&
        toolSwingRemaining_ <= toolContactRemaining) {
        pendingPickaxe_ = true;
        toolSwingAttackPending_ = false;
    }
    toolSwingQueueRemaining_ = std::max(
        0.0, toolSwingQueueRemaining_ - frameSeconds);
    if (toolSwingQueueRemaining_ <= 0.0) {
        toolSwingQueued_ = false;
        toolQueuedSwingHasAttack_ = false;
        toolQueuedResourceTarget_.reset();
    }
    const bool toolSwapWasActive = toolSwapRemaining_ > 0.0;
    toolSwapRemaining_ = std::max(
        0.0, toolSwapRemaining_ - frameSeconds);
    FirstPersonToolVisual desiredToolVisual =
        FirstPersonToolVisual::None;
    if (renderer_ && renderer_->graphicsPanelVisible() &&
        graphicsPanelTab_ == ToolSettingsTab) {
        desiredToolVisual = toolPanelPreviewUsesAxe_
            ? FirstPersonToolVisual::Axe
            : FirstPersonToolVisual::Pickaxe;
    } else {
        switch (hotbarSnapshot.selectedWeapon) {
        case PlayerWeapon::BareHands:
        case PlayerWeapon::Rifle:
            desiredToolVisual = FirstPersonToolVisual::None;
            break;
        case PlayerWeapon::Axe:
            desiredToolVisual = FirstPersonToolVisual::Axe;
            break;
        case PlayerWeapon::Pickaxe:
            desiredToolVisual = FirstPersonToolVisual::Pickaxe;
            break;
        case PlayerWeapon::Club:
            desiredToolVisual = FirstPersonToolVisual::Club;
            break;
        case PlayerWeapon::IceWand:
            desiredToolVisual = FirstPersonToolVisual::IceWand;
            break;
        case PlayerWeapon::FireWand:
            desiredToolVisual = FirstPersonToolVisual::FireWand;
            break;
        case PlayerWeapon::Hammer:
            desiredToolVisual = FirstPersonToolVisual::Hammer;
            break;
        }
    }
    if (!toolViewModelInitialized_) {
        // Establish the initial state without playing an equip animation as
        // the run or settings preview is first created.
        displayedToolVisual_ = desiredToolVisual;
        toolSwapCandidateVisual_ = desiredToolVisual;
        toolSwapDestinationVisual_ = desiredToolVisual;
        toolSwapCandidateSeconds_ = 0.0;
        toolSwapRemaining_ = 0.0;
        toolViewModelInitialized_ = true;
    } else {
        if (desiredToolVisual != toolSwapCandidateVisual_) {
            toolSwapCandidateVisual_ = desiredToolVisual;
            toolSwapCandidateSeconds_ = 0.0;
        } else {
            toolSwapCandidateSeconds_ += frameSeconds;
        }
        const double hideEndRemaining =
            toolSwapDuration_ * (1.0 - ToolSwapHideFraction);
        if (toolSwapWasActive &&
            toolSwapRemaining_ > hideEndRemaining &&
            toolSwapCandidateSeconds_ >= 0.07) {
            // The old model is still descending. Retarget this same swap
            // instead of completing it and starting a second animation.
            toolSwapDestinationVisual_ =
                toolSwapCandidateVisual_;
        }
        if (toolSwapWasActive &&
            toolSwapRemaining_ <= hideEndRemaining) {
            displayedToolVisual_ =
                toolSwapDestinationVisual_;
        }
        if (toolSwapRemaining_ <= 0.0 &&
            toolSwapCandidateSeconds_ >= 0.07 &&
            toolSwapCandidateVisual_ !=
                displayedToolVisual_) {
            toolSwapDestinationVisual_ =
                toolSwapCandidateVisual_;
            toolSwapDuration_ = std::clamp(
                static_cast<double>(toolTuning_.swapDuration) *
                    ToolSwapDurationScale,
                0.40, 1.20);
            toolSwapRemaining_ = toolSwapDuration_;
        }
    }
    const bool queuedResourceStillTargeted =
        !toolQueuedResourceTarget_ ||
        (hotbarSnapshot.aimedResource ==
             toolQueuedResourceTarget_ &&
         std::any_of(
             hotbarSnapshot.resourceNodes.begin(),
             hotbarSnapshot.resourceNodes.end(),
             [this](const ResourceNode& resource) {
                 return resource.id ==
                        *toolQueuedResourceTarget_;
             }));
    if (toolSwingQueued_ && !queuedResourceStillTargeted) {
        toolSwingQueued_ = false;
        toolQueuedSwingHasAttack_ = false;
        toolQueuedResourceTarget_.reset();
        toolSwingQueueRemaining_ = 0.0;
    }
    if (toolSwingQueued_ && toolSwapRemaining_ <= 0.0 &&
        toolSwingRemaining_ <= 0.0 &&
        (displayedToolVisual_ == FirstPersonToolVisual::Axe ||
         displayedToolVisual_ == FirstPersonToolVisual::Club) ==
            toolSwingUsesAxe_) {
        toolSwingDuration_ = toolTuning_.swingDuration;
        toolSwingRemaining_ = toolSwingDuration_;
        toolSwingAttackPending_ =
            toolQueuedSwingHasAttack_;
        toolQueuedSwingHasAttack_ = false;
        toolSwingQueued_ = false;
        toolQueuedResourceTarget_.reset();
        toolSwingQueueRemaining_ = 0.0;
    }
    const SimulationSnapshot& movementSnapshot =
        simulation_.snapshot();
    const bool sprinting =
        acceptsGameplayInput(
            movementSnapshot.state) &&
        input_.sprint &&
        (std::abs(input_.moveForward) > 0.01 ||
         std::abs(input_.moveRight) > 0.01);
    const float targetFov = movementSnapshot.dashing
        ? 88.0F
        : sprinting ? 79.0F : 75.0F;
    const float fovBlend =
        1.0F -
        std::exp(
            -(movementSnapshot.dashing ? 24.0F : 7.5F) *
            static_cast<float>(frameSeconds));
    cameraFov_ +=
        (targetFov - cameraFov_) * fovBlend;
    buildingStatsUpgradeRemaining_ = std::max(
        0.0,
        buildingStatsUpgradeRemaining_ - frameSeconds);
    if (buildingStatsUpgradeRemaining_ <= 0.0) {
        buildingStatsUpgradeEntity_.reset();
    }
    if (weaponRecoilRemaining_ <= 0.0) {
        weaponRecoilStrength_ = 0.0F;
    }
    const RunState frameState = simulation_.snapshot().state;
    const double resourceAnimationSeconds =
        acceptsGameplayInput(frameState) && !hitStopActive
            ? (slowMotion_ ? frameSeconds * 0.2 : frameSeconds)
            : 0.0;
    statusMessageRemaining_ =
        std::max(0.0, statusMessageRemaining_ - frameSeconds);
    cameraShakeRemaining_ = std::max(0.0, cameraShakeRemaining_ - frameSeconds);
    landingResponseRemaining_ = std::max(
        0.0, landingResponseRemaining_ - frameSeconds);
    playerDamageFlashRemaining_ =
        std::max(0.0, playerDamageFlashRemaining_ - frameSeconds);
    damagedBuildingHealthBarRemaining_ = std::max(
        0.0,
        damagedBuildingHealthBarRemaining_ - frameSeconds);
    if (damagedBuildingHealthBarRemaining_ <= 0.0) {
        recentlyDamagedBuilding_.reset();
    }
    woodHudBounceRemaining_ =
        std::max(0.0, woodHudBounceRemaining_ - frameSeconds);
    stoneHudBounceRemaining_ =
        std::max(0.0, stoneHudBounceRemaining_ - frameSeconds);
    coinHudBounceRemaining_ =
        std::max(0.0, coinHudBounceRemaining_ - frameSeconds);
    buildingRotationCooldownRemaining_ = std::max(
        0.0, buildingRotationCooldownRemaining_ - frameSeconds);
    rampSocketLostGraceRemaining_ = std::max(
        0.0,
        rampSocketLostGraceRemaining_ - frameSeconds);
    rampSocketManualOverrideRemaining_ = std::max(
        0.0,
        rampSocketManualOverrideRemaining_ - frameSeconds);
    if (cameraShakeRemaining_ <= 0.0) {
        cameraShakeStrength_ = 0.0;
    }
    presentation::advanceTimeline(effects_, frameSeconds);
    presentation::advanceTimeline(
        arrowVisuals_, frameSeconds);
    presentation::advanceTimeline(
        damageIndicators_, frameSeconds);
    presentation::advanceTimeline(
        floatingDamageNumbers_, frameSeconds);
    presentation::advanceTimeline(
        resourceGainVisuals_, resourceAnimationSeconds);
    presentation::advanceTimeline(
        productionVisuals_, resourceAnimationSeconds);
    presentation::advanceTimeline(
        destroyedResourceVisuals_,
        resourceAnimationSeconds);
    presentation::advanceTimeline(
        destroyedEnemyVisuals_,
        resourceAnimationSeconds);
    presentation::advanceTimeline(
        soldBuildingVisuals_, resourceAnimationSeconds);
    presentation::advanceTimeline(
        buildingImpactVisuals_, frameSeconds);
    presentation::advanceTimeline(
        buildingShotRecoilVisuals_, frameSeconds);
    presentation::advanceTimeline(
        cancelledPlacementPreview_, frameSeconds);
    if (!pendingBuildingPlacement_ &&
        !pendingWallPlacements_.empty()) {
        pendingBuildingPlacement_ =
            pendingWallPlacements_.front();
        pendingWallPlacements_.pop_front();
    }
    if (!pendingBuildingSale_ &&
        !queuedBuildingSales_.empty()) {
        pendingBuildingSale_ =
            queuedBuildingSales_.front();
        queuedBuildingSales_.pop_front();
        const auto& removalSnapshot =
            simulation_.snapshot();
        const EntityId target =
            pendingBuildingSale_->buildingId;
        const auto building = std::find_if(
            removalSnapshot.buildings.begin(),
            removalSnapshot.buildings.end(),
            [target](
                const BuildingInstance& candidate) {
                return candidate.id == target;
            });
        if (building !=
            removalSnapshot.buildings.end()) {
            pendingSoldBuildingVisual_ = *building;
            pendingSoldWallConnections_ =
                building->type == BuildingType::Wall
                    ? wallConnectionMask(
                          removalSnapshot.buildings,
                          building->gridPosition,
                          building->baseHeight)
                    : 0U;
        }
    }
    if (!pendingModularBuildingRemoval_ &&
        !queuedModularBuildingRemovals_.empty()) {
        pendingModularBuildingRemoval_ =
            queuedModularBuildingRemovals_.front();
        queuedModularBuildingRemovals_.pop_front();
        const auto& removalSnapshot =
            simulation_.snapshot();
        const EntityId target =
            pendingModularBuildingRemoval_->buildingId;
        SoldBuildingVisual visual{
            .building = std::nullopt,
            .platformFrame = std::nullopt,
            .modularWall = std::nullopt,
            .ramp = std::nullopt,
            .wallConnections = 0U,
            .remaining = 0.0,
            .duration = 0.0,
        };
        const auto frame = std::find_if(
            removalSnapshot.platformFrames.begin(),
            removalSnapshot.platformFrames.end(),
            [target](
                const PlatformFrameInstance& candidate) {
                return candidate.id == target;
            });
        const auto wall = std::find_if(
            removalSnapshot.modularWalls.begin(),
            removalSnapshot.modularWalls.end(),
            [target](
                const WallInstance& candidate) {
                return candidate.id == target;
            });
        const auto ramp = std::find_if(
            removalSnapshot.ramps.begin(),
            removalSnapshot.ramps.end(),
            [target](
                const RampInstance& candidate) {
                return candidate.id == target;
            });
        if (frame !=
            removalSnapshot.platformFrames.end()) {
            visual.platformFrame = *frame;
        } else if (
            wall !=
            removalSnapshot.modularWalls.end()) {
            visual.modularWall = *wall;
        } else if (
            ramp != removalSnapshot.ramps.end()) {
            visual.ramp = *ramp;
        }
        if (visual.platformFrame ||
            visual.modularWall || visual.ramp) {
            pendingSoldModularVisual_ = visual;
        }
    }
    bool consumedTransientInput = false;
    const double simulationFrameSeconds =
        hitStopActive
            ? 0.0
            : slowMotion_ ? frameSeconds * 0.2
                          : frameSeconds;
    const auto simulationStart = PerformanceClock::now();
    performanceStats_.fixedTicks = fixedStep_.advance(
        simulationFrameSeconds,
        [this, &consumedTransientInput](double deltaSeconds) {
        PlayerCommand tickInput = input_;
        if (!consumedTransientInput) {
            tickInput.lookYaw = pendingYaw_;
            tickInput.lookPitch = pendingPitch_;
            tickInput.jump = pendingJump_;
            tickInput.dash = pendingDash_;
            tickInput.usePickaxe = pendingPickaxe_;
            tickInput.fireRifle = pendingRifleShot_;
            tickInput.fireIceWand = pendingIceWandShot_;
            tickInput.fireFireWand = pendingFireWandShot_;
            tickInput.selectBuilding = pendingBuildingSelection_;
            tickInput.cancelBuilding = pendingBuildingCancel_;
            tickInput.placeBuilding = pendingBuildingPlacement_;
            tickInput.rotateBuilding = pendingBuildingRotation_;
            if (pendingStartWave_) {
                tickInput.startWaveEarly = StartWaveEarlyCommand{};
            }
            if (pendingUnlimitedResources_) {
                tickInput.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
            }
            tickInput.upgradeBuilding = pendingBuildingUpgrade_;
            tickInput.repairBuilding = pendingBuildingRepair_;
            tickInput.sellBuilding = pendingBuildingSale_;
            tickInput.removeModularBuilding =
                pendingModularBuildingRemoval_;
            if (pendingWeaponSelection_) {
                tickInput.selectWeapon = SelectWeaponCommand{
                    *pendingWeaponSelection_};
            }
            if (pendingWeaponUpgrade_) {
                tickInput.upgradeWeapon = UpgradeWeaponCommand{};
            }
            if (pendingBombThrow_) {
                tickInput.useConsumable = UseConsumableCommand{};
            }
            if (pendingInteract_) {
                tickInput.interact = InteractCommand{};
            }
            if (pendingDefeatAllEnemies_) {
                tickInput.defeatAllEnemies = DefeatAllEnemiesCommand{};
            }
            if (pendingToggleInvulnerability_) {
                tickInput.toggleInvulnerability = ToggleInvulnerabilityCommand{};
            }
            if (pendingDamageCore_) {
                tickInput.damageCore = DamageCoreCommand{};
            }
            if (pendingSpawnEnemy_) {
                tickInput.spawnEnemy = SpawnEnemyCommand{
                    debugSpawnType_, debugSpawnCount_};
            }
            tickInput.toggleGate = pendingGateToggle_;
            consumedTransientInput = true;
        }
        if (playerSpawnDropActive_) {
            tickInput = {};
        }
        const auto tickStart = PerformanceClock::now();
        simulation_.tick(deltaSeconds, tickInput);
        performanceStats_.simulationTick.sample(
            performanceMilliseconds(tickStart));
        });
    performanceStats_.simulation.sample(
        performanceMilliseconds(simulationStart));
    if (consumedTransientInput) {
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
        pendingBuildingRepair_.reset();
        pendingBuildingSale_.reset();
        pendingModularBuildingRemoval_.reset();
        pendingWeaponSelection_.reset();
        pendingWeaponUpgrade_ = false;
        pendingBombThrow_ = false;
        pendingInteract_ = false;
        pendingDefeatAllEnemies_ = false;
        pendingToggleInvulnerability_ = false;
        pendingDamageCore_ = false;
        pendingSpawnEnemy_ = false;
        pendingGateToggle_.reset();
    }
    const auto events = simulation_.takeEvents();
    const auto& eventSnapshot = simulation_.snapshot();
    refreshDecorationExclusions(eventSnapshot);
    if (!groundCameraSmoothingInitialized_ ||
        !eventSnapshot.playerGrounded ||
        !groundCameraWasGrounded_) {
        smoothedGroundCameraY_ =
            eventSnapshot.playerPosition.y;
        groundCameraSmoothingInitialized_ = true;
    } else {
        constexpr double MaximumRampCameraLag = 0.16;
        smoothedGroundCameraY_ = std::clamp(
            smoothedGroundCameraY_,
            eventSnapshot.playerPosition.y -
                MaximumRampCameraLag,
            eventSnapshot.playerPosition.y +
                MaximumRampCameraLag);
        const double heightBlend =
            1.0 - std::exp(-14.0 * frameSeconds);
        smoothedGroundCameraY_ +=
            (eventSnapshot.playerPosition.y -
             smoothedGroundCameraY_) *
            heightBlend;
    }
    groundCameraWasGrounded_ =
        eventSnapshot.playerGrounded;
    if (!cameraInertiaInitialized_) {
        previousVisualYaw_ = eventSnapshot.playerYaw;
        previousVisualPitch_ = eventSnapshot.playerPitch;
        cameraInertiaInitialized_ = true;
    }
    const double yawDelta = std::atan2(
        std::sin(eventSnapshot.playerYaw - previousVisualYaw_),
        std::cos(eventSnapshot.playerYaw - previousVisualYaw_));
    const double pitchDelta =
        eventSnapshot.playerPitch - previousVisualPitch_;
    previousVisualYaw_ = eventSnapshot.playerYaw;
    previousVisualPitch_ = eventSnapshot.playerPitch;
    cameraLookYawLag_ = std::clamp(
        cameraLookYawLag_ - yawDelta * 0.22,
        -0.032, 0.032);
    cameraLookPitchLag_ = std::clamp(
        cameraLookPitchLag_ - pitchDelta * 0.18,
        -0.024, 0.024);
    const double lookLagDecay = std::exp(-17.0 * frameSeconds);
    cameraLookYawLag_ *= lookLagDecay;
    cameraLookPitchLag_ *= lookLagDecay;

    const double yawSin = std::sin(eventSnapshot.playerYaw);
    const double yawCos = std::cos(eventSnapshot.playerYaw);
    const double lateralSpeed =
        eventSnapshot.playerHorizontalVelocity.x * yawCos +
        eventSnapshot.playerHorizontalVelocity.z * yawSin;
    const double targetStrafeLean = std::clamp(
        -lateralSpeed / 6.5 * 0.013,
        -0.013, 0.013);
    const double leanBlend =
        1.0 - std::exp(-10.0 * frameSeconds);
    cameraStrafeLean_ +=
        (targetStrafeLean - cameraStrafeLean_) * leanBlend;
    const double impulseDecay = std::exp(-15.0 * frameSeconds);
    cameraImpulseOffset_.x *= impulseDecay;
    cameraImpulseOffset_.y *= impulseDecay;
    cameraImpulseOffset_.z *= impulseDecay;
    if (pendingSoldModularVisual_ &&
        consumedTransientInput) {
        const SoldBuildingVisual& pending =
            *pendingSoldModularVisual_;
        const EntityId id =
            pending.platformFrame
                ? pending.platformFrame->id
                : (pending.modularWall
                       ? pending.modularWall->id
                       : pending.ramp->id);
        const bool stillExists =
            std::any_of(
                eventSnapshot.platformFrames.begin(),
                eventSnapshot.platformFrames.end(),
                [id](
                    const PlatformFrameInstance& frame) {
                    return frame.id == id;
                }) ||
            std::any_of(
                eventSnapshot.modularWalls.begin(),
                eventSnapshot.modularWalls.end(),
                [id](const WallInstance& wall) {
                    return wall.id == id;
                }) ||
            std::any_of(
                eventSnapshot.ramps.begin(),
                eventSnapshot.ramps.end(),
                [id](const RampInstance& ramp) {
                    return ramp.id == id;
                });
        if (!stillExists) {
            SoldBuildingVisual visual = pending;
            constexpr double Duration = 0.52;
            visual.remaining = Duration;
            visual.duration = Duration;
            soldBuildingVisuals_.push_back(visual);
            const double cellSize =
                simulation_.terrain().config().cellSize;
            Vec3 center{};
            float effectScale = 1.0F;
            if (pending.platformFrame) {
                center = {
                    (pending.platformFrame->anchor.x + 1.0) *
                        cellSize,
                    pending.platformFrame->floorHeight,
                    (pending.platformFrame->anchor.z + 1.0) *
                        cellSize,
                };
                effectScale = 1.25F;
            } else if (pending.modularWall) {
                center = {
                    (pending.modularWall->anchor.x + 0.5) *
                        cellSize,
                    pending.modularWall->bottomHeight,
                    (pending.modularWall->anchor.z + 0.5) *
                        cellSize,
                };
                effectScale = 0.78F;
            } else {
                const bool alongZ =
                    pending.ramp->rotation ==
                        Rotation::Deg0 ||
                    pending.ramp->rotation ==
                        Rotation::Deg180;
                const int widthCells =
                    alongZ ? ModularRampWidthCells
                           : ModularRampRunCells;
                const int depthCells =
                    alongZ ? ModularRampRunCells
                           : ModularRampWidthCells;
                center = {
                    (pending.ramp->anchor.x +
                     widthCells * 0.5) *
                        cellSize,
                    pending.ramp->bottomHeight,
                    (pending.ramp->anchor.z +
                     depthCells * 0.5) *
                        cellSize,
                };
                effectScale = 1.35F;
            }
            addEffect(
                PresentationEffectType::Debris,
                center, 0.62, effectScale);
        }
        pendingSoldModularVisual_.reset();
    }
    if (!cameraBobPositionInitialized_) {
        cameraBobPreviousPosition_ =
            eventSnapshot.playerPosition;
        cameraBobPositionInitialized_ = true;
    }
    const double bobDeltaX =
        eventSnapshot.playerPosition.x -
        cameraBobPreviousPosition_.x;
    const double bobDeltaZ =
        eventSnapshot.playerPosition.z -
        cameraBobPreviousPosition_.z;
    const double bobDistance =
        std::hypot(bobDeltaX, bobDeltaZ);
    cameraBobPreviousPosition_ =
        eventSnapshot.playerPosition;
    const bool canBob =
        acceptsGameplayInput(eventSnapshot.state) &&
        eventSnapshot.playerGrounded &&
        bobDistance < 1.5;
    const double measuredBobSpeed = canBob
        ? std::hypot(
              eventSnapshot.playerHorizontalVelocity.x,
              eventSnapshot.playerHorizontalVelocity.z)
        : 0.0;
    const double bobSpeedBlend =
        1.0 - std::exp(-12.0 * frameSeconds);
    cameraBobSpeed_ +=
        (measuredBobSpeed - cameraBobSpeed_) *
        bobSpeedBlend;
    const double targetBobAmount =
        canBob
            ? std::clamp(
                  (cameraBobSpeed_ - 0.2) / 4.2,
                  0.0, 1.0)
            : 0.0;
    const double bobAmountBlend =
        1.0 -
        std::exp(
            -(targetBobAmount > cameraBobAmount_
                  ? 11.0
                  : 8.0) *
            frameSeconds);
    cameraBobAmount_ +=
        (targetBobAmount - cameraBobAmount_) *
        bobAmountBlend;
    if (canBob) {
        cameraBobPhase_ +=
            cameraBobSpeed_ * frameSeconds *
            (sprinting ? 2.15 : 2.35);
    }
    audio_.update(eventSnapshot);
    updateHoverTarget(eventSnapshot, frameSeconds);
    if (buildingContextCardTarget_ &&
        (eventSnapshot.selectedBuilding ||
         eventSnapshot.aimedBuilding !=
             buildingContextCardTarget_)) {
        buildingContextCardTarget_.reset();
        buildingContextCardUpgradeCost_.reset();
        buildingContextCardStats_.reset();
    }
    if (eventSnapshot.buildingPreview) {
        modularPreviewVisualOrigin_.reset();
        const BuildingPreview& preview =
            *eventSnapshot.buildingPreview;
        const Vec3 target = buildingWorldPosition(
            preview.type, preview.gridPosition);
        const Vector2 targetCenter{
            static_cast<float>(target.x),
            static_cast<float>(target.z),
        };
        const bool newSelection =
            !placementPreviewCenter_ ||
            placementPreviewType_ != preview.type;
        const bool changedCell =
            placementPreviewGrid_ &&
            *placementPreviewGrid_ !=
                preview.gridPosition;
        if (newSelection) {
            placementPreviewCenter_ = targetCenter;
            placementRotationYaw_ =
                static_cast<double>(preview.rotation) *
                static_cast<double>(PI * 0.5F);
        } else {
            const float blend =
                1.0F -
                std::exp(
                    -18.0F *
                    static_cast<float>(frameSeconds));
            placementPreviewCenter_->x +=
                (targetCenter.x -
                 placementPreviewCenter_->x) *
                blend;
            placementPreviewCenter_->y +=
                (targetCenter.y -
                 placementPreviewCenter_->y) *
                blend;
            const double targetYaw =
                static_cast<double>(preview.rotation) *
                static_cast<double>(PI * 0.5F);
            const double deltaYaw = std::atan2(
                std::sin(targetYaw - placementRotationYaw_),
                std::cos(targetYaw - placementRotationYaw_));
            placementRotationYaw_ +=
                deltaYaw *
                static_cast<double>(blend);
        }
        if (changedCell && preview.placement.valid()) {
            placementSnapPulseRemaining_ = 0.18;
        }
        placementPreviewGrid_ = preview.gridPosition;
        placementPreviewType_ = preview.type;
    } else if (
        foundationBuildMode_ &&
        (platformFramePreview_ || wallPreview_ ||
         rampPreview_)) {
        const double cellSize =
            simulation_.terrain().config().cellSize;
        Vec3 targetOrigin{};
        if (platformFramePreview_) {
            targetOrigin = {
                (platformFramePreview_->anchor.x + 1.0) *
                    cellSize,
                platformFramePreview_->floorHeight,
                (platformFramePreview_->anchor.z + 1.0) *
                    cellSize,
            };
        } else if (wallPreview_) {
            targetOrigin = {
                (wallPreview_->anchor.x + 0.5) *
                    cellSize,
                wallPreview_->bottomHeight,
                (wallPreview_->anchor.z + 0.5) *
                    cellSize,
            };
        } else if (rampPreview_) {
            const bool alongZ =
                rampPreview_->rotation ==
                    Rotation::Deg0 ||
                rampPreview_->rotation ==
                    Rotation::Deg180;
            const int widthCells =
                alongZ ? ModularRampWidthCells
                       : ModularRampRunCells;
            const int depthCells =
                alongZ ? ModularRampRunCells
                       : ModularRampWidthCells;
            targetOrigin = {
                (rampPreview_->anchor.x +
                 widthCells * 0.5) *
                    cellSize,
                rampPreview_->bottomHeight,
                (rampPreview_->anchor.z +
                 depthCells * 0.5) *
                    cellSize,
            };
        }
        const double targetYaw =
            static_cast<double>(modularRotation_) *
            static_cast<double>(PI * 0.5F);
        const float blend =
            1.0F -
            std::exp(
                -18.0F *
                static_cast<float>(frameSeconds));
        if (!modularPreviewVisualOrigin_) {
            modularPreviewVisualOrigin_ =
                targetOrigin;
        } else {
            modularPreviewVisualOrigin_->x +=
                (targetOrigin.x -
                 modularPreviewVisualOrigin_->x) *
                blend;
            modularPreviewVisualOrigin_->y +=
                (targetOrigin.y -
                 modularPreviewVisualOrigin_->y) *
                blend;
            modularPreviewVisualOrigin_->z +=
                (targetOrigin.z -
                 modularPreviewVisualOrigin_->z) *
                blend;
        }
        const double deltaYaw = std::atan2(
            std::sin(targetYaw - placementRotationYaw_),
            std::cos(targetYaw - placementRotationYaw_));
        placementRotationYaw_ +=
            deltaYaw * static_cast<double>(blend);
        placementPreviewCenter_.reset();
        placementPreviewGrid_.reset();
        placementPreviewType_.reset();
    } else {
        modularPreviewVisualOrigin_.reset();
        placementPreviewCenter_.reset();
        placementPreviewGrid_.reset();
        placementPreviewType_.reset();
        placementSnapPulseRemaining_ = 0.0;
    }
    processPresentationEvents(events, eventSnapshot);
}

} // namespace ian
