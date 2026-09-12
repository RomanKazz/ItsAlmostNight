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

const char* App::toolTuningPath(FirstPersonToolVisual visual) {
    switch (visual) {
    case FirstPersonToolVisual::Axe:
        return "user_settings/first_person_tool_axe.json";
    case FirstPersonToolVisual::Pickaxe:
        return "user_settings/first_person_tool_pickaxe.json";
    case FirstPersonToolVisual::Club:
        return "user_settings/first_person_tool_club.json";
    case FirstPersonToolVisual::IceWand:
        return "user_settings/first_person_tool_ice_wand.json";
    case FirstPersonToolVisual::FireWand:
        return "user_settings/first_person_tool_fire_wand.json";
    case FirstPersonToolVisual::Hammer:
        return "user_settings/first_person_tool_hammer.json";
    case FirstPersonToolVisual::Bomb:
        return "user_settings/first_person_tool_bomb.json";
    case FirstPersonToolVisual::None:
        return "user_settings/first_person_tool_axe.json";
    }
    return "user_settings/first_person_tool_axe.json";
}

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
        static_cast<void>(loadFirstPersonToolTuning(
            toolTuningPath(toolVisual),
            toolTunings_[static_cast<std::size_t>(visual)]));
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
    // The world already receives FXAA and UI text uses SDF. Multisampling the
    // native Retina framebuffer adds a costly full-screen resolve at swap.
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
    static_cast<void>(stoneCrackTexture_.load(
        "assets/vfx/stone_crack.png"));
    for (TextureResource* texture : {
             &vfxImpactTexture_, &vfxExplosionTexture_,
             &vfxElectricRingTexture_, &stoneCrackTexture_}) {
        if (texture->valid()) {
            SetTextureFilter(texture->get(), TEXTURE_FILTER_BILINEAR);
        }
    }
    modularBuildingRenderer_.setRenderer(&*renderer_);
    rebuildTerrainGraphics();
    ui_.initialize();
    uiEditor_.initialize();
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
    for (int visual = static_cast<int>(FirstPersonToolVisual::Axe);
         visual <= static_cast<int>(FirstPersonToolVisual::Bomb);
         ++visual) {
        const auto toolVisual = static_cast<FirstPersonToolVisual>(visual);
        static_cast<void>(saveFirstPersonToolTuning(
            toolTuningPath(toolVisual),
            toolTunings_[static_cast<std::size_t>(visual)]));
    }

    uiEditor_.shutdown();
    ui_.shutdown();
    audio_.shutdown();
    vfxImpactShader_.unload();
    vfxImpactTexture_.unload();
    vfxExplosionTexture_.unload();
    vfxElectricRingTexture_.unload();
    stoneCrackTexture_.unload();
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

void App::drawBuildModePie() const {
    if (!buildModePieVisible_) {
        return;
    }

    constexpr float OuterRadius = 168.0F;
    constexpr float InnerRadius = 58.0F;
    constexpr float LabelRadius = 116.0F;
    constexpr Color FrameColor{104, 60, 37, 255};
    constexpr Color FrameHighlight{230, 188, 147, 255};
    constexpr Color SurfaceColor{117, 76, 52, 250};
    constexpr Color SelectedSurface{230, 188, 147, 255};
    const Vector2 center{
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    struct Segment {
        ActionMode mode;
        float startAngle;
        float endAngle;
        Vector2 direction;
        const char* subtitle;
        Color accent;
    };
    constexpr std::array<Segment, 2> Segments{{
        {ActionMode::Equipment, 180.0F, 360.0F, {0.0F, -1.0F},
         "GATHER & COMBAT", {104, 220, 148, 255}},
        {ActionMode::Buildings, 0.0F, 180.0F, {0.0F, 1.0F},
         "STRUCTURES & MODULAR", {236, 190, 91, 255}},
    }};

    DrawCircleV(
        {center.x + 3.0F, center.y + 5.0F},
        OuterRadius + 9.0F, {49, 30, 22, 115});
    DrawCircleV(center, OuterRadius + 7.0F, FrameColor);
    DrawCircleV(center, OuterRadius + 3.0F, FrameHighlight);
    DrawCircleV(center, OuterRadius, SurfaceColor);

    for (const Segment& segment : Segments) {
        const bool selected =
            buildModePieChoice_ == segment.mode;
        const bool active = actionMode_ == segment.mode;
        const Color fill = selected
            ? SelectedSurface
            : active
                ? Color{151, 104, 70, 252}
                : SurfaceColor;
        DrawRing(
            center, InnerRadius + 5.0F,
            OuterRadius - (selected ? 1.0F : 5.0F),
            segment.startAngle, segment.endAngle,
            64, fill);
        if (selected || active) {
            DrawRing(
                center, OuterRadius - 8.0F,
                OuterRadius - 3.0F,
                segment.startAngle, segment.endAngle,
                64, selected
                    ? UiWhite
                    : segment.accent);
        }

        const Vector2 labelCenter{
            center.x + segment.direction.x * LabelRadius,
            center.y + segment.direction.y * LabelRadius,
        };
        SdfUiTextStyle labelStyle{};
        labelStyle.fillColor = UiWhite;
        labelStyle.outlineColor = {49, 49, 49, 255};
        labelStyle.shadowColor = {82, 58, 39, 220};
        labelStyle.shadowOffsetPixels = {0.0F, 2.0F};
        labelStyle.outlinePixels = 2.0F;
        const std::string_view label =
            actionModeLabel(segment.mode);
        const Vector2 labelSize = measureUiText(label, 17.0F);
        drawStyledSdfUiText(
            label,
            {labelCenter.x - labelSize.x * 0.5F,
             labelCenter.y - 13.0F},
            17.0F, labelStyle);
        const std::string_view subtitle = segment.subtitle;
        const Vector2 subtitleSize =
            measureUiText(subtitle, 9.0F);
        SdfUiTextStyle subtitleStyle = labelStyle;
        subtitleStyle.fillColor = selected
            ? Color{112, 69, 45, 255}
            : Color{238, 218, 183, 245};
        subtitleStyle.outlinePixels = selected ? 0.8F : 1.2F;
        subtitleStyle.shadowOffsetPixels = {0.0F, 1.0F};
        drawStyledSdfUiText(
            subtitle,
            {labelCenter.x - subtitleSize.x * 0.5F,
             labelCenter.y + 10.0F},
            9.0F, subtitleStyle);
        if (active) {
            DrawCircleV(
                {labelCenter.x,
                 labelCenter.y + 31.0F},
                4.0F,
                selected
                    ? FrameColor
                    : segment.accent);
        }
    }

    DrawRectangleRec(
        {center.x - OuterRadius, center.y - 2.0F,
         OuterRadius - InnerRadius - 3.0F, 4.0F},
        FrameColor);
    DrawRectangleRec(
        {center.x + InnerRadius + 3.0F, center.y - 2.0F,
         OuterRadius - InnerRadius - 3.0F, 4.0F},
        FrameColor);
    DrawCircleV(center, InnerRadius + 5.0F, FrameColor);
    DrawCircleV(center, InnerRadius + 1.0F, FrameHighlight);
    DrawCircleV(center, InnerRadius - 4.0F,
                {76, 54, 43, 255});
    const ActionMode centerMode =
        buildModePieChoice_.value_or(actionMode_);
    const std::string_view centerCaption =
        buildModePieChoice_ ? "SELECT" : "CURRENT";
    const Vector2 captionSize =
        measureUiText(centerCaption, 9.0F);
    SdfUiTextStyle centerStyle{};
    centerStyle.fillColor = {242, 222, 190, 255};
    centerStyle.outlineColor = {49, 49, 49, 255};
    centerStyle.shadowColor = {31, 24, 20, 210};
    centerStyle.shadowOffsetPixels = {0.0F, 2.0F};
    centerStyle.outlinePixels = 1.5F;
    drawStyledSdfUiText(
        centerCaption,
        {center.x - captionSize.x * 0.5F,
         center.y - 22.0F},
        9.0F, centerStyle);
    const std::string_view centerLabel =
        actionModeLabel(centerMode);
    const float centerFont = centerLabel.size() > 7U
        ? 13.0F : 15.0F;
    const Vector2 centerLabelSize =
        measureUiText(centerLabel, centerFont);
    centerStyle.fillColor = {255, 239, 199, 255};
    centerStyle.outlinePixels = 2.0F;
    drawStyledSdfUiText(
        centerLabel,
        {center.x - centerLabelSize.x * 0.5F,
         center.y - 5.0F},
        centerFont, centerStyle);

    const float length =
        Vector2Length(buildModePieDirection_);
    if (length > 8.0F) {
        const Vector2 direction =
            Vector2Scale(
                buildModePieDirection_,
                1.0F / length);
        const Vector2 pointer =
            Vector2Add(
                center,
                Vector2Scale(direction, InnerRadius - 13.0F));
        DrawCircleV(pointer, 8.0F,
                    {255, 213, 112, 255});
        DrawCircleLines(
            static_cast<int>(pointer.x),
            static_cast<int>(pointer.y),
            11.0F, {244, 240, 223, 190});
    }

    const std::string key = keyboardKeyName(controlKey(
        userSettings_.controls, ControlAction::BuildMode));
    const std::string hint =
        "HOLD " + key + "   CHOOSE   RELEASE";
    const float hintWidth = measureUiText(hint, 12.0F).x;
    SdfUiTextStyle hintStyle = centerStyle;
    hintStyle.fillColor = {255, 239, 207, 245};
    hintStyle.outlinePixels = 1.7F;
    drawStyledSdfUiText(
        hint,
        {center.x - hintWidth * 0.5F,
         center.y + OuterRadius + 22.0F},
        12.0F, hintStyle);
}


} // namespace ian
