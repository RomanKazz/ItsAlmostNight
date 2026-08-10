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
        if (chest.looseLoot) continue;
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
