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
#include <limits>
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
    return hash;
}

std::vector<DecorationExclusion> makeDecorationExclusions(
    const SimulationSnapshot& snapshot) {
    std::vector<DecorationExclusion> exclusions;
    exclusions.reserve(
        snapshot.resourceNodes.size() + snapshot.lootChests.size() +
        snapshot.mapObstacles.size() + snapshot.challengeColumns.size() +
        snapshot.worldLandmarks.size());
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
        graphicsPanelTab_ == ToolSettingsTab) {
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
        graphicsPanelTab_ == ToolSettingsTab) {
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
    const double speed =
        runGatheringSpeedMultiplier(snapshot.runUpgradeStacks[
            runUpgradeIndex(RunUpgradeEffect::AttackSpeed)]) *
        (quickTool
             ? runGatheringSpeedMultiplier(
                   snapshot.runUpgradeStacks[runUpgradeIndex(
                       RunUpgradeEffect::GatheringSpeed)])
             : 1.0);
    return static_cast<double>(activeToolTuning().swingDuration) / speed;
}

App::App()
    : simulation_(
      loadAppBalance(), loadAppMap(),
          loadAppWorldConfig(),
          loadAppInsightConfig(), loadAppObjectives()),
      environment_(loadAppEnvironment()) {
    static_cast<void>(loadUserSettings(
        UserSettingsPath, userSettings_));
    static_cast<void>(loadMetaProgression(
        MetaProgressionPath, metaProgression_));
    persistedUserSettings_ = userSettings_;
    persistedMetaProgression_ = metaProgression_;
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
    // Tower fire is hitscan in the simulation, but presentation keeps a
    // short-lived projectile/tracer so the player can read where the base is
    // shooting.  Reserve the full bounded visual budget to avoid reallocating
    // during dense nights.
    arrowVisuals_.reserve(1024);
    damageIndicators_.reserve(12);
    floatingDamageNumbers_.reserve(32);
    resourceGainVisuals_.reserve(16);
    destroyedResourceVisuals_.reserve(8);
    destroyedEnemyVisuals_.reserve(16);
    buildingShotRecoilVisuals_.reserve(32);
}

int App::run() {
    // Prepare a suspended run's terrain before the window appears. Continue
    // can then reuse the already uploaded terrain instead of freezing input.
    if (const auto seed = suspendedRunTerrainSeed()) {
        simulation_.regenerateTerrain(*seed);
    }
    // Render the final framebuffer (especially SDF UI text) at the native
    // backing resolution on Retina/HiDPI displays. World resolution remains
    // independently controlled by GraphicsSettings::renderWidth/Height.
    unsigned int windowFlags =
        FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI;
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
    static_cast<void>(vfxExplosionTexture_.load(
        "assets/vfx/brackeys/explosion_6x5.png"));
    static_cast<void>(vfxElectricRingTexture_.load(
        "assets/vfx/brackeys/electric_ring_6x5.png"));
    static_cast<void>(resourceImpactTexture_.load(
        "assets/vfx/stone_crack.png"));
    for (TextureResource* texture : {
             &vfxImpactTexture_,
             &vfxExplosionTexture_,
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

    while (!WindowShouldClose() && !exitRequested_) {
        const auto frameStart = PerformanceClock::now();
        const auto inputStart = PerformanceClock::now();
        processInput();
        performanceStats_.input.sample(
            performanceMilliseconds(inputStart));
        update();
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
                    performanceStats_.simulationTick.lastMilliseconds,
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
                .enemyAiMs = enemyStats.tick.lastMilliseconds,
                .enemyCollisionMs = enemyStats.collision.lastMilliseconds,
                .enemyDrawMs =
                    performanceStats_.enemyRender.lastMilliseconds,
                .blobShadowsMs =
                    performanceStats_.blobShadows.lastMilliseconds,
            });
        }
        persistUserSettings();
    }

    performanceRecorder_.stop();
    static_cast<void>(saveSuspendedRun());
    persistUserSettings(true);
    persistMetaProgression();
    ui_.shutdown();
    audio_.shutdown();
    vfxImpactShader_.unload();
    vfxImpactTexture_.unload();
    vfxExplosionTexture_.unload();
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
    const auto exclusions = makeDecorationExclusions(snapshot);
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
    const auto exclusions = makeDecorationExclusions(snapshot);
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

void App::recordMetaProgression(std::span<const GameEvent> events) {
    if (simulation_.snapshot().sandboxMode) {
        return;
    }
    std::array<bool, PlayerClassDefinitions.size()> unlockedBefore{};
    for (std::size_t index = 0;
         index < PlayerClassDefinitions.size(); ++index) {
        unlockedBefore[index] = isPlayerClassUnlocked(
            PlayerClassDefinitions[index].type, metaProgression_);
    }
    const auto add = [](int& value, int amount = 1) {
        if (amount <= 0) return;
        value = value > std::numeric_limits<int>::max() - amount
            ? std::numeric_limits<int>::max()
            : value + amount;
    };
    bool saveNow = false;
    for (const GameEvent& event : events) {
        switch (event.type) {
        case GameEventType::RunStarted:
        case GameEventType::RunRestarted:
            add(metaProgression_.runsPlayed);
            break;
        case GameEventType::EnemyKilled:
            add(metaProgression_.enemiesDefeated);
            break;
        case GameEventType::LootCollected:
            add(metaProgression_.lootCollected);
            break;
        case GameEventType::ResourceGranted:
            add(metaProgression_.resourcesGathered, event.amount);
            break;
        case GameEventType::WaveStarted:
        case GameEventType::WaveCompleted:
            metaProgression_.bestWave = std::max(
                metaProgression_.bestWave, event.amount);
            break;
        case GameEventType::StageCleared:
            add(metaProgression_.stageClears);
            saveNow = true;
            break;
        case GameEventType::RunEnded:
            saveNow = true;
            break;
        default:
            break;
        }
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
