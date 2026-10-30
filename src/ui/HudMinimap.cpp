#include "ui/UiViewport.hpp"
#include "ui/HudRenderer.hpp"

#include "game/Simulation.hpp"
#include "core/PerformanceStats.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/MinimapSymbols.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace ian {
namespace {

MinimapRenderPerformanceStats minimapPerformanceStats{};

Color objectiveAccent(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone:
        return {184, 151, 255, 255};
    case ObjectiveKind::Challenge:
        return {255, 190, 73, 255};
    case ObjectiveKind::WorldEvent:
        return {79, 220, 207, 255};
    }
    return {235, 214, 170, 255};
}

float fitObjectiveText(
    std::string_view text, float preferred,
    float minimum, float maximumWidth) {
    float size = preferred;
    while (size > minimum &&
           measureUiText(text, size).x > maximumWidth) {
        size -= 0.5F;
    }
    return size;
}

void drawExpandedObjectiveList(
    const SimulationSnapshot& snapshot,
    float mapPanelX, float mapPanelY,
    float mapPanelHeight, float expansion) {
    if (snapshot.objectives.empty() &&
        !snapshot.tutorialObjective) return;
    const float visibility = std::clamp(
        (expansion - 0.48F) / 0.30F, 0.0F, 1.0F);
    if (visibility <= 0.001F) return;
    const auto withAlpha = [visibility](Color color) {
        color.a = static_cast<unsigned char>(
            static_cast<float>(color.a) * visibility);
        return color;
    };

    constexpr float PanelWidth = 292.0F;
    const float panelX = std::max(
        16.0F, mapPanelX - PanelWidth - 22.0F);
    const float panelY = std::max(28.0F, mapPanelY + 8.0F);
    const float availableHeight = std::min(
        mapPanelHeight - 16.0F,
        static_cast<float>(uiScreenHeight()) - panelY - 24.0F);
    constexpr float HeaderHeight = 40.0F;
    constexpr float RowHeight = 52.0F;
    constexpr float RowGap = 7.0F;
    const int maximumRows = std::max(
        1,
        static_cast<int>((availableHeight - HeaderHeight - 22.0F) /
                         (RowHeight + RowGap)));

    std::vector<const ObjectiveStatus*> objectives;
    objectives.reserve(snapshot.objectives.size());
    for (const int index : snapshot.recommendedObjectives) {
        if (index < 0 ||
            static_cast<std::size_t>(index) >= snapshot.objectives.size()) {
            continue;
        }
        const ObjectiveStatus& objective =
            snapshot.objectives[static_cast<std::size_t>(index)];
        if (objective.active && !objective.completed) {
            objectives.push_back(&objective);
        }
    }
    for (const ObjectiveStatus& objective : snapshot.objectives) {
        if (!objective.active || objective.completed ||
            std::ranges::find(objectives, &objective) != objectives.end()) {
            continue;
        }
        objectives.push_back(&objective);
    }

    const bool hasTutorial = snapshot.tutorialObjective.has_value();
    const int totalRows = static_cast<int>(objectives.size()) +
        (hasTutorial ? 1 : 0);
    const int visibleRows = std::min(totalRows, maximumRows);
    const bool hasOverflow = totalRows > visibleRows;
    const float footerHeight = hasOverflow ? 23.0F : 8.0F;
    const float panelHeight = HeaderHeight +
        static_cast<float>(visibleRows) * (RowHeight + RowGap) +
        footerHeight;
    const Rectangle panel{
        panelX, panelY, PanelWidth, panelHeight};
    menuStyle::panel(panel, .8F, withAlpha(menuStyle::Frame));
    menuStyle::text("OBJECTIVES",
        {panel.x + 14, panel.y + 5, panel.width - 28, 30},
        17, withAlpha(menuStyle::Cream), UiFontRole::Display);

    int rowIndex = 0;
    const auto drawRow = [&](std::string_view title,
                             double current, double target,
                             Color accent, int reward) {
        if (rowIndex >= visibleRows) return;
        const Rectangle row{
            panel.x + 8.0F,
            panel.y + HeaderHeight +
                static_cast<float>(rowIndex) * (RowHeight + RowGap),
            panel.width - 16.0F, RowHeight};
        drawSolidRoundedRect(row, .12F, withAlpha(menuStyle::Inset));
        const Vector2 iconCenter{row.x + 19.0F, row.y + 20.0F};
        DrawPoly(iconCenter, 4, 6.5F, 45.0F, withAlpha(accent));
        DrawPolyLines(
            iconCenter, 4, 9.0F, 45.0F,
            withAlpha({255, 239, 196, 190}));

        std::string progress;
        if (target > 0.0) {
            progress = std::to_string(static_cast<int>(
                           std::floor(std::min(current, target)))) +
                "/" + std::to_string(
                    static_cast<int>(std::ceil(target)));
        }
        const float progressWidth = progress.empty()
            ? 0.0F
            : measureUiText(progress, 11.0F).x;
        const float titleX = row.x + 36.0F;
        const float titleMaximumWidth =
            row.x + row.width - 9.0F - titleX -
            (progress.empty() ? 0.0F : progressWidth + 10.0F);
        const float titleSize = fitObjectiveText(
            title, 13.0F, 9.5F, titleMaximumWidth);
        drawUiText(
            title, {titleX, row.y + 8.0F}, titleSize,
            withAlpha({255, 241, 211, 255}));
        if (!progress.empty()) {
            drawUiText(
                progress,
                {row.x + row.width - progressWidth - 9.0F,
                 row.y + 8.5F},
                11.0F, withAlpha({255, 241, 211, 255}));
            const float fraction = std::clamp(
                static_cast<float>(current / target), 0.0F, 1.0F);
            const Rectangle track{
                titleX, row.y + 36.0F,
                row.x + row.width - 9.0F - titleX, 5.0F};
            drawSolidRoundedRect(track, .8F, withAlpha({123, 134, 128, 255}));
            if (fraction > 0.001F) {
                drawSolidRoundedRect(
                    {track.x, track.y, track.width * fraction,
                     track.height},
                    .8F, withAlpha(accent));
            }
        } else if (reward > 0) {
            const std::string rewardText =
                "+" + std::to_string(reward) + " XP";
            drawUiText(
                rewardText, {titleX, row.y + 31.0F}, 10.0F,
                withAlpha({255, 205, 108, 255}));
        }
        ++rowIndex;
    };

    if (snapshot.tutorialObjective) {
        switch (*snapshot.tutorialObjective) {
        case TutorialObjective::MineWood:
            drawRow(
                "CHOP TREES", snapshot.wood,
                snapshot.tutorialWoodTarget,
                {184, 151, 255, 255}, 0);
            break;
        case TutorialObjective::PlaceCore:
            drawRow(
                "PLACE THE CORE", 0.0, 0.0,
                {184, 151, 255, 255}, 0);
            break;
        case TutorialObjective::PrepareForNight:
            drawRow(
                "BUILD DEFENSES", 0.0, 0.0,
                {184, 151, 255, 255}, 0);
            break;
        case TutorialObjective::SurviveFirstWave:
            drawRow(
                "SURVIVE THE NIGHT", 0.0, 0.0,
                {184, 151, 255, 255}, 0);
            break;
        case TutorialObjective::OpenBuildMode:
            drawRow(
                "OPEN BUILD MODE", 0.0, 0.0,
                {184, 151, 255, 255}, 0);
            break;
        }
    }
    for (const ObjectiveStatus* objective : objectives) {
        drawRow(
            objective->definition.title,
            objective->progress,
            objective->definition.target,
            objectiveAccent(objective->definition.kind),
            static_cast<int>(std::lround(
                objective->definition.insightReward)));
    }
    if (hasOverflow) {
        const int hidden = totalRows - visibleRows;
        drawUiText(
            "+" + std::to_string(hidden) + " MORE",
            {panel.x + 14.0F,
             panel.y + panel.height - 20.0F},
            10.0F, withAlpha({214, 184, 148, 255}));
    }
}

Vector2 rotateMinimapOffset(
    double worldX, double worldZ, double playerYaw) {
    const double sine = std::sin(playerYaw);
    const double cosine = std::cos(playerYaw);
    return {
        static_cast<float>(worldX * cosine + worldZ * sine),
        static_cast<float>(-worldX * sine + worldZ * cosine),
    };
}

Vector2 minimapPoint(
    Rectangle bounds, float scale, double playerYaw,
    double centerWorldX, double centerWorldZ,
    double worldX, double worldZ) {
    // The map texture is rendered north-up. Most of its terrain vertices
    // reach this path with zero yaw; avoid trigonometry for every vertex.
    if (playerYaw == 0.0) {
        return {
            bounds.x + bounds.width * 0.5F +
                static_cast<float>(worldX - centerWorldX) * scale,
            bounds.y + bounds.height * 0.5F +
                static_cast<float>(worldZ - centerWorldZ) * scale,
        };
    }
    const Vector2 offset = rotateMinimapOffset(
        worldX - centerWorldX,
        worldZ - centerWorldZ,
        playerYaw);
    return {
        bounds.x + bounds.width * 0.5F + offset.x * scale,
        bounds.y + bounds.height * 0.5F + offset.y * scale,
    };
}

constexpr int CompassFrameSegments = 192;

void drawRoundedCompassPlate(
    Vector2 center, float radius, float, float, float, Color color) {
    // The frame is a circle, independent of heading. Only the map, labels
    // and warning markers rotate; the old hand-drawn waviness has no place here.
    DrawCircleSector(center, radius, 0, 360, CompassFrameSegments, color);
}

void drawRoundedPlayerMarker(
    Vector2 center, float scale, Color fill, float heading = 0) {
    const auto point = [&](float x, float y) {
        return Vector2{center.x + (x * std::cos(heading) - y * std::sin(heading)) * scale,
            center.y + (x * std::sin(heading) + y * std::cos(heading)) * scale};
    };
    const auto chevron = [&](float size, Color color) {
        const Vector2 tip = point(0, -11 * size), left = point(-7 * size, 7 * size),
            notch = point(0, 4 * size), right = point(7 * size, 7 * size);
        DrawTriangle(tip, left, notch, color);
        DrawTriangle(tip, notch, right, color);
    };
    chevron(1.28F, {20, 23, 20, 255});
    chevron(1.0F, fill);
}

constexpr float CachedFrameMapDiameter = 800.0F;

void drawMapIcons(GameUi& ui, const SimulationSnapshot& snapshot,
                  Rectangle bounds, double radius, float expanded, float renderScale) {
    const Vector2 center{bounds.x + bounds.width * .5F, bounds.y + bounds.height * .5F};
    const float scale = bounds.width / static_cast<float>(2 * radius);
    struct Occupied { Vector2 point; float radius; };
    std::vector<Occupied> occupied;
    occupied.reserve(ui.minimapMarkers().size());
    for (const MinimapMarker& marker : ui.minimapMarkers()) {
        if (marker.icon == MapIcon::Wood && !marker.highlighted) {
            const int cellX = static_cast<int>(std::floor(marker.position.x / 24));
            const int cellZ = static_cast<int>(std::floor(marker.position.z / 24));
            if (((cellX % 2 + 2) % 2) != 1 || ((cellZ % 2 + 2) % 2) != 1) continue;
        }
        const Vector2 point = minimapPoint(bounds, scale, 0,
            snapshot.playerPosition.x, snapshot.playerPosition.z, marker.position.x, marker.position.z);
        const bool important = marker.priority >= 3;
        const float size = std::lerp(important ? 18.0F : 11.0F,
            important ? 26.0F : 14.0F, expanded) * renderScale;
        const float distance = Vector2Distance(point, center);
        if (distance > bounds.width * .5F - size * .65F - 3 * renderScale ||
            (!important && distance < 17 * renderScale)) continue;
        const float collisionRadius = size * (important ? .54F : .36F);
        // Keep chest/challenge markers; decorative/resource markers yield first.
        if (marker.priority < 5 && std::ranges::any_of(occupied, [&](const Occupied& other) {
            return Vector2Distance(point, other.point) < collisionRadius + other.radius;
        })) continue;
        occupied.push_back({point, collisionRadius});
        drawMinimapSymbol(marker.icon, point, size, important,
            static_cast<float>(snapshot.playerYaw) * RAD2DEG, marker.highlighted);
    }
}

void drawMapLegend(Rectangle mapBounds, float expansion) {
    if (expansion < .9F) return;
    const float x = mapBounds.x + mapBounds.width + 48;
    const float y = mapBounds.y + 12;
    constexpr float width = 192;
    menuStyle::text("PLACES", {x, y, width, 28}, 18, menuStyle::Muted, UiFontRole::Display);
    constexpr std::array icons{MapIcon::Base, MapIcon::Chest, MapIcon::Supplies,
        MapIcon::PlayerShrine, MapIcon::Industry, MapIcon::Challenge, MapIcon::Ruins, MapIcon::Boss};
    constexpr std::array<std::string_view, 8> labels{
        "BASE", "CHESTS", "SUPPLIES", "SHRINES", "INDUSTRY", "CHALLENGES", "SITES", "BOSS"};
    constexpr float row = 34;
    for (std::size_t i = 0; i < icons.size(); ++i) {
        const float top = y + 36 + static_cast<float>(i) * row;
        drawMinimapSymbol(icons[i], {x + 13, top + row * .5F}, 23, true);
        menuStyle::text(labels[i], {x + 36, top, width - 36, row}, 17,
            menuStyle::Cream, UiFontRole::Display);
    }
    const float resourceY = y + 36 + static_cast<float>(icons.size()) * row + 24;
    DrawLineEx({x, resourceY - 12}, {x + width, resourceY - 12}, 1, {94, 108, 91, 255});
    menuStyle::text("RESOURCES", {x, resourceY, width, 28}, 18,
        menuStyle::Muted, UiFontRole::Display);
    constexpr std::array resources{MapIcon::Wood, MapIcon::Stone, MapIcon::Crystal};
    constexpr std::array<std::string_view, 3> resourceLabels{"WOOD", "STONE", "CRYSTALS"};
    for (std::size_t i = 0; i < resources.size(); ++i) {
        const float top = resourceY + 32 + static_cast<float>(i) * 29;
        drawMinimapSymbol(resources[i], {x + 13, top + 14}, 23);
        menuStyle::text(resourceLabels[i], {x + 36, top, width - 36, 29}, 16,
            menuStyle::Cream, UiFontRole::Display);
    }
    const float biomesY = resourceY + 132;
    DrawLineEx({x, biomesY - 12}, {x + width, biomesY - 12}, 1, {94, 108, 91, 255});
    constexpr std::array<Color, 4> colors{{{91,126,76,255}, {158,132,84,255},
        {111,145,151,255}, {122,87,119,255}}};
    constexpr std::array<std::string_view, 4> biomes{"FOREST", "DESERT", "SNOW", "CORRUPTED"};
    for (std::size_t i = 0; i < biomes.size(); ++i) {
        const float top = biomesY + static_cast<float>(i / 2) * 26;
        const float left = x + static_cast<float>(i % 2) * 100;
        DrawRectangleRounded({left, top + 6, 10, 10}, .3F, 3, colors[i]);
        menuStyle::text(biomes[i], {left + 16, top, 80, 23}, 11,
            menuStyle::Muted, UiFontRole::Display);
    }
}

void ensureMinimapFrameTarget(GameUi& ui) {
    if (ui.minimapFrameTargetReady() ||
        !ui.beginMinimapFrameTarget()) {
        return;
    }
    const float targetSize = static_cast<float>(
        ui.minimapFrameTargetSize());
    const Vector2 center{targetSize * 0.5F, targetSize * 0.5F};
    constexpr float mapRadius = CachedFrameMapDiameter * 0.5F;
    constexpr float frameScale = CachedFrameMapDiameter / 150.0F;
    constexpr float outerPadding = 5.0F * frameScale;
    constexpr float lightPadding = 2.2F * frameScale;
    constexpr float outerSpikeLength = 1.4F * frameScale;
    constexpr float lightSpikeLength = 1.0F * frameScale;
    constexpr float spikeHalfAngle = 0.13F;
    drawRoundedCompassPlate(
        center, mapRadius + outerPadding, outerSpikeLength,
        spikeHalfAngle, 0.0F, {18, 23, 22, 255});
    drawRoundedCompassPlate(
        center, mapRadius + lightPadding, lightSpikeLength,
        spikeHalfAngle, 0.0F, {118, 137, 111, 255});
    // The collapsed border is static in compass coordinates.
    // Cache its 64-segment mesh with the compass frame instead of rebuilding
    // and submitting it every HUD frame.
    constexpr float borderThickness = 1.0F * frameScale;
    DrawRing(
        center,
        mapRadius - borderThickness,
        mapRadius,
        0.0F, 360.0F, CompassFrameSegments,
        {118, 137, 111, 255});
    ui.endMinimapFrameTarget();
}

} // namespace

const MinimapRenderPerformanceStats&
minimapRenderPerformanceStats() noexcept {
    return minimapPerformanceStats;
}

void drawMinimapHud(GameUi& ui, const SimulationSnapshot& snapshot,
                    float expansion, MinimapHudPass pass) {
    const bool prepareOnly = pass == MinimapHudPass::Prepare;
    const ScopedUiCanvas canvas;
    minimapPerformanceStats = {};
    auto minimapSectionStart = PerformanceClock::now();
    if (expansion < 0.0F) return;
    const float rawExpansion = std::clamp(expansion, 0.0F, 1.0F);
    const float expanded =
        rawExpansion * rawExpansion * (3.0F - 2.0F * rawExpansion);
    const float screenMinimum = static_cast<float>(
        std::min(uiScreenWidth(), uiScreenHeight()));
    const float collapsedMapSize = std::clamp(
        screenMinimum * 0.185F, 130.0F, 168.0F);
    const float objectiveWidth = snapshot.objectives.empty() && !snapshot.tutorialObjective
        ? 0.0F : 314.0F;
    const float expandedMapSize = std::max(
        collapsedMapSize,
        std::min(
            static_cast<float>(uiScreenWidth()) * 0.62F,
            std::min(static_cast<float>(uiScreenHeight()) * 0.70F,
                static_cast<float>(uiScreenWidth()) - objectiveWidth - 390.0F)));
    const float displayMapSize = std::lerp(
        collapsedMapSize, expandedMapSize, expanded);
    const float panelPadding = std::lerp(4.0F, 54.0F, expanded);
    const float headerHeight = std::lerp(0.0F, 60.0F, expanded);
    const float panelWidth = displayMapSize + panelPadding * 2.0F + 244.0F * expanded;
    const float collapsedPanelWidth =
        collapsedMapSize + 4.0F * 2.0F;
    const float collapsedCompassMargin = 32.0F;
    const float collapsedPanelX =
        static_cast<float>(uiScreenWidth()) -
        collapsedPanelWidth - 12.0F - collapsedCompassMargin;
    const float expandedPanelX =
        (static_cast<float>(uiScreenWidth()) - panelWidth + objectiveWidth) * 0.5F;
    const float expandedPanelY =
        static_cast<float>(uiScreenHeight()) * 0.5F -
        (displayMapSize + panelPadding * 2.0F + headerHeight) * 0.5F;
    const float panelX = std::lerp(
        collapsedPanelX, expandedPanelX, expanded);
    const float panelY = std::lerp(
        collapsedCompassMargin,
        expandedPanelY, expanded);
    const Rectangle displayMapBounds{
        panelX + panelPadding,
        panelY + panelPadding + headerHeight,
        displayMapSize, displayMapSize,
    };
    const float worldLimit = std::max(
        static_cast<float>(snapshot.worldLimit), 1.0F);
    const double collapsedViewRadius = std::min(
        static_cast<double>(worldLimit), 58.0);
    const double viewRadius = std::lerp(
        collapsedViewRadius,
        static_cast<double>(worldLimit),
        static_cast<double>(expanded));

    const double compassYaw = snapshot.playerYaw * (1.0 - expanded);
    const double centerWorldX = snapshot.playerPosition.x * (1.0 - expanded);
    const double centerWorldZ = snapshot.playerPosition.z * (1.0 - expanded);

    if (!prepareOnly && expanded > 0.001F) {
        DrawRectangle(
            0, 0, uiScreenWidth(), uiScreenHeight(),
            {11, 19, 18,
             static_cast<unsigned char>(
                 std::lround(192.0F * expanded))});
    }
    const int desiredTargetSize = expanded > 0.55F
        ? 1024
        : 512;
    ui.updateMinimapTarget(desiredTargetSize);
    const double minimapNow = GetTime();
    const bool expansionAnimating =
        rawExpansion > 0.001F && rawExpansion < 0.999F;
    const double refreshInterval = expanded > 0.55F
        ? 1.0 / 30.0
        : 1.0 / 20.0;
    const bool refreshTarget = pass != MinimapHudPass::Composite &&
        ui.minimapTargetRefreshDue(minimapNow, refreshInterval, expansionAnimating);
    minimapPerformanceStats.setupMs =
        performanceMilliseconds(minimapSectionStart);
    minimapSectionStart = PerformanceClock::now();
    if (refreshTarget) {
    if (!ui.beginMinimapTarget()) {
        return;
    }
    const float mapSize = static_cast<float>(ui.minimapTargetSize());
    const Rectangle mapBounds{0.0F, 0.0F, mapSize, mapSize};
    const float mapScale = mapSize * 0.5F /
        static_cast<float>(std::max(viewRadius, 1.0));
    const float renderScale = mapSize /
        std::max(displayMapSize, 1.0F);
    constexpr double MapYaw = 0.0;

    // Only copy the landscape view, not the snapshot's owned progression lists.
    SimulationSnapshot mapSnapshot{};
    mapSnapshot.terrainSeed = snapshot.terrainSeed;
    mapSnapshot.terrainSamples = snapshot.terrainSamples;
    mapSnapshot.terrainResolution = snapshot.terrainResolution;
    mapSnapshot.terrainWorldSize = snapshot.terrainWorldSize;
    mapSnapshot.terrainPaths = snapshot.terrainPaths;
    mapSnapshot.ponds = snapshot.ponds;
    mapSnapshot.resourceNodes = snapshot.resourceNodes;
    mapSnapshot.playerPosition.x = centerWorldX;
    mapSnapshot.playerPosition.z = centerWorldZ;
    mapSnapshot.playerYaw = compassYaw;
    ui.drawMinimapLandscape(mapSnapshot, mapBounds, viewRadius);
    ui.updateMinimapMarkers(snapshot);
    const Vector2 mapCenter{
        mapBounds.x + mapSize * 0.5F,
        mapBounds.y + mapSize * 0.5F,
    };

    const float symbolScale =
        std::lerp(1.0F, 1.55F, expanded) * renderScale;
    const auto mapPoint =
        [centerWorldX, centerWorldZ, mapBounds, mapScale, mapYaw = MapYaw](
            double worldX, double worldZ) {
            return minimapPoint(
                mapBounds, mapScale, mapYaw,
                centerWorldX, centerWorldZ, worldX, worldZ);
        };
    const float pointMargin = 12.0F * symbolScale;
    const auto pointVisible = [mapBounds, pointMargin](Vector2 point) {
        return point.x >= mapBounds.x - pointMargin &&
            point.x <= mapBounds.x + mapBounds.width + pointMargin &&
            point.y >= mapBounds.y - pointMargin &&
            point.y <= mapBounds.y + mapBounds.height + pointMargin;
    };

    beginUiScissor(
        static_cast<int>(mapBounds.x),
        static_cast<int>(mapBounds.y),
        static_cast<int>(mapBounds.width),
        static_cast<int>(mapBounds.height));

    drawMapIcons(ui, mapSnapshot, mapBounds, viewRadius, expanded, renderScale);

    const double cellSize = std::max(snapshot.worldCellSize, 0.01);
    const auto modularPoint =
        [&mapPoint, cellSize](GridCoord anchor,
                              double widthCells,
                              double depthCells) {
            return mapPoint(
                (static_cast<double>(anchor.x) + widthCells * 0.5) *
                    cellSize,
                (static_cast<double>(anchor.z) + depthCells * 0.5) *
                    cellSize);
        };
    for (const PlatformFrameInstance& frame : snapshot.platformFrames) {
        const Vector2 point = modularPoint(
            frame.anchor, PlatformFrameWidthCells,
            PlatformFrameWidthCells);
        if (!pointVisible(point)) {
            continue;
        }
        DrawRectangleRec(
            {point.x - 1.7F * symbolScale,
             point.y - 1.7F * symbolScale,
             3.4F * symbolScale, 3.4F * symbolScale},
            {164, 144, 111, 185});
    }
    for (const WallInstance& wall : snapshot.modularWalls) {
        const Vector2 point = modularPoint(wall.anchor, 1.0, 1.0);
        if (!pointVisible(point)) {
            continue;
        }
        const bool alongX =
            wall.rotation == Rotation::Deg0 ||
            wall.rotation == Rotation::Deg180;
        DrawRectangleRec(
            {point.x - (alongX ? 2.5F : 0.8F) * symbolScale,
             point.y - (alongX ? 0.8F : 2.5F) * symbolScale,
             (alongX ? 5.0F : 1.6F) * symbolScale,
             (alongX ? 1.6F : 5.0F) * symbolScale},
            {194, 160, 108, 210});
    }
    for (const RampInstance& ramp : snapshot.ramps) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const Vector2 point = modularPoint(
            ramp.anchor,
            alongZ ? ModularRampWidthCells : ModularRampRunCells,
            alongZ ? ModularRampRunCells : ModularRampWidthCells);
        if (!pointVisible(point)) {
            continue;
        }
        DrawCircleV(
            point, 2.0F * symbolScale,
            {205, 174, 119, 190});
    }

    for (const BuildingInstance& building : snapshot.buildings) {
        const Vec3 position = buildingWorldPosition(building);
        const Vector2 point = mapPoint(position.x, position.z);
        if (building.type != BuildingType::Core &&
            !pointVisible(point)) {
            continue;
        }
        switch (building.type) {
        case BuildingType::Core: {
            const Vector2 delta = Vector2Subtract(point, mapCenter);
            const float distance = Vector2Length(delta);
            const float maximum = mapSize * .5F - 18 * renderScale;
            const Vector2 marker = Vector2Add(mapCenter,
                Vector2Scale(delta, distance > maximum ? maximum / distance : 1));
            const float size = std::lerp(22.0F, 32.0F, expanded) * renderScale;
            drawMinimapSymbol(MapIcon::Base, marker, size, true,
                static_cast<float>(compassYaw) * RAD2DEG);
            break;
        }
        case BuildingType::Turret:
        case BuildingType::GunTurret:
        case BuildingType::Cannon:
        case BuildingType::TeslaTower:
        case BuildingType::LaserTower:
        case BuildingType::Mortar:
        case BuildingType::RepairStation:
    case BuildingType::WindBlower:
    case BuildingType::DecoyBeacon:
        case BuildingType::MissileLauncher:
        case BuildingType::Flamethrower:
        case BuildingType::Catapult:
            DrawCircleV(
                point,
                (building.type == BuildingType::Cannon ? 3.5F : 3.0F) *
                    symbolScale,
                {64, 153, 243, 255});
            DrawCircleV(
                point, 1.1F * symbolScale,
                {73, 54, 38, 255});
            break;
        case BuildingType::Wall:
        case BuildingType::Gate:
            DrawRectangleRec(
                {point.x - 2.5F * symbolScale,
                 point.y - 1.4F * symbolScale,
                 5.0F * symbolScale, 2.8F * symbolScale},
                building.type == BuildingType::Gate
                    ? Color{231, 203, 131, 240}
                    : Color{188, 142, 86, 230});
            break;
        case BuildingType::SlowTrap:
            DrawRing(point, 2.1F * symbolScale,
                     3.0F * symbolScale, 0.0F, 360.0F, 12,
                     {102, 190, 220, 235});
            break;
        case BuildingType::SpikeTrap:
            DrawPoly(
                point, 4, 3.2F * symbolScale, 45.0F,
                {222, 101, 75, 240});
            break;
        case BuildingType::CrystalMine:
        case BuildingType::LumberMill:
        case BuildingType::Quarry: {
            Color color{120, 209, 218, 240};
            if (building.type == BuildingType::LumberMill) {
                color = {112, 184, 91, 240};
            } else if (building.type == BuildingType::Quarry) {
                color = {167, 174, 171, 240};
            }
            DrawRectangleRec(
                {point.x - 3.0F * symbolScale,
                 point.y - 3.0F * symbolScale,
                 6.0F * symbolScale, 6.0F * symbolScale},
                color);
            break;
        }
        case BuildingType::WoodStorage:
        case BuildingType::StoneStorage:
        case BuildingType::CrystalStorage: {
            const Color color =
                building.type == BuildingType::WoodStorage
                    ? Color{151, 98, 54, 240}
                    : building.type == BuildingType::StoneStorage
                        ? Color{151, 158, 166, 240}
                        : Color{112, 126, 222, 240};
            DrawPoly(point, 4, 4.0F * symbolScale,
                     45.0F, color);
            break;
        }
        }
    }

    struct EnemyMarkerAppearance {
        float radius{2.25F};
        Color color{239, 75, 66, 245};
        int priority{1};
    };
    const auto markerAppearance = [](EnemyType type) {
        EnemyMarkerAppearance result{};
        if (type == EnemyType::Fast) {
            result.radius = 1.8F;
        } else if (type == EnemyType::Heavy) {
            result.radius = 2.8F;
            result.priority = 3;
        } else if (type == EnemyType::Boss) {
            result.radius = 4.6F;
            result.color = {255, 123, 55, 255};
            result.priority = 6;
        } else if (type == EnemyType::AirTank) {
            result.radius = 3.2F;
            result.color = {174, 209, 93, 250};
            result.priority = 4;
        } else if (enemyIsFlying(type)) {
            result.color = type==EnemyType::Bee?Color{255,205,54,250}:Color{220,105,232,250};
            result.priority = 2;
        } else if (type == EnemyType::Splitter) {
            result.radius = 3.0F;
            result.color = {87, 194, 92, 250};
            result.priority = 4;
        } else if (type == EnemyType::Splitling) {
            result.radius = 1.55F;
            result.color = {116, 224, 108, 245};
        }
        return result;
    };
    const auto drawEnemyMarker = [symbolScale](
                                     Vector2 point,
                                     EnemyMarkerAppearance appearance,
                                     int clusteredEnemies = 1) {
        const float clusterGrowth = std::min(
            1.7F,
            std::log2(static_cast<float>(
                std::max(clusteredEnemies, 1))) * 0.32F);
        const float radius = appearance.radius + clusterGrowth;
        DrawCircleV(
            point, (radius + 1.0F) * symbolScale,
            {51, 16, 17, 210});
        DrawCircleV(
            point, radius * symbolScale,
            clusteredEnemies > 1
                ? ColorLerp(
                      appearance.color, {255, 151, 91, 255},
                      std::min(0.28F, clusterGrowth * 0.12F))
                : appearance.color);
    };

    if (expanded > 0.55F) {
        for (const EnemyInstance& enemy : snapshot.enemies) {
            if (!enemy.active || enemy.state == EnemyState::Dead || enemy.type == EnemyType::Boss) {
                continue;
            }
            const Vector2 point = mapPoint(
                enemy.position.x, enemy.position.z);
            if (pointVisible(point)) {
                drawEnemyMarker(
                    point, markerAppearance(enemy.type));
            }
        }
    } else {
        // The compact map cannot visually resolve hundreds of overlapping
        // enemy dots. Aggregate them into screen-space cells so crowd cost is
        // bounded while bosses and large enemies keep visual priority.
        constexpr int EnemyClusterCells = 18;
        struct EnemyMarkerCluster {
            Vector2 positionSum{};
            int count{};
            EnemyMarkerAppearance appearance{};
        };
        std::array<
            EnemyMarkerCluster,
            EnemyClusterCells * EnemyClusterCells> clusters{};
        for (const EnemyInstance& enemy : snapshot.enemies) {
            if (!enemy.active || enemy.state == EnemyState::Dead || enemy.type == EnemyType::Boss) {
                continue;
            }
            const Vector2 point = mapPoint(
                enemy.position.x, enemy.position.z);
            if (!pointVisible(point)) {
                continue;
            }
            const float normalizedX = std::clamp(
                (point.x - mapBounds.x) /
                    std::max(mapBounds.width, 1.0F),
                0.0F, 0.9999F);
            const float normalizedY = std::clamp(
                (point.y - mapBounds.y) /
                    std::max(mapBounds.height, 1.0F),
                0.0F, 0.9999F);
            const int cellX = static_cast<int>(
                normalizedX * EnemyClusterCells);
            const int cellY = static_cast<int>(
                normalizedY * EnemyClusterCells);
            EnemyMarkerCluster& cluster = clusters[
                static_cast<std::size_t>(cellY * EnemyClusterCells +
                                         cellX)];
            cluster.positionSum = Vector2Add(
                cluster.positionSum, point);
            ++cluster.count;
            const EnemyMarkerAppearance appearance =
                markerAppearance(enemy.type);
            if (appearance.priority > cluster.appearance.priority) {
                cluster.appearance = appearance;
            } else {
                cluster.appearance.radius = std::max(
                    cluster.appearance.radius, appearance.radius);
            }
        }
        for (const EnemyMarkerCluster& cluster : clusters) {
            if (cluster.count <= 0) {
                continue;
            }
            drawEnemyMarker(
                Vector2Scale(
                    cluster.positionSum,
                    1.0F / static_cast<float>(cluster.count)),
                cluster.appearance, cluster.count);
        }
    }

    for (const auto& enemy : snapshot.enemies) {
        if (!enemy.active || enemy.state == EnemyState::Dead || enemy.type != EnemyType::Boss) continue;
        const Vector2 point = mapPoint(enemy.position.x, enemy.position.z);
        if (!pointVisible(point)) continue;
        const float size = std::lerp(23.0F, 32.0F, expanded) * renderScale;
        drawMinimapSymbol(MapIcon::Boss, point, size, true,
            static_cast<float>(compassYaw) * RAD2DEG);
    }

    const Vector2 player = mapPoint(
        snapshot.playerPosition.x, snapshot.playerPosition.z);
    if (snapshot.nearestChestPosition) {
        const Vector2 chestPoint = mapPoint(
            snapshot.nearestChestPosition->x,
            snapshot.nearestChestPosition->z);
        const Vector2 delta{
            chestPoint.x - player.x,
            chestPoint.y - player.y,
        };
        const float distance = std::sqrt(
            delta.x * delta.x + delta.y * delta.y);
        if (distance > 0.001F) {
            const Vector2 directionToChest{
                delta.x / distance, delta.y / distance};
            const float edgeMargin = 8.0F * symbolScale;
            const float maximumMarkerDistance =
                mapSize * 0.5F - edgeMargin;
            const float playerToChestDistance = std::sqrt(
                (chestPoint.x - mapCenter.x) *
                    (chestPoint.x - mapCenter.x) +
                (chestPoint.y - mapCenter.y) *
                    (chestPoint.y - mapCenter.y));
            const float markerScale = playerToChestDistance >
                    maximumMarkerDistance
                ? maximumMarkerDistance / playerToChestDistance
                : 1.0F;
            const Vector2 marker{
                mapCenter.x +
                    (chestPoint.x - mapCenter.x) * markerScale,
                mapCenter.y +
                    (chestPoint.y - mapCenter.y) * markerScale,
            };
            DrawLineEx(
                player, marker,
                std::max(1.4F, symbolScale * 1.6F),
                {255, 212, 91, 180});
            const Vector2 perpendicular{
                -directionToChest.y, directionToChest.x};
            DrawTriangle(
                {marker.x + directionToChest.x * 7.0F * symbolScale,
                 marker.y + directionToChest.y * 7.0F * symbolScale},
                {marker.x - directionToChest.x * 3.0F * symbolScale +
                     perpendicular.x * 4.0F * symbolScale,
                 marker.y - directionToChest.y * 3.0F * symbolScale +
                     perpendicular.y * 4.0F * symbolScale},
                {marker.x - directionToChest.x * 3.0F * symbolScale -
                     perpendicular.x * 4.0F * symbolScale,
                 marker.y - directionToChest.y * 3.0F * symbolScale -
                     perpendicular.y * 4.0F * symbolScale},
                {255, 220, 105, 255});
        }
    }

    for (std::size_t directionIndex = 0;
         directionIndex < snapshot.upcomingAttackDirections.size();
         ++directionIndex) {
        if (!snapshot.upcomingAttackDirections[directionIndex]) {
            continue;
        }
        Vector2 worldDirection{};
        switch (static_cast<AttackDirection>(directionIndex)) {
        case AttackDirection::North:
            worldDirection = {0.0F, -1.0F};
            break;
        case AttackDirection::East:
            worldDirection = {1.0F, 0.0F};
            break;
        case AttackDirection::South:
            worldDirection = {0.0F, 1.0F};
            break;
        case AttackDirection::West:
            worldDirection = {-1.0F, 0.0F};
            break;
        }
        const Vector2 outward = rotateMinimapOffset(
            worldDirection.x, worldDirection.y,
            MapYaw);
        const Vector2 inward{-outward.x, -outward.y};
        // Keep warnings in their own inner ring so compass letters remain
        // readable on the rotating rim.
        const float markerRadius =
            mapSize * 0.5F - 28.0F * renderScale;
        const Vector2 marker{
            mapCenter.x + outward.x * markerRadius,
            mapCenter.y + outward.y * markerRadius,
        };
        const Vector2 perpendicular{-inward.y, inward.x};
        DrawTriangle(
            {marker.x + inward.x * 7.0F * renderScale,
             marker.y + inward.y * 7.0F * renderScale},
            {marker.x - inward.x * 3.0F * renderScale +
                 perpendicular.x * 4.5F * renderScale,
             marker.y - inward.y * 3.0F * renderScale +
                 perpendicular.y * 4.5F * renderScale},
            {marker.x - inward.x * 3.0F * renderScale -
                 perpendicular.x * 4.5F * renderScale,
             marker.y - inward.y * 3.0F * renderScale -
                 perpendicular.y * 4.5F * renderScale},
            {255, 103, 64, 245});
    }

    EndScissorMode();
    ui.endMinimapTarget();
    ui.markMinimapTargetRefreshed(minimapNow);
    }

    minimapPerformanceStats.targetRefreshMs =
        performanceMilliseconds(minimapSectionStart);
    minimapSectionStart = PerformanceClock::now();

    if (prepareOnly) {
        if (expanded <= 0.001F) ensureMinimapFrameTarget(ui);
        return;
    }

    if (expanded > .01F) {
        const float opacity = std::clamp((expanded - .35F) / .55F, 0.0F, 1.0F);
        const Rectangle panel{panelX, panelY, panelWidth,
            displayMapSize + panelPadding * 2 + headerHeight};
        menuStyle::panel(panel, 1, {33, 43, 36, 255}, opacity);
        if (expanded > .9F) {
            menuStyle::text("WORLD MAP", {panelX + 30, panelY + 14, panelWidth - 60, 44},
                30, menuStyle::Cream, UiFontRole::Display, 1);
            DrawLineEx({panelX + 24, panelY + 68}, {panelX + panelWidth - 24, panelY + 68},
                1, {78, 93, 76, 255});
        }
    }

    const Vector2 displayMapCenter{
        displayMapBounds.x + displayMapSize * 0.5F,
        displayMapBounds.y + displayMapSize * 0.5F,
    };
    const float frameScale = std::clamp(
        displayMapSize / 150.0F, 1.0F, 3.5F);
    const float mapRadius = displayMapSize * 0.5F;
    const float outerPadding = std::lerp(5.0F * frameScale, 7.0F, expanded);
    const float lightPadding = std::lerp(2.2F * frameScale, 3.5F, expanded);
    const float outerSpikeLength = 1.4F * frameScale;
    const float lightSpikeLength = 1.0F * frameScale;
    constexpr float SpikeHalfAngle = 0.13F;
    const float shadowOffset = std::lerp(4.0F * frameScale, 5.0F, expanded);
    const float compassRotation =
        static_cast<float>(compassYaw);
    const float mapRotationDegrees =
        -compassRotation * RAD2DEG;
    const bool hasAttackWarning = std::ranges::any_of(
        snapshot.upcomingAttackDirections,
        [](bool active) { return active; });
    const float warningWave = hasAttackWarning
        ? 0.5F + 0.5F * std::sin(
              static_cast<float>(GetTime()) * 2.0F * PI / 0.75F)
        : 0.0F;
    const float warningPulse = warningWave * warningWave *
        (3.0F - 2.0F * warningWave);
    const float warningPhaseElapsed = static_cast<float>(
        std::max(0.0,
            snapshot.phaseDuration - snapshot.phaseTimeRemaining));
    const float warningIntro = snapshot.state == RunState::Sunset
        ? 1.0F - std::clamp(
              warningPhaseElapsed / 1.5F, 0.0F, 1.0F)
        : 0.0F;
    const bool activeAttack = snapshot.state == RunState::Wave;
    const Color warningColor = activeAttack
        ? Color{255, 78, 66, 255}
        : Color{255, 145, 67, 255};
    constexpr std::array<Vector2, 4> CompassDirections{{
        {0.0F, -1.0F}, {1.0F, 0.0F},
        {0.0F, 1.0F}, {-1.0F, 0.0F},
    }};
    if (expanded <= 0.001F) {
        ensureMinimapFrameTarget(ui);
        const float frameTargetSize = displayMapSize *
            static_cast<float>(ui.minimapFrameTargetSize()) /
            CachedFrameMapDiameter;
        const Rectangle frameBounds{
            displayMapCenter.x - frameTargetSize * 0.5F,
            displayMapCenter.y - frameTargetSize * 0.5F,
            frameTargetSize, frameTargetSize};
        Rectangle shadowBounds = frameBounds;
        shadowBounds.y += shadowOffset;
        ui.drawMinimapFrameTarget(
            shadowBounds, 0.0F,
            {82, 82, 82, 140});
        ui.drawMinimapFrameTarget(
            frameBounds, 0.0F);
    } else {
        drawRoundedCompassPlate(
            {displayMapCenter.x,
             displayMapCenter.y + shadowOffset},
            mapRadius + outerPadding, outerSpikeLength,
            SpikeHalfAngle, compassRotation,
            {82, 82, 82, 140});
        drawRoundedCompassPlate(
            displayMapCenter,
            mapRadius + outerPadding, outerSpikeLength,
            SpikeHalfAngle, compassRotation,
            {18, 23, 22, 255});
        drawRoundedCompassPlate(
            displayMapCenter,
            mapRadius + lightPadding, lightSpikeLength,
            SpikeHalfAngle, compassRotation,
            {118, 137, 111, 255});
    }
    ui.drawMinimapTarget(displayMapBounds, mapRotationDegrees);
    if (expanded > .55F) {
        // Small compass graduations share the cream/black menu palette.
        for (int tick = 0; tick < 48; ++tick) {
            const float angle = static_cast<float>(tick) * 2 * PI / 48 - compassRotation;
            const Vector2 direction{std::cos(angle), std::sin(angle)};
            const float tickLength = tick % 4 == 0 ? 6.0F : 3.0F;
            const float tickRadius = mapRadius - 5.0F;
            DrawLineEx(Vector2Add(displayMapCenter, Vector2Scale(direction, tickRadius)),
                Vector2Add(displayMapCenter, Vector2Scale(direction, tickRadius - tickLength)),
                tick % 4 == 0 ? 1.8F : 1.1F, {198, 209, 179, 140});
        }
    }
    Vector2 displayPlayer = minimapPoint(displayMapBounds,
        displayMapSize / static_cast<float>(2 * viewRadius), compassYaw,
        centerWorldX, centerWorldZ, snapshot.playerPosition.x, snapshot.playerPosition.z);
    const Vector2 playerOffset = Vector2Subtract(displayPlayer, displayMapCenter);
    const float playerDistance = Vector2Length(playerOffset);
    const float innerRadius = mapRadius - 15;
    if (playerDistance > innerRadius) displayPlayer = Vector2Add(displayMapCenter,
        Vector2Scale(playerOffset, innerRadius / playerDistance));
    const float heading = static_cast<float>(snapshot.playerYaw - compassYaw);
    const float coneLength = std::lerp(40.0F, 68.0F, expanded);
    // Transparent fan, clipped analytically to the circular map.
    const auto conePoint = [&](float angle) {
        const Vector2 direction{std::cos(angle), std::sin(angle)};
        const Vector2 relative = Vector2Subtract(displayPlayer, displayMapCenter);
        const float dot = Vector2DotProduct(relative, direction);
        const float available = -dot + std::sqrt(std::max(0.0F,
            dot*dot + innerRadius*innerRadius - Vector2LengthSqr(relative)));
        return Vector2Add(displayPlayer, Vector2Scale(direction, std::min(coneLength, available)));
    };
    for (int segment = 0; segment < 12; ++segment) {
        const float a = heading - PI*.5F - .46F + static_cast<float>(segment) * .92F / 12;
        DrawTriangle(displayPlayer, conePoint(a + .92F / 12), conePoint(a), {255, 249, 180, 38});
    }
    const Color playerColor = snapshot.playerRespawning
        ? Color{174, 181, 181, 240} : Color{255, 241, 199, 255};
    drawRoundedPlayerMarker(displayPlayer,
        std::lerp(1.0F, 1.55F, expanded) * 1.25F, playerColor, heading);
    if (expanded > 0.001F) {
        const float borderThickness = std::lerp(1.0F * frameScale, 1.5F, expanded);
        DrawRing(
            displayMapCenter,
            displayMapSize * 0.5F - borderThickness,
            displayMapSize * 0.5F, 0.0F, 360.0F, CompassFrameSegments,
            {118, 137, 111, 255});
    }
    for (std::size_t index = 0;
         index < snapshot.upcomingAttackDirections.size(); ++index) {
        if (!snapshot.upcomingAttackDirections[index]) {
            continue;
        }
        const Vector2 offset = rotateMinimapOffset(
            CompassDirections[index].x,
            CompassDirections[index].y,
            compassYaw);
        const float angleDegrees =
            std::atan2(offset.y, offset.x) * RAD2DEG;
        DrawRing(
            displayMapCenter,
            mapRadius - 1.5F * frameScale,
            mapRadius + 4.0F * frameScale,
            angleDegrees - 14.0F,
            angleDegrees + 14.0F,
            16,
            {warningColor.r, warningColor.g, warningColor.b,
             static_cast<unsigned char>(
                 145.0F + warningPulse * 90.0F)});
        const float arrowRadius = mapRadius + 8.0F * frameScale;
        const Vector2 arrowCenter{
            displayMapCenter.x + offset.x * arrowRadius,
            displayMapCenter.y + offset.y * arrowRadius};
        const Vector2 tangent{-offset.y, offset.x};
        DrawTriangle(
            {arrowCenter.x - offset.x * 7.0F * frameScale,
             arrowCenter.y - offset.y * 7.0F * frameScale},
            {arrowCenter.x + offset.x * 2.0F * frameScale +
                 tangent.x * 4.0F * frameScale,
             arrowCenter.y + offset.y * 2.0F * frameScale +
                 tangent.y * 4.0F * frameScale},
            {arrowCenter.x + offset.x * 2.0F * frameScale -
                 tangent.x * 4.0F * frameScale,
             arrowCenter.y + offset.y * 2.0F * frameScale -
                 tangent.y * 4.0F * frameScale},
            warningColor);
    }
    minimapPerformanceStats.frameMs =
        performanceMilliseconds(minimapSectionStart);
    minimapSectionStart = PerformanceClock::now();

    const float compassRenderedSize =
        std::lerp(22.0F, 31.0F, expanded);
    const float compassFontSize =
        compassRenderedSize / 1.68F;
    const float compassRadius =
        mapRadius + outerPadding + std::lerp(8.0F * frameScale, 16.0F, expanded);
    const auto drawCompassLabel =
        [compassFontSize, frameScale, warningPulse, warningIntro,
         warningColor](
            std::string_view label, Vector2 center, bool warning) {
            const float pulseAmount = warning ? warningPulse : 0.0F;
            const float renderedFontSize =
                compassFontSize *
                (1.0F + pulseAmount *
                    (0.15F + warningIntro * 0.06F));
            if (warning) {
                DrawCircleV(
                    center,
                    (11.0F + pulseAmount * 5.0F) * frameScale,
                    {warningColor.r, warningColor.g, warningColor.b,
                     static_cast<unsigned char>(
                         24.0F + pulseAmount * 42.0F)});
            }
            const Vector2 size = measureUiText(label, renderedFontSize);
            const Vector2 position{
                center.x - size.x * 0.5F,
                center.y - size.y * 0.5F,
            };
            const Color labelColor = warning
                ? ColorLerp(
                      WHITE, warningColor,
                      0.42F + warningPulse * 0.58F)
                : Color{255, 238, 194, 255};
            drawUiText(label, position, renderedFontSize, labelColor);
        };
    constexpr std::array<std::string_view, 4> CompassLabels{{
        "N", "E", "S", "W"}};
    for (std::size_t index = 0; index < CompassLabels.size(); ++index) {
        const Vector2 offset = rotateMinimapOffset(
            CompassDirections[index].x,
            CompassDirections[index].y,
            compassYaw);
        drawCompassLabel(
            CompassLabels[index],
            {displayMapCenter.x + offset.x * compassRadius,
             displayMapCenter.y + offset.y * compassRadius},
            snapshot.upcomingAttackDirections[index]);
    }
    drawExpandedObjectiveList(
        snapshot, panelX, panelY,
        displayMapSize + panelPadding * 2.0F + headerHeight,
        expanded);
    drawMapLegend(displayMapBounds, expanded);
    minimapPerformanceStats.labelsMs =
        performanceMilliseconds(minimapSectionStart);
}

} // namespace ian
