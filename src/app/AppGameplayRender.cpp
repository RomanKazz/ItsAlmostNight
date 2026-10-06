#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
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
#include <string_view>

namespace ian {
using namespace app_detail;

void App::renderGameplayFrame(
    const SimulationSnapshot& snapshot,
    const SimulationSnapshot& presentationSnapshot,
    const SimulationSnapshot& feedbackSnapshot,
    std::optional<EntityId> pinnedOrdinaryBuilding,
    std::optional<EntityId> pinnedModularBuilding,
    PerformanceClock::time_point renderPreparationStart,
    RenderFrameTimings& timings) {
    const double visualYaw = snapshot.playerYaw;
    const auto arrivalCamera = playerArrival_.cameraMotion();
    const double visualPitch = snapshot.playerPitch +
        arrivalCamera.pitch * motionLandingIntensity_;
    const double cosPitch = std::cos(visualPitch);
    Vector3 position = {
        static_cast<float>(snapshot.playerPosition.x),
        static_cast<float>(
            groundCameraSmoothingInitialized_
                ? smoothedGroundCameraY_
                : snapshot.playerPosition.y),
        static_cast<float>(snapshot.playerPosition.z),
    };
    position.y += static_cast<float>(arrivalCamera.height +
        arrivalCamera.compression * motionLandingIntensity_);
    Vector3 forward = {
        static_cast<float>(std::sin(visualYaw) * cosPitch),
        static_cast<float>(std::sin(visualPitch)),
        static_cast<float>(-std::cos(visualYaw) * cosPitch),
    };
    const float cameraMotionScale =
        snapshot.runUpgradeChoicePending ? 0.0F : 1.0F;
    const float bobAmount =
        static_cast<float>(cameraBobAmount_) *
        (input_.sprint ? 1.12F : 1.0F) *
        motionBobIntensity_ * cameraMotionScale;
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
                motionSwayIntensity_ * cameraMotionScale));
    position.y += static_cast<float>(
        cameraLookPitchLag_ * 0.32) *
        motionSwayIntensity_ * cameraMotionScale;
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
                     motionSwayIntensity_ * cameraMotionScale))));
    if (cameraMotionScale > 0.0F &&
        landingResponseRemaining_ > 0.0 &&
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
                motionShakeIntensity_ * cameraMotionScale));
    position.y +=
        static_cast<float>(cameraImpulseOffset_.y) *
        motionShakeIntensity_ * cameraMotionScale;
    position = Vector3Add(
        position,
        Vector3Scale(
            forward,
            static_cast<float>(cameraImpulseOffset_.z) *
                motionShakeIntensity_ * cameraMotionScale));
    if (cameraMotionScale > 0.0F &&
        weaponRecoilRemaining_ > 0.0 &&
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
    if (cameraMotionScale > 0.0F &&
        cameraShakeRemaining_ > 0.0) {
        const double visualTime = GetTime();
        const float shake =
            static_cast<float>(cameraShakeStrength_ * cameraShakeRemaining_ / 0.35) *
            motionShakeIntensity_;
        position.x += static_cast<float>(std::sin(visualTime * 83.0)) * shake;
        position.y += static_cast<float>(std::cos(visualTime * 97.0)) * shake * 0.7F;
        position.z += static_cast<float>(std::sin(visualTime * 71.0)) * shake * 0.5F;
    }
    bobCameraUp = Vector3RotateByAxisAngle(
        bobCameraUp, forward,
        static_cast<float>(arrivalCamera.roll) * motionLandingIntensity_);
    const Camera3D camera = {
        .position = position,
        .target = Vector3Add(position, forward),
        .up = bobCameraUp,
        .fovy = cameraFov_ + arrivalCamera.fov * motionSwayIntensity_,
        .projection = CAMERA_PERSPECTIVE,
    };
    gameplayCamera_ = camera;
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
    } else if (snapshot.state == RunState::Wave ||
               snapshot.state == RunState::StageClear) {
        automaticTime = 0.75F;
    } else if (snapshot.state == RunState::WaveComplete ||
               snapshot.state == RunState::Victory) {
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
        .cloudShadowStrength = 0.18F * (1.0F - nightAmount),
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
    performanceStats_.renderPreparation.sample(
        performanceMilliseconds(renderPreparationStart));

    renderer_->beginGpuTimer(RendererGpuStage::World);
    const auto shadowStart = PerformanceClock::now();
    drawShadowPass(snapshot, lighting);
    performanceStats_.shadowPass.sample(
        performanceMilliseconds(shadowStart));

    const auto selectionStart = PerformanceClock::now();
    if constexpr (!MinimalWorldVisuals) {
        drawSelectionPass(feedbackSnapshot, camera);
    }
    performanceStats_.selectionPass.sample(
        performanceMilliseconds(selectionStart));

    const auto terrainStart = PerformanceClock::now();
    renderer_->beginWorldPass(environment.skyHorizon, camera);
    renderer_->drawSky(skyState);
    BeginMode3D(camera);
    renderer_->beginWorldShader(lighting);
    WorldMaterialState terrainMaterial{};
    terrainMaterial.terrainAmount = 1.0F;
    terrainMaterial.bakedAo = 0.9F;
    terrainMaterial.distantFadeAmount = 1.0F;
    renderer_->setWorldMaterial(terrainMaterial);
    renderer_->drawTerrain(
        ground, camera.position,
        showTerrainWireframe_, playerArrival_.mountainGrowth());
    performanceStats_.terrainRender.sample(
        performanceMilliseconds(terrainStart));

    const auto worldEntitiesStart = PerformanceClock::now();
    drawWorldEntities(presentationSnapshot, camera, nightAmount,
                      lighting,
                      static_cast<float>(fixedStep_.interpolationAlpha()));
    performanceStats_.worldEntitiesRender.sample(
        performanceMilliseconds(worldEntitiesStart));

    const auto environmentStart = PerformanceClock::now();
    if constexpr (!MinimalWorldVisuals) {
    const auto pondDecorStart = PerformanceClock::now();
    renderer_->beginWorldShader(lighting);
    WorldMaterialState pondRockMaterial{};
    pondRockMaterial.bakedAo = 0.76F;
    pondRockMaterial.screenAoAmount = 0.0F;
    renderer_->setWorldMaterial(pondRockMaterial);
    renderer_->drawPondShoreRocks(camera);
    WorldMaterialState pondPlantMaterial{};
    pondPlantMaterial.bakedAo = 0.82F;
    pondPlantMaterial.screenAoAmount = 0.0F;
    pondPlantMaterial.windAmount = 0.32F;
    pondPlantMaterial.localWindHeight = 1.0F;
    pondPlantMaterial.distantFadeAmount = 0.9F;
    pondPlantMaterial.vegetationAmount = 1.0F;
    pondPlantMaterial.inkOutlineEligible = false;
    renderer_->setWorldMaterial(pondPlantMaterial);
    renderer_->drawPondDecor(camera);
    renderer_->endWorldShader();
    double pondDecorMilliseconds =
        performanceMilliseconds(pondDecorStart);
    const auto waterStart = PerformanceClock::now();
    renderer_->drawWater(camera.position, lighting);
    performanceStats_.waterRender.sample(
        performanceMilliseconds(waterStart));
    const auto pondSurfaceStart = PerformanceClock::now();
    renderer_->beginWorldShader(lighting);
    WorldMaterialState pondSurfaceMaterial{};
    pondSurfaceMaterial.bakedAo = 0.82F;
    pondSurfaceMaterial.screenAoAmount = 0.0F;
    pondSurfaceMaterial.inkOutlineEligible = false;
    renderer_->setWorldMaterial(pondSurfaceMaterial);
    renderer_->drawPondSurfaceDecor(camera);
    renderer_->endWorldShader();
    pondDecorMilliseconds +=
        performanceMilliseconds(pondSurfaceStart);
    performanceStats_.pondDecorRender.sample(
        pondDecorMilliseconds);
    const auto cloudStart = PerformanceClock::now();
    renderer_->drawClouds(
        camera, nightAmount, lighting);
    performanceStats_.cloudRender.sample(
        performanceMilliseconds(cloudStart));
    const auto atmosphereStart = PerformanceClock::now();
    drawAtmosphereParticles(camera, nightAmount);
    performanceStats_.atmosphereRender.sample(
        performanceMilliseconds(atmosphereStart));
    performanceStats_.environmentRender.sample(
        performanceMilliseconds(environmentStart));
    }

    const auto overlayStart = PerformanceClock::now();
    drawBlobShadows(snapshot, camera);
    if constexpr (!MinimalWorldVisuals) {
        drawWorldOverlays(presentationSnapshot, lighting);
        drawPresentationEffects(camera, lighting);
    }
    EndMode3D();
    performanceStats_.overlayRender.sample(
        performanceMilliseconds(overlayStart));

    auto healthBarSnapshot = feedbackSnapshot;
    if (pinnedOrdinaryBuilding) {
        healthBarSnapshot.aimedBuilding =
            pinnedOrdinaryBuilding;
        healthBarSnapshot.aimedModularBuilding.reset();
    } else if (pinnedModularBuilding) {
        healthBarSnapshot.aimedBuilding.reset();
        healthBarSnapshot.aimedModularBuilding =
            pinnedModularBuilding;
    } else {
        if (healthBarSnapshot.aimedBuilding) {
            const auto building = std::ranges::find(
                healthBarSnapshot.buildings,
                *healthBarSnapshot.aimedBuilding,
                &BuildingInstance::id);
            if (building == healthBarSnapshot.buildings.end() ||
                building->health >= building->maxHealth) {
                healthBarSnapshot.aimedBuilding.reset();
            }
        }
        if (healthBarSnapshot.aimedModularBuilding) {
            const EntityId id =
                *healthBarSnapshot.aimedModularBuilding;
            const auto isDamaged = [id](const auto& instances) {
                const auto found = std::ranges::find_if(
                    instances, [id](const auto& instance) {
                        return instance.id == id;
                    });
                return found != instances.end() &&
                    found->health < found->maxHealth;
            };
            if (!isDamaged(healthBarSnapshot.platformFrames) &&
                !isDamaged(healthBarSnapshot.modularWalls) &&
                !isDamaged(healthBarSnapshot.ramps)) {
                healthBarSnapshot.aimedModularBuilding.reset();
            }
        }
    }
    if (!healthBarSnapshot.aimedBuilding &&
        !healthBarSnapshot.aimedModularBuilding &&
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
    renderer_->endGpuTimer(RendererGpuStage::World);
    renderer_->beginGpuTimer(RendererGpuStage::PostProcess);
    const auto postProcessStart = PerformanceClock::now();
    renderer_->setMenuDepthOfField(
        snapshot.runUpgradeChoicePending, 0.1F, 0.1F, 5.0F);
    renderer_->endWorldPass();
    renderer_->setMenuDepthOfField(false);
    performanceStats_.postProcess.sample(
        performanceMilliseconds(postProcessStart));

    // Selection is a readability overlay. Composite it after the world
    // ink pass so black contours from background objects cannot overwrite
    // the white interaction silhouette.
    if constexpr (!MinimalWorldVisuals) {
        renderer_->drawSelectionOutline();
        // Crack sprites are projected from their world-space tangent
        // planes after ink/selection composition. They therefore cannot
        // inherit world outlines while the viewmodel still covers them.
        drawResourceImpactOverlays(camera);
    }
    renderer_->endGpuTimer(RendererGpuStage::PostProcess);

    renderGameplayUi(
        snapshot, presentationSnapshot, feedbackSnapshot,
        healthBarSnapshot, camera, lighting,
        pinnedOrdinaryBuilding, pinnedModularBuilding, timings);
}

} // namespace ian
