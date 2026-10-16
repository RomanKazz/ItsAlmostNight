#include "ui/HudRenderer.hpp"

#include "game/Simulation.hpp"
#include "world/VisualBiomes.hpp"
#include "core/PerformanceStats.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

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
        static_cast<float>(GetScreenHeight()) - panelY - 24.0F);
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
    DrawRectangleRounded(
        panel, 0.06F, 8,
        withAlpha({45, 31, 24, 238}));
    DrawRectangleRoundedLinesEx(
        panel, 0.06F, 8, 2.0F,
        withAlpha({126, 72, 43, 255}));
    drawUiText(
        "OBJECTIVES",
        {panel.x + 14.0F, panel.y + 10.0F},
        16.0F, withAlpha({255, 241, 211, 255}));

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
        DrawRectangleRounded(
            row, 0.12F, 7,
            withAlpha({70, 49, 37, 245}));
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
            DrawRectangleRounded(
                track, 0.8F, 4,
                withAlpha({104, 60, 37, 210}));
            if (fraction > 0.001F) {
                DrawRectangleRounded(
                    {track.x, track.y, track.width * fraction,
                     track.height},
                    0.8F, 4, withAlpha(accent));
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
    const Vector2 offset = rotateMinimapOffset(
        worldX - centerWorldX,
        worldZ - centerWorldZ,
        playerYaw);
    return {
        bounds.x + bounds.width * 0.5F + offset.x * scale,
        bounds.y + bounds.height * 0.5F + offset.y * scale,
    };
}

double minimapTerrainHeight(
    const SimulationSnapshot& snapshot,
    double worldX, double worldZ) {
    if (snapshot.terrainResolution < 2 ||
        snapshot.terrainSamples.empty() ||
        snapshot.terrainWorldSize <= 0.0) {
        return 0.0;
    }
    const int resolution = snapshot.terrainResolution;
    const double halfSize = snapshot.terrainWorldSize * 0.5;
    const double sampleX = std::clamp(
        (worldX + halfSize) / snapshot.terrainWorldSize *
            static_cast<double>(resolution - 1),
        0.0, static_cast<double>(resolution - 1));
    const double sampleZ = std::clamp(
        (worldZ + halfSize) / snapshot.terrainWorldSize *
            static_cast<double>(resolution - 1),
        0.0, static_cast<double>(resolution - 1));
    const int x0 = static_cast<int>(std::floor(sampleX));
    const int z0 = static_cast<int>(std::floor(sampleZ));
    const int x1 = std::min(x0 + 1, resolution - 1);
    const int z1 = std::min(z0 + 1, resolution - 1);
    const auto sample = [&](int x, int z) {
        return static_cast<double>(snapshot.terrainSamples[
            static_cast<std::size_t>(z) *
                static_cast<std::size_t>(resolution) +
            static_cast<std::size_t>(x)]);
    };
    const double amountX = sampleX - static_cast<double>(x0);
    const double amountZ = sampleZ - static_cast<double>(z0);
    const double north = std::lerp(
        sample(x0, z0), sample(x1, z0), amountX);
    const double south = std::lerp(
        sample(x0, z1), sample(x1, z1), amountX);
    return std::lerp(north, south, amountZ);
}

Color minimapHeightColor(double height) {
    constexpr std::array<double, 7> Levels{
        -7.0, -4.0, -1.0, 2.0, 5.0, 9.0, 15.0};
    constexpr std::array<Color, 7> Colors{{
        {31, 61, 47, 255},
        {38, 73, 49, 255},
        {49, 88, 51, 255},
        {65, 103, 56, 255},
        {83, 111, 63, 255},
        {103, 112, 74, 255},
        {121, 118, 92, 255},
    }};
    if (height <= Levels.front()) return Colors.front();
    for (std::size_t index = 1U; index < Levels.size(); ++index) {
        if (height > Levels[index]) continue;
        const float amount = static_cast<float>(std::clamp(
            (height - Levels[index - 1U]) /
                (Levels[index] - Levels[index - 1U]),
            0.0, 1.0));
        return ColorLerp(
            Colors[index - 1U], Colors[index], amount);
    }
    return Colors.back();
}

constexpr int CompassFrameSegments = 64;

float roundedCardinalSpike(float angle, float halfAngle) {
    const float quarterTurn = PI * 0.5F;
    const float distanceToAxis = std::abs(std::remainder(
        angle, quarterTurn));
    const float amount = std::clamp(
        1.0F - distanceToAxis / std::max(halfAngle, 0.001F),
        0.0F, 1.0F);
    const float smoothAmount =
        amount * amount * (3.0F - 2.0F * amount);
    return std::pow(smoothAmount, 1.35F);
}

void drawRoundedCompassPlate(
    Vector2 center, float radius, float spikeLength,
    float spikeHalfAngle, float rotation, Color color) {
    std::array<Vector2, CompassFrameSegments + 2> points{};
    points[0] = center;
    for (int segment = 0; segment <= CompassFrameSegments;
         ++segment) {
        const float angle = -PI * 0.5F -
            static_cast<float>(segment) * 2.0F * PI /
                static_cast<float>(CompassFrameSegments);
        const float contourRadius = radius + spikeLength *
            roundedCardinalSpike(
                angle + rotation, spikeHalfAngle);
        points[static_cast<std::size_t>(segment + 1)] = {
            center.x + std::cos(angle) * contourRadius,
            center.y + std::sin(angle) * contourRadius,
        };
    }
    DrawTriangleFan(
        points.data(), static_cast<int>(points.size()), color);
}

void drawRoundedPlayerMarker(
    Vector2 center, float scale, Color fill) {
    const Color outline{49, 49, 49, 255};
    const Vector2 outerTip{center.x, center.y - 11.0F * scale};
    const Vector2 outerLeft{
        center.x - 6.8F * scale, center.y + 1.5F * scale};
    const Vector2 outerRight{
        center.x + 6.8F * scale, center.y + 1.5F * scale};
    DrawTriangle(outerTip, outerLeft, outerRight, outline);
    DrawCircleV(
        {center.x, center.y + 2.0F * scale},
        7.2F * scale, outline);

    const Vector2 innerTip{center.x, center.y - 8.2F * scale};
    const Vector2 innerLeft{
        center.x - 4.4F * scale, center.y + 1.2F * scale};
    const Vector2 innerRight{
        center.x + 4.4F * scale, center.y + 1.2F * scale};
    DrawTriangle(innerTip, innerLeft, innerRight, fill);
    DrawCircleV(
        {center.x, center.y + 2.0F * scale},
        4.9F * scale, fill);
}

Color minimapBiomeColor(const SimulationSnapshot& snapshot,
    const visual_biomes::Layout& biomes, double x, double z) {
    const Color original = minimapHeightColor(minimapTerrainHeight(snapshot, x, z));
    const auto color = visual_biomes::groundColor(biomes, static_cast<float>(x), static_cast<float>(z),
        {original.r / 255.0F, original.g / 255.0F, original.b / 255.0F});
    return {static_cast<unsigned char>(std::clamp(color[0] * 255, 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(color[1] * 255, 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(color[2] * 255, 0.0F, 255.0F)), 255};
}

void drawMinimapTerrain(
    const SimulationSnapshot& snapshot,
    Rectangle mapBounds, float mapScale,
    double viewRadius, float expanded, double mapYaw) {
    const auto biomes = visual_biomes::generate(snapshot.terrainSeed, snapshot.terrainWorldSize);
    const int cells = expanded > 0.55F ? 64 : 28;
    const double cellWorld = viewRadius * 2.0 /
        static_cast<double>(cells);
    const int verticesPerAxis = cells + 1;
    std::array<Color, 65U * 65U> colors{};
    for (int z = 0; z <= cells; ++z) {
        for (int x = 0; x <= cells; ++x) {
            const double worldX = snapshot.playerPosition.x - viewRadius +
                static_cast<double>(x) * cellWorld;
            const double worldZ = snapshot.playerPosition.z - viewRadius +
                static_cast<double>(z) * cellWorld;
            colors[static_cast<std::size_t>(z) *
                       static_cast<std::size_t>(verticesPerAxis) +
                   static_cast<std::size_t>(x)] =
                minimapBiomeColor(snapshot, biomes, worldX, worldZ);
        }
    }
    const auto emit = [&](int x, int z) {
        const Color color = colors[
            static_cast<std::size_t>(z) *
                static_cast<std::size_t>(verticesPerAxis) +
            static_cast<std::size_t>(x)];
        rlColor4ub(color.r, color.g, color.b, color.a);
        const double worldX = snapshot.playerPosition.x - viewRadius +
            static_cast<double>(x) * cellWorld;
        const double worldZ = snapshot.playerPosition.z - viewRadius +
            static_cast<double>(z) * cellWorld;
        const Vector2 point = minimapPoint(
            mapBounds, mapScale, mapYaw,
            snapshot.playerPosition.x,
            snapshot.playerPosition.z,
            worldX, worldZ);
        rlVertex2f(point.x, point.y);
    };
    rlBegin(RL_TRIANGLES);
    for (int z = 0; z < cells; ++z) {
        for (int x = 0; x < cells; ++x) {
            emit(x, z);
            emit(x, z + 1);
            emit(x + 1, z);
            emit(x + 1, z);
            emit(x, z + 1);
            emit(x + 1, z + 1);
        }
    }
    rlEnd();
    rlColor4ub(255, 255, 255, 255);
}

void drawMinimapPonds(
    const SimulationSnapshot& snapshot,
    Rectangle mapBounds, float mapScale,
    double viewRadius, float expanded, double mapYaw) {
    const auto mapPoint = [&snapshot, mapBounds, mapScale, mapYaw](
                              double x, double z) {
        return minimapPoint(
            mapBounds, mapScale, mapYaw,
            snapshot.playerPosition.x,
            snapshot.playerPosition.z, x, z);
    };
    const auto biomes = visual_biomes::generate(snapshot.terrainSeed, snapshot.terrainWorldSize);
    const int segments = expanded > 0.55F ? 40 : 24;
    const Color water{53, 158, 181, 225};
    const Color shore{127, 215, 207, 205};
    for (const PondDefinition& pond : snapshot.ponds) {
        const double pondExtent =
            std::max(pond.radiusX, pond.radiusZ) + pond.bayRadius;
        if (std::hypot(
                pond.x - snapshot.playerPosition.x,
                pond.z - snapshot.playerPosition.z) >
            viewRadius + pondExtent) {
            continue;
        }
        const Vector2 center = mapPoint(pond.x, pond.z);
        std::array<Vector2, 40> points{};
        const double sine = std::sin(pond.rotation);
        const double cosine = std::cos(pond.rotation);
        for (int segment = 0; segment < segments; ++segment) {
            const double angle =
                static_cast<double>(segment) * 2.0 * PI /
                static_cast<double>(segments);
            const double organicRadius =
                1.0 + std::sin(angle * 3.0 + pond.phase) * 0.095 +
                std::sin(angle * 5.0 - pond.phase * 1.37) * 0.052 +
                std::sin(angle * 7.0 + pond.phase * 0.61) * 0.026;
            const double localX =
                std::cos(angle) * pond.radiusX * organicRadius;
            const double localZ =
                std::sin(angle) * pond.radiusZ * organicRadius;
            points[static_cast<std::size_t>(segment)] = mapPoint(
                pond.x + localX * cosine - localZ * sine,
                pond.z + localX * sine + localZ * cosine);
        }
        for (int segment = 0; segment < segments; ++segment) {
            const int next = (segment + 1) % segments;
            DrawTriangle(
                center,
                points[static_cast<std::size_t>(next)],
                points[static_cast<std::size_t>(segment)], water);
            DrawLineEx(
                points[static_cast<std::size_t>(segment)],
                points[static_cast<std::size_t>(next)],
                std::lerp(0.7F, 1.4F, expanded), shore);
        }
        const Vector2 bayCenter = mapPoint(
            pond.x + std::cos(pond.bayAngle) * pond.radiusX * 0.72,
            pond.z + std::sin(pond.bayAngle) * pond.radiusZ * 0.72);
        DrawCircleV(
            bayCenter,
            static_cast<float>(pond.bayRadius) * mapScale,
            water);
        if (pond.islandRadius > 0.0) {
            const Vector2 island = mapPoint(
                pond.islandX, pond.islandZ);
            DrawCircleV(
                island,
                static_cast<float>(pond.islandRadius) * mapScale,
                minimapBiomeColor(snapshot, biomes, pond.islandX, pond.islandZ));
            DrawCircleLinesV(
                island,
                static_cast<float>(pond.islandRadius) * mapScale,
                {112, 139, 88, 220});
        }
    }
}

constexpr float CachedFrameMapDiameter = 800.0F;

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
    constexpr float outerPadding = 6.0F * frameScale;
    constexpr float lightPadding = 2.8F * frameScale;
    constexpr float outerSpikeLength = 7.0F * frameScale;
    constexpr float lightSpikeLength = 6.2F * frameScale;
    constexpr float spikeHalfAngle = 0.13F;
    drawRoundedCompassPlate(
        center, mapRadius + outerPadding, outerSpikeLength,
        spikeHalfAngle, 0.0F, {49, 49, 49, 255});
    drawRoundedCompassPlate(
        center, mapRadius + lightPadding, lightSpikeLength,
        spikeHalfAngle, 0.0F, {221, 221, 221, 255});
    // The collapsed border is rotationally symmetric and never changes.
    // Cache its 64-segment mesh with the compass frame instead of rebuilding
    // and submitting it every HUD frame.
    constexpr float borderThickness = 3.0F * frameScale;
    DrawRing(
        center,
        mapRadius - borderThickness,
        mapRadius,
        0.0F, 360.0F, 64,
        {221, 221, 221, 255});
    ui.endMinimapFrameTarget();
}

} // namespace

const MinimapRenderPerformanceStats&
minimapRenderPerformanceStats() noexcept {
    return minimapPerformanceStats;
}

void drawMinimapHud(GameUi& ui, const SimulationSnapshot& snapshot,
                    float expansion) {
    minimapPerformanceStats = {};
    auto minimapSectionStart = PerformanceClock::now();
    if (expansion < 0.0F) return;
    const float rawExpansion = std::clamp(expansion, 0.0F, 1.0F);
    const float expanded =
        rawExpansion * rawExpansion * (3.0F - 2.0F * rawExpansion);
    const float screenMinimum = static_cast<float>(
        std::min(GetScreenWidth(), GetScreenHeight()));
    const float collapsedMapSize = std::clamp(
        screenMinimum * 0.16F, 120.0F, 156.0F);
    const float expandedMapSize = std::max(
        collapsedMapSize,
        std::min(
            static_cast<float>(GetScreenWidth()) * 0.68F,
            static_cast<float>(GetScreenHeight()) * 0.70F));
    const float displayMapSize = std::lerp(
        collapsedMapSize, expandedMapSize, expanded);
    const float panelPadding = std::lerp(4.0F, 20.0F, expanded);
    const float headerHeight = std::lerp(0.0F, 42.0F, expanded);
    const float panelWidth = displayMapSize + panelPadding * 2.0F;
    const float collapsedPanelWidth =
        collapsedMapSize + 4.0F * 2.0F;
    const float collapsedCompassMargin = 32.0F;
    const float collapsedPanelX =
        static_cast<float>(GetScreenWidth()) -
        collapsedPanelWidth - 12.0F - collapsedCompassMargin;
    const float expandedPanelX =
        (static_cast<float>(GetScreenWidth()) - panelWidth) * 0.5F;
    const float expandedPanelY =
        static_cast<float>(GetScreenHeight()) * 0.5F -
        displayMapSize * 0.5F - panelPadding - headerHeight;
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

    if (expanded > 0.001F) {
        DrawRectangle(
            0, 0, GetScreenWidth(), GetScreenHeight(),
            {24, 11, 5,
             static_cast<unsigned char>(
                 std::lround(168.0F * expanded))});
    }
    if (expanded > 0.35F) {
        drawUiText(
            "TACTICAL MAP",
            {panelX + panelPadding,
             panelY + 15.0F},
            18.0F, {224, 205, 171, 255});
    }

    const int desiredTargetSize = expanded > 0.55F
        ? 768
        : 512;
    ui.updateMinimapTarget(desiredTargetSize);
    const double minimapNow = GetTime();
    const bool expansionAnimating =
        rawExpansion > 0.001F && rawExpansion < 0.999F;
    const double refreshInterval = expanded > 0.55F
        ? 1.0 / 30.0
        : 1.0 / 20.0;
    const bool refreshTarget = ui.minimapTargetRefreshDue(
        minimapNow, refreshInterval, expansionAnimating);
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

    DrawRectangleRec(mapBounds, {29, 43, 35, 255});
    drawMinimapTerrain(
        snapshot, mapBounds, mapScale,
        viewRadius, expanded, MapYaw);
    drawMinimapPonds(
        snapshot, mapBounds, mapScale, viewRadius, expanded, MapYaw);
    const Vector2 mapCenter{
        mapBounds.x + mapSize * 0.5F,
        mapBounds.y + mapSize * 0.5F,
    };
    DrawCircleLinesV(
        mapCenter, mapSize * 0.425F,
        {116, 135, 91, 95});
    DrawLineEx(
        {mapBounds.x + mapSize * 0.5F, mapBounds.y},
        {mapBounds.x + mapSize * 0.5F,
         mapBounds.y + mapSize},
        renderScale, {214, 205, 169, 24});
    DrawLineEx(
        {mapBounds.x, mapBounds.y + mapSize * 0.5F},
        {mapBounds.x + mapSize,
         mapBounds.y + mapSize * 0.5F},
        renderScale, {214, 205, 169, 24});

    const float symbolScale =
        std::lerp(1.0F, 1.55F, expanded) * renderScale;
    const auto mapPoint =
        [&snapshot, mapBounds, mapScale, mapYaw = MapYaw](
            double worldX, double worldZ) {
            return minimapPoint(
                mapBounds, mapScale, mapYaw,
                snapshot.playerPosition.x,
                snapshot.playerPosition.z,
                worldX, worldZ);
        };
    const float pointMargin = 12.0F * symbolScale;
    const auto pointVisible = [mapBounds, pointMargin](Vector2 point) {
        return point.x >= mapBounds.x - pointMargin &&
            point.x <= mapBounds.x + mapBounds.width + pointMargin &&
            point.y >= mapBounds.y - pointMargin &&
            point.y <= mapBounds.y + mapBounds.height + pointMargin;
    };

    BeginScissorMode(
        static_cast<int>(mapBounds.x),
        static_cast<int>(mapBounds.y),
        static_cast<int>(mapBounds.width),
        static_cast<int>(mapBounds.height));

    for (const ResourceNode& resource : snapshot.resourceNodes) {
        if (!resource.active) {
            continue;
        }
        const Vector2 point = mapPoint(
            resource.position.x, resource.position.z);
        if (!pointVisible(point)) {
            continue;
        }
        if (isDestructibleProp(resource.type)) {
            if (!snapshot.unlimitedResources) {
                continue;
            }
            const float size = 2.8F * symbolScale;
            const Color color = resource.type == ResourceType::Barrel
                ? Color{203, 120, 61, 245}
                : resource.type == ResourceType::ItemCrate
                    ? Color{242, 190, 60, 250}
                    : Color{174, 112, 59, 245};
            DrawRectangleRec(
                {point.x - size, point.y - size,
                 size * 2.0F, size * 2.0F},
                {31, 24, 19, 220});
            if (resource.type == ResourceType::Barrel) {
                DrawRectangleRec(
                    {point.x - size * 0.62F, point.y - size * 0.82F,
                     size * 1.24F, size * 1.64F}, color);
                DrawLineEx(
                    {point.x - size * 0.68F, point.y},
                    {point.x + size * 0.68F, point.y},
                    std::max(1.0F, symbolScale), {57, 37, 25, 255});
            } else {
                DrawRectangleRec(
                    {point.x - size * 0.76F, point.y - size * 0.76F,
                     size * 1.52F, size * 1.52F}, color);
                DrawLineEx(
                    {point.x - size * 0.58F, point.y - size * 0.58F},
                    {point.x + size * 0.58F, point.y + size * 0.58F},
                    std::max(1.0F, symbolScale), {67, 43, 27, 255});
                DrawLineEx(
                    {point.x + size * 0.58F, point.y - size * 0.58F},
                    {point.x - size * 0.58F, point.y + size * 0.58F},
                    std::max(1.0F, symbolScale), {67, 43, 27, 255});
                if (resource.type == ResourceType::ItemCrate) {
                    DrawCircleV(point, 1.1F * symbolScale,
                                {255, 242, 165, 255});
                }
            }
            continue;
        }
        if (!isHarvestableResource(resource.type)) {
            continue;
        }
        const float radius =
            (resource.type == ResourceType::Wood ? 1.35F : 1.2F) *
            symbolScale;
        DrawRectangleRec(
            {point.x - radius, point.y - radius,
             radius * 2.0F, radius * 2.0F},
            resource.type == ResourceType::Wood
                ? Color{91, 143, 75, 125}
                : resource.type == ResourceType::Crystal
                    ? Color{183, 92, 226, 180}
                    : Color{143, 149, 145, 135});
    }

    if (snapshot.unlimitedResources) {
        for (const WorldLandmarkInstance& landmark :
             snapshot.worldLandmarks) {
            const Vector2 point = mapPoint(
                landmark.position.x, landmark.position.z);
            if (!pointVisible(point)) {
                continue;
            }
            const float size = 5.0F * symbolScale;
            const Color fill = landmark.activated
                ? Color{96, 220, 137, 255}
                : landmark.type == WorldLandmarkType::Mine
                    ? Color{151, 164, 184, 255}
                    : Color{220, 157, 73, 255};
            DrawCircleV(point, size + 2.0F, {28, 21, 17, 235});
            if (landmark.type == WorldLandmarkType::Mine) {
                DrawPoly(point, 4, size, 45.0F, fill);
                DrawLineEx(
                    {point.x - size * 0.48F,
                     point.y + size * 0.34F},
                    {point.x + size * 0.48F,
                     point.y - size * 0.34F},
                    std::max(1.2F, 1.4F * symbolScale),
                    {245, 235, 211, 255});
            } else {
                DrawCircleV(point, size, fill);
                DrawRectangleRec(
                    {point.x - size * 0.19F,
                     point.y - size * 0.70F,
                     size * 0.38F, size * 1.40F},
                    {245, 235, 211, 255});
                DrawLineEx(
                    {point.x - size * 0.62F, point.y},
                    {point.x + size * 0.62F, point.y},
                    std::max(1.2F, 1.4F * symbolScale),
                    {245, 235, 211, 255});
            }
        }
    }

    const bool mapRevealsChests =
        snapshot.unlimitedResources ||
        snapshot.lootStacks[lootUpgradeIndex(LootUpgradeEffect::Map)] > 0;
    if (mapRevealsChests || std::ranges::any_of(
            snapshot.lootChests,
            [](const LootChestInstance& chest) {
                return chest.revealed;
            })) {
        for (const LootChestInstance& chest : snapshot.lootChests) {
            if (chest.looseLoot ||
                (!mapRevealsChests && !chest.revealed)) continue;
            const Vector2 point = mapPoint(
                chest.position.x, chest.position.z);
            if (!pointVisible(point)) {
                continue;
            }
            const float size = 3.2F * symbolScale;
            const Color color = chest.type == LootChestType::Stone
                ? Color{170, 183, 195, 255}
                : Color{225, 161, 72, 255};
            DrawRectangleRec(
                {point.x - size - 1.0F, point.y - size - 1.0F,
                 size * 2.0F + 2.0F, size * 2.0F + 2.0F},
                {31, 24, 19, 230});
            DrawRectangleRec(
                {point.x - size, point.y - size,
                 size * 2.0F, size * 2.0F}, color);
            DrawLineEx(
                {point.x - size, point.y - size * 0.35F},
                {point.x + size, point.y - size * 0.35F},
                std::max(1.0F, symbolScale), {59, 43, 29, 255});
            if (chest.loot.available && !chest.loot.collected) {
                const Color lootColor =
                    chest.loot.rarity == LootRarity::Rare
                        ? Color{255, 170, 170, 255}
                        : chest.loot.rarity == LootRarity::Uncommon
                            ? Color{255, 228, 148, 255}
                            : Color{185, 225, 255, 255};
                DrawCircleV(point, 1.5F * symbolScale, lootColor);
            }
        }
    }

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
            const Vector2 delta{
                point.x - mapCenter.x,
                point.y - mapCenter.y};
            const float distance = std::hypot(delta.x, delta.y);
            if (distance <= 0.001F) {
                DrawPoly(
                    mapCenter, 4, 6.0F * symbolScale,
                    45.0F, {255, 210, 83, 255});
                break;
            }
            const Vector2 direction{
                delta.x / distance, delta.y / distance};
            const Vector2 perpendicular{
                -direction.y, direction.x};
            const float maximumDistance =
                mapSize * 0.5F - 18.0F * renderScale;
            const float markerDistance = std::min(
                distance, maximumDistance);
            const Vector2 marker{
                mapCenter.x + direction.x * markerDistance,
                mapCenter.y + direction.y * markerDistance};
            const float pulse = 1.0F + 0.08F *
                std::sin(static_cast<float>(GetTime()) * 5.5F);
            const float arrowLength =
                9.0F * symbolScale * pulse;
            const float arrowWidth =
                6.0F * symbolScale * pulse;
            DrawCircleV(
                marker, 8.5F * symbolScale,
                {33, 27, 19, 225});
            DrawTriangle(
                {marker.x + direction.x * arrowLength,
                 marker.y + direction.y * arrowLength},
                {marker.x - direction.x * arrowLength * 0.55F +
                     perpendicular.x * arrowWidth,
                 marker.y - direction.y * arrowLength * 0.55F +
                     perpendicular.y * arrowWidth},
                {marker.x - direction.x * arrowLength * 0.55F -
                     perpendicular.x * arrowWidth,
                 marker.y - direction.y * arrowLength * 0.55F -
                     perpendicular.y * arrowWidth},
                {255, 207, 72, 255});
            DrawCircleV(
                marker, 2.0F * symbolScale,
                {255, 247, 195, 255});
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
                {238, 182, 89, 245});
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
        } else if (type == EnemyType::Flying) {
            result.color = {220, 105, 232, 250};
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
            if (!enemy.active || enemy.state == EnemyState::Dead) {
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
            if (!enemy.active || enemy.state == EnemyState::Dead) {
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

    const Vector2 displayMapCenter{
        displayMapBounds.x + displayMapSize * 0.5F,
        displayMapBounds.y + displayMapSize * 0.5F,
    };
    const float frameScale = std::clamp(
        displayMapSize / 150.0F, 1.0F, 3.5F);
    const float mapRadius = displayMapSize * 0.5F;
    const float outerPadding = 6.0F * frameScale;
    const float lightPadding = 2.8F * frameScale;
    const float outerSpikeLength = 7.0F * frameScale;
    const float lightSpikeLength = 6.2F * frameScale;
    constexpr float SpikeHalfAngle = 0.13F;
    const float shadowOffset = 4.0F * frameScale;
    const float compassRotation =
        static_cast<float>(snapshot.playerYaw);
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
            shadowBounds, mapRotationDegrees,
            {82, 82, 82, 140});
        ui.drawMinimapFrameTarget(
            frameBounds, mapRotationDegrees);
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
            {49, 49, 49, 255});
        drawRoundedCompassPlate(
            displayMapCenter,
            mapRadius + lightPadding, lightSpikeLength,
            SpikeHalfAngle, compassRotation,
            {221, 221, 221, 255});
    }
    ui.drawMinimapTarget(displayMapBounds, mapRotationDegrees);
    const Color playerColor = snapshot.playerRespawning
        ? Color{174, 181, 181, 240}
        : Color{250, 250, 244, 255};
    drawRoundedPlayerMarker(
        displayMapCenter,
        std::lerp(1.0F, 1.55F, expanded) * 1.25F,
        playerColor);
    if (expanded > 0.001F) {
        const float borderThickness = 3.0F * frameScale;
        DrawRing(
            displayMapCenter,
            displayMapSize * 0.5F - borderThickness,
            displayMapSize * 0.5F, 0.0F, 360.0F, 64,
            {221, 221, 221, 255});
    }
    for (std::size_t index = 0;
         index < snapshot.upcomingAttackDirections.size(); ++index) {
        if (!snapshot.upcomingAttackDirections[index]) {
            continue;
        }
        const Vector2 offset = rotateMinimapOffset(
            CompassDirections[index].x,
            CompassDirections[index].y,
            snapshot.playerYaw);
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
        std::lerp(20.0F, 28.0F, expanded);
    const float compassFontSize =
        compassRenderedSize / 1.68F;
    const float compassRadius =
        mapRadius + outerPadding + outerSpikeLength +
        7.0F * frameScale;
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
                : WHITE;
            drawUiText(label, position, renderedFontSize, labelColor);
        };
    constexpr std::array<std::string_view, 4> CompassLabels{{
        "N", "E", "S", "W"}};
    for (std::size_t index = 0; index < CompassLabels.size(); ++index) {
        const Vector2 offset = rotateMinimapOffset(
            CompassDirections[index].x,
            CompassDirections[index].y,
            snapshot.playerYaw);
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
    minimapPerformanceStats.labelsMs =
        performanceMilliseconds(minimapSectionStart);
}

} // namespace ian
