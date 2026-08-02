#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldConfig.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

void runTerrainHeightfieldTests() {
    ian::WorldConfig config =
        ian::WorldConfig::defaults();
    config.terrainResolution = 33;
    config.terrainWorldSize = 32.0;
    config.coreFlatRadius = 3.0;
    config.terrainSeed = 42U;

    ian::TerrainHeightfield first{config};
    ian::TerrainHeightfield second{config};
    requireNear(
        first.getHeight(
            std::numeric_limits<double>::quiet_NaN(), 0.0),
        0.0, 1e-12,
        "terrain returns safe height for non-finite coordinates");
    require(
        first.samples().size() ==
                second.samples().size() &&
            std::equal(
                first.samples().begin(),
                first.samples().end(),
                second.samples().begin()),
        "same terrain seed is deterministic");

    second.generate(43U);
    require(
        !std::equal(
            first.samples().begin(),
            first.samples().end(),
            second.samples().begin()),
        "different terrain seed changes heightfield");

    requireNear(
        first.getHeight(0.0, 0.0), 0.0, 1e-9,
        "core center is flat");
    requireNear(
        first.getHeight(2.5, 0.0), 0.0, 1e-9,
        "core flat radius remains level");

    const int resolution = first.resolution();
    const double spacing = first.spacing();
    const double halfSize =
        config.terrainWorldSize * 0.5;
    constexpr int CellX = 24;
    constexpr int CellZ = 8;
    const auto samples = first.samples();
    const auto at =
        [samples, resolution](int x, int z) {
            return static_cast<double>(
                samples[
                    static_cast<std::size_t>(z) *
                        static_cast<std::size_t>(
                            resolution) +
                    static_cast<std::size_t>(x)]);
        };
    const double worldX =
        -halfSize +
        (static_cast<double>(CellX) + 0.5) * spacing;
    const double worldZ =
        -halfSize +
        (static_cast<double>(CellZ) + 0.5) * spacing;
    const double expectedMidpoint =
        (at(CellX, CellZ) +
         at(CellX + 1, CellZ) +
         at(CellX, CellZ + 1) +
         at(CellX + 1, CellZ + 1)) *
        0.25;
    requireNear(
        first.getHeight(worldX, worldZ),
        expectedMidpoint, 1e-6,
        "height query bilinearly interpolates samples");

    const ian::Vec3 normal =
        first.getNormal(worldX, worldZ);
    requireNear(
        std::sqrt(
            normal.x * normal.x +
            normal.y * normal.y +
            normal.z * normal.z),
        1.0, 1e-9,
        "terrain normal is normalized");
    require(
        normal.y > 0.0,
        "terrain normal points upward");
    require(
        first.isInside(-halfSize, halfSize) &&
            !first.isInside(halfSize + 0.01, 0.0),
        "terrain reports world bounds");
    const auto verticalHit = first.raycast(
        {0.0, 10.0, 0.0},
        {0.0, -1.0, 0.0}, 20.0);
    require(
        verticalHit.has_value(),
        "terrain raycast finds surface");
    requireNear(
        verticalHit->y,
        first.getHeight(
            verticalHit->x, verticalHit->z),
        1e-6,
        "terrain raycast returns exact heightfield surface");
    require(
        !first.raycast(
             {0.0, 10.0, 0.0},
             {0.0, 1.0, 0.0}, 20.0)
             .has_value(),
        "terrain raycast misses when aimed away");
    const auto [minimumHeight, maximumHeight] =
        first.minMaxHeight();
    require(
        minimumHeight <= 0.0 &&
            maximumHeight >= 0.0 &&
            maximumHeight > minimumHeight,
        "terrain publishes useful height range");

    const auto parsed = ian::parseWorldConfig(R"json({
        "cellSize": 1.0,
        "verticalGridStep": 0.5,
        "maxStoreys": 3,
        "maxWoodSupportLength": 4.0,
        "terrainResolution": 65,
        "terrainWorldSize": 64.0,
        "terrainAmplitude": 2.5,
        "terrainFrequency": 0.03,
        "terrainSeed": 99,
        "coreFlatRadius": 8.0,
        "buildPreviewDistance": 9.0,
        "minimumGroundClearance": 0.12
    })json");
    require(
        parsed.valid() &&
            parsed.config.terrainSeed == 99U &&
            parsed.config.maxStoreys == 3,
        "world configuration parses");

    ian::WorldConfig movementConfig = config;
    movementConfig.coreFlatRadius = 0.0;
    ian::MapDefinition movementMap =
        ian::MapDefinition::defaults();
    ian::Simulation simulation{
        ian::GameBalance::defaults(),
        movementMap, movementConfig};
    simulation.startRun();
    const auto initial = simulation.snapshot();
    requireNear(
        initial.playerPosition.y,
        simulation.terrain().getHeight(
            initial.playerPosition.x,
            initial.playerPosition.z) +
            ian::GameBalance::defaults()
                .gameplay.eyeHeight,
        1e-9,
        "player starts on generated terrain");
    ian::PlayerCommand move{};
    move.moveForward = 1.0;
    simulation.tick(0.2, move);
    const auto moved = simulation.snapshot();
    requireNear(
        moved.playerPosition.y,
        simulation.terrain().getHeight(
            moved.playerPosition.x,
            moved.playerPosition.z) +
            ian::GameBalance::defaults()
                .gameplay.eyeHeight,
        1e-9,
        "grounded player follows terrain height");
}
