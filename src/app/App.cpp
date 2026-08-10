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
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(chest.position.x));
        hash = mixDecorationFingerprint(
            hash, std::bit_cast<std::uint64_t>(chest.position.z));
    }
    return hash;
}

std::vector<DecorationExclusion> makeDecorationExclusions(
    const SimulationSnapshot& snapshot) {
    std::vector<DecorationExclusion> exclusions;
    exclusions.reserve(
        snapshot.resourceNodes.size() + snapshot.lootChests.size() +
        snapshot.mapObstacles.size());
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
        exclusions.push_back({
            .shape = DecorationExclusionShape::Circle,
            .centerX = chest.position.x,
            .centerZ = chest.position.z,
            .radius = 0.82,
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
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(InitialWindowWidth, InitialWindowHeight,
               "It's Almost Night");
    SetExitKey(KEY_NULL);
    ToggleBorderlessWindowed();
    renderer_.emplace();
    renderer_->settings() = userSettings_.graphics;
    renderer_->applyFrameRateLimit();
    renderer_->initialize();
    modularBuildingRenderer_.setRenderer(&*renderer_);
    rebuildTerrainGraphics();
    ui_.initialize();
    audio_.initialize();

    while (!WindowShouldClose() && !exitRequested_) {
        const auto frameStart = PerformanceClock::now();
        processInput();
        update();
        const auto renderStart = PerformanceClock::now();
        render();
        performanceStats_.render.sample(
            performanceMilliseconds(renderStart));
        performanceStats_.frame.sample(
            performanceMilliseconds(frameStart));
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

void App::rebuildTerrainGraphics() {
    if (!renderer_) {
        return;
    }
    const SimulationSnapshot& snapshot = simulation_.snapshot();
    const auto exclusions = makeDecorationExclusions(snapshot);
    decorationExclusionFingerprint_ = decorationFingerprint(snapshot);
    renderer_->rebuildTerrain(simulation_.terrain(), exclusions);
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
    constexpr float PanelWidth = 390.0F;
    constexpr float PanelHeight = 350.0F;
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
    drawLine(
        0, TextFormat(
               "FPS %d  FRAME %.2f ms",
               GetFPS(), performanceStats_.frame.averageMilliseconds));
    drawLine(
        1, TextFormat(
               "UPDATE %.2f ms  FIXED %d",
               performanceStats_.simulation.averageMilliseconds,
               static_cast<int>(performanceStats_.fixedTicks)));
    drawLine(
        2, TextFormat(
               "SIM TICK %.2f ms",
               performanceStats_.simulationTick.averageMilliseconds));
    drawLine(
        3, TextFormat(
               "ENEMY %.2f ms  COLL %.2f ms",
               enemyStats.tick.averageMilliseconds,
               enemyStats.collision.averageMilliseconds));
    drawLine(
        4, TextFormat(
               "HASH %.2f ms  REBUILDS %d",
               enemyStats.spatialRebuild.averageMilliseconds,
               static_cast<int>(enemyStats.spatialRebuilds)));
    drawLine(
        5, TextFormat(
               "ACTIVE %d  VISIBLE %d",
               static_cast<int>(snapshot.activeEnemyCount),
               static_cast<int>(performanceStats_.visibleEnemies)));
    drawLine(
        6, TextFormat(
               "RENDER %.2f ms",
               performanceStats_.render.averageMilliseconds));
    drawLine(
        7, TextFormat(
               "ENEMY DRAW %.2f  INST %.2f ms",
               performanceStats_.enemyRender.averageMilliseconds,
               rendererStats.instancedEnemyDraw.averageMilliseconds));
    drawLine(
        8, TextFormat(
               "INSTANCES %d  BATCHES %d  LOD %d",
               static_cast<int>(rendererStats.instancedEnemyCount),
               static_cast<int>(rendererStats.enemyBatchCount),
               static_cast<int>(rendererStats.lowDetailEnemyCount)));
    drawLine(
        9, TextFormat(
               "BLOB %.2f ms  SHADOWS %d",
               performanceStats_.blobShadows.averageMilliseconds,
               static_cast<int>(performanceStats_.enemyShadowDraws)));
    drawLine(
        10, TextFormat(
                "BLOB TRIANGLES %d",
                static_cast<int>(rendererStats.blobShadowTriangles)));
    const InsightSystem& insight = simulation_.insightSystem();
    const InsightGrantResult& lastInsight = insight.lastGrant();
    drawLine(11, TextFormat(
        "INSIGHT %.1f / %.1f  POINTS %d",
        snapshot.currentInsight, snapshot.requiredInsight,
        snapshot.skillPoints));
    drawLine(12, TextFormat(
        "LAST +%.2f  %s  DR x%.2f",
        lastInsight.finalAmount,
        insightSourceName(lastInsight.source).data(),
        lastInsight.diminishingMultiplier));
    const auto& earned = insight.earnedByCategory();
    drawLine(13, TextFormat(
        "COMBAT %.1f  GATHER %.1f  BUILD %.1f",
        earned[static_cast<std::size_t>(InsightCategory::Combat)],
        earned[static_cast<std::size_t>(InsightCategory::Gathering)],
        earned[static_cast<std::size_t>(InsightCategory::Building)]));
    drawLine(14, TextFormat(
        "REPAIR %.1f  EXPLORE %.1f",
        earned[static_cast<std::size_t>(InsightCategory::Repair)],
        earned[static_cast<std::size_t>(InsightCategory::Exploration)]));
    drawLine(15, TextFormat(
        "BLOCKED DUPLICATES %llu",
        static_cast<unsigned long long>(insight.blockedDuplicateEvents())));
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
