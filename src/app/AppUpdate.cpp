#include "app/App.hpp"
#include "app/ActionModeEquipment.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/BuildingOrientation.hpp"
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
#include <ranges>
#include <string>
#include <vector>

namespace ian {
using namespace app_detail;

void App::update() {
    const double frameSeconds = static_cast<double>(GetFrameTime());
    const RunState currentRunState = simulation_.snapshot().state;
    if ((currentRunState == RunState::Victory ||
         currentRunState == RunState::Defeat) &&
        suspendedRunAvailable()) {
        discardSuspendedRun();
    }
    if (renderer_ &&
        renderer_->settings().fullscreen != fullscreenApplied_) {
        applyFullscreenSetting(renderer_->settings().fullscreen);
    }
    if (pendingLootGrant_) {
        for (int index = 0; index < pendingLootGrant_->count; ++index) {
            simulation_.grantLootUpgrade(
                pendingLootGrant_->effect,
                pendingLootGrant_->rarity);
        }
        pendingLootGrant_.reset();
    }
    if (pendingSandboxCardGrant_) {
        const bool granted = simulation_.grantDebugRunUpgrade(
            *pendingSandboxCardGrant_);
        statusMessage_ = granted
            ? "SANDBOX CARD GRANTED"
            : "CARD ALREADY ACTIVE";
        statusMessageRemaining_ = 1.4;
        if (granted) {
            audio_.playUiConfirm();
        } else {
            audio_.playUiError();
        }
        pendingSandboxCardGrant_.reset();
    }
    const auto& progressionSnapshot = simulation_.snapshot();
    if(progressionSnapshot.runShop.pending && !progressionSnapshot.runUpgradeChoicePending &&
        progressionSnapshot.state!=RunState::Paused) runShopOverlayEntranceSeconds_+=frameSeconds;
    else if(!progressionSnapshot.runShop.pending) runShopOverlayEntranceSeconds_=0;
    if (progressionSnapshot.runUpgradeChoicePending) {
        runUpgradeOverlayEntranceSeconds_ = std::min(
            1.5, runUpgradeOverlayEntranceSeconds_ + frameSeconds);
        const auto hovered = hoveredRunUpgradeChoice(
            progressionSnapshot, GetMousePosition());
        const float hoverBlend = static_cast<float>(
            1.0 - std::exp(-12.0 * frameSeconds));
        for (std::size_t index = 0;
             index < runUpgradeCardHoverAmounts_.size(); ++index) {
            const float target = hovered && *hovered == index
                ? 1.0F : 0.0F;
            runUpgradeCardHoverAmounts_[index] +=
                (target - runUpgradeCardHoverAmounts_[index]) *
                hoverBlend;
        }
    }
    insightPulseRemaining_ = std::max(0.0, insightPulseRemaining_ - frameSeconds);
    insightPointSequenceRemaining_ = std::max(
        0.0, insightPointSequenceRemaining_ - frameSeconds);
    objectivePulseRemaining_ = std::max(
        0.0, objectivePulseRemaining_ - frameSeconds);
    objectiveCompletionRemaining_ = std::max(
        0.0, objectiveCompletionRemaining_ - frameSeconds);
    if (objectiveCompletionRemaining_ <= 0.0) {
        if (!pendingObjectiveCompletionNotices_.empty()) {
            ObjectiveCompletionNotice notice = std::move(
                pendingObjectiveCompletionNotices_.front());
            pendingObjectiveCompletionNotices_.pop_front();
            objectiveCompletionTitle_ = std::move(notice.title);
            objectiveCompletionReward_ = notice.reward;
            objectiveCompletionRemaining_ =
                objectiveCompletionDuration_;
        } else {
            objectiveCompletionTitle_.clear();
            objectiveCompletionReward_ = 0;
        }
    }
    insightGainRemaining_ = std::max(
        0.0, insightGainRemaining_ - frameSeconds);
    if (insightGainRemaining_ <= 0.0) {
        insightGainAmount_ = 0.0;
    }
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
    std::string trackedObjectiveId;
    double trackedObjectiveProgress = 0.0;
    if (progressionSnapshot.tutorialObjective) {
        trackedObjectiveId = "tutorial:" + std::to_string(
            static_cast<int>(*progressionSnapshot.tutorialObjective));
        if (*progressionSnapshot.tutorialObjective ==
            TutorialObjective::MineWood) {
            trackedObjectiveProgress =
                static_cast<double>(progressionSnapshot.wood);
        } else if (*progressionSnapshot.tutorialObjective ==
                   TutorialObjective::PlaceCore) {
            trackedObjectiveProgress =
                progressionSnapshot.coreId ? 1.0 : 0.0;
        }
    } else {
        for (const int index :
             progressionSnapshot.recommendedObjectives) {
            if (index < 0 ||
                static_cast<std::size_t>(index) >=
                    progressionSnapshot.objectives.size()) {
                continue;
            }
            const ObjectiveStatus& objective =
                progressionSnapshot.objectives[
                    static_cast<std::size_t>(index)];
            if (!objective.active || objective.completed) {
                continue;
            }
            trackedObjectiveId = objective.definition.id;
            trackedObjectiveProgress = objective.progress;
            break;
        }
    }
    if (trackedObjectiveId.empty()) {
        objectiveTrackerId_.clear();
        objectiveTrackerProgress_ = 0.0;
        objectiveTrackerExpandedRemaining_ = 0.0;
    } else if (trackedObjectiveId != objectiveTrackerId_) {
        objectiveTrackerId_ = std::move(trackedObjectiveId);
        objectiveTrackerProgress_ = trackedObjectiveProgress;
        objectiveTrackerExpandedRemaining_ = 4.0;
    } else {
        if (trackedObjectiveProgress >
            objectiveTrackerProgress_ + 1e-6) {
            objectiveTrackerExpandedRemaining_ = 3.0;
        }
        objectiveTrackerProgress_ = trackedObjectiveProgress;
        objectiveTrackerExpandedRemaining_ = std::max(
            0.0,
            objectiveTrackerExpandedRemaining_ - frameSeconds);
    }
    const float trackerExpansionTarget =
        objectiveTrackerExpandedRemaining_ > 0.0 ? 1.0F : 0.0F;
    const float trackerExpansionBlend =
        1.0F - std::exp(
            -10.0F * static_cast<float>(frameSeconds));
    objectiveTrackerExpansion_ +=
        (trackerExpansionTarget - objectiveTrackerExpansion_) *
        trackerExpansionBlend;
    if (displayedInsight_ < 0.0) displayedInsight_ = progressionSnapshot.currentInsight;
    const double insightPointProgress =
        presentation::timelineProgress(
            insightPointSequenceRemaining_,
            insightPointSequenceDuration_);
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
    const auto updateHudResourceCounter =
        [frameSeconds](HudResourceCounter& counter, int target) {
            if (counter.target < 0) {
                counter.displayed = static_cast<double>(target);
                counter.target = target;
                return;
            } else if (target != counter.target) {
                constexpr double AnimationSeconds = 0.22;
                constexpr double MinimumUnitsPerSecond = 24.0;
                constexpr double SequentialChangeLimit = 10.0;
                const double change = std::abs(
                    static_cast<double>(target) -
                    counter.displayed);
                counter.unitsPerSecond = std::max(
                    MinimumUnitsPerSecond,
                    change / AnimationSeconds);
                counter.sequential =
                    change <= SequentialChangeLimit;
            }
            counter.target = target;
            if (counter.displayed < static_cast<double>(target)) {
                double step = frameSeconds * counter.unitsPerSecond;
                if (counter.sequential) {
                    // Keep every integer visible for at least one frame.
                    step = std::min(step, 1.0);
                }
                step = std::min(
                    static_cast<double>(target) - counter.displayed,
                    step);
                counter.displayed += std::max(0.0, step);
            } else if (counter.displayed > static_cast<double>(target)) {
                double step = frameSeconds * counter.unitsPerSecond;
                if (counter.sequential) {
                    // Keep every integer visible for at least one frame.
                    step = std::min(step, 1.0);
                }
                step = std::min(
                    counter.displayed - static_cast<double>(target),
                    step);
                counter.displayed -= std::max(0.0, step);
            }
        };
    updateHudResourceCounter(
        woodHudCounter_, progressionSnapshot.wood);
    updateHudResourceCounter(
        stoneHudCounter_, progressionSnapshot.stone);
    updateHudResourceCounter(
        crystalHudCounter_, progressionSnapshot.crystals);
    updateHudResourceCounter(
        coinHudCounter_, progressionSnapshot.coins);
    const auto& hotbarSnapshot = simulation_.snapshot();
    nightForecast_.update(hotbarSnapshot, frameSeconds);
    const bool resetPlayerHealthTrail =
        displayedPlayerHealthTrail_ < 0.0 ||
        std::abs(displayedPlayerHealthMaximum_ -
                 hotbarSnapshot.playerMaxHealth) > 1e-6 ||
        hotbarSnapshot.state == RunState::MainMenu ||
        hotbarSnapshot.playerRespawning ||
        hotbarSnapshot.playerHealth >= displayedPlayerHealthTrail_;
    displayedPlayerHealthMaximum_ = hotbarSnapshot.playerMaxHealth;
    if (resetPlayerHealthTrail) {
        displayedPlayerHealthTrail_ = hotbarSnapshot.playerHealth;
    } else if (playerDamageFlashRemaining_ <= 0.0) {
        const double healthTrailBlend =
            1.0 - std::exp(-3.2 * frameSeconds);
        displayedPlayerHealthTrail_ +=
            (hotbarSnapshot.playerHealth -
             displayedPlayerHealthTrail_) * healthTrailBlend;
        if (displayedPlayerHealthTrail_ -
                hotbarSnapshot.playerHealth < 0.05) {
            displayedPlayerHealthTrail_ = hotbarSnapshot.playerHealth;
        }
    }
    const bool lowHealthActive =
        hotbarSnapshot.state != RunState::MainMenu &&
        hotbarSnapshot.state != RunState::Defeat &&
        !hotbarSnapshot.playerRespawning &&
        hotbarSnapshot.playerHealth > 0.0 &&
        hotbarSnapshot.playerMaxHealth > 0.0;
    const double healthRatio = lowHealthActive
        ? hotbarSnapshot.playerHealth /
              hotbarSnapshot.playerMaxHealth
        : 1.0;
    const float lowHealthTarget = static_cast<float>(
        std::clamp(
            (0.35 - healthRatio) / 0.25,
            0.0, 1.0));
    const float lowHealthBlend =
        1.0F - std::exp(
            -(lowHealthTarget > lowHealthEffect_
                  ? 7.5F
                  : 4.0F) *
            static_cast<float>(frameSeconds));
    lowHealthEffect_ +=
        (lowHealthTarget - lowHealthEffect_) *
        lowHealthBlend;
    renderer_->setLowHealthEffect(
        lowHealthEffect_,
        userSettings_.accessibility.reduceFlashes);
    if (hotbarSnapshot.state == RunState::MainMenu) {
        playerArrival_.reset();
    }
    if (playerArrival_.active() &&
        hotbarSnapshot.state != RunState::MainMenu &&
        hotbarSnapshot.state != RunState::Paused &&
        !hotbarSnapshot.runUpgradeChoicePending &&
        playerArrival_.advance(frameSeconds)) {
        Vec3 landingPosition = hotbarSnapshot.playerPosition;
        landingPosition.y = simulation_.terrain().getHeight(
                                landingPosition.x,
                                landingPosition.z) + 0.04;
        addEffect(PresentationEffectType::PlayerArrivalDust,
                  landingPosition, 0.62, 1.0F);
        addCameraShake(0.18, 0.025);
        audio_.playEvent({.type = GameEventType::PlayerLanded,
                         .position = landingPosition,
                         .intensity = 8.0}, hotbarSnapshot);
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
    outgoingPreviewRemaining_ = std::max(
        0.0, outgoingPreviewRemaining_ - frameSeconds);
    if (outgoingPreviewRemaining_ <= 0.0) {
        outgoingBuildingPreview_.reset();
        outgoingPreviewCenter_.reset();
        outgoingPlatformPreview_.reset();
        outgoingWallPreview_.reset();
        outgoingRampPreview_.reset();
        outgoingModularPreviewOrigin_.reset();
    }
    hotbarModeBlend_ = std::clamp(
        hotbarModeBlend_ +
            (actionMode_ == ActionMode::Buildings ? 1.0F : -1.0F) *
                static_cast<float>(frameSeconds / 0.28),
        0.0F, 1.0F);
    const BuildingType highlightedBuilding =
        hotbarSnapshot.selectedBuilding.value_or(lastBuildingSelection_);
    const ConstructionHotbarLayout constructionLayout =
        makeConstructionHotbarLayout(
            hotbarSnapshot.unlockedBuildings,
            hotbarSnapshot.coreMaxHealth > 0.0,
            hotbarSnapshot.coreLevel,
            hotbarSnapshot.sandboxMode ||
                hotbarSnapshot.unlimitedResources);
    const float constructionTarget = static_cast<float>(
        foundationBuildMode_
            ? constructionLayout.indexOf(modularBuildPiece_)
                  .value_or(0U)
            : constructionLayout.indexOf(highlightedBuilding)
                  .value_or(0U));
    foundationHotbarSelectionPosition_ +=
        (constructionTarget -
         foundationHotbarSelectionPosition_) *
        hotbarBlend;
    const float weaponTarget = static_cast<float>(
        playerWeaponVisibleHotbarIndex(
            hotbarSnapshot.selectedWeapon,
            hotbarSnapshot.unlockedWeapons));
    weaponHotbarSelectionPosition_ +=
        (weaponTarget - weaponHotbarSelectionPosition_) *
        hotbarBlend;
    const bool constructionHotbarActive =
        actionMode_ == ActionMode::Buildings;
    const float buildAlphaTarget = constructionHotbarActive ? 1.0F : 0.0F;
    const float foundationAlphaTarget = buildAlphaTarget;
    buildHotbarSelectionPosition_ = foundationHotbarSelectionPosition_;
    const float weaponAlphaTarget =
        actionMode_ == ActionMode::Equipment ? 1.0F : 0.0F;
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
    const bool toolContactHoldActive =
        toolContactHoldRemaining_ > 0.0;
    toolContactHoldRemaining_ = std::max(
        0.0,
        toolContactHoldRemaining_ - frameSeconds);
    crosshairHitRemaining_ =
        std::max(0.0, crosshairHitRemaining_ - frameSeconds);
    invalidActionRemaining_ =
        std::max(0.0, invalidActionRemaining_ - frameSeconds);
    modularPlacementFailureRepeatWindow_ = std::max(
        0.0,
        modularPlacementFailureRepeatWindow_ - frameSeconds);
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
            (toolContactHoldActive
                 ? 0.0
                 : frameSeconds));
    const bool alternateHarvestContact =
        toolSwingAlternateAnimation_ &&
        (displayedToolVisual_ == FirstPersonToolVisual::Axe ||
         displayedToolVisual_ == FirstPersonToolVisual::Pickaxe ||
         displayedToolVisual_ == FirstPersonToolVisual::Club);
    const double toolContactProgress = alternateHarvestContact
        ? 27.0 / 60.0
        : std::clamp(
              static_cast<double>(activeToolTuning().hitProgress),
              0.25, 0.65);
    const double toolContactRemaining =
        toolSwingDuration_ * (1.0 - toolContactProgress);
    if (toolSwingAttackPending_ &&
        previousToolSwingRemaining > toolContactRemaining &&
        toolSwingRemaining_ <= toolContactRemaining) {
        pendingPickaxe_ = true;
        toolSwingAttackPending_ = false;
    }
    if (toolSwingAttackPending_ &&
        previousToolSwingRemaining > 0.0 &&
        toolSwingRemaining_ <= 0.0) {
        // Never lose an attack if tuning or a long frame skips the authored
        // contact point.
        pendingPickaxe_ = true;
        toolSwingAttackPending_ = false;
    }
    toolSwingQueueRemaining_ = std::max(
        0.0, toolSwingQueueRemaining_ - frameSeconds);
    if (toolSwingQueueRemaining_ <= 0.0) {
        if (toolSwingQueued_ && toolQueuedSwingHasAttack_) {
            // A view-model transition must not permanently swallow gameplay
            // input. The simulation still applies its normal cooldown and
            // input buffer before accepting this fallback request.
            pendingPickaxe_ = true;
        }
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
        desiredToolVisual = toolPanelPreviewVisual_;
    } else if (actionMode_ == ActionMode::Buildings) {
        desiredToolVisual = FirstPersonToolVisual::None;
    } else if (contextualHammerRemaining_ > 0.0) {
        desiredToolVisual = FirstPersonToolVisual::Hammer;
    } else {
        desiredToolVisual = toolVisualForWeapon(
            hotbarSnapshot.selectedWeapon);
    }
    contextualHammerRemaining_ = std::max(
        0.0, contextualHammerRemaining_ - frameSeconds);
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
                static_cast<double>(activeToolTuning().swapDuration) *
                    ToolSwapDurationScale,
                0.30, 0.58);
            toolSwapRemaining_ = toolSwapDuration_;
        }
    }
    if (contextualHammerSwingPending_ &&
        displayedToolVisual_ == FirstPersonToolVisual::Hammer &&
        toolSwapRemaining_ <= 0.0 &&
        toolSwingRemaining_ <= 0.0) {
        toolSwingDuration_ = activeToolTuning().swingDuration;
        toolSwingRemaining_ = toolSwingDuration_;
        toolSwingAttackPending_ = false;
        contextualHammerSwingPending_ = false;
        contextualHammerRemaining_ = std::max(
            contextualHammerRemaining_,
            toolSwingDuration_ + 0.18);
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
        toolSwingDuration_ = toolSwingDurationFor(
            hotbarSnapshot, displayedToolVisual_);
        toolSwingRemaining_ = toolSwingDuration_;
        chooseHarvestSwingAnimation(displayedToolVisual_);
        if (displayedToolVisual_ == FirstPersonToolVisual::Axe ||
            displayedToolVisual_ == FirstPersonToolVisual::Pickaxe ||
            displayedToolVisual_ == FirstPersonToolVisual::Club) {
            audio_.playHarvestSwing();
        }
        toolSwingAttackPending_ =
            toolQueuedSwingHasAttack_;
        toolQueuedSwingHasAttack_ = false;
        toolSwingQueued_ = false;
        toolQueuedResourceTarget_.reset();
        toolSwingQueueRemaining_ = 0.0;
    }
    const SimulationSnapshot& movementSnapshot =
        simulation_.snapshot();
    const float targetFov = movementSnapshot.dashing ? 88.0F : 75.0F;
    const float fovBlend =
        1.0F -
        std::exp(
            -(movementSnapshot.dashing ? 24.0F : 7.5F) *
            static_cast<float>(frameSeconds));
    if (!movementSnapshot.runUpgradeChoicePending) {
        cameraFov_ +=
            (targetFov - cameraFov_) * fovBlend;
    }
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
        !movementSnapshot.runUpgradeChoicePending &&
                acceptsGameplayInput(frameState)
            ? frameSeconds
            : 0.0;
    statusMessageRemaining_ =
        std::max(0.0, statusMessageRemaining_ - frameSeconds);
    statusMessageShakeRemaining_ = std::max(
        0.0, statusMessageShakeRemaining_ - frameSeconds);
    lootDescriptionRemaining_ =
        std::max(0.0, lootDescriptionRemaining_ - frameSeconds);
    cameraShakeRemaining_ = std::max(0.0, cameraShakeRemaining_ - frameSeconds);
    explosionShakeCooldownRemaining_ = std::max(
        0.0, explosionShakeCooldownRemaining_ - frameSeconds);
    landingResponseRemaining_ = std::max(
        0.0, landingResponseRemaining_ - frameSeconds);
    if (movementSnapshot.state != RunState::Paused) {
        playerRespawnRevealRemaining_ = std::max(
            0.0, playerRespawnRevealRemaining_ - frameSeconds);
    }
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
    crystalHudBounceRemaining_ =
        std::max(0.0, crystalHudBounceRemaining_ - frameSeconds);
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
    // Tower damage is simulated when the shot fires, while its visible
    // projectile reaches the enemy only at the end of this timeline.
    for (const ArrowVisual& arrow : arrowVisuals_) {
        const double activeSeconds = std::max(
            0.0, frameSeconds - arrow.startDelayRemaining);
        if (arrow.flashTarget && activeSeconds > 0.0 &&
            arrow.remaining <= activeSeconds) {
            startEnemyHitFlash(*arrow.flashTarget);
        }
    }
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
    // Enemy death is a presentation animation, not gameplay state. It must
    // finish during the post-night upgrade overlay and dawn instead of
    // leaving the final enemy frozen on screen.
    const double enemyDeathAnimationSeconds =
        frameState == RunState::Paused ? 0.0 : frameSeconds;
    presentation::advanceTimeline(
        destroyedEnemyVisuals_,
        enemyDeathAnimationSeconds);
    presentation::advanceTimeline(
        soldBuildingVisuals_, resourceAnimationSeconds);
    presentation::advanceTimeline(
        buildingImpactVisuals_, frameSeconds);
    presentation::advanceTimeline(
        buildingShotRecoilVisuals_, frameSeconds);
    presentation::advanceTimeline(
        cancelledPlacementPreview_, frameSeconds);
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
        frameSeconds;
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
            tickInput.placeBuildingBatch.assign(pendingWallPlacements_.begin(), pendingWallPlacements_.end());
            tickInput.rotateBuilding = pendingBuildingRotation_;
            if (pendingStartWave_) {
                tickInput.startWaveEarly = StartWaveEarlyCommand{};
            }
            if (pendingUnlimitedResources_) {
                tickInput.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
            }
            tickInput.upgradeBuilding = pendingBuildingUpgrade_;
            tickInput.upgradeBuildingBlueprint =
                pendingBuildingBlueprintUpgrade_;
            tickInput.repairBuilding = pendingBuildingRepair_;
            if (pendingPurchaseBombBundle_) {
                tickInput.purchaseBombBundle = PurchaseBombBundleCommand{};
            }
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
            if (pendingRevealChest_) {
                tickInput.revealNearestChest =
                    RevealNearestChestCommand{};
            }
            if (pendingDefeatAllEnemies_) {
                tickInput.defeatAllEnemies = DefeatAllEnemiesCommand{};
            }
            if (pendingToggleInvulnerability_) {
                tickInput.toggleInvulnerability = ToggleInvulnerabilityCommand{};
            }
            if (pendingGrantInsight_) {
                tickInput.grantInsight = GrantInsightCommand{};
            }
            if (pendingDamageCore_) {
                tickInput.damageCore = DamageCoreCommand{};
            }
            if (pendingSpawnEnemy_) {
                tickInput.spawnEnemy = SpawnEnemyCommand{
                    .type = debugSpawnType_,
                    .count = debugSpawnCount_,
                    .eliteAffixes = eliteAffixMask(
                        debugSpawnEliteAffix_),
                };
            }
            if (pendingChainLightning_) {
                tickInput.castChainLightning =
                    CastChainLightningCommand{
                        .firstTarget =
                            simulation_.snapshot().aimedEnemy,
                    };
            }
            tickInput.toggleGate = pendingGateToggle_;
            tickInput.rotatePlacedBuilding =
                pendingPlacedBuildingRotation_;
            consumedTransientInput = true;
        }
        if (playerArrival_.active()) {
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
        pendingWallPlacements_.clear();
        pendingBuildingRotation_ = 0;
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
    }
    simulation_.takeEvents(gameEventBuffer_);
    const std::span<const GameEvent> events = gameEventBuffer_;
    const auto& eventSnapshot = simulation_.snapshot();
    recordMetaProgression(events, eventSnapshot);
    if (actionMode_ == ActionMode::Buildings &&
        lastBuildingSelection_ == BuildingType::Core &&
        !foundationBuildMode_ && eventSnapshot.coreId) {
        selectConstructionHotbarItem(eventSnapshot, 0U);
    }
    if (selectBuildingAfterCorePlacement_) {
        if (actionMode_ != ActionMode::Buildings) {
            selectBuildingAfterCorePlacement_ = false;
        } else if (eventSnapshot.selectedBuilding ||
                   foundationBuildMode_) {
            selectBuildingAfterCorePlacement_ = false;
        } else {
            const ConstructionHotbarLayout layout = makeConstructionHotbarLayout(
                eventSnapshot.unlockedBuildings, true, eventSnapshot.coreLevel,
                eventSnapshot.sandboxMode || eventSnapshot.unlimitedResources);
            if (layout.count > 0U) {
                pendingBuildingCancel_ = false;
                selectConstructionHotbarItem(eventSnapshot,
                    layout.indexOf(lastBuildingSelection_).value_or(0U));
            }
        }
    }
    refreshDecorationExclusions(eventSnapshot);
    for (const BuildingInstance& building :
         eventSnapshot.buildings) {
        if (building.type != BuildingType::Gate) {
            continue;
        }
        const float target = building.open ? 1.0F : 0.0F;
        const auto directionAwayFromPlayer = [&]() {
            const Vec3 center = buildingWorldPosition(building);
            const float yaw =
                static_cast<float>(building.rotation) *
                PI * 0.5F;
            const double offsetX =
                eventSnapshot.playerPosition.x - center.x;
            const double offsetZ =
                eventSnapshot.playerPosition.z - center.z;
            const double playerLocalZ =
                offsetX * std::sin(yaw) +
                offsetZ * std::cos(yaw);
            // A negative angle swings the free edge towards local +Z.
            return playerLocalZ <= 0.0 ? 1.0F : -1.0F;
        };
        const auto visual = std::find_if(
            gateDoorVisuals_.begin(), gateDoorVisuals_.end(),
            [&building](const GateDoorVisual& candidate) {
                return candidate.id == building.id;
            });
        if (visual == gateDoorVisuals_.end()) {
            gateDoorVisuals_.push_back({
                building.id, target,
                building.open
                    ? directionAwayFromPlayer()
                    : 1.0F,
                building.open});
            continue;
        }
        if (building.open && !visual->wasOpen &&
            visual->openAmount <= 0.01F) {
            visual->openDirection =
                directionAwayFromPlayer();
        }
        visual->wasOpen = building.open;
        const float blend = 1.0F - std::exp(
            -13.0F * static_cast<float>(frameSeconds));
        visual->openAmount +=
            (target - visual->openAmount) * blend;
        if (std::abs(visual->openAmount - target) < 0.001F) {
            visual->openAmount = target;
        }
    }
    std::erase_if(
        gateDoorVisuals_,
        [&eventSnapshot](const GateDoorVisual& visual) {
            return std::ranges::none_of(
                eventSnapshot.buildings,
                [&visual](const BuildingInstance& building) {
                    return building.id == visual.id &&
                           building.type == BuildingType::Gate;
                });
        });
    // The attackless flamethrower has no combat rig yet. Keep its manual
    // turns in presentation so world, shadow, selection and aim share a pose.
    std::erase_if(flamethrowerRotationVisuals_, [&eventSnapshot](const auto& visual) {
        return std::ranges::none_of(eventSnapshot.buildings, [&visual](const auto& building) {
            return building.id == visual.id && building.type == BuildingType::Flamethrower;
        });
    });
    for (const auto& building : eventSnapshot.buildings) {
        if (building.type != BuildingType::Flamethrower) continue;
        const double targetYaw = buildingRotationYaw(building.type, building.rotation);
        const auto visual = std::ranges::find(
            flamethrowerRotationVisuals_, building.id, &FlamethrowerRotationVisual::id);
        if (visual == flamethrowerRotationVisuals_.end()) {
            flamethrowerRotationVisuals_.push_back({building.id, targetYaw});
        } else {
            visual->yaw = smoothBuildingAngle(visual->yaw, targetYaw, frameSeconds);
        }
    }
    if (eventSnapshot.runUpgradeChoicePending) {
        if (!groundCameraSmoothingInitialized_) {
            smoothedGroundCameraY_ =
                eventSnapshot.playerPosition.y;
            groundCameraSmoothingInitialized_ = true;
        }
    } else if (!groundCameraSmoothingInitialized_ ||
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
        !eventSnapshot.runUpgradeChoicePending &&
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
            cameraBobSpeed_ * frameSeconds * 2.35;
    }
    audio_.update(eventSnapshot);
    updateHoverTarget(eventSnapshot, frameSeconds);
    if (buildingContextCardTarget_ &&
        (eventSnapshot.selectedBuilding || foundationBuildMode_)) {
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
                buildingRotationYaw(
                    preview.type, preview.rotation);
            placementPreviewEntranceSeconds_ = 0.0;
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
                buildingRotationYaw(
                    preview.type, preview.rotation);
            placementRotationYaw_ = smoothBuildingAngle(
                placementRotationYaw_, targetYaw,
                frameSeconds);
        }
        placementPreviewEntranceSeconds_ = std::min(
            0.5,
            placementPreviewEntranceSeconds_ + frameSeconds);
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
        placementRotationYaw_ = smoothBuildingAngle(
            placementRotationYaw_, targetYaw,
            frameSeconds);
        placementPreviewCenter_.reset();
        placementPreviewGrid_.reset();
        placementPreviewType_.reset();
        placementPreviewEntranceSeconds_ = std::min(
            0.5, placementPreviewEntranceSeconds_ + frameSeconds);
    } else {
        modularPreviewVisualOrigin_.reset();
        placementPreviewCenter_.reset();
        placementPreviewGrid_.reset();
        placementPreviewType_.reset();
        placementPreviewEntranceSeconds_ = 0.0;
        placementSnapPulseRemaining_ = 0.0;
    }
    processPresentationEvents(events, eventSnapshot);
}

} // namespace ian
