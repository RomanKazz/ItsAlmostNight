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
#include <vector>

namespace ian {
namespace {

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
    Rectangle mapBounds, float expanded) {
    const int cells = expanded > 0.55F ? 64 : 40;
    const float cellPixels =
        mapBounds.width / static_cast<float>(cells);
    const double worldLimit = std::max(snapshot.worldLimit, 1.0);
    const double cellWorld = worldLimit * 2.0 /
        static_cast<double>(cells);
    const int verticesPerAxis = cells + 1;
    std::vector<Color> colors(
        static_cast<std::size_t>(verticesPerAxis) *
        static_cast<std::size_t>(verticesPerAxis));
    for (int z = 0; z <= cells; ++z) {
        for (int x = 0; x <= cells; ++x) {
            const double worldX = -worldLimit +
                static_cast<double>(x) * cellWorld;
            const double worldZ = -worldLimit +
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
        rlVertex2f(
            mapBounds.x + static_cast<float>(x) * cellPixels,
            mapBounds.y + static_cast<float>(z) * cellPixels);
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
    Rectangle mapBounds, float mapScale, float expanded) {
    const auto mapPoint = [mapBounds, mapScale](double x, double z) {
        return Vector2{
            mapBounds.x + mapBounds.width * 0.5F +
                static_cast<float>(x) * mapScale,
            mapBounds.y + mapBounds.height * 0.5F +
                static_cast<float>(z) * mapScale,
        };
    };
    constexpr int Segments = 40;
    const Color water{53, 158, 181, 225};
    const Color shore{127, 215, 207, 205};
    for (const PondDefinition& pond : snapshot.ponds) {
        const Vector2 center = mapPoint(pond.x, pond.z);
        std::array<Vector2, Segments> points{};
        const double sine = std::sin(pond.rotation);
        const double cosine = std::cos(pond.rotation);
        for (int segment = 0; segment < Segments; ++segment) {
            const double angle =
                static_cast<double>(segment) * 2.0 * PI /
                static_cast<double>(Segments);
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
        for (int segment = 0; segment < Segments; ++segment) {
            const int next = (segment + 1) % Segments;
            DrawTriangle(
                center,
                points[static_cast<std::size_t>(segment)],
                points[static_cast<std::size_t>(next)], water);
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
                    float expansion) {
    if (expansion < 0.0F) return;
    const float rawExpansion = std::clamp(expansion, 0.0F, 1.0F);
    const float expanded =
        rawExpansion * rawExpansion * (3.0F - 2.0F * rawExpansion);
    const float screenMinimum = static_cast<float>(
        std::min(GetScreenWidth(), GetScreenHeight()));
    const float collapsedMapSize = std::clamp(
        screenMinimum * 0.18F, 132.0F, 176.0F);
    const float expandedMapSize = std::max(
        collapsedMapSize,
        std::min(
            static_cast<float>(GetScreenWidth()) * 0.68F,
            static_cast<float>(GetScreenHeight()) * 0.70F));
    const float mapSize = std::lerp(
        collapsedMapSize, expandedMapSize, expanded);
    const float panelPadding = std::lerp(6.0F, 20.0F, expanded);
    const float headerHeight = std::lerp(0.0F, 42.0F, expanded);
    const float panelWidth = mapSize + panelPadding * 2.0F;
    const float panelHeight =
        mapSize + headerHeight + panelPadding * 2.0F;
    const float collapsedPanelWidth =
        collapsedMapSize + 6.0F * 2.0F;
    const float collapsedPanelX =
        static_cast<float>(GetScreenWidth()) -
        collapsedPanelWidth - 12.0F;
    const float expandedPanelX =
        (static_cast<float>(GetScreenWidth()) - panelWidth) * 0.5F;
    const float expandedPanelY =
        static_cast<float>(GetScreenHeight()) * 0.5F -
        mapSize * 0.5F - panelPadding - headerHeight;
    const float panelX = std::lerp(
        collapsedPanelX, expandedPanelX, expanded);
    const float panelY = std::lerp(12.0F, expandedPanelY, expanded);
    const Rectangle mapBounds{
        panelX + panelPadding,
        panelY + panelPadding + headerHeight,
        mapSize, mapSize,
    };
    const float worldLimit = std::max(
        static_cast<float>(snapshot.worldLimit), 1.0F);
    const float mapScale = mapSize * 0.5F / worldLimit;

    if (expanded > 0.001F) {
        DrawRectangle(
            0, 0, GetScreenWidth(), GetScreenHeight(),
            {24, 11, 5,
             static_cast<unsigned char>(
                 std::lround(168.0F * expanded))});
    }
    ui.drawPanel(
        {panelX, panelY, panelWidth, panelHeight},
        static_cast<unsigned char>(178.0F + expanded * 52.0F));
    if (expanded > 0.35F) {
        drawUiText(
            "TACTICAL MAP",
            {panelX + panelPadding,
             panelY + 15.0F},
            18.0F, {224, 205, 171, 255});
    }

    DrawRectangleRec(mapBounds, {29, 43, 35, 238});
    drawMinimapTerrain(snapshot, mapBounds, expanded);
    drawMinimapPonds(snapshot, mapBounds, mapScale, expanded);
    DrawRectangleLinesEx(
        mapBounds, std::lerp(2.0F, 5.0F, expanded),
        {61, 76, 58, 245});
    const Rectangle playableBounds{
        mapBounds.x + mapSize * 0.075F,
        mapBounds.y + mapSize * 0.075F,
        mapSize * 0.85F,
        mapSize * 0.85F,
    };
    DrawRectangleLinesEx(
        playableBounds, 1.0F, {116, 135, 91, 95});
    DrawLineEx(
        {mapBounds.x + mapSize * 0.5F, mapBounds.y},
        {mapBounds.x + mapSize * 0.5F,
         mapBounds.y + mapSize},
        1.0F, {214, 205, 169, 24});
    DrawLineEx(
        {mapBounds.x, mapBounds.y + mapSize * 0.5F},
        {mapBounds.x + mapSize,
         mapBounds.y + mapSize * 0.5F},
        1.0F, {214, 205, 169, 24});

    const float symbolScale = std::lerp(1.0F, 1.55F, expanded);
    const auto mapPoint =
        [mapBounds, mapScale](double worldX, double worldZ) {
            return Vector2{
                mapBounds.x + mapBounds.width * 0.5F +
                    static_cast<float>(worldX) * mapScale,
                mapBounds.y + mapBounds.height * 0.5F +
                    static_cast<float>(worldZ) * mapScale,
            };
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
        const float radius =
            (resource.type == ResourceType::Wood ? 1.35F : 1.2F) *
            symbolScale;
        DrawRectangleRec(
            {point.x - radius, point.y - radius,
             radius * 2.0F, radius * 2.0F},
            resource.type == ResourceType::Wood
                ? Color{91, 143, 75, 125}
                : Color{143, 149, 145, 135});
    }

    const bool mapRevealsChests =
        snapshot.unlimitedResources ||
        snapshot.lootStacks[lootUpgradeIndex(LootUpgradeEffect::Map)] > 0;
    if (mapRevealsChests) {
        for (const LootChestInstance& chest : snapshot.lootChests) {
            const Vector2 point = mapPoint(
                chest.position.x, chest.position.z);
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
        DrawRectangleRec(
            {point.x - 1.7F * symbolScale,
             point.y - 1.7F * symbolScale,
             3.4F * symbolScale, 3.4F * symbolScale},
            {164, 144, 111, 185});
    }
    for (const WallInstance& wall : snapshot.modularWalls) {
        const Vector2 point = modularPoint(wall.anchor, 1.0, 1.0);
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
        DrawCircleV(
            point, 2.0F * symbolScale,
            {205, 174, 119, 190});
    }

    for (const BuildingInstance& building : snapshot.buildings) {
        const Vec3 position = buildingWorldPosition(building);
        const Vector2 point = mapPoint(position.x, position.z);
        switch (building.type) {
        case BuildingType::Core:
            DrawPoly(point, 4, 5.5F * symbolScale, 45.0F,
                     {255, 210, 83, 255});
            DrawCircleV(
                point, 1.7F * symbolScale,
                {255, 245, 188, 255});
            break;
        case BuildingType::Turret:
        case BuildingType::Cannon:
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
        case BuildingType::GoldMine:
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
        }
    }

    for (const EnemyInstance& enemy : snapshot.enemies) {
        if (!enemy.active || enemy.state == EnemyState::Dead) {
            continue;
        }
        const Vector2 point = mapPoint(enemy.position.x, enemy.position.z);
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
    const Vector2 direction{
        static_cast<float>(std::sin(snapshot.playerYaw)),
        static_cast<float>(-std::cos(snapshot.playerYaw)),
    };
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
            const Rectangle targetBounds{
                mapBounds.x + edgeMargin,
                mapBounds.y + edgeMargin,
                mapBounds.width - edgeMargin * 2.0F,
                mapBounds.height - edgeMargin * 2.0F};
            const Vector2 marker{
                std::clamp(chestPoint.x,
                           targetBounds.x,
                           targetBounds.x + targetBounds.width),
                std::clamp(chestPoint.y,
                           targetBounds.y,
                           targetBounds.y + targetBounds.height)};
            DrawLineEx(
                player, marker,
                std::max(1.4F, symbolScale * 1.6F),
                {255, 212, 91, 180});
            const Vector2 perpendicular{
                -directionToChest.y, directionToChest.x};
            DrawTriangle(
                {marker.x + directionToChest.x * 7.0F,
                 marker.y + directionToChest.y * 7.0F},
                {marker.x - directionToChest.x * 3.0F +
                     perpendicular.x * 4.0F,
                 marker.y - directionToChest.y * 3.0F +
                     perpendicular.y * 4.0F},
                {marker.x - directionToChest.x * 3.0F -
                     perpendicular.x * 4.0F,
                 marker.y - directionToChest.y * 3.0F -
                     perpendicular.y * 4.0F},
                {255, 220, 105, 255});
        }
    }

    if (snapshot.upcomingAttackDirection) {
        Vector2 marker{
            mapBounds.x + mapBounds.width * 0.5F,
            mapBounds.y + mapBounds.height * 0.5F,
        };
        Vector2 inward{};
        switch (*snapshot.upcomingAttackDirection) {
        case AttackDirection::North:
            marker.y = mapBounds.y + 5.0F;
            inward.y = 1.0F;
            break;
        case AttackDirection::East:
            marker.x = mapBounds.x + mapBounds.width - 5.0F;
            inward.x = -1.0F;
            break;
        case AttackDirection::South:
            marker.y = mapBounds.y + mapBounds.height - 5.0F;
            inward.y = -1.0F;
            break;
        case AttackDirection::West:
            marker.x = mapBounds.x + 5.0F;
            inward.x = 1.0F;
            break;
        }
        const Vector2 perpendicular{-inward.y, inward.x};
        DrawTriangle(
            {marker.x + inward.x * 7.0F,
             marker.y + inward.y * 7.0F},
            {marker.x - inward.x * 3.0F + perpendicular.x * 4.5F,
             marker.y - inward.y * 3.0F + perpendicular.y * 4.5F},
            {marker.x - inward.x * 3.0F - perpendicular.x * 4.5F,
             marker.y - inward.y * 3.0F - perpendicular.y * 4.5F},
            {255, 103, 64, 245});
    }

    EndScissorMode();
    DrawRectangleLinesEx(mapBounds, 2.0F, {196, 172, 126, 230});
    const float compassFontSize =
        std::lerp(15.0F, 24.0F, expanded);
    const float compassInset = compassFontSize * 0.72F;
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
    drawCompassLabel(
        "N", {mapBounds.x + mapBounds.width * 0.5F,
              mapBounds.y + compassInset});
    drawCompassLabel(
        "E", {mapBounds.x + mapBounds.width - compassInset,
              mapBounds.y + mapBounds.height * 0.5F});
    drawCompassLabel(
        "S", {mapBounds.x + mapBounds.width * 0.5F,
              mapBounds.y + mapBounds.height - compassInset});
    drawCompassLabel(
        "W", {mapBounds.x + compassInset,
              mapBounds.y + mapBounds.height * 0.5F});
}

} // namespace ian
