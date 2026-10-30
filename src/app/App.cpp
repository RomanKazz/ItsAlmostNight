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
#include <bit>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

namespace ian {
namespace {

constexpr int InitialWindowWidth = 1280;
constexpr int InitialWindowHeight = 720;
constexpr std::string_view UserSettingsPath =
    "user_settings/game_settings.json";
constexpr std::string_view MetaProgressionPath =
    "user_settings/meta_progression.json";

QuestSystem loadAppQuests() {
    try {
        return QuestSystem(loadQuestDefinitions("assets/data/meta_quests.json"));
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "QUESTS: using built-in catalog: %s", error.what());
        return QuestSystem{};
    }
}

std::uint64_t mixDecorationFingerprint(
    std::uint64_t hash, std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    return hash ^ (value + (hash << 6U) + (hash >> 2U));
}

std::uint64_t decorationFingerprint(
    const SimulationSnapshot& snapshot) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const ResourceNode& resource : snapshot.resourceNodes) {
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(resource.position.x));
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(resource.position.z));
    }
    for (const LootChestInstance& chest : snapshot.lootChests) {
        if (chest.looseLoot) continue;
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(chest.position.x));
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(chest.position.z));
    }
    for (const ChallengeColumnInstance& column : snapshot.challengeColumns) {
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(column.position.x));
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(column.position.z));
    }
    for (const WorldLandmarkInstance& landmark : snapshot.worldLandmarks) {
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(landmark.position.x));
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(landmark.position.z));
    }
    for (const auto& scenery : snapshot.worldScenery) {
        hash = mixDecorationFingerprint(hash, scenery.id.index);
        hash = mixDecorationFingerprint(hash, std::bit_cast<std::uint64_t>(scenery.position.x));
        hash = mixDecorationFingerprint(hash, std::bit_cast<std::uint64_t>(scenery.position.z));
    }
    return hash;
}

std::vector<DecorationExclusion> makeDecorationExclusions(
    const SimulationSnapshot& snapshot) {
    std::vector<DecorationExclusion> exclusions;
    exclusions.reserve(
        snapshot.resourceNodes.size() + snapshot.lootChests.size() +
        snapshot.mapObstacles.size() + snapshot.challengeColumns.size() +
        snapshot.worldLandmarks.size() + snapshot.worldScenery.size());
    for (const ResourceNode& resource : snapshot.resourceNodes) {
        // Include inactive nodes: their current position remains reserved
        // until the resource actually relocates on respawn.
        exclusions.push_back({
            .shape = DecorationExclusionShape::Circle,
            .centerX = resource.position.x,
            .centerZ = resource.position.z,
            .radius = std::max(resource.radius, 0.0),
        });
    }
    for (const LootChestInstance& chest : snapshot.lootChests) {
        if (chest.looseLoot) continue;
        exclusions.push_back({
            .shape = DecorationExclusionShape::Circle,
            .centerX = chest.position.x,
            .centerZ = chest.position.z,
            .radius = 0.82,
        });
    }
    for (const ChallengeColumnInstance& column : snapshot.challengeColumns) {
        exclusions.push_back({
            .shape = DecorationExclusionShape::Circle,
            .centerX = column.position.x,
            .centerZ = column.position.z,
            .radius = 2.2,
        });
    }
    for (const WorldLandmarkInstance& landmark : snapshot.worldLandmarks) {
        exclusions.push_back({
            .shape = DecorationExclusionShape::Circle,
            .centerX = landmark.position.x,
            .centerZ = landmark.position.z,
            .radius = landmark.collisionRadius + 1.5,
        });
    }
    for (const auto& scenery : snapshot.worldScenery) {
        const auto box = worldSceneryCollisionBox(scenery);
        exclusions.push_back({.shape = DecorationExclusionShape::Rectangle,
            .centerX = scenery.position.x, .centerZ = scenery.position.z,
            .halfWidth = (box.maxX-box.minX)*.5, .halfDepth = (box.maxZ-box.minZ)*.5});
    }
    for (const MapObstacle& obstacle : snapshot.mapObstacles) {
        exclusions.push_back({
            .shape = DecorationExclusionShape::Rectangle,
            .centerX =
                (obstacle.collision.minX + obstacle.collision.maxX) * 0.5,
            .centerZ =
                (obstacle.collision.minZ + obstacle.collision.maxZ) * 0.5,
            .halfWidth =
                (obstacle.collision.maxX - obstacle.collision.minX) * 0.5,
            .halfDepth =
                (obstacle.collision.maxZ - obstacle.collision.minZ) * 0.5,
        });
    }
    return exclusions;
}

} // namespace

using namespace app_detail;

FirstPersonToolTuning& App::activeToolTuning() {
    FirstPersonToolVisual visual = displayedToolVisual_;
    if (renderer_ && renderer_->graphicsPanelVisible() &&
        debugSettings_ && graphicsPanelTab_ == ToolSettingsTab) {
        visual = toolPanelPreviewVisual_;
    }
    const std::size_t index = visual == FirstPersonToolVisual::None
        ? static_cast<std::size_t>(FirstPersonToolVisual::Axe)
        : static_cast<std::size_t>(visual);
    return toolTunings_[index];
}

const FirstPersonToolTuning& App::activeToolTuning() const {
    FirstPersonToolVisual visual = displayedToolVisual_;
    if (renderer_ && renderer_->graphicsPanelVisible() &&
        debugSettings_ && graphicsPanelTab_ == ToolSettingsTab) {
        visual = toolPanelPreviewVisual_;
    }
    const std::size_t index = visual == FirstPersonToolVisual::None
        ? static_cast<std::size_t>(FirstPersonToolVisual::Axe)
        : static_cast<std::size_t>(visual);
    return toolTunings_[index];
}

double App::toolSwingDurationFor(
    const SimulationSnapshot& snapshot,
    FirstPersonToolVisual visual) const {
    const bool quickTool = visual == FirstPersonToolVisual::Axe ||
        visual == FirstPersonToolVisual::Pickaxe;
    const double speed = quickTool
        ? snapshot.upgradeStats.toolAttackSpeed
        : snapshot.upgradeStats.playerAttackSpeed;
    return static_cast<double>(activeToolTuning().swingDuration) /
        std::max(0.05, speed);
}

App::App()
    : simulation_(
      loadAppBalance(), loadAppMap(),
          loadAppWorldConfig(),
          loadAppInsightConfig(), loadAppObjectives()),
      environment_(loadAppEnvironment()),
      questSystem_(loadAppQuests()) {
    static_cast<void>(loadUserSettings(
        UserSettingsPath, userSettings_));
    static_cast<void>(loadMetaProgression(
        MetaProgressionPath, metaProgression_));
    persistedUserSettings_ = userSettings_;
    persistedMetaProgression_ = metaProgression_;
    static_cast<void>(questSystem_.refresh(metaProgression_));
    persistMetaProgression();
    initializeLocalization();
    setLanguage(userSettings_.language);
    audio_.settings() = userSettings_.audio;
    motionBobIntensity_ = userSettings_.motion.bobIntensity;
    motionShakeIntensity_ = userSettings_.motion.shakeIntensity;
    motionLandingIntensity_ = userSettings_.motion.landingIntensity;
    motionSwayIntensity_ = userSettings_.motion.swayIntensity;
    for (int visual = static_cast<int>(FirstPersonToolVisual::Axe);
         visual <= static_cast<int>(FirstPersonToolVisual::Bomb);
         ++visual) {
        const auto toolVisual = static_cast<FirstPersonToolVisual>(visual);
        toolTunings_[static_cast<std::size_t>(visual)] =
            defaultFirstPersonToolTuning(toolVisual);
    }
    effects_.reserve(128);
    gameEventBuffer_.reserve(256);
    damageIndicators_.reserve(12);
    floatingDamageNumbers_.reserve(32);
    resourceGainVisuals_.reserve(16);
    destroyedResourceVisuals_.reserve(8);
    destroyedEnemyVisuals_.reserve(16);
    buildingShotRecoilVisuals_.reserve(32);
}

int App::run() {
    // Build the saved menu view before the window appears. When no menu
    // view is saved, prebuild the suspended run's terrain for Continue.
    const bool hasMenuBackgroundView = loadMenuBackgroundView();
    if (hasMenuBackgroundView && menuBackgroundTerrainSeed_) {
        simulation_.regenerateTerrain(
            *menuBackgroundTerrainSeed_);
    } else if (const auto seed = suspendedRunTerrainSeed()) {
        simulation_.regenerateTerrain(*seed);
    }
    // Native HiDPI output multiplies the final framebuffer area on Retina
    // displays even though the world target keeps its configured resolution.
    // Keep the standard-density presentation by default so UI and final
    // compositing do not consume several extra milliseconds every frame.
    unsigned int windowFlags = FLAG_WINDOW_RESIZABLE;
    if (std::getenv("IAN_HIDPI") != nullptr) {
        windowFlags |= FLAG_WINDOW_HIGHDPI;
    }
    // On macOS the multisampled default framebuffer also avoids a driver
    // presentation path that can throttle a single-sample fullscreen
    // drawable to the ProMotion refresh cadence even with swap interval 0.
    // The 3D world still renders to its own target; this primarily affects
    // final HUD geometry and presentation.
    if (userSettings_.graphics.quality == GraphicsQuality::High) {
        windowFlags |= FLAG_MSAA_4X_HINT;
    }
    SetConfigFlags(windowFlags);
    InitWindow(InitialWindowWidth, InitialWindowHeight,
               "FORTBONK");
    SetExitKey(KEY_NULL);
    applyFullscreenSetting(userSettings_.graphics.fullscreen);
    renderer_.emplace();
    renderer_->settings() = userSettings_.graphics;
    renderer_->applyFrameRateLimit();
    renderer_->initialize();
    static_cast<void>(vfxImpactShader_.load(
        nullptr, "assets/shaders/vfx_impact.fs"));
    if (vfxImpactShader_.valid()) {
        vfxImpactUseTextureLocation_ = GetShaderLocation(
            vfxImpactShader_.get(), "useTexture");
    }
    static_cast<void>(vfxImpactTexture_.load(
        "assets/vfx/brackeys/impact_6x5.png"));
    static_cast<void>(vfxElectricRingTexture_.load(
        "assets/vfx/brackeys/electric_ring_6x5.png"));
    static_cast<void>(resourceImpactTexture_.load(
        "assets/vfx/stone_crack.png"));
    for (TextureResource* texture : {
             &vfxImpactTexture_,
             &vfxElectricRingTexture_, &resourceImpactTexture_}) {
        if (texture->valid()) {
            SetTextureFilter(texture->get(), TEXTURE_FILTER_BILINEAR);
        }
    }
    modularBuildingRenderer_.setRenderer(&*renderer_);
    rebuildTerrainGraphics();
    ui_.initialize();
    audio_.initialize();
    performanceLoggingApplied_ =
        renderer_->settings().performanceLogging;
    if (performanceLoggingApplied_) {
        static_cast<void>(performanceRecorder_.start(
            "performance_logs"));
    }

    PerformanceClock::time_point previousFrameStart{};
    bool hasPreviousFrameStart = false;
    while (!WindowShouldClose() && !exitRequested_) {
        const auto frameStart = PerformanceClock::now();
        const bool windowFocusedAtFrameStart = IsWindowFocused();
        const double frameIntervalMilliseconds = hasPreviousFrameStart
            ? performanceMilliseconds(previousFrameStart, frameStart)
            : 0.0;
        previousFrameStart = frameStart;
        hasPreviousFrameStart = true;
        const RunState stateBeforeInput = simulation_.snapshot().state;
        performanceStats_.minimapPreparation.sample(0.0);
        const auto inputStart = PerformanceClock::now();
        processInput();
        performanceStats_.input.sample(
            performanceMilliseconds(inputStart));
        update();
        performanceStats_.worldObjectTimings.fill(0.0);
        const auto renderStart = PerformanceClock::now();
        render();
        const double renderMilliseconds =
            performanceMilliseconds(renderStart);
        performanceStats_.render.sample(
            std::max(
                0.0,
                renderMilliseconds -
                    renderer_->framePacingMilliseconds()));
        performanceStats_.frame.sample(
            performanceMilliseconds(frameStart));
        const bool performanceLoggingRequested =
            renderer_->settings().performanceLogging;
        if (performanceLoggingRequested !=
            performanceLoggingApplied_) {
            performanceLoggingApplied_ =
                performanceLoggingRequested;
            if (performanceLoggingRequested) {
                static_cast<void>(performanceRecorder_.start(
                    "performance_logs"));
            } else {
                performanceRecorder_.stop();
            }
        }
        if (performanceRecorder_.active()) {
            const SimulationSnapshot& performanceSnapshot =
                simulation_.snapshot();
            const EnemyPerformanceStats& enemyStats =
                simulation_.enemyPerformanceStats();
            const RendererPerformanceStats& rendererStats =
                renderer_->performanceStats();
            const GraphicsSettings& graphicsSettings =
                renderer_->settings();
            const bool hadSimulationTick =
                performanceStats_.fixedTicks > 0U;
            const int selectionKind =
                performanceSnapshot.buildingPreview ? 9 :
                performanceSnapshot.aimedResource ? 1 :
                performanceSnapshot.aimedEnemy ? 2 :
                performanceSnapshot.aimedBuilding ? 3 :
                performanceSnapshot.aimedChest ? 4 :
                performanceSnapshot.aimedModularBuilding ? 5 :
                performanceSnapshot.aimedChallengeColumn ? 6 :
                performanceSnapshot.aimedWorldLandmark ? 7 :
                performanceSnapshot.aimedLoot ? 8 : 0;
            performanceRecorder_.record({
                .frame = performanceFrameIndex_++,
                .sessionSeconds = GetTime(),
                .runState = static_cast<int>(performanceSnapshot.state),
                .buildMode = performanceSnapshot.selectedBuilding.has_value(),
                .modularBuildMode = foundationBuildMode_,
                .fixedTicks = performanceStats_.fixedTicks,
                .activeEnemies = performanceSnapshot.activeEnemyCount,
                .visibleEnemies = performanceStats_.visibleEnemies,
                .buildings = performanceSnapshot.buildings.size(),
                .modularPieces =
                    performanceSnapshot.platformFrames.size() +
                    performanceSnapshot.modularWalls.size() +
                    performanceSnapshot.ramps.size(),
                .frameMs = performanceStats_.frame.lastMilliseconds,
                .inputMs = performanceStats_.input.lastMilliseconds,
                .updateMs = performanceStats_.simulation.lastMilliseconds,
                .simulationTickMs =
                    hadSimulationTick
                        ? performanceStats_.simulationTick.lastMilliseconds
                        : 0.0,
                .renderMs = performanceStats_.render.lastMilliseconds,
                .presentMs = performanceStats_.present.lastMilliseconds,
                .presentPacingMs =
                    renderer_->framePacingMilliseconds(),
                .presentSwapMs =
                    renderer_->frameSwapMilliseconds(),
                .renderPreparationMs =
                    performanceStats_.renderPreparation.lastMilliseconds,
                .shadowMs = performanceStats_.shadowPass.lastMilliseconds,
                .selectionMs =
                    performanceStats_.selectionPass.lastMilliseconds,
                .terrainMs =
                    performanceStats_.terrainRender.lastMilliseconds,
                .worldObjectsMs =
                    performanceStats_.worldEntitiesRender.lastMilliseconds,
                .decorationsMs =
                    performanceStats_.decorationsRender.lastMilliseconds,
                .grassMs =
                    performanceStats_.grassRender.lastMilliseconds,
                .environmentMs =
                    performanceStats_.environmentRender.lastMilliseconds,
                .pondDecorMs =
                    performanceStats_.pondDecorRender.lastMilliseconds,
                .waterMs =
                    performanceStats_.waterRender.lastMilliseconds,
                .cloudMs =
                    performanceStats_.cloudRender.lastMilliseconds,
                .atmosphereMs =
                    performanceStats_.atmosphereRender.lastMilliseconds,
                .overlaysMs =
                    performanceStats_.overlayRender.lastMilliseconds,
                .postProcessMs =
                    performanceStats_.postProcess.lastMilliseconds,
                .uiMs = performanceStats_.uiRender.lastMilliseconds,
                .viewModelMs =
                    performanceStats_.viewModelRender.lastMilliseconds,
                .worldUiMs =
                    performanceStats_.worldUiRender.lastMilliseconds,
                .interactionPromptMs =
                    performanceStats_.interactionPromptRender
                        .lastMilliseconds,
                .hudCoreMs =
                    performanceStats_.hudCoreRender.lastMilliseconds,
                .hudTargetHealthMs =
                    performanceStats_.hudTargetHealthRender.lastMilliseconds,
                .hudResourcesMs =
                    performanceStats_.hudResourcesRender.lastMilliseconds,
                .hudVitalityMs =
                    performanceStats_.hudVitalityRender.lastMilliseconds,
                .hudInformationMs =
                    performanceStats_.hudInformationRender.lastMilliseconds,
                .hudObjectivesMs =
                    performanceStats_.hudObjectivesRender.lastMilliseconds,
                .hudLootMs =
                    performanceStats_.hudLootRender.lastMilliseconds,
                .hudPhaseMs =
                    performanceStats_.hudPhaseRender.lastMilliseconds,
                .hudHotbarMs =
                    performanceStats_.hudHotbarRender.lastMilliseconds,
                .hudFeedbackMs =
                    performanceStats_.hudFeedbackRender.lastMilliseconds,
                .hudTextFlushMs =
                    performanceStats_.hudTextFlushRender.lastMilliseconds,
                .resourceGainMs =
                    performanceStats_.resourceGainRender.lastMilliseconds,
                .minimapMs =
                    performanceStats_.minimapRender.lastMilliseconds,
                .minimapSetupMs =
                    performanceStats_.minimapSetupRender.lastMilliseconds,
                .minimapTargetRefreshMs =
                    performanceStats_.minimapTargetRefreshRender
                        .lastMilliseconds,
                .minimapFrameMs =
                    performanceStats_.minimapFrameRender.lastMilliseconds,
                .minimapLabelsMs =
                    performanceStats_.minimapLabelsRender.lastMilliseconds,
                .uiOverlaysMs =
                    performanceStats_.uiOverlaysRender.lastMilliseconds,
                .uiTextMs =
                    performanceStats_.uiTextRender.lastMilliseconds,
                .uiTextSubmissions =
                    uiTextPerformanceStats().submissions,
                .uiTextCharacters =
                    uiTextPerformanceStats().characters,
                .gpuWorldMs =
                    rendererStats.gpuWorld.lastMilliseconds,
                .gpuPostProcessMs =
                    rendererStats.gpuPostProcess.lastMilliseconds,
                .gpuViewModelMs =
                    rendererStats.gpuViewModel.lastMilliseconds,
                .gpuViewModelTargetMs =
                    rendererStats.gpuViewModelTarget.lastMilliseconds,
                .gpuViewModelCompositeMs =
                    rendererStats.gpuViewModelComposite.lastMilliseconds,
                .gpuUiMs =
                    rendererStats.gpuUi.lastMilliseconds,
                .enemyAiMs = hadSimulationTick
                    ? enemyStats.tick.lastMilliseconds : 0.0,
                .enemyCollisionMs = hadSimulationTick
                    ? enemyStats.collision.lastMilliseconds : 0.0,
                .enemyDrawMs =
                    performanceStats_.enemyRender.lastMilliseconds,
                .blobShadowsMs =
                    performanceStats_.blobShadows.lastMilliseconds,
                .windowWidth = GetScreenWidth(),
                .windowHeight = GetScreenHeight(),
                .renderWidth = graphicsSettings.renderWidth,
                .renderHeight = graphicsSettings.renderHeight,
                .frameRateLimit = graphicsSettings.frameRateLimit,
                .graphicsQuality =
                    static_cast<int>(graphicsSettings.quality),
                .selectionKind = selectionKind,
                .resourceNodes = performanceSnapshot.resourceNodes.size(),
                .coinPickups = performanceSnapshot.coinPickups.size(),
                .lootChests = performanceSnapshot.lootChests.size(),
                .presentationEffects = effects_.size(),
                .enemyProjectiles =
                    performanceSnapshot.enemyProjectiles.size(),
                .instancedEnemies = rendererStats.instancedEnemyCount,
                .enemyBatches = rendererStats.enemyBatchCount,
                .lowDetailEnemies = rendererStats.lowDetailEnemyCount,
                .blobShadowCount = rendererStats.blobShadowCount,
                .blobShadowTriangles =
                    rendererStats.blobShadowTriangles,
                .enemyFullAiUpdates = hadSimulationTick
                    ? enemyStats.fullAiUpdates : 0U,
                .enemyThrottledAiMoves = hadSimulationTick
                    ? enemyStats.throttledAiMoves : 0U,
                .enemySpatialRebuilds = hadSimulationTick
                    ? enemyStats.spatialRebuilds : 0U,
                .enemyStructureGridRebuilds = hadSimulationTick
                    ? enemyStats.structureGridRebuilds : 0U,
                .enemySpatialRebuildMs = hadSimulationTick
                    ? enemyStats.spatialRebuild.lastMilliseconds : 0.0,
                .simulationTickTotalMs =
                    performanceStats_.simulationTickTotal.lastMilliseconds,
                .selectionCompositeMs =
                    performanceStats_.selectionComposite.lastMilliseconds,
                .resourceImpactOverlaysMs =
                    performanceStats_.resourceImpactOverlays
                        .lastMilliseconds,
                .gpuTimersSupported = rendererStats.gpuTimersSupported,
                .gpuWorldSamples = rendererStats.gpuWorld.sampleCount,
                .gpuPostProcessSamples =
                    rendererStats.gpuPostProcess.sampleCount,
                .gpuUiSamples = rendererStats.gpuUi.sampleCount,
                .frameIntervalMs = frameIntervalMilliseconds,
                .gpuShadowMs =
                    rendererStats.gpuShadow.lastMilliseconds,
                .gpuSelectionMs =
                    rendererStats.gpuSelection.lastMilliseconds,
                .gpuSceneMs =
                    rendererStats.gpuScene.lastMilliseconds,
                .gpuShadowSamples =
                    rendererStats.gpuShadow.sampleCount,
                .gpuSelectionSamples =
                    rendererStats.gpuSelection.sampleCount,
                .gpuSceneSamples =
                    rendererStats.gpuScene.sampleCount,
                .gpuTerrainMs =
                    rendererStats.gpuTerrain.lastMilliseconds,
                .gpuObjectsMs =
                    rendererStats.gpuObjects.lastMilliseconds,
                .gpuEnvironmentMs =
                    rendererStats.gpuEnvironment.lastMilliseconds,
                .gpuOverlaysMs =
                    rendererStats.gpuOverlays.lastMilliseconds,
                .gpuTerrainSamples =
                    rendererStats.gpuTerrain.sampleCount,
                .gpuObjectsSamples =
                    rendererStats.gpuObjects.sampleCount,
                .gpuEnvironmentSamples =
                    rendererStats.gpuEnvironment.sampleCount,
                .gpuOverlaysSamples =
                    rendererStats.gpuOverlays.sampleCount,
                .worldObjectTimings = performanceStats_.worldObjectTimings,
                .gpuViewModelTargetSamples = rendererStats.gpuViewModelTarget.sampleCount,
                .gpuViewModelCompositeSamples = rendererStats.gpuViewModelComposite.sampleCount,
                .gpuQueriesIssued = rendererStats.gpuQueriesIssued,
                .outputWidth = GetRenderWidth(),
                .outputHeight = GetRenderHeight(),
                .runStateChanged = stateBeforeInput != performanceSnapshot.state,
                .minimapPreparationMs = performanceStats_.minimapPreparation.lastMilliseconds,
                .terrainChunksSubmitted = rendererStats.terrainChunksSubmitted,
                .terrainChunksCulled = rendererStats.terrainChunksCulled,
                .windowFocused = IsWindowFocused(),
                .windowMinimized = IsWindowMinimized(),
                .windowFocusChanged = windowFocusedAtFrameStart != IsWindowFocused(),
                .finalBatchSubmissionMs = rendererStats.finalBatchSubmissionMs,
                .presentThreadCpuMs = rendererStats.presentThreadCpuMs,
                .presentNotRunningMs = rendererStats.presentNotRunningMs,
                .presentThreadCpuValid = rendererStats.presentThreadCpuValid,
                .defenseShotEvents = hadSimulationTick
                    ? static_cast<std::size_t>(std::ranges::count_if(gameEventBuffer_, [](const GameEvent& event) {
                        return event.type == GameEventType::TowerFired || event.type == GameEventType::CannonFired;
                      })) : 0U,
                .activeDefenseProjectiles = static_cast<std::size_t>(
                    std::ranges::count_if(performanceSnapshot.defenseProjectiles, [](const auto& shot) { return shot.active; }) +
                    std::ranges::count_if(performanceSnapshot.cannonProjectiles, [](const auto& shot) { return shot.active; })),
#ifdef NDEBUG
                .optimizedBuild = true,
#endif
                .ssaoEnabled = renderer_->settings().ssao,
                .bloomEnabled = renderer_->settings().bloom,
                .inkOutlinesEnabled = renderer_->settings().inkOutlines,
                .postProcessingEnabled = renderer_->settings().postProcessing,
            });
        }
        persistUserSettings();
    }

    performanceRecorder_.stop();
    static_cast<void>(saveSuspendedRun());
    persistUserSettings(true);
    persistMetaProgression();
    feedback_.clear();
    arrivalHudLayer_.unload();
    ui_.shutdown();
    audio_.shutdown();
    vfxImpactShader_.unload();
    vfxImpactTexture_.unload();
    vfxElectricRingTexture_.unload();
    resourceImpactTexture_.unload();
    modularBuildingRenderer_.setRenderer(nullptr);
    renderer_->shutdown();
    renderer_.reset();
    CloseWindow();
    return 0;
}

void App::rebuildTerrainGraphics() {
    if (!renderer_) {
        return;
    }
    const SimulationSnapshot& snapshot = simulation_.snapshot();
    auto exclusions = makeDecorationExclusions(snapshot);
    exclusions.push_back({.shape = DecorationExclusionShape::Circle,
        .centerX = simulation_.playerSpawn().x, .centerZ = simulation_.playerSpawn().z, .radius = 4.5});
    decorationExclusionFingerprint_ = decorationFingerprint(snapshot);
    renderer_->rebuildTerrain(simulation_.terrain(), exclusions);
}

void App::applyFullscreenSetting(bool fullscreen) {
    const bool borderless = IsWindowState(
        FLAG_BORDERLESS_WINDOWED_MODE);
    if (borderless != fullscreen) {
        ToggleBorderlessWindowed();
    }
    if (!fullscreen) {
        // Preserve the original fitted windowed behavior. Render resolution
        // is configured independently and never grows the native window.
        MaximizeWindow();
    }
    fullscreenApplied_ = fullscreen;
}

void App::refreshDecorationExclusions(
    const SimulationSnapshot& snapshot) {
    if (!renderer_) {
        return;
    }
    const std::uint64_t fingerprint =
        decorationFingerprint(snapshot);
    if (fingerprint == decorationExclusionFingerprint_) {
        return;
    }
    decorationExclusionFingerprint_ = fingerprint;
    auto exclusions = makeDecorationExclusions(snapshot);
    exclusions.push_back({.shape = DecorationExclusionShape::Circle,
        .centerX = simulation_.playerSpawn().x, .centerZ = simulation_.playerSpawn().z, .radius = 4.5});
    renderer_->rebuildDecorationExclusions(exclusions);
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
        .controls = userSettings_.controls,
        .accessibility = userSettings_.accessibility,
        .language = userSettings_.language,
        .debugHotkeysEnabled = userSettings_.debugHotkeysEnabled,
    };
    if (current == persistedUserSettings_ ||
        (!force && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
        return;
    }
    if (saveUserSettings(UserSettingsPath, current)) {
        userSettings_ = current;
        persistedUserSettings_ = current;
    }
}

void App::persistMetaProgression() {
    if (metaProgression_ == persistedMetaProgression_) return;
    if (saveMetaProgression(MetaProgressionPath, metaProgression_)) {
        persistedMetaProgression_ = metaProgression_;
    }
}

void App::recordMetaProgression(std::span<const GameEvent> events,
                                const SimulationSnapshot& snapshot) {
    if (metaProgression_.placementControlHintsUsed < 2 &&
        std::ranges::any_of(events, [](const GameEvent& event) {
            return event.type == GameEventType::BuildingPlaced ||
                event.type == GameEventType::ModularBuildingPlaced;
        })) {
        // A drag placement and its automatic foundations are one action.
        ++metaProgression_.placementControlHintsUsed;
        persistMetaProgression();
    }
    if (snapshot.sandboxMode) {
        return;
    }
    std::array<bool, PlayerClassDefinitions.size()> unlockedBefore{};
    for (std::size_t index = 0;
         index < PlayerClassDefinitions.size(); ++index) {
        unlockedBefore[index] = isPlayerClassUnlocked(
            PlayerClassDefinitions[index].type, metaProgression_);
    }
    const auto update = questGameplayTracker_.record(
        questSystem_, metaProgression_, events, snapshot);
    for(const auto& completed:update.completed) {
        const auto definition=std::ranges::find(questSystem_.definitions(),completed.id,&QuestDefinition::id);
        if(definition!=questSystem_.definitions().end())
            questCompletionNotices_.push_back({completed.id,
                definition->notification.empty()?definition->title:definition->notification,completed.reward});
    }
    const bool runBoundary = std::ranges::any_of(events, [](const auto& event) {
        return event.type == GameEventType::RunStarted ||
            event.type == GameEventType::RunRestarted ||
            event.type == GameEventType::RunEnded ||
            event.type == GameEventType::StageCleared;
    });
    bool saveNow = runBoundary || !update.completed.empty();
    if (std::ranges::any_of(events, [](const GameEvent& event) {
            return event.type == GameEventType::RunStarted ||
                event.type == GameEventType::RunRestarted ||
                event.type == GameEventType::RunEnded;
        })) {
        silverHudRemaining_ = 0.0;
    }
    for (const auto& event : events) {
        if (event.type != GameEventType::MetaSilverCollected || event.amount <= 0) continue;
        silverHudRemaining_ = 3.0;
        saveNow = true;
    }
    for (std::size_t index = 0;
         index < PlayerClassDefinitions.size(); ++index) {
        if (unlockedBefore[index] || !isPlayerClassUnlocked(
                PlayerClassDefinitions[index].type,
                metaProgression_)) {
            continue;
        }
        statusMessage_ = localizeText("CLASS UNLOCKED: ") +
            localizeText(PlayerClassDefinitions[index].name);
        statusMessageRemaining_ = 4.5;
        audio_.playUiConfirm();
        saveNow = true;
    }
    if (saveNow) persistMetaProgression();
}

bool App::questCompletionToastVisible() const {
    const auto& snapshot=simulation_.snapshot();
    return !hudHidden_ && !playerArrival_.active() && snapshot.state!=RunState::MainMenu &&
        (snapshot.state!=RunState::Paused || (feedback_.needsCapture() && resumeAfterFeedback_)) && !snapshot.runUpgradeChoicePending && !snapshot.runShop.pending &&
        !coreDefenseMenuVisible_ && !(renderer_ && renderer_->graphicsPanelVisible());
}

void App::updateQuestCompletionNotices(double seconds) {
    if(questCompletionNotices_.empty() || !questCompletionToastVisible()) return;
    if(questCompletionRemaining_<=0) {
        questCompletionRemaining_=QuestCompletionNoticeDuration;
        audio_.playUiReward();
    }
    questCompletionRemaining_=std::max(0.0,questCompletionRemaining_-seconds);
    if(questCompletionRemaining_<=0) questCompletionNotices_.pop_front();
}

bool App::resetAllProgress() {
    MetaProgression fresh;
    questSystem_.initialize(fresh);
    // Remove recoverable old runs as well, so a backup cannot resurrect them.
    for(const auto path:{"user_settings/meta_progression.json.bak","user_settings/meta_progression.json.tmp",
        "user_settings/suspended_run.json","user_settings/suspended_run.json.tmp",
        "user_settings/suspended_run.json.bak","user_settings/suspended_run.bin"}) {
        std::error_code error;
        std::filesystem::remove(path,error);
        if(error) return false;
    }
    if(!saveMetaProgression(MetaProgressionPath,fresh)) return false;
    metaProgression_=fresh;
    persistedMetaProgression_=fresh;
    questGameplayTracker_={};
    questCompletionNotices_.clear();questCompletionRemaining_=0;
    pendingObjectiveCompletionNotices_.clear();objectiveCompletionTitle_.clear();objectiveCompletionRemaining_=0;
    questMenuState_={};unlockMenuState_={};shopMenuState_={};classSelectionState_={};pauseMenuState_={};
    selectedPlayerClass_=DefaultPlayerClass;
    resumeAfterSettings_=false;
    simulation_.returnToMainMenu();
    simulation_.takeEvents(gameEventBuffer_);gameEventBuffer_.clear();
    discardSuspendedRun();
    resetMainMenuInput();resetRunInputState();fixedStep_.reset();playerArrival_.reset();
    effects_.clear();floatingDamageNumbers_.clear();statusMessage_.clear();statusMessageRemaining_=0;
    if(renderer_ && renderer_->graphicsPanelVisible()) closeSettings();
    EnableCursor();
    return true;
}

void App::drawPerformanceOverlay(
    const SimulationSnapshot& snapshot) const {
    if (!renderer_) {
        return;
    }

    const EnemyPerformanceStats& enemyStats =
        simulation_.enemyPerformanceStats();
    const RendererPerformanceStats& rendererStats =
        renderer_->performanceStats();
    constexpr float PanelX = 18.0F;
    constexpr float PanelY = 48.0F;
    constexpr float PanelWidth = 870.0F;
    constexpr float PanelHeight = 482.0F;
    constexpr float FontSize = 15.0F;
    constexpr float LineHeight = 18.0F;
    DrawRectangleRounded(
        {PanelX, PanelY, PanelWidth, PanelHeight},
        0.08F, 8, {10, 15, 19, 232});
    DrawRectangleLinesEx(
        {PanelX, PanelY, PanelWidth, PanelHeight},
        1.0F, {103, 139, 130, 220});
    drawUiText(
        "PERFORMANCE [SHIFT+F10]",
        {PanelX + 10.0F, PanelY + 8.0F},
        FontSize, {255, 218, 139, 255});
    const auto drawLine = [](int index, const char* text) {
        drawUiText(
            text,
            {PanelX + 10.0F,
             78.0F + static_cast<float>(index) * LineHeight},
            FontSize, {220, 235, 226, 255});
    };
    const auto metricLine = [&drawLine](
                                int index, const char* name,
                                const PerformanceMetric& metric) {
        drawLine(index, TextFormat(
            "%-13s %6.2f ms   PEAK %6.2f",
            name, metric.averageMilliseconds,
            metric.recentPeakMilliseconds));
    };
    const auto drawDetailLine = [](int index, const char* text,
                                   Color color = Color{220, 235, 226, 255}) {
        drawUiText(
            text,
            {PanelX + 450.0F,
             78.0F + static_cast<float>(index) * LineHeight},
            FontSize, color);
    };
    const auto detailMetricLine = [&drawDetailLine](
                                      int index, const char* name,
                                      const PerformanceMetric& metric) {
        drawDetailLine(index, TextFormat(
            "%-13s %6.2f ms   PEAK %6.2f",
            name, metric.averageMilliseconds,
            metric.recentPeakMilliseconds));
    };
    drawLine(0, TextFormat(
        "FPS %d   FIXED TICKS %d",
        GetFPS(), static_cast<int>(performanceStats_.fixedTicks)));
    metricLine(1, "FRAME", performanceStats_.frame);
    metricLine(2, "INPUT", performanceStats_.input);
    metricLine(3, "UPDATE", performanceStats_.simulation);
    metricLine(4, "SIM TICK", performanceStats_.simulationTick);
    metricLine(5, "RENDER+WAIT", performanceStats_.render);
    metricLine(6, "PRESENT/WAIT", performanceStats_.present);
    metricLine(7, "RENDER PREP", performanceStats_.renderPreparation);
    metricLine(8, "SHADOW", performanceStats_.shadowPass);
    metricLine(9, "SELECTION", performanceStats_.selectionPass);
    metricLine(10, "TERRAIN+SKY", performanceStats_.terrainRender);
    metricLine(11, "WORLD OBJECTS", performanceStats_.worldEntitiesRender);
    metricLine(12, "WATER/CLOUD", performanceStats_.environmentRender);
    metricLine(13, "OVERLAYS", performanceStats_.overlayRender);
    metricLine(14, "POSTPROCESS", performanceStats_.postProcess);
    metricLine(15, "HUD/UI", performanceStats_.uiRender);
    drawLine(16, TextFormat(
        "ENEMY AI %.2f  COLL %.2f  HASH %.2f",
        enemyStats.tick.averageMilliseconds,
        enemyStats.collision.averageMilliseconds,
        enemyStats.spatialRebuild.averageMilliseconds));
    drawLine(17, TextFormat(
        "ENEMY DRAW %.2f  INST %.2f ms",
        performanceStats_.enemyRender.averageMilliseconds,
        rendererStats.instancedEnemyDraw.averageMilliseconds));
    drawLine(18, TextFormat(
        "ACTIVE %d  VISIBLE %d  BATCHES %d  LOD %d",
        static_cast<int>(snapshot.activeEnemyCount),
        static_cast<int>(performanceStats_.visibleEnemies),
        static_cast<int>(rendererStats.enemyBatchCount),
        static_cast<int>(rendererStats.lowDetailEnemyCount)));
    drawLine(19, TextFormat(
        "BLOB %.2f ms  SHADOWS %d  TRIANGLES %d",
        performanceStats_.blobShadows.averageMilliseconds,
        static_cast<int>(performanceStats_.enemyShadowDraws),
        static_cast<int>(rendererStats.blobShadowTriangles)));
    drawLine(20, TextFormat(
        "DECOR %.2f ms  GRASS %.2f ms",
        performanceStats_.decorationsRender.averageMilliseconds,
        performanceStats_.grassRender.averageMilliseconds));
    drawLine(21, performanceRecorder_.active()
        ? "SESSION RECORDING: performance_logs/*.csv"
        : renderer_->settings().performanceLogging
              ? "SESSION RECORDING: FAILED"
              : "SESSION RECORDING: OFF");
    drawLine(22, "AVG = LOAD   PEAK = RECENT HITCH");
    drawDetailLine(
        0, "CPU HUD/UI BREAKDOWN", {255, 218, 139, 255});
    detailMetricLine(
        1, "VIEWMODEL", performanceStats_.viewModelRender);
    detailMetricLine(
        2, "WORLD UI", performanceStats_.worldUiRender);
    detailMetricLine(
        3, "PROMPT", performanceStats_.interactionPromptRender);
    detailMetricLine(
        4, "HUD CORE", performanceStats_.hudCoreRender);
    detailMetricLine(
        5, "RESOURCE FX", performanceStats_.resourceGainRender);
    detailMetricLine(
        6, "MINIMAP", performanceStats_.minimapRender);
    detailMetricLine(
        7, "UI OVERLAYS", performanceStats_.uiOverlaysRender);
    detailMetricLine(
        8, "UI TEXT", performanceStats_.uiTextRender);
    const UiTextPerformanceStats textStats = uiTextPerformanceStats();
    drawDetailLine(9, TextFormat(
        "TEXT CALLS %d   CHARS %d",
        static_cast<int>(textStats.submissions),
        static_cast<int>(textStats.characters)));
    drawDetailLine(
        11,
        rendererStats.gpuTimersSupported
            ? "GPU TIME (ASYNC)"
            : "GPU TIME: UNSUPPORTED",
        rendererStats.gpuTimersSupported
            ? Color{143, 210, 255, 255}
            : Color{244, 112, 91, 255});
    detailMetricLine(12, "GPU WORLD", rendererStats.gpuWorld);
    detailMetricLine(
        13, "GPU POST", rendererStats.gpuPostProcess);
    detailMetricLine(
        14, "GPU VIEWMODEL", rendererStats.gpuViewModel);
    detailMetricLine(
        15, "  TARGET", rendererStats.gpuViewModelTarget);
    detailMetricLine(
        16, "  COMPOSITE", rendererStats.gpuViewModelComposite);
    detailMetricLine(17, "GPU UI", rendererStats.gpuUi);
}

} // namespace ian
