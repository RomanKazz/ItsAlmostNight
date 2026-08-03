#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"

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
    if (skillTree_.update(static_cast<float>(frameSeconds))) {
        audio_.playUiConfirm();
    }
    const auto hotbarSnapshot = simulation_.snapshot();
    if (hotbarSnapshot.state != RunState::MainMenu &&
        hotbarSnapshot.state != RunState::Paused) {
        worldRevealElapsed_ = std::min(
            0.9,
            worldRevealElapsed_ + frameSeconds);
    }
    const float hotbarBlend =
        1.0F - std::exp(
                   -14.0F *
                   static_cast<float>(frameSeconds));
    const float buildingTarget = static_cast<float>(
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
    const float buildAlphaTarget =
        !foundationBuildMode_ &&
                hotbarSnapshot.selectedBuilding
            ? 1.0F
            : 0.0F;
    const float foundationAlphaTarget =
        foundationBuildMode_ ? 1.0F : 0.0F;
    buildHotbarSelectionAlpha_ +=
        (buildAlphaTarget - buildHotbarSelectionAlpha_) *
        hotbarBlend;
    foundationHotbarSelectionAlpha_ +=
        (foundationAlphaTarget -
         foundationHotbarSelectionAlpha_) *
        hotbarBlend;
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
    bool desiredToolUsesAxe = toolSwapWasActive
        ? toolSwapDestinationUsesAxe_
        : displayedToolUsesAxe_;
    if (renderer_ && renderer_->graphicsPanelVisible() &&
        graphicsPanelTab_ == 4) {
        desiredToolUsesAxe = toolPanelPreviewUsesAxe_;
    } else if (hotbarSnapshot.aimedResource) {
        const auto resource = std::find_if(
            hotbarSnapshot.resourceNodes.begin(),
            hotbarSnapshot.resourceNodes.end(),
            [&hotbarSnapshot](const ResourceNode& node) {
                return node.id == *hotbarSnapshot.aimedResource;
            });
        if (resource != hotbarSnapshot.resourceNodes.end()) {
            desiredToolUsesAxe =
                resource->type == ResourceType::Wood;
        }
    }
    if (desiredToolUsesAxe != toolSwapCandidateUsesAxe_) {
        toolSwapCandidateUsesAxe_ = desiredToolUsesAxe;
        toolSwapCandidateSeconds_ = 0.0;
    } else {
        toolSwapCandidateSeconds_ += frameSeconds;
    }
    if (toolSwapWasActive &&
        toolSwapRemaining_ <= toolSwapDuration_ * 0.5) {
        displayedToolUsesAxe_ =
            toolSwapDestinationUsesAxe_;
    }
    if (toolSwapRemaining_ <= 0.0 &&
        toolSwapCandidateSeconds_ >= 0.07 &&
        toolSwapCandidateUsesAxe_ !=
            displayedToolUsesAxe_) {
        toolSwapDestinationUsesAxe_ =
            toolSwapCandidateUsesAxe_;
        toolSwapDuration_ = std::max(
            static_cast<double>(toolTuning_.swapDuration),
            0.05);
        toolSwapRemaining_ = toolSwapDuration_;
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
        displayedToolUsesAxe_ == toolSwingUsesAxe_) {
        toolSwingDuration_ = toolTuning_.swingDuration;
        toolSwingRemaining_ = toolSwingDuration_;
        toolSwingAttackPending_ =
            toolQueuedSwingHasAttack_;
        toolQueuedSwingHasAttack_ = false;
        toolSwingQueued_ = false;
        toolQueuedResourceTarget_.reset();
        toolSwingQueueRemaining_ = 0.0;
    }
    const bool sprinting =
        acceptsGameplayInput(
            simulation_.snapshot().state) &&
        input_.sprint &&
        (std::abs(input_.moveForward) > 0.01 ||
         std::abs(input_.moveRight) > 0.01);
    const float targetFov = sprinting ? 79.0F : 75.0F;
    const float fovBlend =
        1.0F -
        std::exp(
            -7.5F * static_cast<float>(frameSeconds));
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
    for (GrassClearArea& area : grassClearAreas_) {
        area.amount = std::min(
            1.0F,
            area.amount +
                static_cast<float>(
                    resourceAnimationSeconds / 0.48));
    }
    if (!pendingBuildingPlacement_ &&
        !pendingWallPlacements_.empty()) {
        pendingBuildingPlacement_ =
            pendingWallPlacements_.front();
        pendingWallPlacements_.erase(
            pendingWallPlacements_.begin());
    }
    if (!pendingBuildingSale_ &&
        !queuedBuildingSales_.empty()) {
        pendingBuildingSale_ =
            queuedBuildingSales_.front();
        queuedBuildingSales_.erase(
            queuedBuildingSales_.begin());
        const auto removalSnapshot =
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
        queuedModularBuildingRemovals_.erase(
            queuedModularBuildingRemovals_.begin());
        const auto removalSnapshot =
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
    fixedStep_.advance(
        simulationFrameSeconds,
        [this, &consumedTransientInput](double deltaSeconds) {
        PlayerCommand tickInput = input_;
        if (!consumedTransientInput) {
            tickInput.lookYaw = pendingYaw_;
            tickInput.lookPitch = pendingPitch_;
            tickInput.jump = pendingJump_;
            tickInput.usePickaxe = pendingPickaxe_;
            tickInput.fireRifle = pendingRifleShot_;
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
            if (pendingWeaponToggle_) {
                tickInput.toggleWeapon = ToggleWeaponCommand{};
            }
            if (pendingWeaponUpgrade_) {
                tickInput.upgradeWeapon = UpgradeWeaponCommand{};
            }
            if (pendingBombThrow_) {
                tickInput.useConsumable = UseConsumableCommand{};
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
        simulation_.tick(deltaSeconds, tickInput);
        });
    if (consumedTransientInput) {
        pendingYaw_ = 0.0;
        pendingPitch_ = 0.0;
        pendingJump_ = false;
        pendingPickaxe_ = false;
        pendingRifleShot_ = false;
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
        pendingWeaponToggle_ = false;
        pendingWeaponUpgrade_ = false;
        pendingBombThrow_ = false;
        pendingDefeatAllEnemies_ = false;
        pendingToggleInvulnerability_ = false;
        pendingDamageCore_ = false;
        pendingSpawnEnemy_ = false;
        pendingGateToggle_.reset();
    }
    const auto events = simulation_.takeEvents();
    const auto eventSnapshot = simulation_.snapshot();
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
    const auto addProductionVisual =
        [this, &eventSnapshot](EntityId buildingId,
                               UiResourceIcon icon,
                               int amount) {
            const auto building = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [buildingId](const BuildingInstance& candidate) {
                    return candidate.id == buildingId;
                });
            if (building == eventSnapshot.buildings.end()) {
                return;
            }
            constexpr std::size_t MaximumVisuals = 12;
            if (productionVisuals_.size() >= MaximumVisuals) {
                productionVisuals_.erase(
                    productionVisuals_.begin());
            }
            constexpr double Duration = 0.95;
            Vec3 position = buildingWorldPosition(*building);
            position.y = 1.35;
            productionVisuals_.push_back({
                .buildingId = buildingId,
                .icon = icon,
                .position = position,
                .amount = amount,
                .remaining = Duration,
                .duration = Duration,
            });
        };
    for (const auto& event : events) {
        audio_.playEvent(event, eventSnapshot);
        if (event.type == GameEventType::CannonFired &&
            event.sourceId) {
            addBuildingShotRecoil(
                *event.sourceId, 0.18, 0.13F);
        } else if (
            event.type == GameEventType::ProjectileHit &&
            event.sourceId) {
            addBuildingShotRecoil(
                *event.sourceId, 0.12, 0.075F);
        }
        const bool playerHit =
            event.type == GameEventType::ResourceHit ||
            event.type == GameEventType::ResourceCollected ||
            event.type == GameEventType::PickaxeHit ||
            (event.type == GameEventType::ProjectileHit &&
             !event.sourceId);
        if (playerHit) {
            crosshairHitRemaining_ = crosshairHitDuration_;
            crosshairHitCritical_ = event.critical;
        }
        if (event.type == GameEventType::WeaponFired) {
            weaponRecoilDuration_ = 0.16;
            weaponRecoilRemaining_ = weaponRecoilDuration_;
            weaponRecoilStrength_ = 0.045F;
            addCameraImpulse({0.0, 0.008, -0.012});
        } else if (
            event.type == GameEventType::PickaxeHit ||
            event.type == GameEventType::ResourceHit ||
            event.type ==
                GameEventType::ResourceCollected) {
            weaponRecoilDuration_ = 0.12;
            weaponRecoilRemaining_ = weaponRecoilDuration_;
            weaponRecoilStrength_ =
                event.critical ? 0.035F : 0.024F;
            addCameraImpulse({0.0, -0.006, 0.008});
        }
        const bool resourceImpact =
            event.type == GameEventType::ResourceHit ||
            event.type == GameEventType::ResourceCollected;
        if (event.critical && !resourceImpact) {
            hitStopRemaining_ =
                std::max(hitStopRemaining_, 0.045);
        }
        if (event.type == GameEventType::ResourceHit) {
            toolContactHoldRemaining_ = std::max(
                toolContactHoldRemaining_, 0.025);
        } else if (event.type == GameEventType::ResourceCollected) {
            toolContactHoldRemaining_ = std::max(
                toolContactHoldRemaining_, 0.04);
        } else if (
            event.type == GameEventType::BuildingDestroyed ||
            event.type ==
                GameEventType::ModularBuildingDestroyed) {
            hitStopRemaining_ =
                std::max(hitStopRemaining_, 0.055);
        }
        if (event.type == GameEventType::BuildingRejected ||
            event.type ==
                GameEventType::BuildingUpgradeRejected ||
            event.type ==
                GameEventType::BuildingRepairRejected ||
            event.type ==
                GameEventType::BuildingSellRejected ||
            event.type ==
                GameEventType::WeaponUpgradeRejected ||
            event.type ==
                GameEventType::GateToggleRejected) {
            invalidActionRemaining_ = 0.22;
        }
        if (event.type == GameEventType::ProjectileHit &&
            event.sourceId) {
            const auto source = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [&event](const BuildingInstance& building) {
                    return building.id == *event.sourceId &&
                           building.type == BuildingType::Turret;
                });
            if (source != eventSnapshot.buildings.end()) {
                Vec3 origin = buildingWorldPosition(*source);
                origin.y = 1.4;
                const double deltaX = event.position.x - origin.x;
                const double deltaY = event.position.y - origin.y;
                const double deltaZ = event.position.z - origin.z;
                const double distance = std::sqrt(
                    deltaX * deltaX + deltaY * deltaY +
                    deltaZ * deltaZ);
                const double duration =
                    std::clamp(distance / 18.0, 0.08, 0.35);
                arrowVisuals_.push_back({
                    .origin = origin,
                    .target = event.position,
                    .remaining = duration,
                    .duration = duration,
                });
            }
        }
        if (event.type == GameEventType::ResourceHit) {
            addEffect(PresentationEffectType::Hit,
                      event.position, 0.34);
            if (event.resourceType) {
                addEffect(
                    *event.resourceType == ResourceType::Wood
                        ? PresentationEffectType::ResourceHitWood
                        : PresentationEffectType::ResourceHitStone,
                    event.position, 0.46,
                    event.critical ? 1.3F : 1.0F,
                    event.entityId);
            }
        } else if (
            event.type == GameEventType::ProjectileHit ||
            event.type == GameEventType::PickaxeHit) {
            addEffect(PresentationEffectType::Hit, event.position, 0.22);
        } else if (event.type == GameEventType::ResourceCollected) {
            if (event.resourceType) {
                const auto node = std::find_if(
                    eventSnapshot.resourceNodes.begin(),
                    eventSnapshot.resourceNodes.end(),
                    [&event](const ResourceNode& candidate) {
                        return event.entityId &&
                               candidate.id == *event.entityId;
                    });
                const Vec3 center =
                    node != eventSnapshot.resourceNodes.end()
                        ? node->position
                        : event.position;
                const auto type =
                    *event.resourceType == ResourceType::Wood
                        ? PresentationEffectType::ResourceDestroyedWood
                        : PresentationEffectType::ResourceDestroyedStone;
                addEffect(type, center, 0.92, 1.0F);
                destroyedResourceVisuals_.push_back({
                    .type = *event.resourceType,
                    .visualVariant =
                        node != eventSnapshot.resourceNodes.end()
                            ? static_cast<std::size_t>(
                                  node->id.index %
                                  TreeVisualVariantCount)
                            : 0U,
                    .visualYaw =
                        node != eventSnapshot.resourceNodes.end()
                            ? static_cast<float>(node->visualYaw)
                            : 0.0F,
                    .visualScale =
                        node != eventSnapshot.resourceNodes.end()
                            ? static_cast<float>(node->visualScale)
                            : 1.0F,
                    .position = center,
                    .remaining = 0.42,
                    .duration = 0.42,
                });
            }
        } else if (event.type == GameEventType::Explosion) {
            addEffect(PresentationEffectType::Explosion, event.position, 0.8);
            addCameraShake(0.25, 0.12);
            const double distance = std::hypot(
                event.position.x - eventSnapshot.playerPosition.x,
                event.position.z - eventSnapshot.playerPosition.z);
            const double proximity = std::clamp(
                1.0 - distance / 30.0, 0.0, 1.0);
            addCameraImpulse({0.0, 0.035 * proximity,
                              -0.025 * proximity});
        } else if (
            event.type == GameEventType::BuildingDestroyed ||
            event.type ==
                GameEventType::ModularBuildingDestroyed) {
            if (event.building) {
                constexpr double Duration = 0.52;
                const std::uint8_t connections =
                    event.building->type ==
                            BuildingType::Wall
                        ? wallConnectionMask(
                              eventSnapshot.buildings,
                              event.building
                                  ->gridPosition,
                              event.building
                                  ->baseHeight)
                        : 0U;
                soldBuildingVisuals_.push_back({
                    .building = *event.building,
                    .platformFrame = std::nullopt,
                    .modularWall = std::nullopt,
                    .ramp = std::nullopt,
                    .wallConnections = connections,
                    .remaining = Duration,
                    .duration = Duration,
                });
            }
            if (event.platformFrame || event.modularWall ||
                event.ramp) {
                constexpr double Duration = 0.52;
                soldBuildingVisuals_.push_back({
                    .building = std::nullopt,
                    .platformFrame = event.platformFrame,
                    .modularWall = event.modularWall,
                    .ramp = event.ramp,
                    .remaining = Duration,
                    .duration = Duration,
                });
            }
            addEffect(PresentationEffectType::Debris, event.position, 0.8);
            addCameraShake(0.18, 0.08);
        } else if (
            (event.type == GameEventType::BuildingDamaged ||
             event.type ==
                 GameEventType::ModularBuildingDamaged) &&
                   event.entityId) {
            addEffect(
                PresentationEffectType::BuildingDamaged,
                event.position, 0.3);
            recentlyDamagedBuilding_ = *event.entityId;
            damagedBuildingHealthBarRemaining_ = 1.6;
            const auto building = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [&event](const BuildingInstance& candidate) {
                    return candidate.id == *event.entityId;
                });
            const auto attacker =
                event.sourceId
                    ? std::find_if(
                          eventSnapshot.enemies.begin(),
                          eventSnapshot.enemies.end(),
                          [&event](const EnemyInstance& enemy) {
                              return enemy.id ==
                                     *event.sourceId;
                          })
                    : eventSnapshot.enemies.end();
            if (building != eventSnapshot.buildings.end() &&
                attacker != eventSnapshot.enemies.end()) {
                const Vec3 center =
                    buildingWorldPosition(*building);
                const double deltaX =
                    center.x - attacker->position.x;
                const double deltaZ =
                    center.z - attacker->position.z;
                const double length = std::sqrt(
                    deltaX * deltaX + deltaZ * deltaZ);
                if (length > 1e-6) {
                    constexpr double Duration = 0.28;
                    buildingImpactVisuals_.push_back({
                        .id = *event.entityId,
                        .direction = {
                            deltaX / length, 0.0,
                            deltaZ / length,
                        },
                        .remaining = Duration,
                        .duration = Duration,
                    });
                }
            }
        } else if (event.type == GameEventType::BossRamImpact) {
            addEffect(PresentationEffectType::RamImpact, event.position, 0.7);
            addCameraShake(0.35, 0.2);
        } else if (event.type == GameEventType::CoreDamaged) {
            addCameraShake(0.1, 0.04);
            addCameraImpulse({0.0, -0.008, 0.012});
        } else if (event.type == GameEventType::PlayerLanded) {
            landingResponseDuration_ = 0.24;
            landingResponseRemaining_ = landingResponseDuration_;
            landingResponseStrength_ = std::clamp(
                (event.intensity - 1.0) / 7.0,
                0.25, 1.0);
            addCameraImpulse({0.0,
                              -0.012 * landingResponseStrength_,
                              0.0});
        } else if (event.type == GameEventType::BuildingPlaced &&
                   event.buildingType) {
            grassClearAreas_.push_back({
                .center = {
                    static_cast<float>(event.position.x),
                    static_cast<float>(event.position.z),
                },
                .innerRadius = static_cast<float>(
                    buildingFootprintHalfExtent(
                        *event.buildingType) +
                    0.18),
                .amount = 0.0F,
            });
            float effectScale = 1.0F;
            if (*event.buildingType == BuildingType::Core) {
                effectScale = 1.45F;
            } else if (*event.buildingType == BuildingType::Wall ||
                       *event.buildingType == BuildingType::Gate) {
                effectScale = 0.78F;
            } else if (*event.buildingType ==
                       BuildingType::SlowTrap) {
                effectScale = 0.68F;
            }
            addEffect(PresentationEffectType::BuildingPlaced,
                      event.position, 0.7, effectScale,
                      event.entityId);
        } else if (event.type == GameEventType::BuildingUpgraded &&
                   event.buildingType) {
            if (event.entityId) {
                buildingStatsUpgradeEntity_ = *event.entityId;
                buildingStatsUpgradeRemaining_ =
                    buildingStatsUpgradeDuration_;
            }
            float effectScale = 1.0F;
            if (*event.buildingType == BuildingType::Core) {
                effectScale = 1.45F;
            } else if (*event.buildingType == BuildingType::Wall ||
                       *event.buildingType == BuildingType::Gate) {
                effectScale = 0.78F;
            } else if (*event.buildingType ==
                       BuildingType::SlowTrap) {
                effectScale = 0.68F;
            }
            effectScale *= 1.15F;
            addEffect(PresentationEffectType::BuildingUpgrade,
                      event.position, 0.85, effectScale);
        } else if (event.type ==
                       GameEventType::BuildingRepaired &&
                   event.entityId && event.buildingType) {
            addEffect(
                PresentationEffectType::BuildingRepaired,
                event.position, 0.62,
                static_cast<float>(
                    buildingFootprintHalfExtent(
                        *event.buildingType)));
            targetHealthBar_.notifyRepair(*event.entityId);
            recentlyDamagedBuilding_ = *event.entityId;
            damagedBuildingHealthBarRemaining_ = 1.25;
        } else if (
            event.type ==
                GameEventType::ModularBuildingRepaired &&
            event.entityId) {
            addEffect(
                PresentationEffectType::BuildingRepaired,
                event.position, 0.62, 1.0F);
            targetHealthBar_.notifyRepair(*event.entityId);
            recentlyDamagedBuilding_ = *event.entityId;
            damagedBuildingHealthBarRemaining_ = 1.25;
        } else if (event.type == GameEventType::BuildingSold &&
                   event.entityId) {
            if (pendingSoldBuildingVisual_ &&
                pendingSoldBuildingVisual_->id ==
                    *event.entityId) {
                constexpr double Duration = 0.52;
                soldBuildingVisuals_.push_back({
                    .building = *pendingSoldBuildingVisual_,
                    .platformFrame = std::nullopt,
                    .modularWall = std::nullopt,
                    .ramp = std::nullopt,
                    .wallConnections =
                        pendingSoldWallConnections_,
                    .remaining = Duration,
                    .duration = Duration,
                });
                addEffect(
                    PresentationEffectType::Debris,
                    event.position, 0.62,
                    static_cast<float>(
                        buildingFootprintHalfExtent(
                            pendingSoldBuildingVisual_->type)));
            }
            pendingSoldBuildingVisual_.reset();
            pendingSoldWallConnections_ = 0U;
        } else if (
            event.type ==
            GameEventType::BuildingSellRejected) {
            pendingSoldBuildingVisual_.reset();
            pendingSoldWallConnections_ = 0U;
        }
        if (event.type == GameEventType::ResourceHit ||
            event.type == GameEventType::ResourceCollected) {
            addCameraShake(0.1, 0.06);
            if (event.resourceType && event.amount > 0) {
                addResourceGainVisual(
                    *event.resourceType, event.position,
                    event.amount);
            }
        } else if (event.type == GameEventType::ResourceGranted &&
                   event.resourceType) {
            if (*event.resourceType == ResourceType::Wood) {
                woodHudBounceRemaining_ = 0.28;
            } else {
                stoneHudBounceRemaining_ = 0.28;
            }
            if (event.entityId && event.buildingType &&
                (*event.buildingType ==
                     BuildingType::LumberMill ||
                 *event.buildingType ==
                     BuildingType::Quarry) &&
                event.amount > 0) {
                addProductionVisual(
                    *event.entityId,
                    *event.resourceType == ResourceType::Wood
                        ? UiResourceIcon::Wood
                        : UiResourceIcon::Stone,
                    event.amount);
            }
        } else if (
            event.type == GameEventType::GoldProduced &&
            event.entityId && event.amount > 0) {
            addProductionVisual(
                *event.entityId, UiResourceIcon::Crystal,
                event.amount);
        } else if (event.type == GameEventType::PickaxeHit) {
            Vec3 numberPosition = event.position;
            numberPosition.y += 1.2;
            addFloatingDamageNumber(
                numberPosition, event.damage, event.critical);
        }
        constexpr std::size_t MaxDestroyedEnemyVisuals =
            192U;
        if (event.type == GameEventType::EnemyKilled &&
            event.entityId &&
            destroyedEnemyVisuals_.size() <
                MaxDestroyedEnemyVisuals) {
            const auto enemy = std::find_if(
                eventSnapshot.enemies.begin(),
                eventSnapshot.enemies.end(),
                [&event](const EnemyInstance& candidate) {
                    return candidate.id == *event.entityId;
                });
            if (enemy != eventSnapshot.enemies.end()) {
                constexpr double DeathDuration = 0.9;
                destroyedEnemyVisuals_.push_back({
                    .type = enemy->type,
                    .position = enemy->position,
                    .yaw = enemy->yaw,
                    .remaining = DeathDuration,
                    .duration = DeathDuration,
                });
            }
        }
        if (event.type == GameEventType::PlayerDamaged) {
            addDamageIndicator(event.position, eventSnapshot, false);
            playerDamageFlashRemaining_ = 0.18;
            const double deltaX =
                event.position.x - eventSnapshot.playerPosition.x;
            const double deltaZ =
                event.position.z - eventSnapshot.playerPosition.z;
            const double length = std::hypot(deltaX, deltaZ);
            if (length > 1e-6) {
                const double rightX =
                    std::cos(eventSnapshot.playerYaw);
                const double rightZ =
                    std::sin(eventSnapshot.playerYaw);
                addCameraImpulse({
                    -(deltaX * rightX + deltaZ * rightZ) /
                        length * 0.025,
                    0.012,
                    0.018,
                });
            }
        } else if (event.type == GameEventType::CoreDamaged) {
            addDamageIndicator(event.position, eventSnapshot, false);
        } else if (event.type == GameEventType::BossRamImpact) {
            addDamageIndicator(event.position, eventSnapshot, true);
        }

        std::string message;
        if (event.type == GameEventType::BuildingUpgraded && event.buildingType) {
            message =
                std::string(buildingDisplayName(
                    *event.buildingType)) +
                " upgraded";
        } else if (event.type == GameEventType::BuildingUpgradeRejected &&
                   event.upgradeError) {
            message = upgradeErrorMessage(*event.upgradeError);
        } else if (event.type == GameEventType::BuildingRepaired && event.buildingType) {
            message =
                std::string(buildingDisplayName(
                    *event.buildingType)) +
                " repaired";
        } else if (
            event.type ==
            GameEventType::ModularBuildingRepaired) {
            message = "Structure repaired";
        } else if ((event.type == GameEventType::BuildingRepairRejected ||
                    event.type == GameEventType::BuildingSellRejected) &&
                   event.buildingActionError) {
            message = buildingActionErrorMessage(*event.buildingActionError);
        } else if (event.type == GameEventType::BuildingSold && event.buildingType) {
            message =
                std::string(buildingDisplayName(
                    *event.buildingType)) +
                " sold";
        } else if (event.type == GameEventType::WeaponUpgraded) {
            message = "Rifle upgraded to level " + std::to_string(event.amount);
        } else if (event.type == GameEventType::WeaponUpgradeRejected &&
                   event.weaponUpgradeError) {
            message = weaponUpgradeErrorMessage(*event.weaponUpgradeError);
        } else if (event.type == GameEventType::GateToggleRejected) {
            message = "Gate blocked or not under crosshair";
        } else if (event.type == GameEventType::PlayerDied) {
            message = event.amount > 0
                          ? "You died: " +
                                std::to_string(event.amount) +
                                " resources lost"
                          : "You died";
        } else if (event.type == GameEventType::PlayerRespawned) {
            message = "Respawned at Core";
        } else if (event.type == GameEventType::WaveRewardGranted) {
            message =
                "Night cleared: +" +
                std::to_string(event.amount) +
                " Crystals";
        }
        if (!message.empty()) {
            statusMessage_ = std::move(message);
            statusMessageRemaining_ = 2.5;
        }
    }
}

void App::updateHoverTarget(const SimulationSnapshot& snapshot,
                            double frameSeconds) {
    constexpr double HoverGraceSeconds = 0.2;
    const auto clearHover = [this]() {
        hoveredResource_.reset();
        hoveredBuilding_.reset();
        hoveredEnemy_.reset();
        hoveredBuildingUpgradeCost_.reset();
        hoveredBuildingStats_.reset();
        hoverGraceRemaining_ = 0.0;
        buildingHoverSeconds_ = 0.0;
    };
    if (snapshot.selectedBuilding) {
        clearHover();
        return;
    }

    if (snapshot.aimedResource) {
        hoveredResource_ = snapshot.aimedResource;
        hoveredBuilding_.reset();
        hoveredEnemy_.reset();
        hoveredBuildingUpgradeCost_.reset();
        hoveredBuildingStats_.reset();
        buildingHoverSeconds_ = 0.0;
        hoverGraceRemaining_ = HoverGraceSeconds;
        return;
    }
    if (snapshot.aimedBuilding) {
        if (hoveredBuilding_ == snapshot.aimedBuilding) {
            buildingHoverSeconds_ += frameSeconds;
        } else {
            buildingHoverSeconds_ = frameSeconds;
        }
        hoveredResource_.reset();
        hoveredBuilding_ = snapshot.aimedBuilding;
        hoveredEnemy_.reset();
        hoveredBuildingUpgradeCost_ =
            snapshot.aimedBuildingUpgradeCost;
        hoveredBuildingStats_ =
            snapshot.aimedBuildingStats;
        if (buildingContextCardTarget_ ==
            snapshot.aimedBuilding) {
            buildingContextCardUpgradeCost_ =
                snapshot.aimedBuildingUpgradeCost;
            buildingContextCardStats_ =
                snapshot.aimedBuildingStats;
        }
        hoverGraceRemaining_ = HoverGraceSeconds;
        return;
    }
    if (snapshot.aimedEnemy) {
        hoveredResource_.reset();
        hoveredBuilding_.reset();
        hoveredEnemy_ = snapshot.aimedEnemy;
        hoveredBuildingUpgradeCost_.reset();
        hoveredBuildingStats_.reset();
        buildingHoverSeconds_ = 0.0;
        hoverGraceRemaining_ = HoverGraceSeconds;
        return;
    }

    hoverGraceRemaining_ =
        std::max(0.0, hoverGraceRemaining_ - frameSeconds);
    const bool resourceValid =
        !hoveredResource_ ||
        std::any_of(
            snapshot.resourceNodes.begin(), snapshot.resourceNodes.end(),
            [this](const ResourceNode& resource) {
                return resource.active &&
                       resource.id == *hoveredResource_;
            });
    const bool buildingValid =
        !hoveredBuilding_ ||
        std::any_of(
            snapshot.buildings.begin(), snapshot.buildings.end(),
            [this](const BuildingInstance& building) {
                return building.id == *hoveredBuilding_;
            });
    const bool enemyValid =
        !hoveredEnemy_ ||
        std::any_of(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [this](const EnemyInstance& enemy) {
                return enemy.active && enemy.id == *hoveredEnemy_;
            });
    if (hoverGraceRemaining_ <= 0.0 || !resourceValid ||
        !buildingValid || !enemyValid) {
        clearHover();
    }
}

void App::addEffect(PresentationEffectType type, Vec3 position,
                    double duration, float scale,
                    std::optional<EntityId> entityId,
                    double startDelay) {
    constexpr std::size_t MaxEffects = 128;
    if (effects_.size() >= MaxEffects) {
        effects_.erase(effects_.begin());
    }
    effects_.push_back({
        .type = type,
        .entityId = entityId,
        .position = position,
        .remaining = duration,
        .duration = duration,
        .startDelayRemaining =
            std::max(0.0, startDelay),
        .scale = scale,
    });
}

void App::addCameraShake(double duration, double strength) {
    cameraShakeRemaining_ = std::max(cameraShakeRemaining_, duration);
    cameraShakeStrength_ = std::max(cameraShakeStrength_, strength);
}

void App::addCameraImpulse(Vec3 localOffset) {
    cameraImpulseOffset_.x = std::clamp(
        cameraImpulseOffset_.x + localOffset.x,
        -0.06, 0.06);
    cameraImpulseOffset_.y = std::clamp(
        cameraImpulseOffset_.y + localOffset.y,
        -0.07, 0.07);
    cameraImpulseOffset_.z = std::clamp(
        cameraImpulseOffset_.z + localOffset.z,
        -0.06, 0.06);
}

void App::addDamageIndicator(Vec3 sourcePosition,
                             const SimulationSnapshot& snapshot, bool severe) {
    const double offsetX = sourcePosition.x - snapshot.playerPosition.x;
    const double offsetZ = sourcePosition.z - snapshot.playerPosition.z;
    const double worldAngle = std::atan2(offsetX, -offsetZ);
    const double relativeAngle =
        std::atan2(std::sin(worldAngle - snapshot.playerYaw),
                   std::cos(worldAngle - snapshot.playerYaw));
    for (auto& indicator : damageIndicators_) {
        const double difference =
            std::atan2(std::sin(indicator.relativeAngle - relativeAngle),
                       std::cos(indicator.relativeAngle - relativeAngle));
        if (std::abs(difference) < 0.2) {
            indicator.relativeAngle = relativeAngle;
            indicator.severe = indicator.severe || severe;
            indicator.duration = indicator.severe ? 1.4 : 1.0;
            indicator.remaining = indicator.duration;
            return;
        }
    }
    constexpr std::size_t MaxDamageIndicators = 12;
    if (damageIndicators_.size() >= MaxDamageIndicators) {
        damageIndicators_.erase(damageIndicators_.begin());
    }
    const double duration = severe ? 1.4 : 1.0;
    damageIndicators_.push_back({
        .relativeAngle = relativeAngle,
        .remaining = duration,
        .duration = duration,
        .severe = severe,
    });
}

void App::addFloatingDamageNumber(
    Vec3 position, double damage, bool critical) {
    constexpr std::size_t MaximumNumbers = 32;
    if (floatingDamageNumbers_.size() >= MaximumNumbers) {
        floatingDamageNumbers_.erase(
            floatingDamageNumbers_.begin());
    }
    constexpr double Duration = 0.85;
    const float direction =
        (floatingDamageNumbers_.size() % 2U) == 0U ? -1.0F : 1.0F;
    floatingDamageNumbers_.push_back({
        .position = position,
        .damage = damage,
        .remaining = Duration,
        .duration = Duration,
        .horizontalDrift = direction * (critical ? 22.0F : 14.0F),
        .critical = critical,
    });
}

void App::addResourceGainVisual(
    ResourceType type, Vec3 position, int amount) {
    constexpr std::size_t MaximumVisuals = 16;
    if (resourceGainVisuals_.size() >= MaximumVisuals) {
        resourceGainVisuals_.erase(resourceGainVisuals_.begin());
    }
    constexpr double Duration = ResourcePickupFlightSeconds;
    resourceGainVisuals_.push_back({
        .type = type,
        .position = position,
        .amount = amount,
        .remaining = Duration,
        .duration = Duration,
    });
}

} // namespace ian
