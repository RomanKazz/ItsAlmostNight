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
        "maximumFoundationHeightDifference": 1.1,
        "pondMinimumCount": 4,
        "pondMaximumCount": 6,
        "pondMaximumAreaFraction": 0.13,
        "pondMaximumDepth": 1.8,
        "pondWaveSpeed": 0.3,
        "pondShorelineWidth": 2.8,
        "pondSeed": 12345,
        "pondShallowColor": [0.1, 0.7, 0.8],
        "pondDeepColor": [0.02, 0.2, 0.5]
    })json");
    require(
        parsed.valid() &&
            parsed.config.terrainSeed == 99U &&
            parsed.config.terrainFeatureSize == 64.0 &&
            parsed.config.terrainTerraceHeight == 2.0 &&
            parsed.config.terrainSlopeWidth == 12.0 &&
            parsed.config.maximumFoundationHeightDifference == 1.1 &&
            parsed.config.maxStoreys == 3 &&
            parsed.config.pondMinimumCount == 4 &&
            parsed.config.pondMaximumCount == 6 &&
            parsed.config.pondMaximumAreaFraction == 0.13 &&
            parsed.config.pondMaximumDepth == 1.8 &&
            parsed.config.pondWaveSpeed == 0.3 &&
            parsed.config.pondShorelineWidth == 2.8 &&
            parsed.config.pondSeed == 12345U &&
            parsed.config.pondShallowColor[1] == 0.7 &&
            parsed.config.pondDeepColor[2] == 0.5,
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
    require(
        defaultTerrain.ponds().size() >= 3U &&
            defaultTerrain.ponds().size() <= 7U,
        "default terrain generates three to seven ponds");
    ian::TerrainHeightfield deterministicPonds{
        ian::WorldConfig::defaults()};
    require(
        deterministicPonds.ponds().size() ==
            defaultTerrain.ponds().size() &&
            std::equal(
                defaultTerrain.ponds().begin(),
                defaultTerrain.ponds().end(),
                deterministicPonds.ponds().begin(),
                [](const ian::PondDefinition& left,
                   const ian::PondDefinition& right) {
                    return left.x == right.x &&
                           left.z == right.z &&
                           left.radiusX == right.radiusX &&
                           left.radiusZ == right.radiusZ;
                }),
        "pond layout is deterministic for same seed");
    deterministicPonds.generate(deterministicPonds.seed() + 1U);
    require(
        deterministicPonds.ponds().size() !=
                defaultTerrain.ponds().size() ||
            !std::equal(
                defaultTerrain.ponds().begin(),
                defaultTerrain.ponds().end(),
                deterministicPonds.ponds().begin(),
                [](const ian::PondDefinition& left,
                   const ian::PondDefinition& right) {
                    return left.x == right.x && left.z == right.z;
                }),
        "terrain seed changes pond layout");
    const auto& defaultConfig = defaultTerrain.config();
    constexpr double PondSampleStep = 2.0;
    const double terrainHalfSize =
        defaultConfig.terrainWorldSize * 0.5;
    std::size_t waterSamples = 0U;
    std::size_t totalSamples = 0U;
    for (double z = -terrainHalfSize;
         z <= terrainHalfSize; z += PondSampleStep) {
        for (double x = -terrainHalfSize;
             x <= terrainHalfSize; x += PondSampleStep) {
            ++totalSamples;
            if (defaultTerrain.waterDepth(x, z) > 0.02) {
                ++waterSamples;
            }
        }
    }
    const double pondAreaFraction =
        static_cast<double>(waterSamples) /
        static_cast<double>(totalSamples);
    require(
        pondAreaFraction > 0.01 && pondAreaFraction <= 0.15,
        "ponds occupy bounded map area");
    for (double coordinate = -32.0;
         coordinate <= 32.0; coordinate += 1.0) {
        require(
            !defaultTerrain.isDeepWater(coordinate, 0.0) &&
                !defaultTerrain.isDeepWater(0.0, coordinate),
            "ponds preserve cardinal attack routes");
    }
    require(
        defaultTerrain.waterDepth(0.0, 0.0) <= 0.02,
        "ponds avoid core start zone");
    bool foundDeepWater = false;
    for (const ian::PondDefinition& pond : defaultTerrain.ponds()) {
        for (int z = -4; z <= 4 && !foundDeepWater; ++z) {
            for (int x = -4; x <= 4; ++x) {
                if (defaultTerrain.isDeepWater(
                        pond.x + x * pond.radiusX / 10.0,
                        pond.z + z * pond.radiusZ / 10.0)) {
                    foundDeepWater = true;
                    break;
                }
            }
        }
    }
    require(foundDeepWater, "pond basins contain deep water");
    ian::WorldConfig pondSeedConfig = defaultConfig;
    pondSeedConfig.terrainResolution = 129;
    for (std::uint32_t seed = 1U; seed <= 12U; ++seed) {
        pondSeedConfig.terrainSeed = seed;
        ian::TerrainHeightfield seededPonds{pondSeedConfig};
        require(
            seededPonds.ponds().size() >=
                    static_cast<std::size_t>(
                        pondSeedConfig.pondMinimumCount) &&
                seededPonds.ponds().size() <=
                    static_cast<std::size_t>(
                        pondSeedConfig.pondMaximumCount),
            "every terrain seed keeps configured pond count range");
    }
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
