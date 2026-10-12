#include "ui/BossHealthBar.hpp"
#include "ui/UiRenderLayer.hpp"
#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "graphics/WorldTransforms.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

namespace ian {
namespace {

const char* modularPlacementMessage(
    std::optional<ModularPlacementError> error) {
    if (!error) {
        return "AIM AT TERRAIN";
    }
    switch (*error) {
    case ModularPlacementError::None:
        return "LMB BUILD";
    case ModularPlacementError::Occupied:
        return "PLACE OCCUPIED";
    case ModularPlacementError::OutOfBounds:
        return "OUTSIDE MAP";
    case ModularPlacementError::TooFar:
        return "TOO FAR";
    case ModularPlacementError::SupportTooLong:
        return "SUPPORTS TOO LONG";
    case ModularPlacementError::PlayerOverlap:
        return "PLAYER IN THE WAY";
    case ModularPlacementError::TerrainIntersection:
        return "TERRAIN INTERSECTION";
    case ModularPlacementError::MaximumStorey:
        return "MAXIMUM STOREY";
    case ModularPlacementError::NoSupport:
        return "NO STRUCTURAL SUPPORT";
    case ModularPlacementError::ResourceBlocked:
        return "CLEAR RESOURCE FIRST";
    case ModularPlacementError::InsufficientResources:
        return "NOT ENOUGH RESOURCES";
    case ModularPlacementError::CoreLevelRequired:
        return "UPGRADE CORE TO LEVEL 2";
    case ModularPlacementError::OutsideCoreArea:
        return "OUTSIDE CORE RADIUS";
    }
    return "CANNOT BUILD";
}

void drawModularPlacementError(
    std::optional<ModularPlacementError> error,
    float y, float fontSize, float horizontalOffset) {
    const char* message = modularPlacementMessage(error);
    SdfUiTextStyle style{};
    style.fillColor = {255, 112, 94, 255};
    style.outlineColor = {76, 40, 33, 255};
    style.shadowColor = {43, 25, 21, 225};
    style.shadowOffsetPixels = {0.0F, 3.0F};
    style.outlinePixels = 2.0F;
    style.softness = 0.65F;
    const float messageWidth = measureUiText(
        message, fontSize).x;
    drawStyledSdfUiText(
        message,
        {(static_cast<float>(GetScreenWidth()) - messageWidth) *
                 0.5F +
             horizontalOffset,
         y},
        fontSize, style);
}

void drawDragPlacementSummary(
    GameUi& ui, Texture2D primaryIcon, int primaryCount,
    Texture2D secondaryIcon, int secondaryCount,
    const ResourceCost& totalCost, float y,
    bool affordable) {
    constexpr float CountFontSize = 21.0F;
    constexpr float PieceIconSize = 42.0F;
    constexpr float CostFontSize = 17.0F;
    constexpr float CostIconSize = 28.0F;
    constexpr float TextIconGap = 5.0F;
    constexpr float PieceGap = 18.0F;
    constexpr float CostRowOffset = 47.0F;

    const auto entryWidth = [](Texture2D icon, int count) {
        if (count <= 0) return 0.0F;
        const std::string text = std::to_string(count) + " x";
        return measureUiText(text, CountFontSize).x +
            (IsTextureValid(icon)
                 ? TextIconGap + PieceIconSize
                 : 0.0F);
    };
    const float primaryWidth =
        entryWidth(primaryIcon, primaryCount);
    const float secondaryWidth =
        entryWidth(secondaryIcon, secondaryCount);
    const float piecesWidth = primaryWidth +
        (secondaryCount > 0
             ? PieceGap + secondaryWidth
             : 0.0F);
    const float costWidth = hud_detail::resourceCostRowWidth(
        totalCost, CostFontSize, CostIconSize);
    const float screenCenter =
        static_cast<float>(GetScreenWidth()) * 0.5F;
    float x = screenCenter - piecesWidth * 0.5F;

    SdfUiTextStyle countStyle{};
    countStyle.fillColor = {255, 235, 184, 255};
    countStyle.outlineColor = {49, 49, 49, 255};
    countStyle.shadowColor = {49, 35, 28, 235};
    countStyle.shadowOffsetPixels = {0.0F, 2.5F};
    countStyle.outlinePixels = 1.7F;
    const auto drawPiece = [&](Texture2D icon, int count) {
        const std::string text = std::to_string(count) + " x";
        drawStyledSdfUiText(
            text, {x, y + 8.0F}, CountFontSize,
            countStyle);
        x += measureUiText(text, CountFontSize).x;
        if (IsTextureValid(icon)) {
            x += TextIconGap;
            DrawTexturePro(
                icon,
                {0.0F, 0.0F,
                 static_cast<float>(icon.width),
                 static_cast<float>(icon.height)},
                {x, y - 2.0F,
                 PieceIconSize, PieceIconSize},
                {}, 0.0F, WHITE);
            x += PieceIconSize;
        }
    };

    drawPiece(primaryIcon, primaryCount);
    if (secondaryCount > 0) {
        x += PieceGap;
        drawPiece(secondaryIcon, secondaryCount);
    }
    hud_detail::drawResourceCostRow(
        ui, totalCost,
        {screenCenter - costWidth * 0.5F,
         y + CostRowOffset},
        CostFontSize, CostIconSize,
        affordable
            ? Color{226, 218, 197, 245}
            : Color{246, 112, 94, 255});
}

} // namespace

using namespace app_detail;

void App::renderGameplayUi(
    const SimulationSnapshot& snapshot,
    const SimulationSnapshot& presentationSnapshot,
    const SimulationSnapshot& feedbackSnapshot,
    const SimulationSnapshot& healthBarSnapshot,
    const Camera3D& camera, const WorldLighting& lighting,
    std::optional<EntityId> pinnedOrdinaryBuilding,
    std::optional<EntityId> pinnedModularBuilding,
    RenderFrameTimings& timings) {
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
uiStart = PerformanceClock::now();
const float arrivalHudOpacity = playerArrival_.hudOpacity();
const bool showGameplayHud = !hudHidden_ && arrivalHudOpacity > 0.0F;
const bool showWorldHud = showGameplayHud && arrivalHudOpacity >= 1.0F;

const bool tuningPreview =
    renderer_->graphicsPanelVisible() &&
    graphicsPanelTab_ == ToolSettingsTab;
const bool showFirstPersonTool =
    (tuningPreview ||
     displayedToolVisual_ != FirstPersonToolVisual::None) &&
    (tuningPreview || actionModeUsesEquipment(actionMode_) ||
     toolSwapRemaining_ > 0.0) &&
    showGameplayHud &&
    (tuningPreview || actionMode_ == ActionMode::Buildings ||
     toolSwapRemaining_ > 0.0 ||
     (!snapshot.selectedBuilding && !foundationBuildMode_)) &&
    !snapshot.playerRespawning;
const auto viewModelStart = PerformanceClock::now();
if (showFirstPersonTool) {
    FirstPersonToolVisual toolVisual =
        displayedToolVisual_;
    const float swingProgress =
        toolSwingRemaining_ > 0.0 &&
                toolSwingDuration_ > 0.0
            ? std::clamp(
                  static_cast<float>(
                      1.0 - toolSwingRemaining_ /
                                toolSwingDuration_),
                  0.0F, 1.0F)
            : 0.0F;
    const bool fireWand =
        toolVisual == FirstPersonToolVisual::FireWand;
    const bool elementalWand = fireWand ||
        toolVisual == FirstPersonToolVisual::IceWand;
    const double wandChargeDuration = fireWand
        ? snapshot.fireWandChargeDuration
        : snapshot.iceWandChargeDuration;
    const double wandChargeRemaining = fireWand
        ? snapshot.fireWandChargeRemaining
        : snapshot.iceWandChargeRemaining;
    const float iceChargeProgress =
        elementalWand && (!fireWand || wandChargeRemaining > 0.0) && wandChargeDuration > 0.0
            ? std::clamp(static_cast<float>(
                  1.0 - wandChargeRemaining /
                      wandChargeDuration), 0.0F, 1.0F)
            : 0.0F;
    const float iceRecoilProgress =
        elementalWand &&
                iceWandRecoilDuration_ > 0.0
            ? std::clamp(static_cast<float>(
                  iceWandRecoilRemaining_ /
                      iceWandRecoilDuration_), 0.0F, 1.0F)
            : 0.0F;
    Camera3D viewModelCamera{
        .position = {},
        .target = {0.0F, 0.0F, -1.0F},
        .up = {0.0F, 1.0F, 0.0F},
        .fovy = 58.0F,
        .projection = CAMERA_PERSPECTIVE,
    };
    if (toolVisual == FirstPersonToolVisual::Axe ||
        toolVisual == FirstPersonToolVisual::Pickaxe ||
        toolVisual == FirstPersonToolVisual::Club) {
        // Blender camera transform converted from Z-up to glTF/raylib Y-up.
        // The GLB was exported without its camera, so keep the authored pose
        // here while the tool and swing pivot come directly from the asset.
        viewModelCamera.position = {1.30710F, 0.83238F, 0.36867F};
        viewModelCamera.target = {0.39416F, 0.43122F, 0.29472F};
        viewModelCamera.up = {-0.40002F, 0.91585F, -0.03238F};
        viewModelCamera.fovy = 25.4F;
    }
    FirstPersonToolTuning renderTuning = activeToolTuning();
    renderTuning.position.y -= activeToolTuning().swapDrop *
        (1.0F - arrivalHudOpacity);
    if (toolSwapRemaining_ > 0.0 &&
        toolSwapDuration_ > 0.0) {
        const float progress = std::clamp(
            static_cast<float>(
                1.0 - toolSwapRemaining_ /
                          toolSwapDuration_),
            0.0F, 1.0F);
        const float hideFraction =
            static_cast<float>(ToolSwapHideFraction);
        const float phase = progress < hideFraction
            ? progress / hideFraction
            : (1.0F - progress) /
                  (1.0F - hideFraction);
        const float clampedPhase =
            std::clamp(phase, 0.0F, 1.0F);
        const float smooth =
            clampedPhase * clampedPhase *
            (3.0F - 2.0F * clampedPhase);
        renderTuning.position.y -=
            activeToolTuning().swapDrop * smooth;
    }
    if (renderer_->beginFirstPersonToolPass(&lighting, &camera)) {
        BeginMode3D(viewModelCamera);
        renderer_->beginGpuTimer(
            RendererGpuStage::ViewModelTarget);
        static_cast<void>(renderer_->drawFirstPersonTool(
            toolVisual,
            swingProgress,
            static_cast<float>(cameraBobPhase_),
            static_cast<float>(cameraBobAmount_),
            renderTuning,
            iceChargeProgress,
            iceRecoilProgress,
            toolSwingAlternateAnimation_));
        renderer_->endGpuTimer(
            RendererGpuStage::ViewModelTarget);
        EndMode3D();
        renderer_->beginGpuTimer(
            RendererGpuStage::ViewModelComposite);
        renderer_->endFirstPersonToolPass(
            activeToolTuning(), &lighting);
        renderer_->drawFirstPersonSwingTrail(
            toolVisual, swingProgress,
            static_cast<float>(cameraBobPhase_),
            static_cast<float>(cameraBobAmount_),
            renderTuning, viewModelCamera,
            toolSwingAlternateAnimation_);
        renderer_->endGpuTimer(
            RendererGpuStage::ViewModelComposite);
    }
}
viewModelMilliseconds =
    performanceMilliseconds(viewModelStart);
renderer_->beginGpuTimer(RendererGpuStage::Ui);

// Resolve once so the health bar and upgrade price share the same target.
const auto interactionPromptBuildStart = PerformanceClock::now();
auto interactionSnapshot = feedbackSnapshot;
interactionSnapshot.aimedModularBuilding.reset();
if (pinnedModularBuilding) {
    interactionSnapshot.aimedEnemy.reset();
    interactionSnapshot.aimedResource.reset();
    interactionSnapshot.aimedModularBuilding = pinnedModularBuilding;
}
const auto interactionPrompt = showGameplayHud
    ? buildInteractionPrompt(interactionSnapshot, camera) : std::nullopt;
const auto upgradePromptTarget = interactionPrompt && !interactionPrompt->occluded &&
    interactionPrompt->targetKind == InteractionPromptTargetKind::Building
        ? std::optional<EntityId>{interactionPrompt->targetId} : std::nullopt;
interactionPromptMilliseconds = performanceMilliseconds(interactionPromptBuildStart);

// World-space UI is composited after post-processing so health bars,
// levels and production rewards remain crisp above pixelization.
const auto worldUiStart = PerformanceClock::now();
if (showWorldHud) {
    BeginMode3D(camera);
    // These are overlays. Keeping them out of the depth buffer also
    // prevents the viewmodel's independent camera depth from affecting
    // their visibility.
    rlDisableDepthTest();
    targetHealthBar_.draw(
        healthBarSnapshot, camera,
        simulation_.terrain(),
        [this](const EnemyInstance& enemy)
            -> std::optional<BoundingBox> {
            Vector3 position = enemyRenderPosition(enemy);
            position.y += static_cast<float>(
                simulation_.terrain().getHeight(
                    enemy.position.x, enemy.position.z));
            const BoundingBox bounds = renderer_->enemyWorldBounds(
                enemyModelVisual(enemy), position,
                static_cast<float>(enemy.yaw),
                enemyVisualScale(enemy.type) *
                    (enemy.eliteAffixes != 0U
                        ? 1.08F : 1.0F));
            return world_transforms::finite(bounds)
                ? std::optional<BoundingBox>{bounds}
                : std::nullopt;
        }, upgradePromptTarget, [this](const BuildingInstance& building) {
            return renderer_->buildingVisualHeight(building.type, building.level);
        });
    // Submit every world-space overlay while depth testing is still disabled.
    // Individual bars and production rewards intentionally share this flush.
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    EndMode3D();
}
worldUiMilliseconds =
    performanceMilliseconds(worldUiStart);

std::optional<ScopedUiRenderLayer> arrivalHudPass;
if (showGameplayHud && arrivalHudOpacity < 1.0F) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    if (!arrivalHudLayer_.valid() ||
        arrivalHudLayer_.get().texture.width != width ||
        arrivalHudLayer_.get().texture.height != height) {
        static_cast<void>(arrivalHudLayer_.load(width, height));
    }
    if (arrivalHudLayer_.valid()) {
        arrivalHudPass.emplace(arrivalHudLayer_.get());
    }
} else if (arrivalHudLayer_.valid()) {
    arrivalHudLayer_.unload();
}

if (iceImpactFlashRemaining_ > 0.0 &&
    !userSettings_.accessibility.reduceFlashes) {
    const auto alpha = static_cast<unsigned char>(
        std::clamp(95.0 * iceImpactFlashRemaining_ / 0.10,
                   0.0, 95.0));
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  {142, 229, 255, alpha});
}
if (showGameplayHud) {
    beginSdfUiTextBatch();
    drawFloatingDamageNumbers(camera);
    endSdfUiTextBatch();
}
if (showTerrainWireframe_) {
    drawUiText(
        TextFormat(
            "TERRAIN SEED: %u  CTRL+F7 SAME  CTRL+F9 NEW",
            snapshot.terrainSeed),
        {24.0F,
         static_cast<float>(
             GetScreenHeight()) -
             44.0F},
        16.0F, {245, 224, 154, 235});
}
if (showColliders_) {
    for (const SharedSupport& support :
         snapshot.sharedSupports) {
        if (!support.active || support.referenceCount >= 4U) {
            continue;
        }
        const Vector2 screen =
            GetWorldToScreen(
                {
                    static_cast<float>(
                        support.top.x),
                    static_cast<float>(
                        support.top.y + 0.18),
                    static_cast<float>(
                        support.top.z),
                },
                camera);
        if (screen.x < 0.0F ||
            screen.y < 0.0F ||
            screen.x >
                static_cast<float>(
                    GetScreenWidth()) ||
            screen.y >
                static_cast<float>(
                    GetScreenHeight())) {
            continue;
        }
        drawUiText(
            TextFormat(
                "S%u x%u", support.id,
                support.referenceCount),
            screen, 12.0F,
            {255, 213, 91, 235});
    }
}

// Interaction prompts use a world anchor but are composited in
// screen space after the 3D/post-process passes.
const auto interactionPromptStart = PerformanceClock::now();
if (showGameplayHud) {
    interactionPromptRenderer_.draw(
        interactionPrompt, camera,
        ui_, userSettings_.controls);
}
interactionPromptMilliseconds +=
    performanceMilliseconds(interactionPromptStart);

auto hudSnapshot = presentationSnapshot;
bool showBuildingContextCard =
    pinnedOrdinaryBuilding.has_value() || pinnedModularBuilding.has_value();
if (pinnedOrdinaryBuilding) {
    hudSnapshot.aimedBuilding = pinnedOrdinaryBuilding;
    hudSnapshot.aimedBuildingUpgradeCost =
        buildingContextCardUpgradeCost_;
    hudSnapshot.aimedBuildingStats =
        buildingContextCardStats_;
}
const auto hudCoreStart = PerformanceClock::now();
if (showGameplayHud) {
    beginSdfUiTextBatch();
    const auto targetHealthStart = PerformanceClock::now();
    targetHealthBar_.drawScreenOverlay();
    hudTargetHealthMilliseconds =
        performanceMilliseconds(targetHealthStart);
    drawHud(ui_, hudSnapshot,
    {
            .damageIndicators = damageIndicators_,
            .statusMessage = statusMessage_,
            .statusMessageRemaining = statusMessageRemaining_,
            .statusMessageShakeRemaining =
                statusMessageShakeRemaining_,
            .actionMode = actionMode_,
            .hotbarModeBlend = hotbarModeBlend_,
            .foundationBuildMode =
                foundationBuildMode_,
            .selectedModularBuildPiece =
                modularBuildHotbarIndex(
                    modularBuildPiece_).value_or(0U),
            .buildingHotbarCategory =
                buildingHotbarCategory_,
            .buildHotbarSelectionPosition =
                buildHotbarSelectionPosition_,
            .buildHotbarSelectionAlpha =
                buildHotbarSelectionAlpha_,
            .foundationHotbarSelectionPosition =
                foundationHotbarSelectionPosition_,
            .foundationHotbarSelectionAlpha =
                foundationHotbarSelectionAlpha_,
            .weaponHotbarSelectionPosition =
                weaponHotbarSelectionPosition_,
            .weaponHotbarSelectionAlpha =
                weaponHotbarSelectionAlpha_,
            .informationExpansion =
                minimapExpansion_,
            .mapOverlayOpen =
                minimapExpansion_ > 0.01F,
            .minimapHidden = false,
            .showFps =
                userSettings_.accessibility.showFps,
            .showCoreHealth =
                presentationSnapshot.state == RunState::Wave ||
                presentationSnapshot.state == RunState::Sunset ||
                (presentationSnapshot.coreMaxHealth > 0.0 &&
                 presentationSnapshot.coreHealth <
                     presentationSnapshot.coreMaxHealth) ||
                (presentationSnapshot.coreId &&
                 recentlyDamagedBuilding_ ==
                     presentationSnapshot.coreId &&
                 damagedBuildingHealthBarRemaining_ > 0.0),
            .playerHealthTrail =
                displayedPlayerHealthTrail_,
            .playerDamagePulse = static_cast<float>(
                std::clamp(
                    playerDamageFlashRemaining_ / 0.18,
                    0.0, 1.0)),
            .showBuildingContextCard =
                showBuildingContextCard,
            .buildingContextCardEntranceSeconds =
                static_cast<float>(buildingContextCardEntranceSeconds_),
            .repairSweepActive =
                repairSweepActive_,
            .woodResourcePop =
                woodHudBounceRemaining_ > 0.0
                    ? static_cast<float>(
                          std::sin(
                              (1.0 -
                               woodHudBounceRemaining_ /
                                   0.28) *
                              PI))
                    : 0.0F,
            .stoneResourcePop =
                stoneHudBounceRemaining_ > 0.0
                    ? static_cast<float>(
                          std::sin(
                              (1.0 -
                               stoneHudBounceRemaining_ /
                                   0.28) *
                              PI))
                    : 0.0F,
            .crystalResourcePop =
                crystalHudBounceRemaining_ > 0.0
                    ? static_cast<float>(
                          std::sin(
                              (1.0 -
                               crystalHudBounceRemaining_ /
                                   0.28) *
                              PI))
                    : 0.0F,
            .coinResourcePop =
                coinHudBounceRemaining_ > 0.0
                    ? static_cast<float>(
                          std::sin(
                              (1.0 -
                               coinHudBounceRemaining_ /
                                   0.32) *
                              PI))
                    : 0.0F,
            .displayedWood = woodHudCounter_.displayed,
            .displayedStone = stoneHudCounter_.displayed,
            .displayedCrystals = crystalHudCounter_.displayed,
            .displayedCoins = coinHudCounter_.displayed,
            .displayedInsight = displayedInsight_,
            .insightPulse = insightPulseRemaining_ > 0.0
                ? insightPulseRemaining_ / insightPulseDuration_ : 0.0,
            .treePointPulse = insightPointSequenceRemaining_ > 0.0
                ? std::sin((1.0 - insightPointSequenceRemaining_ /
                    insightPointSequenceDuration_) * PI) : 0.0,
            .objectivePulseId = objectivePulseId_,
            .objectivePulse = objectivePulseRemaining_ > 0.0
                ? objectivePulseRemaining_ /
                      objectivePulseDuration_
                : 0.0,
            .objectiveTrackerExpansion =
                objectiveTrackerExpansion_,
            .objectiveCompletionTitle =
                objectiveCompletionTitle_,
            .objectiveCompletionReward =
                objectiveCompletionReward_,
            .objectiveCompletionProgress =
                objectiveCompletionRemaining_ > 0.0
                    ? 1.0 - objectiveCompletionRemaining_ /
                          objectiveCompletionDuration_
                    : 0.0,
            .insightGainAmount = insightGainAmount_,
            .insightGainProgress = insightGainRemaining_ > 0.0
                ? 1.0 - insightGainRemaining_ /
                      insightGainDuration_
                : 0.0,
            .crosshairHitRemaining =
                crosshairHitRemaining_,
            .crosshairHitDuration =
                crosshairHitDuration_,
            .crosshairHitCritical =
                crosshairHitCritical_,
            .invalidActionRemaining =
                invalidActionRemaining_,
            .weaponRecoilAmount =
                weaponRecoilRemaining_ > 0.0 &&
                        weaponRecoilDuration_ > 0.0
                    ? std::sin(
                          static_cast<float>(
                              (1.0 -
                               weaponRecoilRemaining_ /
                                   weaponRecoilDuration_) *
                              PI)) *
                          weaponRecoilStrength_ /
                          0.11F
                    : 0.0F,
            .buildingStatsUpgradeEntity =
                buildingStatsUpgradeEntity_,
            .buildingStatsUpgradeRemaining =
                buildingStatsUpgradeRemaining_,
            .buildingStatsUpgradeDuration =
                buildingStatsUpgradeDuration_,
    },
        camera, userSettings_.controls);
    drawBossHealthBar(snapshot.enemies,uiScreenWidth());
    const auto& hudTimings = hudRenderPerformanceStats();
    hudResourcesMilliseconds = hudTimings.resourcesMs;
    hudVitalityMilliseconds = hudTimings.vitalityMs;
    hudInformationMilliseconds = hudTimings.informationMs;
    hudObjectivesMilliseconds = hudTimings.objectivesMs;
    hudLootMilliseconds = hudTimings.lootMs;
    hudPhaseMilliseconds = hudTimings.phaseMs;
    hudHotbarMilliseconds = hudTimings.hotbarMs;
    hudFeedbackMilliseconds = hudTimings.feedbackMs;
    const auto textFlushStart = PerformanceClock::now();
    endSdfUiTextBatch();
    hudTextFlushMilliseconds =
        performanceMilliseconds(textFlushStart);
}
hudCoreMilliseconds =
    performanceMilliseconds(hudCoreStart);
if (showGameplayHud && foundationBuildMode_) {
    const float modularErrorShake =
        invalidActionRemaining_ > 0.0
            ? std::sin(static_cast<float>(GetTime()) * 105.0F) *
                  5.0F * static_cast<float>(std::min(
                             invalidActionRemaining_ / 0.22, 1.0))
            : 0.0F;
    bool previewValid = false;
    std::size_t plannedCount = 1U;
    std::optional<ModularPlacementError>
        previewError;
    switch (modularBuildPiece_) {
    case ModularBuildPiece::Foundation:
    case ModularBuildPiece::FloorPlatform:
        if (platformFramePreview_) {
            previewValid =
                platformFramePreview_->valid();
            previewError =
                platformFramePreview_->error;
        }
        if (!modularPlatformDragPreviews_.empty()) {
            plannedCount =
                modularPlatformDragPreviews_.size();
            const auto invalid = std::find_if(
                modularPlatformDragPreviews_.begin(),
                modularPlatformDragPreviews_.end(),
                [](const PlatformFramePlacement&
                       placement) {
                    return !placement.valid();
                });
            previewValid =
                invalid ==
                modularPlatformDragPreviews_.end();
            previewError =
                previewValid
                    ? ModularPlacementError::None
                    : invalid->error;
        }
        break;
    case ModularBuildPiece::Wall:
        if (wallPreview_) {
            previewValid =
                wallPreview_->valid();
            previewError =
                wallPreview_->error;
        }
        if (!modularWallDragPreviews_.empty()) {
            plannedCount =
                modularWallDragPreviews_.size();
            const auto invalid = std::find_if(
                modularWallDragPreviews_.begin(),
                modularWallDragPreviews_.end(),
                [](const WallPlacement& placement) {
                    return !placement.valid();
                });
            previewValid =
                invalid ==
                modularWallDragPreviews_.end();
            previewError =
                previewValid
                    ? ModularPlacementError::None
                    : invalid->error;
        }
        break;
    case ModularBuildPiece::Ramp:
        if (rampPreview_) {
            previewValid =
                rampPreview_->valid();
            previewError =
                rampPreview_->error;
        }
        if (!modularRampDragPreviews_.empty()) {
            plannedCount =
                modularRampDragPreviews_.size();
            const auto invalid = std::find_if(
                modularRampDragPreviews_.begin(),
                modularRampDragPreviews_.end(),
                [](const RampPlacement& placement) {
                    return !placement.valid();
                });
            previewValid =
                invalid ==
                modularRampDragPreviews_.end();
            previewError =
                previewValid
                    ? ModularPlacementError::None
                    : invalid->error;
        }
        break;
    }
    if (!modularDragPiece_) {
        ResourceCost totalCost =
            snapshot.modularBuildingCosts[
                static_cast<std::size_t>(
                    modularBuildPiece_)];
        const int automaticFoundationCount =
            static_cast<int>(
                modularAutomaticFoundationPreviews_.size());
        const ResourceCost foundationCost =
            snapshot.modularBuildingCosts[
                static_cast<std::size_t>(
                    ModularBuildPiece::Foundation)];
        totalCost.wood +=
            foundationCost.wood * automaticFoundationCount;
        totalCost.stone +=
            foundationCost.stone * automaticFoundationCount;
        totalCost.crystals +=
            foundationCost.crystals * automaticFoundationCount;
        const Texture2D pieceIcon =
            modularBuildPiece_ == ModularBuildPiece::Wall
                ? ui_.buildingTexture(BuildingType::Wall)
                : ui_.modularBuildingTexture(
                      modularBuildPiece_);
        const float y =
            static_cast<float>(GetScreenHeight()) *
                0.5F +
            84.0F;
        if (previewError != ModularPlacementError::Occupied) {
            drawDragPlacementSummary(
                ui_, pieceIcon, 1,
                ui_.modularBuildingTexture(
                    ModularBuildPiece::Foundation),
                automaticFoundationCount,
                totalCost, y,
                snapshot.unlimitedResources ||
                    (snapshot.wood >= totalCost.wood &&
                     snapshot.stone >= totalCost.stone &&
                     snapshot.crystals >= totalCost.crystals));
        }
        if (previewError &&
            *previewError != ModularPlacementError::None &&
            *previewError !=
                ModularPlacementError::InsufficientResources) {
            drawModularPlacementError(
                previewError, y + 82.0F, 18.0F,
                modularErrorShake);
        }
    } else {
        ResourceCost cost =
            snapshot.modularBuildingCosts[
                static_cast<std::size_t>(
                    *modularDragPiece_)];
        int automaticFoundationCount = 0;
        if (modularDragUsesAutomaticFoundation_) {
            const int foundationCount =
                *modularDragPiece_ ==
                        ModularBuildPiece::Ramp
                    ? 2
                    : 1;
            for (int index = 0;
                 index < foundationCount; ++index) {
                cost = addResourceCosts(
                    cost,
                    snapshot.modularBuildingCosts[
                        static_cast<std::size_t>(
                            ModularBuildPiece::Foundation)]);
            }
            automaticFoundationCount =
                foundationCount *
                static_cast<int>(plannedCount);
        }
        const int count =
            static_cast<int>(plannedCount);
        const ResourceCost totalCost{
            cost.wood * count,
            cost.stone * count,
            cost.crystals * count,
        };
        const Texture2D pieceIcon =
            *modularDragPiece_ == ModularBuildPiece::Wall
                ? ui_.buildingTexture(BuildingType::Wall)
                : ui_.modularBuildingTexture(
                      *modularDragPiece_);
        const float y =
            static_cast<float>(GetScreenHeight()) *
                0.5F +
            84.0F;
        if (previewError != ModularPlacementError::Occupied) {
            drawDragPlacementSummary(
                ui_, pieceIcon, count,
                ui_.modularBuildingTexture(
                    ModularBuildPiece::Foundation),
                automaticFoundationCount,
                totalCost, y,
                snapshot.unlimitedResources ||
                    (snapshot.wood >= totalCost.wood &&
                     snapshot.stone >= totalCost.stone &&
                     snapshot.crystals >= totalCost.crystals));
        }
        if (!previewValid &&
            previewError !=
                ModularPlacementError::InsufficientResources) {
            drawModularPlacementError(
                previewError, y + 82.0F, 17.0F,
                modularErrorShake);
        }
    }
}
if (showGameplayHud && wallDragStart_ && wallDragEnd_ &&
    placementDragType_) {
    const auto cells =
        placementLine(
            *placementDragType_, *wallDragStart_,
            *wallDragEnd_, placementDragAxis_);
    const ResourceCost buildingCost =
        snapshot.buildingCosts[
            static_cast<std::size_t>(
                *placementDragType_)];
    const int count =
        static_cast<int>(cells.size());
    const ResourceCost totalCost{
        buildingCost.wood * count,
        buildingCost.stone * count,
        buildingCost.crystals * count,
    };
    const float y =
        static_cast<float>(GetScreenHeight()) * 0.5F +
        84.0F;
    drawDragPlacementSummary(
        ui_, ui_.buildingTexture(*placementDragType_),
        count, {}, 0, totalCost, y,
        snapshot.unlimitedResources ||
            (snapshot.wood >= totalCost.wood &&
             snapshot.stone >= totalCost.stone &&
             snapshot.crystals >= totalCost.crystals));
}
if (showGameplayHud && removalDragActive_) {
    const std::string removalHint =
        "REMOVE x" +
        std::to_string(
            removalDragTargets_.size()) +
        "   RELEASE X TO CONFIRM";
    drawCenteredUiText(
        removalHint,
        static_cast<float>(
            GetScreenHeight() / 2 + 76),
        20.0F, {255, 104, 91, 255});
}
if (showGameplayHud && removalDragActive_ &&
    !structuralRiskIds_.empty()) {
    drawCenteredUiText(
        "COLLAPSE RISK: " +
            std::to_string(
                structuralRiskIds_.size()) +
            " DEPENDENT PARTS",
        static_cast<float>(
            GetScreenHeight() / 2 +
            (removalDragActive_ ? 106 : 132)),
        17.0F, {255, 197, 82, 255});
}
const auto resourceGainStart = PerformanceClock::now();
if (showGameplayHud) drawResourceGainVisuals(camera);
resourceGainMilliseconds =
    performanceMilliseconds(resourceGainStart);

const auto minimapStart = PerformanceClock::now();
if (showGameplayHud) {
    drawMinimapHud(ui_, presentationSnapshot, minimapExpansion_);
    const auto& minimapTimings =
        minimapRenderPerformanceStats();
    minimapSetupMilliseconds = minimapTimings.setupMs;
    minimapTargetRefreshMilliseconds =
        minimapTimings.targetRefreshMs;
    minimapFrameMilliseconds = minimapTimings.frameMs;
    minimapLabelsMilliseconds = minimapTimings.labelsMs;
}
minimapMilliseconds =
    performanceMilliseconds(minimapStart);

if (arrivalHudPass) {
    arrivalHudPass.reset();
    const Texture2D texture = arrivalHudLayer_.get().texture;
    const auto alpha = static_cast<unsigned char>(std::lround(arrivalHudOpacity * 255.0F));
    // The transparent layer stores premultiplied RGB. Fade RGB and alpha
    // together so text and icons retain their brightness as they appear.
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(texture,
        {0, 0, static_cast<float>(texture.width), -static_cast<float>(texture.height)},
        {0, 0, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
        {}, 0.0F, {alpha, alpha, alpha, alpha});
    EndBlendMode();
}

const auto uiOverlaysStart = PerformanceClock::now();
const bool unobscuredColorPreview =
    renderer_->graphicsPanelVisible() &&
    graphicsPanelTab_ == ColorSettingsTab;
// Run-state and level-up overlays contain several animated SDF labels. Draw
// them as one atlas batch: rendering every label separately is especially
// expensive while the level-up cards animate and their scale/alpha changes
// every frame, preventing the per-label texture cache from being reused.
beginSdfUiTextBatch();
if (showWorldHud && !unobscuredColorPreview) {
    drawRunStateOverlay(snapshot);
}
drawRunUpgradeOverlay(
    ui_, snapshot,
    runUpgradeOverlayEntranceSeconds_,
    runUpgradeOverlayExitSeconds_,
    runUpgradePendingChoice_,
    runUpgradeCardHoverAmounts_,std::nullopt,userSettings_.accessibility.reduceFlashes);
drawRunShopOverlay(ui_, snapshot, runShopKeyboardFocus_,std::nullopt,runShopOverlayEntranceSeconds_,
    userSettings_.accessibility.reduceFlashes);
endSdfUiTextBatch();
if (snapshot.state == RunState::Paused &&
    !renderer_->graphicsPanelVisible()) {
    const float centerX =
        static_cast<float>(GetScreenWidth()) * 0.5F;
    const float centerY =
        static_cast<float>(GetScreenHeight()) * 0.5F;
    ui_.drawPanel(
        {centerX - 260.0F, centerY - 252.0F,
         520.0F, 504.0F}, 250);
    float menuY = centerY - 144.0F;
    pendingResumeFromUi_ =
        ui_.drawButton(
            {centerX - 190.0F, menuY,
             380.0F, 58.0F},
            "RESUME") ||
        pendingResumeFromUi_;
    menuY += 70.0F;
    if (ui_.drawButton(
            {centerX - 190.0F, menuY,
             380.0F, 58.0F},
            "SETTINGS")) {
        renderer_->setGraphicsPanelVisible(true);
    }
    menuY += 70.0F;
    pendingRestartFromUi_ =
        ui_.drawButton(
            {centerX - 190.0F, menuY,
             380.0F, 58.0F},
            "RESTART RUN") ||
        pendingRestartFromUi_;
    menuY += 70.0F;
    pendingReturnToMenuFromUi_ =
        ui_.drawButton(
            {centerX - 190.0F, menuY,
             380.0F, 58.0F},
            snapshot.sandboxMode
                ? "MAIN MENU"
                : "SAVE & MAIN MENU") ||
        pendingReturnToMenuFromUi_;
    menuY += 70.0F;
    if (ui_.drawButton(
            {centerX - 190.0F, menuY,
             380.0F, 58.0F},
            snapshot.sandboxMode
                ? "EXIT"
                : "SAVE & EXIT")) {
        if (snapshot.sandboxMode || saveSuspendedRun()) {
            exitRequested_ = true;
        } else {
            audio_.playUiError();
        }
    }
}
uiOverlaysMilliseconds +=
    performanceMilliseconds(uiOverlaysStart);
}

} // namespace ian
