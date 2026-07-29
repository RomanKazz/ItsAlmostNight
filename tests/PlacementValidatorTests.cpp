#include "TestHarness.hpp"
#include "buildings/PlacementValidator.hpp"

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
        "ground PlatformFrame snaps to the 2x2 frame grid");
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
    }

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
}
