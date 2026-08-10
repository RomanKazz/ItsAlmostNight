#include "TestHarness.hpp"
#include "buildings/FoundationSystem.hpp"
#include "buildings/RampPlacementDirection.hpp"
#include "buildings/StructuralSupportGraph.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

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
        const auto collapsePreview =
            graph.dependentIds(ground);
        require(
            collapsePreview.size() == 2U &&
                std::ranges::find(collapsePreview, upper) !=
                    collapsePreview.end() &&
                std::ranges::find(collapsePreview, roof) !=
                    collapsePreview.end(),
            "structural graph exposes collapse preview ids");
        require(
            graph.collapseRiskIds(ground) == collapsePreview,
            "single support collapse preview matches dependents");
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
    {
        ian::StructuralSupportGraph graph;
        const ian::EntityId left{10U, 1U};
        const ian::EntityId right{11U, 1U};
        const ian::EntityId bridge{12U, 1U};
        const std::array supports{left, right};
        require(
            graph.add(left, true) &&
                graph.add(right, true) &&
                graph.add(bridge, false, supports),
            "structural graph creates redundant support");
        require(
            graph.dependentCount(left) == 1U &&
                graph.collapseRiskIds(left).empty(),
            "collapse preview respects alternate support");
        require(
            graph.collapseRiskIds(supports) ==
                std::vector<ian::EntityId>{bridge},
            "collapse preview combines multiple removed supports");
    }
    {
        ian::StructuralSupportGraph graph;
        const ian::EntityId root{20U, 1U};
        const ian::EntityId child{21U, 1U};
        const ian::EntityId grandchild{22U, 1U};
        require(
            graph.add(root, true) &&
                graph.add(
                    child, false,
                    std::span<const ian::EntityId>{&root, 1U}) &&
                graph.dependentCount(root) == 1U,
            "dependent count cache stores initial traversal");
        require(
            graph.add(
                grandchild, false,
                std::span<const ian::EntityId>{&child, 1U}) &&
                graph.dependentCount(root) == 2U,
            "adding support invalidates dependent count cache");
        require(
            graph.remove(grandchild) &&
                graph.dependentCount(root) == 1U,
            "removing support invalidates dependent count cache");
    }
    {
        constexpr std::size_t ChainLength = 2048;
        ian::StructuralSupportGraph graph;
        ian::EntityId previous{1000U, 1U};
        require(graph.add(previous, true),
                "stress graph creates grounded root");
        for (std::size_t index = 1; index < ChainLength; ++index) {
            const ian::EntityId current{
                1000U + static_cast<std::uint32_t>(index), 1U};
            require(
                graph.add(
                    current, false,
                    std::span<const ian::EntityId>{&previous, 1U}),
                "stress graph extends support chain");
            previous = current;
        }
        const ian::EntityId root{1000U, 1U};
        require(
            graph.collapseRiskIds(root).size() == ChainLength - 1U,
            "collapse preview scales across deep support chain");
        require(
            graph.remove(root) &&
                graph.unsupportedCount() == ChainLength - 1U,
            "deep support loss propagates without recursion");
        require(
            graph.update(0.0, true, 0.0).size() ==
                    ChainLength - 1U &&
                graph.nodeCount() == 0U,
            "deep cascade removes every unsupported node");
    }

    ian::WorldConfig config =
        ian::WorldConfig::defaults();
    config.terrainResolution = 65;
    config.terrainWorldSize = 48.0;
    config.coreFlatRadius = 20.0;
    config.buildPreviewDistance = 20.0;
    ian::TerrainHeightfield terrain{config};
    const ian::Vec3 player{-5.0, 1.7, 0.0};

    {
        ian::WorldConfig unevenConfig = config;
        unevenConfig.coreFlatRadius = 0.0;
        unevenConfig.terrainBuildPlateauRadius = 2.0;
        unevenConfig.terrainFeatureSize = 20.0;
        unevenConfig.terrainSlopeWidth = 8.0;
        unevenConfig.terrainAmplitude = 6.0;
        unevenConfig.buildPreviewDistance = 200.0;
        unevenConfig.maxWoodSupportLength = 8.0;
        unevenConfig.maximumFoundationHeightDifference = 2.0;
        ian::TerrainHeightfield unevenTerrain{
            unevenConfig};
        ian::BuildGrid emptyGrid{unevenConfig};
        ian::PlacementValidator isolatedValidator{
            unevenTerrain, emptyGrid};
        bool verifiedInheritedLevel = false;
        for (int z = -16;
             z <= 14 && !verifiedInheritedLevel;
             z += ian::PlatformFrameWidthCells) {
            for (int x = -16; x <= 14;
                 x += ian::PlatformFrameWidthCells) {
                const auto hitAt =
                    [&unevenTerrain](int anchorX,
                                     int anchorZ) {
                        const double x = anchorX + 1.0;
                        const double z = anchorZ + 1.0;
                        return ian::Vec3{
                            x,
                            unevenTerrain.getHeight(x, z),
                            z,
                        };
                    };
                const ian::Vec3 firstHit = hitAt(x, z);
                const ian::Vec3 secondHit =
                    hitAt(
                        x +
                            ian::PlatformFrameWidthCells,
                        z);
                const auto isolatedFirst =
                    isolatedValidator
                        .validateGroundPlatformFrame(
                            firstHit, firstHit);
                const auto isolatedSecond =
                    isolatedValidator
                        .validateGroundPlatformFrame(
                            secondHit, secondHit);
                if (!isolatedFirst.valid() ||
                    !isolatedSecond.valid() ||
                    std::abs(
                        isolatedFirst.floorHeight -
                        isolatedSecond.floorHeight) <
                        1e-6) {
                    continue;
                }
                const bool firstIsHigher =
                    isolatedFirst.floorHeight >
                    isolatedSecond.floorHeight;
                const ian::Vec3 sourceHit =
                    firstIsHigher ? firstHit : secondHit;
                const ian::Vec3 targetHit =
                    firstIsHigher ? secondHit : firstHit;
                const auto& sourcePlacement =
                    firstIsHigher
                        ? isolatedFirst
                        : isolatedSecond;
                const auto& targetPlacement =
                    firstIsHigher
                        ? isolatedSecond
                        : isolatedFirst;
                if (
                    !isolatedValidator
                         .validateGroundPlatformFrameAt(
                             targetPlacement.anchor,
                             sourcePlacement.floorHeight,
                             targetHit)
                         .valid()) {
                    continue;
                }
                ian::FoundationSystem unevenFrames{
                    unevenTerrain, unevenConfig};
                const auto placedLeft =
                    unevenFrames.placePlatformFrame(
                        unevenFrames.previewFoundation(
                            sourceHit, sourceHit));
                const auto inheritedPreview =
                    unevenFrames.previewFoundation(
                        targetHit, targetHit);
                require(
                    placedLeft && inheritedPreview.valid() &&
                        std::abs(
                            inheritedPreview.floorHeight -
                            placedLeft->floorHeight) < 1e-9,
                    "adjacent ground frame inherits the existing base elevation");
                verifiedInheritedLevel = true;
                break;
            }
        }
        require(
            verifiedInheritedLevel,
            "uneven terrain fixture finds a compatible inherited level");
    }
    {
        ian::FoundationSystem mixedNeighbours{
            terrain, config};
        const auto lowNeighbour =
            mixedNeighbours.placePlatformFrame({
                .anchor = {-2, 1, 0},
                .floorHeight = 1.0,
                .storey = 0,
            });
        const auto highNeighbour =
            mixedNeighbours.placePlatformFrame({
                .anchor = {2, 5, 0},
                .floorHeight = 5.0,
                .storey = 0,
            });
        const auto betweenLevels =
            mixedNeighbours.previewFoundation(
                {0.2, 0.0, 0.2},
                {0.2, 1.7, 0.2});
        require(
            lowNeighbour && highNeighbour &&
                betweenLevels.valid() &&
                std::abs(
                    betweenLevels.floorHeight - 1.0) <
                    1e-9,
            "different neighbouring levels snap only to a terrain-compatible level");
    }

    ian::FoundationSystem frames{terrain, config};
    {
        ian::FoundationSystem resetIds{terrain, config};
        const auto beforeReset = resetIds.placePlatformFrame(
            resetIds.previewFoundation(
                {0.2, 0.0, 0.2}, player));
        require(beforeReset.has_value(),
                "foundation reset fixture places first frame");
        resetIds.reset();
        const auto afterReset = resetIds.placePlatformFrame(
            resetIds.previewFoundation(
                {0.2, 0.0, 0.2}, player));
        require(
            afterReset && afterReset->id != beforeReset->id,
            "foundation reset never aliases a previous run ID");
    }
    const auto first = frames.placePlatformFrame(
        frames.previewFoundation(
            {0.2, 0.0, 0.2}, player));
    const auto neighbour =
        frames.placePlatformFrame(
            frames.previewFoundation(
                {2.2, 0.0, 0.2}, player));
    require(
        first && neighbour &&
            first->anchor ==
                ian::GridCoord{0, first->anchor.yLevel, 0} &&
            neighbour->anchor.x == 2 &&
            frames.platformFrames().size() == 2,
        "only 2x2 PlatformFrames place on frame grid");
    const auto occupiedFoundation =
        frames.previewFoundation(
            {0.2, 0.0, 0.2}, player);
    require(
        !occupiedFoundation.valid() &&
            occupiedFoundation.storey == 0 &&
            occupiedFoundation.error ==
                ian::ModularPlacementError::Occupied,
        "foundation stays on terrain instead of auto-stacking");
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
            destructibleFrames.previewFoundation(
                {0.2, 0.0, 0.2}, player));
    const auto initiallyDamagedFrame = destructible
        ? destructibleFrames.damage(
              destructible->id, 100.0)
        : std::nullopt;
    require(
        initiallyDamagedFrame &&
            destructibleFrames.restoreHealthFraction(0.15) ==
                ian::PlatformFrameMaxHealth * 0.15 &&
            destructibleFrames.platformFrames().front().health ==
                245.0,
        "field repair restores modular structure health");
    const auto destroyedFrame =
        destructible
            ? destructibleFrames.damage(
                  destructible->id,
                  245.0)
            : std::nullopt;
    require(
        destroyedFrame && destroyedFrame->destroyed &&
            destructibleFrames.platformFrames().empty() &&
            destructibleFrames.grid()
                    .occupiedCellCount() == 0,
        "lethal PlatformFrame damage removes structure and occupancy");

    ian::FoundationSystem tower{terrain, config};
    const auto ground = tower.placePlatformFrame(
        tower.previewFoundation(
            {0.2, 0.0, 0.2}, player));
    const auto upperPreview =
        tower.previewFloorPlatform(
            ground->anchor, 1,
            ground->floorHeight +
                ian::modularStoreyHeight(config),
            player);
    const auto upper =
        tower.placePlatformFrame(upperPreview);
    const auto roof =
        tower.placePlatformFrame(
            tower.previewFloorPlatform(
                upper->anchor, 2,
                upper->floorHeight +
                    ian::modularStoreyHeight(config),
                player));
    require(
        tower
                .previewFloorPlatform(
                    upper->anchor, 2,
                    upper->floorHeight +
                        ian::modularStoreyHeight(config) +
                        config.verticalGridStep,
                    player)
                .error ==
            ian::ModularPlacementError::NoSupport,
        "floor platform cannot create an implicit off-grid level");
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
                .previewFloorPlatform(
                    roof->anchor, 3,
                    roof->floorHeight + storeyHeight,
                    player)
                .error ==
            ian::ModularPlacementError::MaximumStorey,
        "frame stack respects maximum storeys");
    ian::FoundationSystem bridgeFrames{
        terrain, config};
    const auto bridgeGround =
        bridgeFrames.placePlatformFrame(
            bridgeFrames.previewFoundation(
                {0.2, 0.0, 0.2}, player));
    const auto bridgeUpper =
        bridgeFrames.placePlatformFrame(
            bridgeFrames.previewFloorPlatform(
                bridgeGround->anchor, 1,
                bridgeGround->floorHeight +
                    storeyHeight,
                player));
    require(
        bridgeGround && bridgeUpper,
        "edge extension fixture builds its source stack");
    const auto unsupportedEdge =
        bridgeFrames.previewFloorPlatform(
            {2, 0, 0}, bridgeUpper->storey,
            bridgeUpper->floorHeight, player);
    require(
        !unsupportedEdge.valid() &&
            unsupportedEdge.error ==
                ian::ModularPlacementError::NoSupport,
        "floor platform never creates a hidden foundation column");
    require(
        bridgeFrames
                .previewFloorPlatform(
                    bridgeGround->anchor, 0,
                    bridgeGround->floorHeight, player)
                .error ==
            ian::ModularPlacementError::NoSupport,
        "floor platform tool rejects the foundation storey");
    const auto edgeFoundation =
        bridgeFrames.placePlatformFrame(
            bridgeFrames.previewFoundation(
                {2.2, 0.0, 0.2}, player));
    const auto supportedEdge =
        bridgeFrames.previewFloorPlatform(
            {2, 0, 0}, bridgeUpper->storey,
            bridgeUpper->floorHeight, player);
    const auto placedEdge =
        bridgeFrames.placePlatformFrame(
            supportedEdge);
    require(
        edgeFoundation && supportedEdge.valid() &&
            placedEdge &&
            placedEdge->storey == upper->storey &&
            bridgeFrames.structuralGraph().dependentCount(
                edgeFoundation->id) == 1U,
        "floor platform uses an existing foundation below");
    const auto occupiedEdge =
        bridgeFrames.previewFloorPlatform(
            {2, 0, 0}, bridgeUpper->storey,
            bridgeUpper->floorHeight, player);
    require(
        !occupiedEdge.valid() &&
            occupiedEdge.error ==
                ian::ModularPlacementError::Occupied,
        "edge extension rejects an already occupied target floor");

    ian::WorldConfig scaledConfig = config;
    scaledConfig.cellSize = 1.25;
    scaledConfig.verticalGridStep = 0.25;
    ian::TerrainHeightfield scaledTerrain{
        scaledConfig};
    ian::FoundationSystem scaledFrames{
        scaledTerrain, scaledConfig};
    const auto scaledGround =
        scaledFrames.placePlatformFrame(
            scaledFrames.previewFoundation(
                {0.2, 0.0, 0.2}, player));
    const auto scaledUpper =
        scaledFrames.placePlatformFrame(
            scaledFrames.previewFloorPlatform(
                scaledGround->anchor, 1,
                scaledGround->floorHeight +
                    ian::modularStoreyHeight(
                        scaledConfig),
                player));
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
        envelope.previewFoundation(
            {0.2, 0.0, 0.2}, player));
    ian::FoundationSystem wallEnvelope{
        terrain, config};
    const auto wallBase =
        wallEnvelope.placePlatformFrame(
            wallEnvelope.previewFoundation(
                {0.2, 0.0, 0.2}, player));
    const auto wall = wallEnvelope.placeWall(
        wallEnvelope.previewWall(
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
                -ian::PlatformFrameWidthCells &&
            envelope
                    .previewRamp(
                        {0.2, 0.0, 0.2}, player,
                        ian::Rotation::Deg0)
                    .anchor.z ==
                0 &&
            envelope
                    .previewRamp(
                        {0.2, 0.0, 0.2}, player,
                        ian::Rotation::Deg90)
                    .anchor.x ==
                -ian::PlatformFrameWidthCells &&
            envelope
                    .previewRamp(
                        {0.2, 0.0, 0.2}, player,
                        ian::Rotation::Deg270)
                    .anchor.x ==
                0,
        "ramp low half begins directly above its supporting platform");
    const auto rampSockets =
        ian::platformRampEdgeSockets(
            {0, 0, 0}, 2.0, 1.0);
    const auto northEdgeSnap =
        ian::platformEdgeSnapAtAim(
            {0, 0, 0}, 2.0,
            {1.0, 5.0, -5.0},
            {0.0, -3.0, 7.0}, 1.0);
    const auto eastEdgeSnap =
        ian::platformEdgeSnapAtAim(
            {0, 0, 0}, 2.0,
            {-5.0, 5.0, 1.0},
            {7.0, -3.0, 0.0}, 1.0);
    require(
        northEdgeSnap && eastEdgeSnap &&
            northEdgeSnap->extensionAnchor ==
                ian::GridCoord{0, 0, 2} &&
            eastEdgeSnap->extensionAnchor ==
                ian::GridCoord{2, 0, 0} &&
            std::abs(northEdgeSnap->marker.z - 2.0) <
                1e-9 &&
            std::abs(eastEdgeSnap->marker.x - 2.0) <
                1e-9,
        "platform edge aim snaps to the adjacent frame and keeps marker on the edge");
    require(
        !ian::platformEdgeSnapAtAim(
             {0, 0, 0}, 2.0,
             {1.0, 5.0, -5.0},
             {0.0, -3.0, 6.0}, 1.0),
        "platform center aim does not trigger edge snapping");
    bool everySocketWinsAtCrosshair = true;
    const ian::Vec3 socketViewer{1.0, 5.0, -5.0};
    for (const ian::RampEdgeSocket& socket :
         rampSockets) {
        const ian::Vec3 look{
            socket.position.x - socketViewer.x,
            socket.position.y - socketViewer.y,
            socket.position.z - socketViewer.z,
        };
        const auto nearest =
            ian::nearestRampEdgeSocket(
                {0, 0, 0}, 2.0, 1.0,
                socketViewer, look);
        everySocketWinsAtCrosshair &=
            nearest &&
            nearest->rotation == socket.rotation;
    }
    require(
        everySocketWinsAtCrosshair &&
            rampSockets[0].neighborAnchor.z == 2 &&
            rampSockets[1].neighborAnchor.x == -2 &&
            rampSockets[2].neighborAnchor.z == -2 &&
            rampSockets[3].neighborAnchor.x == 2,
        "ramp sockets select the platform edge nearest the crosshair");
    const ian::Vec3 freeEdgeViewer{1.0, 5.0, -5.0};
    const ian::Vec3 exactFreeEdgeAim{
        rampSockets[0].position.x - freeEdgeViewer.x,
        rampSockets[0].position.y - freeEdgeViewer.y,
        rampSockets[0].position.z - freeEdgeViewer.z,
    };
    require(
        ian::rampSocketAimScore(
            rampSockets[0],
            freeEdgeViewer,
            exactFreeEdgeAim) <=
                ian::RampSocketAcquisitionAimScore &&
            ian::rampSocketAimScore(
                rampSockets[0],
                freeEdgeViewer,
                {1.0, 0.0, 0.0}) >
                ian::RampSocketAcquisitionAimScore,
        "a free ramp edge can be acquired through open air without "
        "capturing a clearly off-axis edge");
    const std::array<std::pair<ian::Vec3, ian::Rotation>, 4>
        viewDirections{{
            {{0.0, -0.4, 1.0}, ian::Rotation::Deg0},
            {{-1.0, -0.4, 0.0}, ian::Rotation::Deg90},
            {{0.0, -0.4, -1.0}, ian::Rotation::Deg180},
            {{1.0, -0.4, 0.0}, ian::Rotation::Deg270},
        }};
    bool everyViewDirectionSelectsForwardEdge = true;
    for (const auto& [look, expected] : viewDirections) {
        const auto aligned =
            ian::mostViewAlignedRampEdgeSocket(
                {0, 0, 0}, 2.0, 1.0, look);
        everyViewDirectionSelectsForwardEdge &=
            aligned && aligned->rotation == expected;
    }
    require(
        everyViewDirectionSelectsForwardEdge,
        "ramp edge direction follows the player's horizontal view");
    require(
        ian::rampSupportAnchorAtAim(
            {1.0, 2.0, 0.99},
            {0.0, -0.4, 1.0}, 1.0) ==
                ian::GridCoord{0, 0, -2} &&
            ian::rampSupportAnchorAtAim(
                {1.0, 2.0, 1.01},
                {0.0, -0.4, 1.0}, 1.0) ==
                ian::GridCoord{0, 0, 0} &&
            ian::rampSupportAnchorAtAim(
                {1.0, 2.0, 3.01},
                {0.0, -0.4, 1.0}, 1.0) ==
                ian::GridCoord{0, 0, 2} &&
            ian::rampSupportAnchorAtAim(
                {1.01, 2.0, 1.0},
                {-1.0, -0.4, 0.0}, 1.0) ==
                ian::GridCoord{2, 0, 0},
        "ramp support selection switches at platform"
        " half-way boundaries one platform closer");
    require(
        ian::rampTopPlatformAnchor(
            {0, 0, 0}, ian::Rotation::Deg0) ==
                ian::GridCoord{0, 0, 4} &&
            ian::rampTopPlatformAnchor(
                {-2, 0, 0},
                ian::Rotation::Deg90) ==
                ian::GridCoord{-4, 0, 0} &&
            ian::rampTopPlatformAnchor(
                {0, 0, -2},
                ian::Rotation::Deg180) ==
                ian::GridCoord{0, 0, -4} &&
            ian::rampTopPlatformAnchor(
                {0, 0, 0},
                ian::Rotation::Deg270) ==
                ian::GridCoord{4, 0, 0},
        "ramp top edge maps to the adjacent PlatformFrame anchor");
    require(
        ian::rampSocketContainsFloorAim(
            rampSockets[0],
            {1.0, 2.0, 1.5}, 1.0) &&
            ian::rampSocketContainsFloorAim(
                rampSockets[0],
                {1.0, 2.0, 2.5}, 1.0) &&
            !ian::rampSocketContainsFloorAim(
                rampSockets[0],
                {1.0, 2.0, 1.49}, 1.0) &&
            !ian::rampSocketContainsFloorAim(
                rampSockets[0],
                {1.0, 2.0, 2.51}, 1.0),
        "ramp edge safe zone spans half a cell on both sides");
    require(
        base && wallBase && wall && ramp &&
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

    ian::FoundationSystem rampSupportEnvelope{
        terrain, config};
    const auto rearPlatform =
        rampSupportEnvelope.placePlatformFrame(
            rampSupportEnvelope.previewFoundation(
                {-1.8, 0.0, 0.2}, player));
    const auto platformUnderRamp =
        rampSupportEnvelope.placePlatformFrame(
            rampSupportEnvelope.previewFoundation(
                {0.2, 0.0, 0.2}, player));
    require(
        rearPlatform && platformUnderRamp,
        "ramp dependency fixture creates adjacent platforms");
    const auto edgeRampPreview =
        rampSupportEnvelope.previewRamp(
            {0.2, 0.0, 0.2}, player,
            ian::Rotation::Deg270);
    const auto edgeRamp =
        rampSupportEnvelope.placeRamp(
            edgeRampPreview);
    require(
        edgeRampPreview.valid() && edgeRamp &&
            edgeRamp->anchor.x ==
                platformUnderRamp->anchor.x &&
            rampSupportEnvelope.structuralGraph()
                    .dependentCount(
                        platformUnderRamp->id, false) ==
                1U &&
            rampSupportEnvelope.structuralGraph()
                    .dependentCount(
                        rearPlatform->id, false) == 0U,
        "ramp starts over the edge platform and ignores the platform behind it");
    require(
        rampSupportEnvelope.remove(
            rearPlatform->id) &&
            rampSupportEnvelope.ramps().size() == 1U &&
            rampSupportEnvelope.ramps()[0].supportState ==
                ian::StructuralSupportState::Supported,
        "removing the platform behind the ramp does not weaken it");
    const auto woundedWall =
        wallEnvelope.damage(wall->id, 30.0);
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
        wallEnvelope.repair(wall->id, 100, 100);
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
                envelope.previewFoundation(
                    {0.2, 0.0, 0.2}, player))
            .has_value(),
        "cleared PlatformFrame cells can be rebuilt");
}
