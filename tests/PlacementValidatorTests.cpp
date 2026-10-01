#include "TestHarness.hpp"
#include "buildings/PlacementValidator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

void runPlacementValidatorTests() {
    ian::WorldConfig config =
        ian::WorldConfig::defaults();
    config.terrainResolution = 65;
    config.terrainWorldSize = 48.0;
    config.coreFlatRadius = 2.0;
    config.buildPreviewDistance = 12.0;
    ian::TerrainHeightfield terrain{config};
    ian::BuildGrid grid{config};
    ian::PlacementValidator validator{
        terrain, grid};

    const ian::Vec3 hit{
        4.2,
        terrain.getHeight(4.2, 4.2),
        4.2,
    };
    const ian::Vec3 player{0.0, 1.7, 0.0};
    const auto frame =
        validator.validateGroundPlatformFrame(
            hit, player);
    require(
        frame.valid() &&
            frame.anchor.x == 4 &&
            frame.anchor.z == 4 &&
            frame.storey == 0,
        "ground PlatformFrame uses the one-cell placement grid");

    for (const auto& [point, expected] :
         std::array<std::pair<ian::Vec3, ian::GridCoord>, 2>{{
             {{5.2, 0.0, 3.2}, {5, 0, 3}},
             {{-3.2, 0.0, -1.2}, {-3, 0, -1}}}}) {
        const auto shifted = validator.validateGroundPlatformFrame(point, player);
        require(shifted.valid() && shifted.anchor.x == expected.x &&
                    shifted.anchor.z == expected.z,
                "manual foundations accept odd and negative one-cell anchors");
    }

    for (const auto [aim, expected] : std::array<std::pair<double, int>, 4>{{
             {0.49, 0}, {0.51, 1}, {-0.49, 0}, {-0.51, -1}}}) {
        const auto snapped = validator.validateGroundPlatformFrame({aim, 0.0, aim}, player);
        require(snapped.anchor.x == expected && snapped.anchor.z == expected,
                "ground foundation validator uses midpoint snapping in both directions");
    }

    ian::WorldConfig noisyFlatConfig = config;
    noisyFlatConfig.terrainAmplitude = 0.0;
    noisyFlatConfig.coreFlatRadius = 0.0;
    noisyFlatConfig.terrainBuildPlateauRadius = 2.0;
    noisyFlatConfig.terrainSurfaceNoiseAmplitude = 0.08;
    noisyFlatConfig.pondMinimumCount = 0;
    noisyFlatConfig.pondMaximumCount = 0;
    noisyFlatConfig.pondMaximumAreaFraction = 0.0;
    ian::TerrainHeightfield noisyFlatTerrain{
        noisyFlatConfig};
    ian::BuildGrid noisyFlatGrid{noisyFlatConfig};
    ian::PlacementValidator noisyFlatValidator{
        noisyFlatTerrain, noisyFlatGrid};
    double lowestVisibleHeight =
        std::numeric_limits<double>::infinity();
    double highestVisibleHeight =
        -std::numeric_limits<double>::infinity();
    for (int z = -16; z <= 16; z += 2) {
        for (int x = -16; x <= 16; x += 2) {
            const double sampleX =
                static_cast<double>(x) + 0.7;
            const double sampleZ =
                static_cast<double>(z) + 1.3;
            const double visibleHeight =
                noisyFlatTerrain.getHeight(sampleX, sampleZ);
            lowestVisibleHeight = std::min(
                lowestVisibleHeight, visibleHeight);
            highestVisibleHeight = std::max(
                highestVisibleHeight, visibleHeight);
            requireNear(
                noisyFlatTerrain.getConstructionHeight(
                    sampleX, sampleZ),
                0.0, 1e-9,
                "construction terrain excludes decorative surface noise");
            requireNear(
                noisyFlatValidator
                    .stableGroundPlatformHeight(
                        {x, 0, z}, 0.05),
                0.20, 1e-9,
                "surface noise does not split one flat terrace into different construction heights");
        }
    }
    require(
        highestVisibleHeight - lowestVisibleHeight > 0.01,
        "decorative surface noise remains visible on flat terrain");
    require(
        frame.supports[0].top.y ==
                frame.floorHeight &&
            frame.supports[3].top.y ==
                frame.floorHeight &&
            frame.supports[3].top.x -
                    frame.supports[0].top.x ==
                config.cellSize *
                    ian::PlatformFrameWidthCells,
        "PlatformFrame is horizontal and exactly 2x2 cells");
    for (const auto& support : frame.supports) {
        require(
            support.length >=
                    config.minimumGroundClearance &&
                support.length <=
                    config.maxWoodSupportLength,
            "all four integrated supports reach terrain");
        requireNear(
            support.bottom.y,
            terrain.getHeight(
                support.bottom.x,
                support.bottom.z),
            1e-9,
            "foundation support stretches exactly to terrain");
    }

    double highestTerrain = -1e9;
    for (int zIndex = 0; zIndex < 5; ++zIndex) {
        for (int xIndex = 0; xIndex < 5; ++xIndex) {
            const double x =
                frame.anchor.x * config.cellSize +
                xIndex *
                    (ian::PlatformFrameWidthCells *
                     config.cellSize / 4.0);
            const double z =
                frame.anchor.z * config.cellSize +
                zIndex *
                    (ian::PlatformFrameWidthCells *
                     config.cellSize / 4.0);
            highestTerrain = std::max(
                highestTerrain,
                terrain.getHeight(x, z));
        }
    }
    const double clearanceBoundary =
        highestTerrain + config.minimumGroundClearance;
    require(
        validator
            .validateGroundPlatformFrameAt(
                frame.anchor, clearanceBoundary,
                player)
            .valid(),
        "terrain may touch the exact minimum floor clearance boundary");
    require(
        validator
                .validateGroundPlatformFrameAt(
                    frame.anchor,
                    clearanceBoundary - 2e-6,
                    player)
                .error ==
            ian::ModularPlacementError::TerrainIntersection,
        "terrain just above floor clearance rejects placement");

    double lowestCorner = 1e9;
    for (const auto& support : frame.supports) {
        lowestCorner = std::min(
            lowestCorner, support.bottom.y);
    }
    ian::WorldConfig exactSupportConfig = config;
    exactSupportConfig.maxWoodSupportLength =
        clearanceBoundary - lowestCorner;
    ian::TerrainHeightfield exactSupportTerrain{
        exactSupportConfig};
    ian::BuildGrid exactSupportGrid{
        exactSupportConfig};
    ian::PlacementValidator exactSupportValidator{
        exactSupportTerrain, exactSupportGrid};
    require(
        exactSupportValidator
            .validateGroundPlatformFrameAt(
                frame.anchor, clearanceBoundary,
                player)
            .valid(),
        "support at the exact maximum length is allowed");
    ian::WorldConfig overSupportConfig = exactSupportConfig;
    overSupportConfig.maxWoodSupportLength -= 2e-6;
    ian::TerrainHeightfield overSupportTerrain{
        overSupportConfig};
    ian::BuildGrid overSupportGrid{overSupportConfig};
    ian::PlacementValidator overSupportValidator{
        overSupportTerrain, overSupportGrid};
    require(
        overSupportValidator
                .validateGroundPlatformFrameAt(
                    frame.anchor, clearanceBoundary,
                    player)
                .error ==
            ian::ModularPlacementError::SupportTooLong,
        "support just beyond maximum length rejects placement");

    require(
        grid.occupy(
            {9000U, 1U}, frame.anchor,
            ian::Footprint::TwoByTwo, 1,
            ian::OccupancyLayer::Floor),
        "PlatformFrame fixture occupies four cells");
    require(
        validator
                .validateGroundPlatformFrame(
                    hit, player)
                .error ==
            ian::ModularPlacementError::Occupied,
        "validator reports occupied PlatformFrame cells");

    const ian::Vec3 overlapHit{
        hit.x + 4.0,
        terrain.getHeight(
            hit.x + 4.0, hit.z),
        hit.z,
    };
    const auto overlapPreview =
        validator.validateGroundPlatformFrame(
            overlapHit, player);
    require(
        overlapPreview.valid(),
        "player overlap fixture first validates");
    require(
        validator
                .validateGroundPlatformFrame(
                    overlapHit,
                    {
                        overlapPreview.anchor.x *
                                config.cellSize +
                            config.cellSize,
                        overlapPreview.floorHeight + 1.7,
                        overlapPreview.anchor.z *
                                config.cellSize +
                            config.cellSize,
                    })
                .valid(),
        "player position does not block PlatformFrame placement");

    require(
        validator
                .validateGroundPlatformFrame(
                    {24.2, 0.0, 24.2},
                    {20.0, 1.7, 20.0})
                .error ==
            ian::ModularPlacementError::OutOfBounds,
        "validator rejects frame outside terrain");

    ian::WorldConfig shortSupportConfig = config;
    shortSupportConfig.maxWoodSupportLength = 0.05;
    ian::TerrainHeightfield shortTerrain{
        shortSupportConfig};
    ian::BuildGrid shortGrid{shortSupportConfig};
    ian::PlacementValidator shortValidator{
        shortTerrain, shortGrid};
    require(
        shortValidator
                .validateGroundPlatformFrame(
                    hit, player)
                .error ==
            ian::ModularPlacementError::SupportTooLong,
        "validator reports excessive ground support length");

    ian::WorldConfig steepConfig = config;
    steepConfig.maximumFoundationHeightDifference = 0.01;
    ian::TerrainHeightfield steepTerrain{steepConfig};
    ian::BuildGrid steepGrid{steepConfig};
    ian::PlacementValidator steepValidator{
        steepTerrain, steepGrid};
    bool rejectedSlope = false;
    for (int z = -18; z <= 18 && !rejectedSlope; z += 2) {
        for (int x = -18; x <= 18; x += 2) {
            const ian::Vec3 sample{
                static_cast<double>(x),
                steepTerrain.getHeight(x, z),
                static_cast<double>(z),
            };
            if (steepValidator
                    .validateGroundPlatformFrame(sample, sample)
                    .error ==
                ian::ModularPlacementError::TerrainIntersection) {
                rejectedSlope = true;
                break;
            }
        }
    }
    require(
        rejectedSlope,
        "foundation height tolerance rejects excessive slope");
}
