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
#include <string>
#include <utility>

namespace ian {
namespace {

constexpr int InitialWindowWidth = 1280;
constexpr int InitialWindowHeight = 720;
constexpr std::string_view UserSettingsPath =
    "user_settings/game_settings.json";

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
          loadAppWorldConfig(), loadAppSkills(),
          loadAppInsightConfig(), loadAppObjectives()),
      environment_(loadAppEnvironment()),
      skillTree_(simulation_.skillTree()) {
    static_cast<void>(loadUserSettings(
        UserSettingsPath, userSettings_));
    persistedUserSettings_ = userSettings_;
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
    arrowVisuals_.reserve(64);
    damageIndicators_.reserve(12);
    floatingDamageNumbers_.reserve(32);
    resourceGainVisuals_.reserve(16);
    destroyedResourceVisuals_.reserve(8);
    destroyedEnemyVisuals_.reserve(16);
    buildingShotRecoilVisuals_.reserve(32);
}

int App::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(InitialWindowWidth, InitialWindowHeight,
               "FORTBONK");
    SetExitKey(KEY_NULL);
    applyFullscreenSetting(userSettings_.graphics.fullscreen);
    renderer_.emplace();
    renderer_->settings() = userSettings_.graphics;
    renderer_->applyFrameRateLimit();
    renderer_->initialize();
    modularBuildingRenderer_.setRenderer(&*renderer_);
    rebuildTerrainGraphics();
    ui_.initialize();
    skillTree_.initialize();
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
        performanceStats_.render.sample(
            performanceMilliseconds(renderStart));
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
                .overlaysMs =
                    performanceStats_.overlayRender.lastMilliseconds,
                .postProcessMs =
                    performanceStats_.postProcess.lastMilliseconds,
                .uiMs = performanceStats_.uiRender.lastMilliseconds,
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
    persistUserSettings(true);
    for (int visual = static_cast<int>(FirstPersonToolVisual::Axe);
         visual <= static_cast<int>(FirstPersonToolVisual::Bomb);
         ++visual) {
        const auto toolVisual = static_cast<FirstPersonToolVisual>(visual);
        static_cast<void>(saveFirstPersonToolTuning(
            toolTuningPath(toolVisual),
            toolTunings_[static_cast<std::size_t>(visual)]));
    }

    skillTree_.shutdown();
    ui_.shutdown();
    audio_.shutdown();
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
        // A maximized native window retains minimize and reliable Alt+Tab.
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
    constexpr float PanelWidth = 450.0F;
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
}

void App::drawBuildModePie() const {
    if (!buildModePieVisible_) {
        return;
    }

    constexpr float OuterRadius = 174.0F;
    constexpr float InnerRadius = 62.0F;
    constexpr float LabelRadius = 119.0F;
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
    constexpr std::array<Segment, 4> Segments{{
        {ActionMode::Tools, 137.0F, 223.0F, {-1.0F, 0.0F},
         "GATHER & REPAIR", {104, 190, 132, 255}},
        {ActionMode::Weapons, 227.0F, 313.0F, {0.0F, -1.0F},
         "COMBAT", {222, 105, 92, 255}},
        {ActionMode::Buildings, -43.0F, 43.0F, {1.0F, 0.0F},
         "STRUCTURES", {236, 190, 91, 255}},
        {ActionMode::Modular, 47.0F, 133.0F, {0.0F, 1.0F},
         "FLOORS & RAMPS", {100, 164, 224, 255}},
    }};

    DrawCircleV(
        {center.x + 4.0F, center.y + 7.0F},
        OuterRadius + 12.0F, {5, 7, 12, 145});
    DrawCircleV(center, OuterRadius + 8.0F,
                {247, 225, 177, 105});
    DrawCircleV(center, OuterRadius + 3.0F,
                {14, 18, 25, 248});

    const auto& snapshot = simulation_.snapshot();
    const bool weaponsAvailable = std::ranges::any_of(
        PlayerCombatHotbarOrder,
        [&snapshot](PlayerWeapon weapon) {
            return snapshot.unlockedWeapons[
                static_cast<std::size_t>(weapon)];
        });

    for (const Segment& segment : Segments) {
        const bool selected =
            buildModePieChoice_ == segment.mode;
        const bool active = actionMode_ == segment.mode;
        const bool available =
            segment.mode != ActionMode::Weapons ||
            weaponsAvailable;
        Color fill = selected
            ? (available
                   ? segment.accent
                   : Color{91, 91, 96, 255})
            : active
                ? Color{
                      static_cast<unsigned char>(
                          segment.accent.r * 0.48F),
                      static_cast<unsigned char>(
                          segment.accent.g * 0.48F),
                      static_cast<unsigned char>(
                          segment.accent.b * 0.48F),
                      245}
                : Color{43, 51, 64, 242};
        DrawRing(
            center, InnerRadius + 5.0F,
            OuterRadius - (selected ? 0.0F : 5.0F),
            segment.startAngle, segment.endAngle,
            32, fill);
        if (active) {
            DrawRing(
                center, OuterRadius - 10.0F,
                OuterRadius - 5.0F,
                segment.startAngle, segment.endAngle,
                32, segment.accent);
        }

        const Vector2 labelCenter{
            center.x + segment.direction.x * LabelRadius,
            center.y + segment.direction.y * LabelRadius,
        };
        const Color textColor = !available
            ? Color{171, 169, 164, 235}
            : selected
                ? Color{25, 26, 27, 255}
                : Color{250, 241, 220, 255};
        const std::string_view label =
            actionModeLabel(segment.mode);
        const Vector2 labelSize = measureUiText(label, 17.0F);
        drawUiText(
            label,
            {labelCenter.x - labelSize.x * 0.5F,
             labelCenter.y - 13.0F},
            17.0F, textColor);
        const std::string_view subtitle = available
            ? segment.subtitle
            : "LOCKED • SKILL TREE";
        const Vector2 subtitleSize =
            measureUiText(subtitle, 9.0F);
        drawUiText(
            subtitle,
            {labelCenter.x - subtitleSize.x * 0.5F,
             labelCenter.y + 10.0F},
            9.0F,
            selected
                ? Color{39, 42, 44, 230}
                : Color{207, 211, 214, 220});
        if (active) {
            DrawCircleV(
                {labelCenter.x,
                 labelCenter.y + 31.0F},
                4.0F,
                selected
                    ? Color{25, 26, 27, 255}
                    : segment.accent);
        }
    }

    DrawCircleV(center, InnerRadius + 5.0F,
                {245, 224, 178, 145});
    DrawCircleV(center, InnerRadius,
                {18, 22, 30, 255});
    const ActionMode centerMode =
        buildModePieChoice_.value_or(actionMode_);
    const std::string_view centerCaption =
        buildModePieChoice_ ? "SELECT" : "CURRENT";
    const Vector2 captionSize =
        measureUiText(centerCaption, 9.0F);
    drawUiText(
        centerCaption,
        {center.x - captionSize.x * 0.5F,
         center.y - 22.0F},
        9.0F, {178, 183, 190, 240});
    const std::string_view centerLabel =
        actionModeLabel(centerMode);
    const float centerFont = centerLabel.size() > 7U
        ? 13.0F : 15.0F;
    const Vector2 centerLabelSize =
        measureUiText(centerLabel, centerFont);
    drawUiText(
        centerLabel,
        {center.x - centerLabelSize.x * 0.5F,
         center.y - 5.0F},
        centerFont, {255, 238, 196, 255});

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
                    {255, 245, 220, 255});
        DrawCircleLines(
            static_cast<int>(pointer.x),
            static_cast<int>(pointer.y),
            11.0F, {255, 245, 220, 155});
    }

    const std::string key = keyboardKeyName(controlKey(
        userSettings_.controls, ControlAction::BuildMode));
    drawCenteredUiText(
        "HOLD " + key + "  •  MOVE MOUSE  •  RELEASE",
        center.y + OuterRadius + 22.0F,
        13.0F, {247, 237, 215, 245});
    drawCenteredUiText(
        std::string{"TAP: "} +
            actionModeLabel(previousActionMode_),
        center.y + OuterRadius + 43.0F,
        10.0F, {183, 189, 198, 225});
}


} // namespace ian
