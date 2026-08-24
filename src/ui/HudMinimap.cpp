#include "ui/HudRenderer.hpp"

#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace ian {
namespace {

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

void drawMinimapTerrain(
    const SimulationSnapshot& snapshot,
    Rectangle mapBounds, float mapScale,
    double viewRadius, float expanded) {
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
                minimapHeightColor(
                    minimapTerrainHeight(snapshot, worldX, worldZ));
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
            mapBounds, mapScale, snapshot.playerYaw,
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
    double viewRadius, float expanded) {
    const auto mapPoint = [&snapshot, mapBounds, mapScale](
                              double x, double z) {
        return minimapPoint(
            mapBounds, mapScale, snapshot.playerYaw,
            snapshot.playerPosition.x,
            snapshot.playerPosition.z, x, z);
    };
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
                minimapHeightColor(minimapTerrainHeight(
                    snapshot, pond.islandX, pond.islandZ)));
            DrawCircleLinesV(
                island,
                static_cast<float>(pond.islandRadius) * mapScale,
                {112, 139, 88, 220});
        }
    }
}

} // namespace

void drawMinimapHud(GameUi& ui, const SimulationSnapshot& snapshot,
                    float expansion, float topInset) {
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
    const float collapsedPanelX =
        static_cast<float>(GetScreenWidth()) -
        collapsedPanelWidth - 12.0F;
    const float expandedPanelX =
        (static_cast<float>(GetScreenWidth()) - panelWidth) * 0.5F;
    const float expandedPanelY =
        static_cast<float>(GetScreenHeight()) * 0.5F -
        displayMapSize * 0.5F - panelPadding - headerHeight;
    const float panelX = std::lerp(
        collapsedPanelX, expandedPanelX, expanded);
    const float panelY = std::lerp(
        12.0F + std::max(topInset, 0.0F),
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
        : expanded > 0.02F ? 512 : 256;
    ui.updateMinimapTarget(desiredTargetSize);
    if (!ui.beginMinimapTarget()) {
        return;
    }
    const float mapSize = static_cast<float>(ui.minimapTargetSize());
    const Rectangle mapBounds{0.0F, 0.0F, mapSize, mapSize};
    const float mapScale = mapSize * 0.5F /
        static_cast<float>(std::max(viewRadius, 1.0));
    const float renderScale = mapSize /
        std::max(displayMapSize, 1.0F);

    DrawRectangleRec(mapBounds, {29, 43, 35, 255});
    drawMinimapTerrain(
        snapshot, mapBounds, mapScale,
        viewRadius, expanded);
    drawMinimapPonds(
        snapshot, mapBounds, mapScale, viewRadius, expanded);
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
        [&snapshot, mapBounds, mapScale](
            double worldX, double worldZ) {
            return minimapPoint(
                mapBounds, mapScale, snapshot.playerYaw,
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

    for (const EnemyInstance& enemy : snapshot.enemies) {
        if (!enemy.active || enemy.state == EnemyState::Dead) {
            continue;
        }
        const Vector2 point = mapPoint(enemy.position.x, enemy.position.z);
        if (!pointVisible(point)) {
            continue;
        }
        float radius = 2.25F;
        Color color{239, 75, 66, 245};
        if (enemy.type == EnemyType::Fast) {
            radius = 1.8F;
        } else if (enemy.type == EnemyType::Heavy) {
            radius = 2.8F;
        } else if (enemy.type == EnemyType::Boss) {
            radius = 4.6F;
            color = {255, 123, 55, 255};
        } else if (enemy.type == EnemyType::Flying) {
            color = {220, 105, 232, 250};
        } else if (enemy.type == EnemyType::Splitter) {
            radius = 3.0F;
            color = {87, 194, 92, 250};
        } else if (enemy.type == EnemyType::Splitling) {
            radius = 1.55F;
            color = {116, 224, 108, 245};
        }
        DrawCircleV(
            point, (radius + 1.0F) * symbolScale,
            {51, 16, 17, 210});
        DrawCircleV(point, radius * symbolScale, color);
    }

    const Vector2 player = mapPoint(
        snapshot.playerPosition.x, snapshot.playerPosition.z);
    // The map rotates with the camera, so the player's heading remains up.
    const Vector2 direction{0.0F, -1.0F};
    const Vector2 side{-direction.y, direction.x};
    const Color playerColor = snapshot.playerRespawning
        ? Color{174, 181, 181, 240}
        : Color{250, 250, 244, 255};
    const Vector2 outerTip{
        player.x + direction.x * 12.5F * symbolScale,
        player.y + direction.y * 12.5F * symbolScale,
    };
    const Vector2 outerBase{
        player.x + direction.x * 5.8F * symbolScale,
        player.y + direction.y * 5.8F * symbolScale,
    };
    const Vector2 outerLeft{
        outerBase.x - side.x * 5.2F * symbolScale,
        outerBase.y - side.y * 5.2F * symbolScale,
    };
    const Vector2 outerRight{
        outerBase.x + side.x * 5.2F * symbolScale,
        outerBase.y + side.y * 5.2F * symbolScale,
    };
    DrawTriangle(
        outerTip, outerLeft, outerRight,
        {30, 35, 38, 255});
    DrawCircleV(
        player, 7.2F * symbolScale,
        {30, 35, 38, 255});

    const Vector2 innerTip{
        player.x + direction.x * 10.5F * symbolScale,
        player.y + direction.y * 10.5F * symbolScale,
    };
    const Vector2 innerBase{
        player.x + direction.x * 5.8F * symbolScale,
        player.y + direction.y * 5.8F * symbolScale,
    };
    const Vector2 innerLeft{
        innerBase.x - side.x * 3.5F * symbolScale,
        innerBase.y - side.y * 3.5F * symbolScale,
    };
    const Vector2 innerRight{
        innerBase.x + side.x * 3.5F * symbolScale,
        innerBase.y + side.y * 3.5F * symbolScale,
    };
    DrawTriangle(
        innerTip, innerLeft, innerRight,
        playerColor);
    DrawCircleV(
        player, 5.2F * symbolScale,
        playerColor);

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
            snapshot.playerYaw);
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

    const Vector2 displayMapCenter{
        displayMapBounds.x + displayMapSize * 0.5F,
        displayMapBounds.y + displayMapSize * 0.5F,
    };
    DrawCircleV(
        displayMapCenter, displayMapSize * 0.5F + 3.0F,
        {43, 34, 27, 225});
    ui.drawMinimapTarget(displayMapBounds);
    const float borderThickness = std::lerp(1.25F, 5.0F, expanded);
    DrawRing(
        displayMapCenter,
        displayMapSize * 0.5F - borderThickness,
        displayMapSize * 0.5F, 0.0F, 360.0F, 96,
        {196, 172, 126, 230});
    const float compassFontSize =
        std::lerp(15.0F, 24.0F, expanded);
    const float compassRadius =
        displayMapSize * 0.5F - compassFontSize * 0.78F;
    const auto drawCompassLabel =
        [compassFontSize](std::string_view label, Vector2 center) {
            const Vector2 size = measureUiText(label, compassFontSize);
            const Vector2 position{
                center.x - size.x * 0.5F,
                center.y - size.y * 0.5F,
            };
            drawUiText(
                label, {position.x + 1.5F, position.y + 1.5F},
                compassFontSize, {18, 22, 20, 235});
            drawUiText(
                label, position, compassFontSize,
                {248, 235, 201, 255});
        };
    constexpr std::array<std::string_view, 4> CompassLabels{{
        "N", "E", "S", "W"}};
    constexpr std::array<Vector2, 4> CompassDirections{{
        {0.0F, -1.0F}, {1.0F, 0.0F},
        {0.0F, 1.0F}, {-1.0F, 0.0F},
    }};
    for (std::size_t index = 0; index < CompassLabels.size(); ++index) {
        const Vector2 offset = rotateMinimapOffset(
            CompassDirections[index].x,
            CompassDirections[index].y,
            snapshot.playerYaw);
        drawCompassLabel(
            CompassLabels[index],
            {displayMapCenter.x + offset.x * compassRadius,
             displayMapCenter.y + offset.y * compassRadius});
    }
}

} // namespace ian
