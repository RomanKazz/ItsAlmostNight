#include "TestHarness.hpp"
#include "buildings/FoundationSystem.hpp"
#include "buildings/StructuralSupportGraph.hpp"

#include <algorithm>
#include <array>
#include <cmath>

void runFoundationSystemTests() {
    {
        ian::StructuralSupportGraph graph;
        const ian::EntityId ground{1U, 1U};
        const ian::EntityId upper{2U, 1U};
        const ian::EntityId roof{3U, 1U};
        require(
            graph.add(ground, true) &&
                graph.add(
                    upper, false,
                    std::span<const ian::EntityId>{
                        &ground, 1U}) &&
                graph.add(
                    roof, false,
                    std::span<const ian::EntityId>{
                        &upper, 1U}),
            "structural graph creates grounded frame chain");
        require(
            graph.dependentCount(ground) == 2U &&
                graph.dependentCount(upper) == 1U &&
                graph.dependentCount(roof) == 0U,
            "structural graph reports recursive collapse impact");
        require(
            graph.remove(ground) &&
                graph.unsupportedCount() == 2,
            "removing ground frame invalidates upper chain");
        require(
            graph.update(0.6, true, 1.0).empty() &&
                graph.update(0.5, true, 1.0).size() ==
                    2,
            "unsupported frames collapse after delay");
    }

    ian::WorldConfig config =
        ian::WorldConfig::defaults();
    config.terrainResolution = 65;
    config.terrainWorldSize = 48.0;
    config.coreFlatRadius = 20.0;
    config.buildPreviewDistance = 20.0;
    ian::TerrainHeightfield terrain{config};
    const ian::Vec3 player{-5.0, 1.7, 0.0};

    ian::FoundationSystem frames{terrain, config};
    const auto first = frames.placePlatformFrame(
        frames.previewPlatformFrame(
            {0.2, 0.0, 0.2}, player));
    const auto neighbour =
        frames.placePlatformFrame(
            frames.previewPlatformFrame(
                {2.2, 0.0, 0.2}, player));
    require(
        first && neighbour &&
            first->anchor ==
                ian::GridCoord{0, first->anchor.yLevel, 0} &&
            neighbour->anchor.x == 2 &&
            frames.platformFrames().size() == 2,
        "only 2x2 PlatformFrames place on frame grid");
    require(
        first->health == ian::PlatformFrameMaxHealth &&
            first->maxHealth ==
                ian::PlatformFrameMaxHealth,
        "PlatformFrame starts with combat health");
    const auto woundedFrame =
        frames.damage(first->id, 25.0);
    require(
        woundedFrame && !woundedFrame->destroyed &&
            woundedFrame->platformFrame &&
            woundedFrame->platformFrame->health ==
                ian::PlatformFrameMaxHealth - 25.0,
        "PlatformFrame damage persists health");
    const auto repairedFrame =
        frames.repair(first->id, 100, 100);
    require(
        repairedFrame.valid() &&
            repairedFrame.platformFrame &&
            repairedFrame.platformFrame->health ==
                ian::PlatformFrameMaxHealth &&
            repairedFrame.cost.wood > 0 &&
            repairedFrame.cost.stone > 0,
        "PlatformFrame repair restores health for resources");
    const auto surfaceHit =
        frames.raycastPlatformSurface(
            {1.0, first->floorHeight + 3.0, 1.0},
            {0.0, -1.0, 0.0}, 5.0);
    require(
        surfaceHit &&
            std::abs(
                surfaceHit->x - 1.0) < 1e-9 &&
            std::abs(
                surfaceHit->y -
                first->floorHeight) < 1e-9 &&
            std::abs(
                surfaceHit->z - 1.0) < 1e-9,
        "platform surface raycast returns exact floor hit");
    require(
        !frames.raycastPlatformSurface(
            {5.0, first->floorHeight + 3.0, 5.0},
            {0.0, -1.0, 0.0}, 5.0),
        "platform surface raycast rejects floor plane outside frame");
    require(
        frames.supportSystem()
                .activeSupportCount() == 6 &&
            std::count_if(
                frames.supportSystem().supports().begin(),
                frames.supportSystem().supports().end(),
                [](const ian::SharedSupport& support) {
                    return support.active &&
                           support.referenceCount == 2U;
                }) == 2,
        "adjacent PlatformFrames share corner supports");
    require(
        frames.remove(neighbour->id) &&
            frames.grid().occupiedCellCount() == 4 &&
            frames.supportSystem()
                    .activeSupportCount() == 4,
        "frame removal releases occupancy and shared supports");

    ian::FoundationSystem destructibleFrames{
        terrain, config};
    const auto destructible =
        destructibleFrames.placePlatformFrame(
            destructibleFrames.previewPlatformFrame(
                {0.2, 0.0, 0.2}, player));
    const auto destroyedFrame =
        destructible
            ? destructibleFrames.damage(
                  destructible->id,
                  ian::PlatformFrameMaxHealth)
            : std::nullopt;
    require(
        destroyedFrame && destroyedFrame->destroyed &&
            destructibleFrames.platformFrames().empty() &&
            destructibleFrames.grid()
                    .occupiedCellCount() == 0,
        "lethal PlatformFrame damage removes structure and occupancy");

    ian::FoundationSystem tower{terrain, config};
    const auto ground = tower.placePlatformFrame(
        tower.previewPlatformFrame(
            {0.2, 0.0, 0.2}, player));
    const auto upperPreview =
        tower.previewPlatformFrame(
            {0.2, 0.0, 0.2}, player);
    const auto upper =
        tower.placePlatformFrame(upperPreview);
    const auto roof =
        tower.placePlatformFrame(
            tower.previewPlatformFrame(
                {0.2, 0.0, 0.2}, player));
    const double storeyHeight =
        ian::modularStoreyHeight(config);
    require(
        ground && upper && roof &&
            upper->storey == 1 &&
            roof->storey == 2 &&
            std::abs(
                upper->floorHeight -
                ground->floorHeight -
                storeyHeight) < 1e-9 &&
            std::abs(
                roof->floorHeight -
                upper->floorHeight -
                storeyHeight) < 1e-9,
        "stacked frames snap exactly four cells apart");
    require(
        std::all_of(
            upper->supports.begin(),
            upper->supports.end(),
            [ground, storeyHeight](
                const ian::FoundationSupport& support) {
                return std::abs(
                           support.bottom.y -
                           ground->floorHeight) <
                           1e-9 &&
                       std::abs(
                           support.length -
                           storeyHeight) < 1e-9;
            }),
        "upper frame includes four full-height supports");
    require(
        tower
                .previewPlatformFrame(
                    {0.2, 0.0, 0.2}, player)
                .error ==
            ian::ModularPlacementError::MaximumStorey,
        "frame stack respects maximum storeys");

    ian::WorldConfig scaledConfig = config;
    scaledConfig.cellSize = 1.25;
    scaledConfig.verticalGridStep = 0.25;
    ian::TerrainHeightfield scaledTerrain{
        scaledConfig};
    ian::FoundationSystem scaledFrames{
        scaledTerrain, scaledConfig};
    const auto scaledGround =
        scaledFrames.placePlatformFrame(
            scaledFrames.previewPlatformFrame(
                {0.2, 0.0, 0.2}, player));
    const auto scaledUpper =
        scaledFrames.placePlatformFrame(
            scaledFrames.previewPlatformFrame(
                {0.2, 0.0, 0.2}, player));
    require(
        scaledGround && scaledUpper &&
            std::abs(
                scaledUpper->floorHeight -
                scaledGround->floorHeight -
                4.0 * scaledConfig.cellSize) <
                1e-9,
        "storey height remains exactly four cells at another grid scale");

    const auto aimedUpper = tower.raycast(
        {1.0, upper->floorHeight, 4.0},
        {0.0, 0.0, -1.0}, 6.0);
    require(
        aimedUpper == upper->id,
        "raycast selects the precise frame level");
    const auto aimedUpperSupport = tower.raycast(
        {
            upper->supports[0].top.x,
            (upper->supports[0].top.y +
             upper->supports[0].bottom.y) *
                0.5,
            upper->supports[0].top.z - 2.0,
        },
        {0.0, 0.0, 1.0}, 4.0);
    require(
        aimedUpperSupport == upper->id,
        "raycast treats integrated supports as part of PlatformFrame");
    const auto upperBuildingSurface =
        tower.buildingSurface(0, 0, 2);
    require(
        upperBuildingSurface &&
            upperBuildingSurface->storey ==
                roof->storey &&
            std::abs(
                upperBuildingSurface->height -
                roof->floorHeight) < 1e-9,
        "building footprint resolves the highest complete platform surface");
    require(
        !tower.buildingSurface(1, 0, 2),
        "partially supported building footprint is rejected");
    require(
        tower.buildingFootprintIntersectsPlatform(
            1, 0, 2),
        "partial platform overlap remains detectable for placement rejection");

    tower.setStructuralCollapseDelay(1.0);
    require(
        tower.remove(ground->id) &&
            tower.platformFrames().size() == 2 &&
            std::all_of(
                tower.platformFrames().begin(),
                tower.platformFrames().end(),
                [](const ian::PlatformFrameInstance& frame) {
                    return frame.supportState ==
                           ian::StructuralSupportState::
                               Unsupported;
                }),
        "removing lower frame marks all dependent frames unsupported");
    require(
        !tower.updateStructuralSupport(0.9) &&
            tower.platformFrames().size() == 2,
        "unsupported frames remain during warning delay");
    const bool towerCollapsed =
        tower.updateStructuralSupport(0.2);
    const auto collapsedTowerParts =
        tower.takeCollapsedBuildings();
    require(
        towerCollapsed &&
            tower.platformFrames().empty() &&
            tower.grid().occupiedCellCount() == 0 &&
            tower.supportSystem()
                    .activeSupportCount() == 0,
        "collapsed frames release render data occupancy and supports");
    require(
        collapsedTowerParts.size() == 2U &&
            std::ranges::all_of(
                collapsedTowerParts,
                [](const auto& part) {
                    return part.destroyed &&
                           part.platformFrame.has_value();
                }),
        "structural collapse exposes removed parts for effects");

    ian::FoundationSystem envelope{terrain, config};
    const auto base = envelope.placePlatformFrame(
        envelope.previewPlatformFrame(
            {0.2, 0.0, 0.2}, player));
    const auto wall = envelope.placeWall(
        envelope.previewWall(
            {0.2, 0.0, 1.2}, player,
            ian::Rotation::Deg90));
    const auto rampPreview =
        envelope.previewRamp(
            {0.2, 0.0, 0.2}, player,
            ian::Rotation::Deg180);
    const auto ramp = envelope.placeRamp(
        rampPreview);
    const auto rampSnapA =
        envelope.previewRamp(
            {0.2, 0.0, 0.2}, player,
            ian::Rotation::Deg180);
    const auto rampSnapB =
        envelope.previewRamp(
            {1.2, 0.0, 1.2}, player,
            ian::Rotation::Deg180);
    const auto rampSnapC =
        envelope.previewRamp(
            {2.2, 0.0, 0.2}, player,
            ian::Rotation::Deg180);
    require(
        rampSnapA.anchor == rampSnapB.anchor &&
            rampSnapA.anchor != rampSnapC.anchor,
        "ramp anchor snaps in two-cell platform steps");
    require(
        rampPreview.anchor.x == 0 &&
            rampPreview.anchor.z ==
                -ian::ModularRampRunCells &&
            envelope
                    .previewRamp(
                        {0.2, 0.0, 0.2}, player,
                        ian::Rotation::Deg0)
                    .anchor.z == 1 &&
            envelope
                    .previewRamp(
                        {0.2, 0.0, 0.2}, player,
                        ian::Rotation::Deg90)
                    .anchor.x ==
                -ian::ModularRampRunCells &&
            envelope
                    .previewRamp(
                        {0.2, 0.0, 0.2}, player,
                        ian::Rotation::Deg270)
                    .anchor.x == 1,
        "ramp footprint begins outside the supporting platform edge");
    require(
        base && wall && ramp &&
            std::abs(
                wall->topHeight -
                wall->bottomHeight -
                storeyHeight) < 1e-9 &&
            std::abs(
                ramp->topHeight -
                ramp->bottomHeight -
                storeyHeight) < 1e-9,
        "walls and ramps use the same four-cell storey height");
    require(
        envelope
                .previewRamp(
                    {2.2, 0.0, 0.2}, player,
                    ian::Rotation::Deg180)
                .error ==
            ian::ModularPlacementError::NoSupport,
        "two-cell-wide ramp requires support along its full lower edge");
    require(
        envelope
                .previewRamp(
                    {4.2, 0.0, 0.2}, player,
                    ian::Rotation::Deg0)
                .error ==
            ian::ModularPlacementError::NoSupport,
        "ramp requires an existing PlatformFrame");
    const auto woundedWall =
        envelope.damage(wall->id, 30.0);
    const auto woundedRamp =
        envelope.damage(ramp->id, 40.0);
    require(
        woundedWall && woundedWall->wall &&
            woundedWall->wall->health ==
                ian::ModularWallMaxHealth - 30.0 &&
            woundedRamp && woundedRamp->ramp &&
            woundedRamp->ramp->health ==
                ian::ModularRampMaxHealth - 40.0,
        "walls and ramps share modular combat damage");
    const auto repairedWall =
        envelope.repair(wall->id, 100, 100);
    require(
        repairedWall.valid() && repairedWall.wall &&
            repairedWall.wall->health ==
                ian::ModularWallMaxHealth,
        "wall repair restores modular health");
    const auto repairedRamp =
        envelope.repair(ramp->id, 100, 100);
    require(
        repairedRamp.valid() && repairedRamp.ramp &&
            repairedRamp.ramp->health ==
                ian::ModularRampMaxHealth,
        "ramp repair returns the updated ramp instance");

    const std::size_t elementCount =
        envelope.platformFrames().size() +
        envelope.walls().size() +
        envelope.ramps().size();
    require(
        envelope.clear() == elementCount &&
            envelope.platformFrames().empty() &&
            envelope.walls().empty() &&
            envelope.ramps().empty() &&
            envelope.grid().occupiedCellCount() == 0 &&
            envelope.structuralGraph().nodeCount() == 0,
        "debug clear removes every modular element");
    require(
        envelope
            .placePlatformFrame(
                envelope.previewPlatformFrame(
                    {0.2, 0.0, 0.2}, player))
            .has_value(),
        "cleared PlatformFrame cells can be rebuilt");
}
