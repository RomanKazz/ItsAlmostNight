#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "graphics/WorldTransforms.hpp"
#include "localization/Localization.hpp"
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
    }
    return "CANNOT BUILD";
}

} // namespace

using namespace app_detail;

void App::render() {
    const auto& snapshot = simulation_.snapshot();
    structuralRiskIds_.clear();
    std::vector<EntityId> structuralRiskRoots;
    const auto addStructuralRiskRoot =
        [&structuralRiskRoots](EntityId support) {
            if (std::ranges::find(
                    structuralRiskRoots, support) ==
                structuralRiskRoots.end()) {
                structuralRiskRoots.push_back(support);
            }
        };
    if (!foundationBuildMode_ &&
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
        structuralRiskIds_ =
            simulation_.structuralCollapseRisk(
                structuralRiskRoots);
        structuralRiskCacheRoots_ = structuralRiskRoots;
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
    if (presentationSnapshot.aimedEnemy) {
        presentationSnapshot.aimedResource.reset();
        presentationSnapshot.aimedBuilding.reset();
        presentationSnapshot.aimedModularBuilding.reset();
    }
    // Interaction feedback follows the exact aim, like chest selection.
    // The separate presentation snapshot may keep HUD context stable, but
    // outlines, prompts and healthbars must enter/leave together.
    auto feedbackSnapshot = snapshot;
    feedbackSnapshot.aimedBuildingUpgradeCost =
        snapshot.aimedBuildingUpgradeCost;
    feedbackSnapshot.aimedBuildingStats =
        snapshot.aimedBuildingStats;
    if (feedbackSnapshot.aimedEnemy) {
        feedbackSnapshot.aimedResource.reset();
        feedbackSnapshot.aimedBuilding.reset();
        feedbackSnapshot.aimedModularBuilding.reset();
    }

    if (snapshot.state == RunState::MainMenu) {
        interactionPromptRenderer_.reset();
        renderer_->beginUiOnlyFrame({18, 22, 31, 255});
        const float centerX =
            static_cast<float>(GetScreenWidth()) * 0.5F;
        const float centerY =
            static_cast<float>(GetScreenHeight()) * 0.5F;
        ui_.drawPanel({centerX - 420.0F, centerY - 330.0F,
                       840.0F, 660.0F});
        ui_.drawInsetPanel({centerX - 380.0F, centerY - 254.0F,
                            760.0F, 128.0F});
        drawCentered("IT'S ALMOST NIGHT",
                     static_cast<int>(centerY) - 230, 42,
                     {245, 220, 174, 255});
        if (!renderer_->graphicsPanelVisible()) {
        pendingStartFromUi_ =
            ui_.drawButton({centerX - 210.0F, centerY - 84.0F,
                            420.0F, 64.0F},
                           "START RUN") ||
            pendingStartFromUi_;
        if (ui_.drawButton(
                {centerX - 210.0F, centerY - 6.0F,
                 420.0F, 64.0F},
                "SETTINGS")) {
            renderer_->setGraphicsPanelVisible(true);
        }
        pendingOpenSkillTreeFromUi_ =
            ui_.drawButton(
                {centerX - 210.0F, centerY + 72.0F,
                 420.0F, 64.0F},
                "TREE OF KNOWLEDGE") ||
            pendingOpenSkillTreeFromUi_;
        if (ui_.drawButton(
                {centerX - 210.0F, centerY + 150.0F,
                 420.0F, 64.0F},
                "EXIT GAME")) {
            exitRequested_ = true;
        }
        drawCentered("ENTER: PLAY  •  F2: SETTINGS  •  K: TREE",
                     static_cast<int>(centerY) + 250, 16,
                     {199, 174, 142, 255});
        }
    } else {
        const double visualYaw = snapshot.playerYaw;
        const double visualPitch = snapshot.playerPitch;
        const double cosPitch = std::cos(visualPitch);
        Vector3 position = {
            static_cast<float>(snapshot.playerPosition.x),
            static_cast<float>(
                groundCameraSmoothingInitialized_
                    ? smoothedGroundCameraY_
                    : snapshot.playerPosition.y),
            static_cast<float>(snapshot.playerPosition.z),
        };
        position.y += static_cast<float>(playerSpawnDropHeight_);
        Vector3 forward = {
            static_cast<float>(std::sin(visualYaw) * cosPitch),
            static_cast<float>(std::sin(visualPitch)),
            static_cast<float>(-std::cos(visualYaw) * cosPitch),
        };
        const float bobAmount =
            static_cast<float>(cameraBobAmount_) *
            (input_.sprint ? 1.12F : 1.0F) *
            motionBobIntensity_;
        const float bobSide =
            static_cast<float>(
                std::sin(cameraBobPhase_)) *
            0.012F * bobAmount;
        const float bobVertical =
            -static_cast<float>(
                std::abs(std::sin(cameraBobPhase_))) *
            0.024F * bobAmount;
        const Vector3 bobRight = {
            static_cast<float>(
                std::cos(visualYaw)),
            0.0F,
            static_cast<float>(
                std::sin(visualYaw)),
        };
        position = Vector3Add(
            position,
            Vector3Scale(
                bobRight,
                static_cast<float>(cameraLookYawLag_ * 0.42) *
                    motionSwayIntensity_));
        position.y += static_cast<float>(
            cameraLookPitchLag_ * 0.32) *
            motionSwayIntensity_;
        position = Vector3Add(
            position,
            Vector3Scale(bobRight, bobSide));
        position.y += bobVertical;
        Vector3 bobCameraUp = Vector3Normalize(
            Vector3Add(
                {0.0F, 1.0F, 0.0F},
                Vector3Scale(
                    bobRight,
                    (-static_cast<float>(
                         std::sin(cameraBobPhase_)) *
                         0.0045F * bobAmount -
                     static_cast<float>(
                         cameraStrafeLean_) *
                         motionSwayIntensity_))));
        if (landingResponseRemaining_ > 0.0 &&
            landingResponseDuration_ > 0.0) {
            const double progress = std::clamp(
                1.0 - landingResponseRemaining_ /
                          landingResponseDuration_,
                0.0, 1.0);
            const float landingCurve = static_cast<float>(
                std::sin(progress * PI) *
                landingResponseStrength_) *
                motionLandingIntensity_;
            position.y -= landingCurve * 0.052F;
            forward.y -= landingCurve * 0.012F;
            forward = Vector3Normalize(forward);
        }
        position = Vector3Add(
            position,
            Vector3Scale(
                bobRight,
                static_cast<float>(cameraImpulseOffset_.x) *
                    motionShakeIntensity_));
        position.y +=
            static_cast<float>(cameraImpulseOffset_.y) *
            motionShakeIntensity_;
        position = Vector3Add(
            position,
            Vector3Scale(
                forward,
                static_cast<float>(cameraImpulseOffset_.z) *
                    motionShakeIntensity_));
        if (weaponRecoilRemaining_ > 0.0 &&
            weaponRecoilDuration_ > 0.0) {
            const float progress = std::clamp(
                static_cast<float>(
                    1.0 -
                    weaponRecoilRemaining_ /
                        weaponRecoilDuration_),
                0.0F, 1.0F);
            const float recoil =
                std::sin(progress * PI) *
                weaponRecoilStrength_;
            position = Vector3Subtract(
                position, Vector3Scale(forward, recoil));
            forward.y += recoil * 0.38F;
            forward = Vector3Normalize(forward);
        }
        if (cameraShakeRemaining_ > 0.0) {
            const double visualTime = GetTime();
            const float shake =
                static_cast<float>(cameraShakeStrength_ * cameraShakeRemaining_ / 0.35) *
                motionShakeIntensity_;
            position.x += static_cast<float>(std::sin(visualTime * 83.0)) * shake;
            position.y += static_cast<float>(std::cos(visualTime * 97.0)) * shake * 0.7F;
            position.z += static_cast<float>(std::sin(visualTime * 71.0)) * shake * 0.5F;
        }
        const Camera3D camera = {
            .position = position,
            .target = Vector3Add(position, forward),
            .up = bobCameraUp,
            .fovy = cameraFov_,
            .projection = CAMERA_PERSPECTIVE,
        };
        constexpr Color DayGround{66, 112, 67, 255};
        constexpr Color NightGround{28, 52, 50, 255};

        float automaticTime = environment_.timeOfDay();
        if (snapshot.state == RunState::Gathering ||
            snapshot.state == RunState::BuildPhase) {
            automaticTime = 0.25F;
        } else if (snapshot.state == RunState::Sunset) {
            const double duration = std::max(snapshot.phaseDuration, 0.001);
            const float progress = static_cast<float>(
                1.0 - snapshot.phaseTimeRemaining / duration);
            automaticTime = 0.25F + std::clamp(progress, 0.0F, 1.0F) * 0.5F;
        } else if (snapshot.state == RunState::Wave) {
            automaticTime = 0.75F;
        } else if (snapshot.state == RunState::WaveComplete) {
            const double duration = std::max(snapshot.phaseDuration, 0.001);
            const float progress = static_cast<float>(
                1.0 - snapshot.phaseTimeRemaining / duration);
            automaticTime = 0.75F + std::clamp(progress, 0.0F, 1.0F) * 0.5F;
        }
        environment_.setAutomaticTime(automaticTime);
        const EnvironmentState environment = environment_.state();
        const float nightAmount = environment.nightFactor;
        const Color ground = {
            static_cast<unsigned char>(
                static_cast<float>(DayGround.r) +
                (static_cast<float>(NightGround.r) -
                 static_cast<float>(DayGround.r)) *
                    nightAmount),
            static_cast<unsigned char>(
                static_cast<float>(DayGround.g) +
                (static_cast<float>(NightGround.g) -
                 static_cast<float>(DayGround.g)) *
                    nightAmount),
            static_cast<unsigned char>(
                static_cast<float>(DayGround.b) +
                (static_cast<float>(NightGround.b) -
                 static_cast<float>(DayGround.b)) *
                    nightAmount),
            255,
        };
        const Vector3 lightDirection =
            Vector3Scale(environment.celestialDirection, -1.0F);
        const WorldLighting lighting{
            .cameraPosition = camera.position,
            .sunDirection = lightDirection,
            .sunColor = environment.sunColor,
            .sunIntensity = environment.sunIntensity,
            .skyAmbientColor = environment.skyAmbientColor,
            .groundAmbientColor = environment.groundAmbientColor,
            .ambientIntensity = environment.ambientIntensity,
            .cloudShadowStrength = 0.20F * (1.0F - nightAmount),
            .fogColor = colorToVector(environment.fogColor),
            .fogStart = environment.fogStart,
            .fogEnd = environment.fogEnd,
            .dayNightTint = environment.dayNightTint,
            .exposure = environment.exposure,
            .saturation = environment.saturation,
        };
        const Vector3 cameraRight =
            Vector3Normalize(Vector3CrossProduct(forward, {0.0F, 1.0F, 0.0F}));
        const Vector3 cameraUp =
            Vector3Normalize(Vector3CrossProduct(cameraRight, forward));
        const SkyState skyState{
            .cameraForward = forward,
            .cameraRight = cameraRight,
            .cameraUp = cameraUp,
            .verticalFovDegrees = camera.fovy,
            .zenithColor = colorToVector(environment.skyTop),
            .horizonColor = colorToVector(environment.skyHorizon),
            .lowerSkyColor = colorToVector(environment.lowerSky),
            .celestialDirection = environment.celestialDirection,
            .celestialColor = environment.celestialColor,
            .celestialIntensity = environment.sunIntensity,
            .nightAmount = nightAmount,
            .timeSeconds = static_cast<float>(GetTime()),
            .exposure = environment.exposure,
            .saturation = environment.saturation,
        };

        renderer_->setWorldReveal(
            worldRevealOrigin_,
            static_cast<float>(worldRevealElapsed_));
        drawShadowPass(snapshot, lighting);
        drawSelectionPass(feedbackSnapshot, camera);
        renderer_->beginWorldPass(environment.skyHorizon);
        renderer_->drawSky(skyState);
        BeginMode3D(camera);
        renderer_->beginWorldShader(lighting);
        WorldMaterialState terrainMaterial{};
        terrainMaterial.terrainAmount = 1.0F;
        terrainMaterial.bakedAo = 0.9F;
        renderer_->setWorldMaterial(terrainMaterial);
        renderer_->drawTerrain(
            ground, camera.position,
            showTerrainWireframe_);
        drawWorldEntities(presentationSnapshot, camera, nightAmount,
                          lighting,
                          static_cast<float>(fixedStep_.interpolationAlpha()));
        renderer_->beginWorldShader(lighting);
        WorldMaterialState pondRockMaterial{};
        pondRockMaterial.bakedAo = 0.76F;
        renderer_->setWorldMaterial(pondRockMaterial);
        renderer_->drawPondShoreRocks();
        WorldMaterialState pondPlantMaterial{};
        pondPlantMaterial.bakedAo = 0.82F;
        pondPlantMaterial.windAmount = 0.32F;
        pondPlantMaterial.localWindHeight = 1.0F;
        renderer_->setWorldMaterial(pondPlantMaterial);
        renderer_->drawPondDecor();
        renderer_->endWorldShader();
        renderer_->drawWater(camera.position, lighting);
        renderer_->beginWorldShader(lighting);
        WorldMaterialState pondSurfaceMaterial{};
        pondSurfaceMaterial.bakedAo = 0.82F;
        renderer_->setWorldMaterial(pondSurfaceMaterial);
        renderer_->drawPondSurfaceDecor();
        renderer_->endWorldShader();
        renderer_->drawClouds(
            camera.position, nightAmount, lighting);
        drawAtmosphereParticles(camera, nightAmount);
        drawBlobShadows(snapshot, camera);
        drawWorldOverlays(presentationSnapshot, lighting);
        drawPresentationEffects();
        EndMode3D();
        renderer_->drawSelectionOutline();

        auto healthBarSnapshot = feedbackSnapshot;
        if (!healthBarSnapshot.aimedBuilding &&
            recentlyDamagedBuilding_ &&
            damagedBuildingHealthBarRemaining_ > 0.0) {
            const bool isBuilding =
                std::any_of(
                    healthBarSnapshot.buildings.begin(),
                    healthBarSnapshot.buildings.end(),
                    [this](
                        const BuildingInstance& building) {
                        return building.id ==
                               *recentlyDamagedBuilding_;
                    });
            if (isBuilding) {
                healthBarSnapshot.aimedBuilding =
                    recentlyDamagedBuilding_;
            } else {
                const bool isModular =
                    std::any_of(
                        healthBarSnapshot.platformFrames.begin(),
                        healthBarSnapshot.platformFrames.end(),
                        [this](
                            const PlatformFrameInstance& frame) {
                            return frame.id ==
                                   *recentlyDamagedBuilding_;
                        }) ||
                    std::any_of(
                        healthBarSnapshot.modularWalls.begin(),
                        healthBarSnapshot.modularWalls.end(),
                        [this](const WallInstance& wall) {
                            return wall.id ==
                                   *recentlyDamagedBuilding_;
                        }) ||
                    std::any_of(
                        healthBarSnapshot.ramps.begin(),
                        healthBarSnapshot.ramps.end(),
                        [this](const RampInstance& ramp) {
                            return ramp.id ==
                                   *recentlyDamagedBuilding_;
                        });
                if (isModular) {
                    healthBarSnapshot
                        .aimedModularBuilding =
                        recentlyDamagedBuilding_;
                }
            }
        }
        renderer_->endWorldPass();

        const bool tuningPreview =
            renderer_->graphicsPanelVisible() &&
            graphicsPanelTab_ == ToolSettingsTab;
        const bool showFirstPersonTool =
            (tuningPreview ||
             displayedToolVisual_ != FirstPersonToolVisual::None) &&
            (tuningPreview || actionModeUsesEquipment(actionMode_)) &&
            !snapshot.selectedBuilding &&
            !foundationBuildMode_ &&
            !snapshot.playerRespawning;
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
                elementalWand && wandChargeDuration > 0.0
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
            const Camera3D viewModelCamera{
                .position = {},
                .target = {0.0F, 0.0F, -1.0F},
                .up = {0.0F, 1.0F, 0.0F},
                .fovy = 58.0F,
                .projection = CAMERA_PERSPECTIVE,
            };
            FirstPersonToolTuning renderTuning = toolTuning_;
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
                    clampedPhase *
                    (clampedPhase *
                         (clampedPhase * 6.0F - 15.0F) +
                     10.0F);
                renderTuning.position.y -=
                    toolTuning_.swapDrop * smooth;
            }
            if (renderer_->beginFirstPersonToolPass()) {
                BeginMode3D(viewModelCamera);
                static_cast<void>(renderer_->drawFirstPersonTool(
                    toolVisual,
                    swingProgress,
                    static_cast<float>(cameraBobPhase_),
                    static_cast<float>(cameraBobAmount_),
                    renderTuning,
                    iceChargeProgress,
                    iceRecoilProgress));
                EndMode3D();
                renderer_->endFirstPersonToolPass(toolTuning_);
            }
        }

        // World-space UI is composited after post-processing so health bars,
        // levels and production rewards remain crisp above pixelization.
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
                    enemyModelVisual(enemy.type), position,
                    static_cast<float>(enemy.yaw),
                    enemyVisualScale(enemy.type));
                return world_transforms::finite(bounds)
                    ? std::optional<BoundingBox>{bounds}
                    : std::nullopt;
            });
        drawProductionVisuals(camera);
        rlEnableDepthTest();
        EndMode3D();

        if (playerDamageFlashRemaining_ > 0.0 &&
            !userSettings_.accessibility.reduceFlashes) {
            const auto alpha = static_cast<unsigned char>(
                90.0 * playerDamageFlashRemaining_ / 0.18);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          {190, 24, 24, alpha});
        }
        if (iceImpactFlashRemaining_ > 0.0 &&
            !userSettings_.accessibility.reduceFlashes) {
            const auto alpha = static_cast<unsigned char>(
                std::clamp(95.0 * iceImpactFlashRemaining_ / 0.10,
                           0.0, 95.0));
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          {142, 229, 255, alpha});
        }
        drawFloatingDamageNumbers(camera);
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
                if (!support.active) {
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
        interactionPromptRenderer_.draw(
            buildInteractionPrompt(feedbackSnapshot, camera), camera,
            ui_, userSettings_.controls);

        auto hudSnapshot = presentationSnapshot;
        bool showBuildingContextCard =
            buildingContextCardTarget_.has_value();
        if (buildingContextCardTarget_) {
            const auto pinned = std::find_if(
                snapshot.buildings.begin(),
                snapshot.buildings.end(),
                [this](const BuildingInstance& candidate) {
                    return candidate.id ==
                           *buildingContextCardTarget_;
                });
            if (pinned == snapshot.buildings.end()) {
                buildingContextCardTarget_.reset();
                buildingContextCardUpgradeCost_.reset();
                buildingContextCardStats_.reset();
                showBuildingContextCard = false;
            } else {
                hudSnapshot.aimedBuilding =
                    buildingContextCardTarget_;
                hudSnapshot.aimedBuildingUpgradeCost =
                    buildingContextCardUpgradeCost_;
                hudSnapshot.aimedBuildingStats =
                    buildingContextCardStats_;
            }
        }
        drawHud(
            ui_, hudSnapshot,
            {
                    .damageIndicators = damageIndicators_,
                    .statusMessage = statusMessage_,
                    .statusMessageRemaining = statusMessageRemaining_,
                    .hideBottomHints = hideBottomHud_,
                    .actionMode = actionMode_,
                    .foundationBuildMode =
                        foundationBuildMode_,
                    .selectedModularBuildPiece =
                        static_cast<std::size_t>(
                            modularBuildPiece_),
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
                    .minimapHidden = minimapHidden_,
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
                    .showBuildingContextCard =
                        showBuildingContextCard,
                    .repairSweepActive =
                        repairSweepActive_,
                    .woodResourceBounce =
                        woodHudBounceRemaining_ > 0.0
                            ? static_cast<float>(
                                  std::sin(
                                      (1.0 -
                                       woodHudBounceRemaining_ /
                                           0.28) *
                                      PI) *
                                  10.0)
                            : 0.0F,
                    .stoneResourceBounce =
                        stoneHudBounceRemaining_ > 0.0
                            ? static_cast<float>(
                                  std::sin(
                                      (1.0 -
                                       stoneHudBounceRemaining_ /
                                           0.28) *
                                      PI) *
                                  10.0)
                            : 0.0F,
                    .goldResourceBounce = 0.0F,
                    .woodResourcePulse =
                        static_cast<float>(
                            woodHudBounceRemaining_ / 0.28),
                    .stoneResourcePulse =
                        static_cast<float>(
                            stoneHudBounceRemaining_ / 0.28),
                    .goldResourcePulse = 0.0F,
                    .coinResourceBounce =
                        coinHudBounceRemaining_ > 0.0
                            ? static_cast<float>(
                                  std::sin(
                                      (1.0 -
                                       coinHudBounceRemaining_ /
                                           0.32) *
                                      PI) *
                                  12.0)
                            : 0.0F,
                    .coinResourcePulse =
                        static_cast<float>(
                            coinHudBounceRemaining_ / 0.32),
                    .displayedInsight = displayedInsight_,
                    .insightPulse = insightPulseRemaining_ > 0.0
                        ? insightPulseRemaining_ / insightPulseDuration_ : 0.0,
                    .insightGainAmount = insightGainAmount_,
                    .insightGainRemaining = insightGainRemaining_,
                    .insightGainDuration = insightGainDuration_,
                    .treePointPulse = insightPointSequenceRemaining_ > 0.0
                        ? std::sin((1.0 - insightPointSequenceRemaining_ /
                            insightPointSequenceDuration_) * PI) : 0.0,
                    .objectivePulseId = objectivePulseId_,
                    .objectivePulse = objectivePulseRemaining_ > 0.0
                        ? objectivePulseRemaining_ /
                              objectivePulseDuration_
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
        if (foundationBuildMode_) {
            const char* pieceName = "FOUNDATION 2x2";
            int previewStorey = 0;
            bool previewValid = false;
            std::size_t plannedCount = 1U;
            std::optional<ModularPlacementError>
                previewError;
            switch (modularBuildPiece_) {
            case ModularBuildPiece::Foundation:
            case ModularBuildPiece::FloorPlatform:
                pieceName =
                    modularBuildPiece_ ==
                            ModularBuildPiece::Foundation
                        ? "FOUNDATION 2x2"
                        : "FLOOR PLATFORM 2x2";
                if (platformFramePreview_) {
                    previewValid =
                        platformFramePreview_->valid();
                    previewError =
                        platformFramePreview_->error;
                    previewStorey =
                        platformFramePreview_->storey;
                }
                if (!modularPlatformDragPreviews_.empty()) {
                    plannedCount =
                        modularPlatformDragPreviews_.size();
                    previewStorey =
                        modularPlatformDragPreviews_
                            .front()
                            .storey;
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
                pieceName = "WALL";
                if (wallPreview_) {
                    previewValid =
                        wallPreview_->valid();
                    previewError =
                        wallPreview_->error;
                    previewStorey =
                        wallPreview_->storey;
                }
                if (!modularWallDragPreviews_.empty()) {
                    plannedCount =
                        modularWallDragPreviews_.size();
                    previewStorey =
                        modularWallDragPreviews_
                            .front()
                            .storey;
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
                pieceName = "RAMP";
                if (rampPreview_) {
                    previewValid =
                        rampPreview_->valid();
                    previewError =
                        rampPreview_->error;
                    previewStorey =
                        rampPreview_->targetStorey;
                }
                if (!modularRampDragPreviews_.empty()) {
                    plannedCount =
                        modularRampDragPreviews_.size();
                    previewStorey =
                        modularRampDragPreviews_
                            .front()
                            .targetStorey;
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
            std::string pieceLabel = pieceName;
            if (modularDragPiece_) {
                pieceLabel += " x" +
                    std::to_string(plannedCount);
            }
            std::string foundationHint =
                pieceLabel + "   LEVEL " +
                std::to_string(previewStorey) +
                "   LMB DRAG";
            foundationHint += "   V PIECE";
            if (modularBuildPiece_ ==
                    ModularBuildPiece::Wall ||
                modularBuildPiece_ ==
                    ModularBuildPiece::Ramp) {
                foundationHint += "   WHEEL ROTATE";
            }
            foundationHint += "   RMB CANCEL";
            const Color messageColor =
                previewValid
                    ? Color{126, 239, 151, 255}
                    : Color{246, 112, 94, 255};
            drawCenteredUiText(
                foundationHint,
                static_cast<float>(
                    GetScreenHeight() / 2 + 76),
                18.0F, {245, 235, 214, 245});
            drawCenteredUiText(
                modularBuildPiece_ ==
                            ModularBuildPiece::
                                FloorPlatform &&
                        !previewError
                    ? "AIM AT PLATFORM OR RAMP"
                    : modularPlacementMessage(
                          previewError),
                static_cast<float>(
                    GetScreenHeight() / 2 + 102),
                18.0F, messageColor);
            if (modularDragPiece_) {
                const ResourceCost cost =
                    snapshot.modularBuildingCosts[
                        static_cast<std::size_t>(
                            *modularDragPiece_)];
                const int count =
                    static_cast<int>(plannedCount);
                const std::string lineCost =
                    std::to_string(count) +
                    " PIECES    W:" +
                    std::to_string(cost.wood * count) +
                    "  S:" +
                    std::to_string(cost.stone * count) +
                    "  C:" +
                    std::to_string(cost.gold * count);
                constexpr float Width = 470.0F;
                const float x =
                    static_cast<float>(GetScreenWidth()) *
                        0.5F -
                    Width * 0.5F;
                const float y =
                    static_cast<float>(GetScreenHeight()) *
                        0.5F +
                    130.0F;
                ui_.drawPanel(
                    {x, y, Width, 54.0F}, 230);
                const float fontSize = fitUiTextSize(
                    lineCost, 15.0F, 9.0F, Width - 24.0F);
                const float textWidth =
                    measureUiText(lineCost, fontSize).x;
                drawUiText(
                    lineCost,
                    {x + (Width - textWidth) * 0.5F,
                     y + 12.0F},
                    fontSize, {255, 235, 184, 255});
            }
        }
        if (wallDragStart_ && wallDragEnd_ &&
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
            const std::string lineCost =
                std::to_string(count) +
                " BUILDINGS    W:" +
                std::to_string(buildingCost.wood * count) +
                "  S:" +
                std::to_string(buildingCost.stone * count) +
                "  C:" +
                std::to_string(buildingCost.gold * count);
            constexpr float Width = 470.0F;
            const float x =
                static_cast<float>(GetScreenWidth()) * 0.5F -
                Width * 0.5F;
            const float y =
                static_cast<float>(GetScreenHeight()) * 0.5F +
                104.0F;
            ui_.drawPanel({x, y, Width, 58.0F}, 230);
            const float fontSize = fitUiTextSize(
                lineCost, 15.0F, 9.0F, Width - 24.0F);
            const float textWidth =
                measureUiText(lineCost, fontSize).x;
            drawUiText(
                lineCost,
                {x + (Width - textWidth) * 0.5F,
                 y + 14.0F},
                fontSize, {255, 235, 184, 255});
        }
        if (removalDragActive_) {
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
        if (!structuralRiskIds_.empty()) {
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
        drawResourceGainVisuals(camera);

        drawMinimapHud(
            ui_, presentationSnapshot,
            minimapHidden_ && minimapExpansion_ < 0.01F
                ? -1.0F
                : minimapExpansion_);

        drawRunStateOverlay(snapshot);
        if (snapshot.state == RunState::Paused &&
            !renderer_->graphicsPanelVisible() &&
            !skillTree_.isOpen()) {
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
            pendingReturnToMenuFromUi_ =
                ui_.drawButton(
                    {centerX - 190.0F, menuY,
                     380.0F, 58.0F},
                    "RETURN TO MAIN MENU") ||
                pendingReturnToMenuFromUi_;
            menuY += 70.0F;
            if (ui_.drawButton(
                    {centerX - 190.0F, menuY,
                     380.0F, 58.0F},
                    "EXIT GAME")) {
                exitRequested_ = true;
            }
        }
    }

    if (skillTree_.isOpen()) {
        skillTree_.draw(ui_);
    } else {
        drawBuildModePie();
        if (renderer_->graphicsPanelVisible()) {
            drawGraphicsPanel();
        }
        drawEnemySpawnMenu();
    }
    drawObjectiveDebugMenu(snapshot);
    if (performanceOverlayVisible_) {
        drawPerformanceOverlay(snapshot);
    }
    const bool uiCursorVisible =
        snapshot.state == RunState::MainMenu ||
        snapshot.state == RunState::Paused ||
        renderer_->graphicsPanelVisible() ||
        skillTree_.isOpen() || enemySpawnMenuVisible_;
    if (uiCursorVisible) {
        HideCursor();
        ui_.drawCursor();
    }
    renderer_->endFrame();
}


} // namespace ian
