#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace ian {
namespace {

constexpr int InitialWindowWidth = 1280;
constexpr int InitialWindowHeight = 720;
constexpr double BuildingHealthBarDwellSeconds = 0.15;
constexpr std::string_view UserSettingsPath =
    "user_settings/game_settings.json";

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

App::App()
    : simulation_(
          loadAppBalance(), loadAppMap(),
          loadAppWorldConfig()),
      environment_(loadAppEnvironment()) {
    static_cast<void>(loadUserSettings(
        UserSettingsPath, userSettings_));
    audio_.settings() = userSettings_.audio;
    motionBobIntensity_ = userSettings_.motion.bobIntensity;
    motionShakeIntensity_ = userSettings_.motion.shakeIntensity;
    motionLandingIntensity_ = userSettings_.motion.landingIntensity;
    motionSwayIntensity_ = userSettings_.motion.swayIntensity;
    static_cast<void>(loadFirstPersonToolTuning(
        "user_settings/first_person_tool.json", toolTuning_));
    effects_.reserve(128);
    arrowVisuals_.reserve(64);
    damageIndicators_.reserve(12);
    floatingDamageNumbers_.reserve(32);
    resourceGainVisuals_.reserve(16);
    destroyedResourceVisuals_.reserve(8);
    destroyedEnemyVisuals_.reserve(16);
    buildingShotRecoilVisuals_.reserve(32);
}

int App::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT |
                   FLAG_MSAA_4X_HINT);
    InitWindow(InitialWindowWidth, InitialWindowHeight,
               "It's Almost Night");
    SetExitKey(KEY_NULL);
    ToggleBorderlessWindowed();
    SetTargetFPS(144);
    renderer_.emplace();
    renderer_->settings() = userSettings_.graphics;
    renderer_->initialize();
    modularBuildingRenderer_.setRenderer(&*renderer_);
    renderer_->rebuildTerrain(simulation_.terrain());
    ui_.initialize();
    audio_.initialize();

    while (!WindowShouldClose()) {
        processInput();
        update();
        render();
        persistUserSettings();
    }

    persistUserSettings(true);
    static_cast<void>(saveFirstPersonToolTuning(
        "user_settings/first_person_tool.json", toolTuning_));

    ui_.shutdown();
    audio_.shutdown();
    modularBuildingRenderer_.setRenderer(nullptr);
    renderer_->shutdown();
    renderer_.reset();
    CloseWindow();
    return 0;
}

void App::persistUserSettings(bool force) {
    if (!renderer_) {
        return;
    }
    UserSettings current{
        .graphics = renderer_->settings(),
        .audio = audio_.settings(),
        .motion = {
            .bobIntensity = motionBobIntensity_,
            .shakeIntensity = motionShakeIntensity_,
            .landingIntensity = motionLandingIntensity_,
            .swayIntensity = motionSwayIntensity_,
        },
    };
    if (current == userSettings_ ||
        (!force && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        return;
    }
    if (saveUserSettings(UserSettingsPath, current)) {
        userSettings_ = current;
    }
}

void App::render() {
    const auto snapshot = simulation_.snapshot();
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
    structuralRiskIds_ =
        simulation_.structuralCollapseRisk(
            structuralRiskRoots);
    auto presentationSnapshot = snapshot;
    presentationSnapshot.aimedResource = hoveredResource_;
    presentationSnapshot.aimedBuilding = hoveredBuilding_;
    presentationSnapshot.aimedEnemy = hoveredEnemy_;
    presentationSnapshot.aimedBuildingUpgradeCost =
        hoveredBuildingUpgradeCost_;
    presentationSnapshot.aimedBuildingStats =
        hoveredBuildingStats_;

    if (snapshot.state == RunState::MainMenu) {
        renderer_->beginUiOnlyFrame({18, 22, 31, 255});
        const float centerX =
            static_cast<float>(GetScreenWidth()) * 0.5F;
        const float centerY =
            static_cast<float>(GetScreenHeight()) * 0.5F;
        ui_.drawPanel({centerX - 420.0F, centerY - 250.0F,
                       840.0F, 500.0F});
        ui_.drawInsetPanel({centerX - 380.0F, centerY - 164.0F,
                            760.0F, 128.0F});
        drawCentered("IT'S ALMOST NIGHT",
                     static_cast<int>(centerY) - 140, 42,
                     {245, 220, 174, 255});
        pendingStartFromUi_ =
            ui_.drawButton({centerX - 200.0F, centerY + 4.0F,
                            400.0F, 72.0F},
                           "START RUN") ||
            pendingStartFromUi_;
        pendingOpenSkillTreeFromUi_ =
            ui_.drawButton(
                {centerX - 200.0F, centerY + 90.0F,
                 400.0F, 72.0F},
                "TREE OF KNOWLEDGE") ||
            pendingOpenSkillTreeFromUi_;
        drawCentered("ENTER  •  K: TREE",
                     static_cast<int>(centerY) + 196, 16,
                     {199, 174, 142, 255});
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
        drawSelectionPass(presentationSnapshot, camera);
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
                          lighting);
        renderer_->drawClouds(
            camera.position, nightAmount, lighting);
        drawAtmosphereParticles(camera, nightAmount);
        drawBlobShadows(snapshot, camera);
        drawWorldOverlays(presentationSnapshot, lighting);
        drawPresentationEffects();
        EndMode3D();
        renderer_->drawSelectionOutline();

        auto healthBarSnapshot = presentationSnapshot;
        if (healthBarSnapshot.aimedBuilding &&
            buildingHoverSeconds_ <
                BuildingHealthBarDwellSeconds) {
            healthBarSnapshot.aimedBuilding.reset();
        }
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
            graphicsPanelTab_ == 4;
        const bool showFirstPersonTool =
            tuningPreview ||
            (snapshot.selectedWeapon == PlayerWeapon::Pickaxe &&
             !snapshot.selectedBuilding &&
             !foundationBuildMode_ &&
             !snapshot.playerRespawning);
        if (showFirstPersonTool) {
            const bool useAxe = displayedToolUsesAxe_;
            const float swingProgress =
                toolSwingRemaining_ > 0.0 &&
                        toolSwingDuration_ > 0.0
                    ? std::clamp(
                          static_cast<float>(
                              1.0 - toolSwingRemaining_ /
                                        toolSwingDuration_),
                          0.0F, 1.0F)
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
                const float halfProgress =
                    progress < 0.5F
                        ? progress * 2.0F
                        : (1.0F - progress) * 2.0F;
                const float smooth =
                    halfProgress * halfProgress *
                    (3.0F - 2.0F * halfProgress);
                renderTuning.position.y -=
                    toolTuning_.swapDrop * smooth;
            }
            if (renderer_->beginFirstPersonToolPass()) {
                BeginMode3D(viewModelCamera);
                static_cast<void>(renderer_->drawFirstPersonTool(
                    useAxe ? FirstPersonToolVisual::Axe
                           : FirstPersonToolVisual::Pickaxe,
                    swingProgress,
                    static_cast<float>(cameraBobPhase_),
                    static_cast<float>(cameraBobAmount_),
                    renderTuning));
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
            simulation_.terrain());
        drawProductionVisuals(camera);
        rlEnableDepthTest();
        EndMode3D();

        if (playerDamageFlashRemaining_ > 0.0) {
            const auto alpha = static_cast<unsigned char>(
                90.0 * playerDamageFlashRemaining_ / 0.18);
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          {190, 24, 24, alpha});
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
            camera);
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
                const float textWidth =
                    measureUiText(lineCost, 15.0F).x;
                drawUiText(
                    lineCost,
                    {x + (Width - textWidth) * 0.5F,
                     y + 12.0F},
                    15.0F, {255, 235, 184, 255});
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
            const float textWidth =
                measureUiText(lineCost, 15.0F).x;
            drawUiText(
                lineCost,
                {x + (Width - textWidth) * 0.5F,
                 y + 14.0F},
                15.0F, {255, 235, 184, 255});
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
            minimapExpansion_);

        drawRunStateOverlay(snapshot);
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
    renderer_->endFrame();
}

void App::drawBuildModePie() const {
    if (!buildModePieVisible_) {
        return;
    }

    constexpr float OuterRadius = 150.0F;
    constexpr float InnerRadius = 42.0F;
    constexpr float ArrowRadius = 78.0F;
    const Vector2 center{
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    const bool buildingsSelected =
        buildModePieChoice_ ==
        BuildModePieChoice::Buildings;
    const bool foundationsSelected =
        buildModePieChoice_ ==
        BuildModePieChoice::Foundations;

    DrawCircleV(center, OuterRadius + 7.0F,
                {247, 224, 173, 95});
    DrawCircleV(center, OuterRadius,
                {15, 18, 25, 238});
    DrawCircleSector(
        center, OuterRadius - 5.0F, 90.0F,
        270.0F, 48,
        buildingsSelected
            ? Color{239, 197, 101, 225}
            : Color{52, 62, 78, 220});
    DrawCircleSector(
        center, OuterRadius - 5.0F, -90.0F,
        90.0F, 48,
        foundationsSelected
            ? Color{239, 197, 101, 225}
            : Color{52, 62, 78, 220});
    DrawLineEx(
        {center.x, center.y - OuterRadius + 5.0F},
        {center.x, center.y - InnerRadius},
        3.0F, {20, 24, 32, 180});
    DrawLineEx(
        {center.x, center.y + InnerRadius},
        {center.x, center.y + OuterRadius - 5.0F},
        3.0F, {20, 24, 32, 180});
    DrawCircleV(center, InnerRadius + 4.0F,
                {247, 224, 173, 130});
    DrawCircleV(center, InnerRadius,
                {20, 24, 32, 255});

    const auto drawLabel =
        [](std::string_view label, Vector2 position,
           bool selected, bool activeMode) {
            const Color color =
                selected
                    ? Color{31, 27, 20, 255}
                    : Color{242, 232, 211, 255};
            const Vector2 size =
                measureUiText(label, 16.0F);
            drawUiText(
                label,
                {position.x - size.x * 0.5F,
                 position.y - size.y * 0.5F},
                16.0F, color);
            if (activeMode) {
                DrawCircleV(
                    {position.x,
                     position.y + 37.0F},
                    5.0F,
                    selected
                        ? Color{31, 27, 20, 255}
                        : Color{239, 197, 101, 255});
            }
        };
    drawLabel("BUILDINGS",
              {center.x - 93.0F, center.y},
              buildingsSelected,
              !foundationBuildMode_);
    drawLabel("PLATFORMS",
              {center.x + 93.0F, center.y},
              foundationsSelected,
              foundationBuildMode_);

    const float length =
        Vector2Length(buildModePieDirection_);
    if (length > 1.0F) {
        const Vector2 direction =
            Vector2Scale(
                buildModePieDirection_,
                1.0F / length);
        const Vector2 arrowCenter =
            Vector2Add(
                center,
                Vector2Scale(direction, ArrowRadius));
        const Vector2 tip =
            Vector2Add(
                arrowCenter,
                Vector2Scale(direction, 17.0F));
        const Vector2 arrowBase =
            Vector2Subtract(
                arrowCenter,
                Vector2Scale(direction, 2.0F));
        const Vector2 tail =
            Vector2Subtract(
                arrowCenter,
                Vector2Scale(direction, 13.0F));
        const Vector2 perpendicular{
            -direction.y, direction.x};
        const Color arrowColor{
            255, 247, 224, 255};
        DrawLineEx(tail, arrowBase, 7.0F,
                   arrowColor);
        DrawTriangle(
            tip,
            Vector2Add(
                arrowBase,
                Vector2Scale(perpendicular, 9.0F)),
            Vector2Subtract(
                arrowBase,
                Vector2Scale(perpendicular, 9.0F)),
            arrowColor);
    } else {
        DrawCircleV(center, 7.0F,
                    {255, 247, 224, 255});
    }

    drawCenteredUiText(
        "HOLD TAB  |  RELEASE TO SELECT",
        center.y + OuterRadius + 20.0F,
        14.0F, {242, 232, 211, 235});
}


} // namespace ian
