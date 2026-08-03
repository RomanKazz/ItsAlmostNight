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
        "terrainFeatureSize": 64.0,
        "terrainTerraceHeight": 2.0,
        "terrainSlopeWidth": 12.0,
        "terrainSurfaceNoiseAmplitude": 0.06,
        "terrainBuildPlateauRadius": 11.0,
        "coreFlatRadius": 8.0,
        "buildPreviewDistance": 9.0,
        "minimumGroundClearance": 0.12,
        "maximumFoundationHeightDifference": 1.1
    })json");
    require(
        parsed.valid() &&
            parsed.config.terrainSeed == 99U &&
            parsed.config.terrainFeatureSize == 64.0 &&
            parsed.config.terrainTerraceHeight == 2.0 &&
            parsed.config.terrainSlopeWidth == 12.0 &&
            parsed.config.maximumFoundationHeightDifference == 1.1 &&
            parsed.config.maxStoreys == 3,
        "world configuration parses");

    ian::WorldConfig boundaryConfig =
        ian::WorldConfig::defaults();
    boundaryConfig.terrainResolution = 129;
    boundaryConfig.terrainWorldSize = 128.0;
    boundaryConfig.coreFlatRadius = 8.0;
    boundaryConfig.terrainBoundaryRiseWidth = 24.0;
    boundaryConfig.terrainBoundaryRiseHeight = 20.0;
    ian::TerrainHeightfield boundaryTerrain{boundaryConfig};
    require(
        boundaryTerrain.getHeight(63.5, 0.0) >
            boundaryTerrain.getHeight(35.0, 0.0) + 12.0,
        "terrain rises strongly near world boundary");
    double lowestBoundaryPeak =
        std::numeric_limits<double>::infinity();
    double highestBoundaryPeak =
        -std::numeric_limits<double>::infinity();
    for (double z = -52.0; z <= 52.0; z += 8.0) {
        const double height =
            boundaryTerrain.getHeight(63.5, z);
        lowestBoundaryPeak =
            std::min(lowestBoundaryPeak, height);
        highestBoundaryPeak =
            std::max(highestBoundaryPeak, height);
    }
    require(
        highestBoundaryPeak - lowestBoundaryPeak > 5.0,
        "boundary terrain forms varied mountain peaks");
    require(
        ian::WorldConfig::defaults().terrainWorldSize >= 384.0 &&
            ian::WorldConfig::defaults().terrainResolution >= 513,
        "default world uses large chunk-ready terrain");

    ian::TerrainHeightfield defaultTerrain{
        ian::WorldConfig::defaults()};
    double plateauMinimum =
        std::numeric_limits<double>::infinity();
    double plateauMaximum =
        -std::numeric_limits<double>::infinity();
    constexpr double EastPlateauX = 50.4;
    constexpr double EastPlateauZ = -40.32;
    for (double z = -7.0; z <= 7.0; z += 2.0) {
        for (double x = -7.0; x <= 7.0; x += 2.0) {
            const double height = defaultTerrain.getHeight(
                EastPlateauX + x,
                EastPlateauZ + z);
            plateauMinimum = std::min(plateauMinimum, height);
            plateauMaximum = std::max(plateauMaximum, height);
        }
    }
    require(
        plateauMaximum - plateauMinimum < 0.02,
        "terrain provides wide deterministic build plateaus");
    double maximumConnectionStep = 0.0;
    double previousConnectionHeight =
        defaultTerrain.getHeight(0.0, 0.0);
    constexpr int ConnectionSamples = 80;
    for (int index = 1; index <= ConnectionSamples; ++index) {
        const double progress =
            static_cast<double>(index) /
            static_cast<double>(ConnectionSamples);
        const double height = defaultTerrain.getHeight(
            EastPlateauX * progress,
            EastPlateauZ * progress);
        maximumConnectionStep = std::max(
            maximumConnectionStep,
            std::abs(height - previousConnectionHeight));
        previousConnectionHeight = height;
    }
    require(
        maximumConnectionStep < 0.35,
        "build plateaus remain connected by walkable slopes");

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
