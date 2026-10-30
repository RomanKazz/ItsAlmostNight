#include "ui/UiViewport.hpp"
#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "graphics/WorldTransforms.hpp"
#include "localization/Localization.hpp"
#include "ui/UiText.hpp"
#include "ui/UiHintStyle.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace ian {
namespace {

void drawFpsCounter(float opacity = 1.0F) {
    constexpr float FontSize = 16.0F;
    constexpr float HorizontalPadding = 10.0F;
    constexpr float VerticalPadding = 5.0F;
    constexpr float Top = 66.0F;
    const std::string_view label{TextFormat("%d FPS", GetFPS())};
    const Vector2 textSize = measureUiText(label, FontSize);
    const float screenMinimum = static_cast<float>(
        std::min(uiScreenWidth(), uiScreenHeight()));
    const float minimapSize = std::clamp(
        screenMinimum * 0.16F, 120.0F, 156.0F);
    constexpr float MinimapRightMargin = 12.0F;
    constexpr float CompassMargin = 32.0F;
    constexpr float Gap = 18.0F;
    const float minimapOuterLeft = static_cast<float>(uiScreenWidth()) -
        MinimapRightMargin - CompassMargin * 2.0F - minimapSize;
    const Rectangle bounds{
        minimapOuterLeft - Gap - textSize.x -
            HorizontalPadding * 2.0F,
        Top,
        textSize.x + HorizontalPadding * 2.0F,
        textSize.y + VerticalPadding * 2.0F,
    };
    DrawRectangleRounded(bounds, 0.22F, 6, Fade({7, 10, 14, 215}, opacity));
    DrawRectangleRoundedLinesEx(
        bounds, 0.22F, 6, 1.0F, Fade({103, 139, 130, 220}, opacity));
    drawUiText(
        label,
        {bounds.x + HorizontalPadding,
         bounds.y + VerticalPadding},
        FontSize, Fade({235, 245, 238, 255}, opacity));
}

} // namespace

using namespace app_detail;

void App::render() {
    renderer_->beginFrame();
    beginUiTextPerformanceFrame();
    if (feedback_.active() && feedback_.ready()) {
        renderer_->beginOverlayPass();
        drawFeedback();
        renderer_->endFrame(false);
        return;
    }
    renderer_->setGpuProfilingEnabled(
        renderer_->settings().performanceLogging ||
        performanceOverlayVisible_);
    const auto renderPreparationStart = PerformanceClock::now();
    RenderFrameTimings timings{
        .uiStart = renderPreparationStart,
    };
    auto& uiStart = timings.uiStart;
    auto& viewModelMilliseconds = timings.viewModelMilliseconds;
    auto& worldUiMilliseconds = timings.worldUiMilliseconds;
    auto& interactionPromptMilliseconds =
        timings.interactionPromptMilliseconds;
    auto& hudCoreMilliseconds = timings.hudCoreMilliseconds;
    auto& hudTargetHealthMilliseconds =
        timings.hudTargetHealthMilliseconds;
    auto& hudResourcesMilliseconds = timings.hudResourcesMilliseconds;
    auto& hudVitalityMilliseconds = timings.hudVitalityMilliseconds;
    auto& hudInformationMilliseconds =
        timings.hudInformationMilliseconds;
    auto& hudObjectivesMilliseconds = timings.hudObjectivesMilliseconds;
    auto& hudLootMilliseconds = timings.hudLootMilliseconds;
    auto& hudPhaseMilliseconds = timings.hudPhaseMilliseconds;
    auto& hudHotbarMilliseconds = timings.hudHotbarMilliseconds;
    auto& hudFeedbackMilliseconds = timings.hudFeedbackMilliseconds;
    auto& hudTextFlushMilliseconds = timings.hudTextFlushMilliseconds;
    auto& resourceGainMilliseconds = timings.resourceGainMilliseconds;
    auto& minimapMilliseconds = timings.minimapMilliseconds;
    auto& minimapSetupMilliseconds = timings.minimapSetupMilliseconds;
    auto& minimapTargetRefreshMilliseconds =
        timings.minimapTargetRefreshMilliseconds;
    auto& minimapFrameMilliseconds = timings.minimapFrameMilliseconds;
    auto& minimapLabelsMilliseconds = timings.minimapLabelsMilliseconds;
    auto& uiOverlaysMilliseconds = timings.uiOverlaysMilliseconds;
    // Freeze the simulation immediately, but capture its original presentation
    // before drawing a new pause menu or the feedback controls.
    std::optional<SimulationSnapshot> captureSnapshot;
    if (feedback_.needsCapture() && resumeAfterFeedback_) {
        captureSnapshot = simulation_.snapshot();
        captureSnapshot->state = captureSnapshot->stateBeforePause;
    }
    const auto& snapshot = captureSnapshot ? *captureSnapshot : simulation_.snapshot();
    std::optional<EntityId> pinnedOrdinaryBuilding;
    std::optional<EntityId> pinnedModularBuilding;
    if (buildingContextCardTarget_ && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        buildingContextCardTarget_.reset();
        buildingContextCardStats_.reset();
        buildingContextCardUpgradeCost_.reset();
    }
    if (buildingContextCardTarget_) {
        // Refresh live values while holding, including upgrades made with U.
        if (snapshot.aimedBuilding == buildingContextCardTarget_) {
            buildingContextCardStats_ = snapshot.aimedBuildingStats;
            buildingContextCardUpgradeCost_ = snapshot.aimedBuildingUpgradeCost;
        }
        const EntityId target = *buildingContextCardTarget_;
        if (std::ranges::any_of(
                snapshot.buildings,
                [target](const BuildingInstance& building) {
                    return building.id == target;
                })) {
            pinnedOrdinaryBuilding = target;
        } else if (
            std::ranges::any_of(
                snapshot.platformFrames,
                [target](const PlatformFrameInstance& frame) {
                    return frame.id == target;
                }) ||
            std::ranges::any_of(
                snapshot.modularWalls,
                [target](const WallInstance& wall) {
                    return wall.id == target;
                }) ||
            std::ranges::any_of(
                snapshot.ramps,
                [target](const RampInstance& ramp) {
                    return ramp.id == target;
                })) {
            pinnedModularBuilding = target;
        } else {
            buildingContextCardTarget_.reset();
            buildingContextCardUpgradeCost_.reset();
            buildingContextCardStats_.reset();
        }
    }
    if (buildingContextCardTarget_) {
        const double frameSeconds = static_cast<double>(
            std::clamp(GetFrameTime(), 0.0F, 0.10F));
        if (buildingContextCardAnimatedTarget_ != buildingContextCardTarget_) {
            buildingContextCardAnimatedTarget_ = buildingContextCardTarget_;
            buildingContextCardEntranceSeconds_ = frameSeconds;
        } else {
            buildingContextCardEntranceSeconds_ = std::min(
                0.20, buildingContextCardEntranceSeconds_ + frameSeconds);
        }
    } else {
        buildingContextCardAnimatedTarget_.reset();
        buildingContextCardEntranceSeconds_ = 0.0;
    }
    std::vector<EntityId> structuralRiskRoots;
    const auto addStructuralRiskRoot =
        [&structuralRiskRoots](EntityId support) {
            if (std::ranges::find(
                    structuralRiskRoots, support) ==
                structuralRiskRoots.end()) {
                structuralRiskRoots.push_back(support);
            }
        };
    if (removalDragActive_ &&
        !foundationBuildMode_ &&
        !snapshot.selectedBuilding &&
        snapshot.aimedModularBuilding &&
        std::ranges::any_of(
            snapshot.platformFrames,
            [&snapshot](const PlatformFrameInstance& frame) {
                return frame.id ==
                       *snapshot.aimedModularBuilding;
            })) {
        addStructuralRiskRoot(
            *snapshot.aimedModularBuilding);
    }
    for (const EntityId target : removalDragTargets_) {
        if (std::ranges::any_of(
                snapshot.platformFrames,
                [target](const PlatformFrameInstance& frame) {
                    return frame.id == target;
                })) {
            addStructuralRiskRoot(target);
        }
    }
    const std::uint64_t structuralRevision =
        simulation_.structuralRevision();
    if (!structuralRiskCacheValid_ ||
        structuralRevision != structuralRiskCacheRevision_ ||
        structuralRiskRoots != structuralRiskCacheRoots_) {
        if (structuralRiskRoots.empty()) {
            structuralRiskIds_.clear();
        } else {
            structuralRiskIds_ = simulation_.structuralCollapseRisk(structuralRiskRoots);
        }
        structuralRiskCacheRoots_ = std::move(structuralRiskRoots);
        structuralRiskCacheRevision_ = structuralRevision;
        structuralRiskCacheValid_ = true;
    }
    auto presentationSnapshot = snapshot;
    // Keep the presentation target sticky for world/HUD context while the
    // interaction feedback below follows the exact current aim.
    presentationSnapshot.aimedResource = hoveredResource_
        ? hoveredResource_ : snapshot.aimedResource;
    presentationSnapshot.aimedBuilding = hoveredBuilding_
        ? hoveredBuilding_ : snapshot.aimedBuilding;
    presentationSnapshot.aimedEnemy = hoveredEnemy_
        ? hoveredEnemy_ : snapshot.aimedEnemy;
    presentationSnapshot.aimedBuildingUpgradeCost =
        hoveredBuildingUpgradeCost_;
    presentationSnapshot.aimedBuildingStats =
        hoveredBuildingStats_;
    if (pinnedOrdinaryBuilding) {
        presentationSnapshot.aimedBuilding =
            pinnedOrdinaryBuilding;
        presentationSnapshot.aimedModularBuilding.reset();
        presentationSnapshot.aimedBuildingUpgradeCost =
            buildingContextCardUpgradeCost_;
        presentationSnapshot.aimedBuildingStats =
            buildingContextCardStats_;
    } else if (pinnedModularBuilding) {
        presentationSnapshot.aimedBuilding.reset();
        presentationSnapshot.aimedModularBuilding =
            pinnedModularBuilding;
    }
    if (snapshot.selectedBuilding || foundationBuildMode_) {
        presentationSnapshot.aimedBuilding.reset();
        presentationSnapshot.aimedModularBuilding.reset();
    }
    if (presentationSnapshot.aimedEnemy &&
        !pinnedOrdinaryBuilding && !pinnedModularBuilding) {
        presentationSnapshot.aimedResource.reset();
        presentationSnapshot.aimedBuilding.reset();
        presentationSnapshot.aimedModularBuilding.reset();
    }
    // Selection feedback and tactical overlays must use the same presented
    // target. Otherwise the outline disappears on the exact ray miss while
    // the range indicator remains visible during hover grace.
    auto feedbackSnapshot = snapshot;
    feedbackSnapshot.aimedEnemy = hoveredEnemy_
        ? hoveredEnemy_ : snapshot.aimedEnemy;
    feedbackSnapshot.aimedBuilding =
        presentationSnapshot.aimedBuilding;
    feedbackSnapshot.aimedModularBuilding =
        presentationSnapshot.aimedModularBuilding;
    feedbackSnapshot.aimedBuildingUpgradeCost =
        snapshot.aimedBuildingUpgradeCost;
    feedbackSnapshot.aimedBuildingStats =
        snapshot.aimedBuildingStats;
    if (snapshot.selectedBuilding || foundationBuildMode_) {
        feedbackSnapshot.aimedBuilding.reset();
        feedbackSnapshot.aimedModularBuilding.reset();
    }
    if (feedbackSnapshot.aimedEnemy &&
        !pinnedOrdinaryBuilding && !pinnedModularBuilding) {
        feedbackSnapshot.aimedResource.reset();
        feedbackSnapshot.aimedBuilding.reset();
        feedbackSnapshot.aimedModularBuilding.reset();
    }

    if (snapshot.state == RunState::MainMenu) {
        interactionPromptRenderer_.reset();
        drawMainMenuWorld(snapshot);
        drawMainMenu();
    } else {
        renderGameplayFrame(
            snapshot, presentationSnapshot, feedbackSnapshot,
            pinnedOrdinaryBuilding, pinnedModularBuilding,
            renderPreparationStart, timings);
    }

    {
        const ScopedUiCanvas canvas;
        const auto commonUiOverlaysStart = PerformanceClock::now();
        if (renderer_->graphicsPanelVisible()) {
            drawGraphicsPanel();
        }
        drawEnemySpawnMenu();
        drawItemGrantMenu();
        drawSandboxCardMenu(snapshot);
        drawCoreDefenseMenu();
        drawObjectiveDebugMenu(snapshot);
        if(questCompletionToastVisible() && !questCompletionNotices_.empty() && questCompletionRemaining_>0)
            drawQuestCompletionToast(ui_,questCompletionNotices_.front(),questCompletionRemaining_,
                userSettings_.accessibility.reduceFlashes,
                {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())});
        const float restartProgress = static_cast<float>(restartHold_.progress());
        if (restartProgress > 0.15F && snapshot.state != RunState::MainMenu) {
            const float width = 220.0F;
            const float left = (static_cast<float>(uiScreenWidth()) - width) * 0.5F;
            const float top = static_cast<float>(uiScreenHeight()) * 0.12F;
            const float opacity = std::clamp((restartProgress - 0.15F) * 10.0F, 0.0F, 1.0F);
            hintStyle::panel({left-16,top-12,width+32,68},.8F,opacity);
            hintStyle::text(userSettings_.language==Language::Russian ? "ПЕРЕЗАПУСК" : "RESTART",
                {left,top,width,28},20,menuStyle::Cream,UiFontRole::Display,opacity,1);
            DrawRectangleRec({left, top + 30, width, 6}, Fade({70, 80, 73, 255}, opacity));
            DrawRectangleRec({left, top + 30, width * restartProgress, 6},
                Fade({255, 204, 0, 255}, opacity));
        }
        if (!hudHidden_ && !playerArrival_.active() && performanceOverlayVisible_) {
            drawPerformanceOverlay(snapshot);
        }
        if (!hudHidden_ && playerArrival_.hudOpacity() > 0.0F && userSettings_.accessibility.showFps) {
            drawFpsCounter(playerArrival_.hudOpacity());
        }
        const bool uiCursorVisible =
            (snapshot.state == RunState::MainMenu &&
             !menuBackgroundEditing_) ||
            snapshot.state == RunState::Paused ||
            snapshot.state == RunState::StageClear ||
            snapshot.state == RunState::Victory || snapshot.runShop.pending || snapshot.runUpgradeChoicePending ||
            renderer_->graphicsPanelVisible() || enemySpawnMenuVisible_ ||
            itemGrantMenuVisible_ || sandboxCardMenuVisible_ ||
            (coreDefenseMenuVisible_ && coreMenuPointerActive_);
        if (uiCursorVisible) {
            HideCursor();
            ui_.drawCursor();
        }
        uiOverlaysMilliseconds +=
            performanceMilliseconds(commonUiOverlaysStart);
    }
    if (snapshot.state != RunState::MainMenu) {
        renderer_->endGpuTimer(RendererGpuStage::Ui);
        performanceStats_.viewModelRender.sample(
            viewModelMilliseconds);
        performanceStats_.worldUiRender.sample(
            worldUiMilliseconds);
        performanceStats_.interactionPromptRender.sample(
            interactionPromptMilliseconds);
        performanceStats_.hudCoreRender.sample(
            hudCoreMilliseconds);
        performanceStats_.hudTargetHealthRender.sample(
            hudTargetHealthMilliseconds);
        performanceStats_.hudResourcesRender.sample(
            hudResourcesMilliseconds);
        performanceStats_.hudVitalityRender.sample(
            hudVitalityMilliseconds);
        performanceStats_.hudInformationRender.sample(
            hudInformationMilliseconds);
        performanceStats_.hudObjectivesRender.sample(
            hudObjectivesMilliseconds);
        performanceStats_.hudLootRender.sample(
            hudLootMilliseconds);
        performanceStats_.hudPhaseRender.sample(
            hudPhaseMilliseconds);
        performanceStats_.hudHotbarRender.sample(
            hudHotbarMilliseconds);
        performanceStats_.hudFeedbackRender.sample(
            hudFeedbackMilliseconds);
        performanceStats_.hudTextFlushRender.sample(
            hudTextFlushMilliseconds);
        performanceStats_.resourceGainRender.sample(
            resourceGainMilliseconds);
        performanceStats_.minimapRender.sample(
            minimapMilliseconds);
        performanceStats_.minimapSetupRender.sample(
            minimapSetupMilliseconds);
        performanceStats_.minimapTargetRefreshRender.sample(
            minimapTargetRefreshMilliseconds);
        performanceStats_.minimapFrameRender.sample(
            minimapFrameMilliseconds);
        performanceStats_.minimapLabelsRender.sample(
            minimapLabelsMilliseconds);
        performanceStats_.uiOverlaysRender.sample(
            uiOverlaysMilliseconds);
        performanceStats_.uiTextRender.sample(
            uiTextPerformanceStats().milliseconds);
        performanceStats_.uiRender.sample(
            performanceMilliseconds(uiStart) + timings.uiPreparationMilliseconds);
    } else {
        performanceStats_.renderPreparation.sample(
            performanceMilliseconds(renderPreparationStart));
    }
    if (feedback_.needsCapture()) {
        captureFeedbackFrame();
        if (feedback_.ready()) drawFeedback();
    }
    const auto presentStart = PerformanceClock::now();
    renderer_->endFrame();
    performanceStats_.present.sample(
        performanceMilliseconds(presentStart));
}


} // namespace ian
