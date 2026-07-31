#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "world/CollisionWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

void runCollisionWorldTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "collision fixture creates core");

    ian::CollisionWorld collision;
    collision.syncBuildings(buildings.buildings());

    const auto movedThroughCore =
        collision.moveCircle({0.0, 1.7, 4.0}, {0.0, 0.0, -6.0},
                             ian::CollisionWorld::PlayerRadius);
    require(movedThroughCore.z < 0.0,
            "player can pass through crystal core");

    const auto sliding =
        collision.moveCircle({1.9, 1.7, 4.0}, {0.0, 0.0, -4.0}, ian::CollisionWorld::PlayerRadius);
    require(sliding.z < 1.0, "player can slide past collider edge");

    const auto coreBox = ian::buildingCollisionBox(ian::BuildingType::Core, {0, 0});
    require(coreBox.minX == -1.0 && coreBox.maxX == 1.0 &&
                coreBox.minZ == -1.0 && coreBox.maxZ == 1.0,
            "core collider occupies two-by-two footprint");
    require(collision.overlapsCircle({1.2, 1.7, 0.0}, ian::CollisionWorld::PlayerRadius, coreBox),
            "circle overlap detects player beside core");
    require(!collision.overlapsCircle({3.0, 1.7, 0.0}, ian::CollisionWorld::PlayerRadius, coreBox),
            "circle overlap rejects distant player");
    require(
        !collision.overlapsBox(
            ian::buildingCollisionBox(
                ian::BuildingType::Core,
                {0, 0}, 4.0)),
        "colliders on separate storeys may share horizontal footprint");

    require(collision.overlapsBox({-8.5, -7.5, -7.5, -6.5}),
            "box overlap detects static graybox obstacle");
    require(!collision.overlapsBox({20.0, 21.0, 20.0, 21.0}),
            "box overlap accepts free world area");

    const auto wall =
        buildings.place(ian::BuildingType::Wall, {3, 0}, 0, 30, 30);
    require(wall.has_value(), "collision fixture creates wall");
    collision.syncBuildings(buildings.buildings());
    const auto thinWallCollider =
        std::find_if(
            collision.colliders().begin(),
            collision.colliders().end(),
            [](const ian::CollisionBox& box) {
                return std::isfinite(
                           box.maximumBlockingEyeY) &&
                       (box.maxZ - box.minZ) <= 0.21;
            });
    require(
        thinWallCollider != collision.colliders().end(),
        "wall uses thin collider matching fence model");
    const auto stoppedByWall =
        collision.moveCircle({3.5, 1.7, 3.0}, {0.0, 0.0, -4.0},
                             ian::CollisionWorld::PlayerRadius);
    require(stoppedByWall.z > 1.0,
            "standing player cannot cross wall");
    const auto jumpedOverWall =
        collision.moveCircle({3.5, 2.3, 3.0}, {0.0, 0.0, -4.0},
                             ian::CollisionWorld::PlayerRadius);
    require(jumpedOverWall.z < 0.0,
            "jumping player can cross wall");
    const auto escapedWall =
        collision.moveCircle({3.5, 1.7, 0.5}, {0.0, 0.0, 1.5},
                             ian::CollisionWorld::PlayerRadius);
    require(escapedWall.z > 1.0,
            "player can leave wall collider after landing inside it");
    const auto passedBesideWall =
        collision.moveCircle({2.0, 1.7, 1.05}, {3.0, 0.0, 0.0},
                             ian::CollisionWorld::PlayerRadius);
    require(passedBesideWall.x > 4.5,
            "thin wall collider leaves free space beside fence");

    const auto turret = buildings.place(
        ian::BuildingType::Turret, {6, 0}, 0, 25, 15);
    require(turret.has_value(),
            "collision fixture creates turret");
    collision.syncBuildings(buildings.buildings());
    const auto passedThroughTurret =
        collision.moveCircle(
            {3.0, 1.7, 0.0}, {6.0, 0.0, 0.0},
            ian::CollisionWorld::PlayerRadius);
    require(
        passedThroughTurret.x > 8.5,
        "player can walk through non-wall defensive buildings");

    const auto gate = buildings.place(ian::BuildingType::Gate, {0, 4}, 0, 15, 5);
    require(gate.has_value(), "collision fixture creates gate");
    collision.syncBuildings(buildings.buildings());
    const auto stoppedByGate =
        collision.moveCircle({0.0, 1.7, 7.0}, {0.0, 0.0, -4.0},
                             ian::CollisionWorld::PlayerRadius);
    require(stoppedByGate.z > 4.8, "closed gate blocks player");

    require(buildings.toggleGate(gate->building.id).has_value(), "gate opens");
    collision.syncBuildings(buildings.buildings());
    const auto passedGate =
        collision.moveCircle({0.0, 1.7, 7.0}, {0.0, 0.0, -4.0},
                             ian::CollisionWorld::PlayerRadius);
    require(passedGate.z < 4.0, "open gate allows player through");

    ian::CollisionWorld modularCollision{48.0, {}};
    const std::vector<ian::BuildingInstance>
        elevatedBuildings{{
            .id = {99U, 1U},
            .type = ian::BuildingType::Turret,
            .gridPosition = {8, 8},
            .baseHeight = 3.0,
            .platformStorey = -1,
            .foundationBottomHeight = 0.0,
        }};
    modularCollision.syncBuildings(
        elevatedBuildings);
    require(
        modularCollision.modularSurfaceHeight(
            8.5, 8.5, 3.1) ==
            std::optional<double>{3.0},
        "automatic building foundation exposes a walkable surface");
    const std::array<ian::PlatformFrameInstance, 1>
        modularFrames{{
            {
                .id = {100U, 1U},
                .anchor = {15, 0, 0},
                .floorHeight = 2.0,
                .storey = 0,
            },
        }};
    const std::array<ian::WallInstance, 1>
        modularWalls{{
            {
                .id = {101U, 1U},
                .anchor = {18, 5, 0},
                .rotation = ian::Rotation::Deg0,
                .bottomHeight = 2.0,
                .topHeight = 6.0,
                .storey = 1,
            },
        }};
    const std::array<ian::RampInstance, 1>
        modularRamps{{
            {
                .id = {102U, 1U},
                .anchor = {16, 0, 0},
                .rotation = ian::Rotation::Deg270,
                .bottomHeight = 2.0,
                .topHeight = 6.0,
                .targetStorey = 1,
            },
        }};
    modularCollision.syncModularBuildings({
        modularFrames,
        modularWalls,
        modularRamps,
        1.0,
    });
    require(
        modularCollision.modularSurfaceHeight(
            15.5, 0.5, 2.1) ==
            std::optional<double>{2.0} &&
            !modularCollision.modularSurfaceHeight(
                15.5, 0.5, 1.0),
        "modular floor supports player without upward teleport");
    require(
        !modularCollision.modularSurfaceHeight(
            14.7, 0.5, 2.1) &&
            modularCollision.playerSupportHeight(
                14.7, 0.5,
                ian::CollisionWorld::PlayerRadius,
                2.1) == std::optional<double>{2.0} &&
            !modularCollision.playerSupportHeight(
                14.6, 0.5,
                ian::CollisionWorld::PlayerRadius,
                2.1),
        "player radius remains supported at a platform edge"
        " without extending support too far");
    require(
        modularCollision.modularCeilingHeight(
            15.5, 0.5, 1.2, 1.9) ==
            std::optional<double>{1.5} &&
            !modularCollision.modularCeilingHeight(
                14.0, 0.5, 1.2, 1.9),
        "platform underside stops upward movement only inside"
        " its footprint");
    const auto rampLow =
        modularCollision.modularSurfaceHeight(
            17.0, 0.5, 6.1);
    const auto rampHigh =
        modularCollision.modularSurfaceHeight(
            19.0, 0.5, 6.1);
    require(
        rampLow && rampHigh &&
            std::abs(*rampLow - 3.0) < 1e-9 &&
            std::abs(*rampHigh - 5.0) < 1e-9,
        "ramp exposes continuous directional surface");
    const auto sprintLanding =
        modularCollision.sweptPlayerLanding(
            {16.8, 4.0, 0.5},
            {17.1, 3.8, 0.5},
            ian::CollisionWorld::PlayerRadius,
            3.0, 2.9);
    require(
        sprintLanding &&
            sprintLanding->surfaceHeight >= 2.8 &&
            sprintLanding->surfaceHeight <= 3.1 &&
            !modularCollision.sweptPlayerLanding(
                {16.8, 2.7, 0.5},
                {17.1, 2.6, 0.5},
                ian::CollisionWorld::PlayerRadius,
                0.9, 0.8),
        "descending sprint lands on a rising ramp without"
        " pulling a player up from below");
    const auto rampCeiling =
        modularCollision.modularCeilingHeight(
            17.0, 0.5, 2.0, 3.0);
    require(
        rampCeiling &&
            std::abs(*rampCeiling - 2.82) < 1e-9,
        "ramp underside follows its continuous slope");
    const auto escapedEmbeddedPlatform =
        modularCollision.moveCircle(
            {15.5, 1.9, 0.5},
            {-2.0, 0.0, 0.0},
            ian::CollisionWorld::PlayerRadius,
            0.85);
    require(
        escapedEmbeddedPlatform.x < 14.7,
        "player can escape a platform intersection caused by"
        " vertical movement");

    ian::CollisionWorld rampSweepCollision(
        48.0, {});
    const std::array<ian::RampInstance, 1>
        sweepRamp{{
            {
                .id = {103U, 1U},
                .anchor = {0, 0, 0},
                .rotation = ian::Rotation::Deg270,
                .bottomHeight = 0.0,
                .topHeight = 4.0,
                .targetStorey = 1,
            },
        }};
    rampSweepCollision.syncModularBuildings({
        std::span<
            const ian::PlatformFrameInstance>{},
        std::span<const ian::WallInstance>{},
        sweepRamp,
        1.0,
    });
    const ian::CollisionBox lowFirstCell{
        0.2, 0.8, 0.2, 0.8, 0.5, 0.0};
    const ian::CollisionBox lowSecondCell{
        1.2, 1.8, 0.2, 0.8, 0.5, 0.0};
    const ian::CollisionBox tallSecondCell{
        1.2, 1.8, 0.2, 0.8, 1.7, 0.0};
    const ian::CollisionBox highPlatformSecondCell{
        1.2, 1.8, 0.2, 0.8, 1.0, 0.82};
    require(
        rampSweepCollision.overlapsBox(
            lowFirstCell) &&
            !rampSweepCollision.overlapsBox(
                lowSecondCell) &&
            rampSweepCollision.overlapsBox(
                tallSecondCell) &&
            !rampSweepCollision.overlapsRampBox(
                lowSecondCell) &&
            rampSweepCollision.overlapsRampBox(
                highPlatformSecondCell),
        "ramp collision blocks its low start but"
        " preserves usable clearance under higher cells"
        " without allowing an intersecting platform");
    const auto stoppedOnRamp =
        rampSweepCollision.moveCircle(
            {-1.0, 1.7, 1.0},
            {6.0, 0.0, 0.0},
            ian::CollisionWorld::PlayerRadius,
            0.65);
    const auto clearedRamp =
        rampSweepCollision.moveCircle(
            {-1.0, 5.7, 1.0},
            {6.0, 0.0, 0.0},
            ian::CollisionWorld::PlayerRadius,
            4.65);
    require(
        stoppedOnRamp.x < 0.7 &&
            clearedRamp.x > 4.5,
        "swept movement cannot tunnel through a ramp"
        " above the reachable step height");
    const auto escapedEmbeddedRamp =
        rampSweepCollision.moveCircle(
            {2.0, 2.0, 1.0},
            {-3.0, 0.0, 0.0},
            ian::CollisionWorld::PlayerRadius,
            0.95);
    require(
        escapedEmbeddedRamp.x < -0.3,
        "player can escape a ramp intersection caused by"
        " vertical movement");
    const std::array<ian::PlatformFrameInstance, 1>
        overheadFrame{{
            {
                .id = {104U, 1U},
                .anchor = {0, 0, 0},
                .floorHeight = 4.0,
                .storey = 1,
            },
        }};
    rampSweepCollision.syncModularBuildings({
        overheadFrame,
        std::span<const ian::WallInstance>{},
        std::span<const ian::RampInstance>{},
        1.0,
    });
    require(
        rampSweepCollision
                .moveCircle(
                    {-1.0, 1.7, 1.0},
                    {4.0, 0.0, 0.0},
                    ian::CollisionWorld::
                        PlayerRadius,
                    0.65)
                .x > 2.5,
        "raised-surface sweep does not block"
        " movement below an upper floor");

    const auto belowUpperWall =
        modularCollision.moveCircle(
            {18.5, 1.7, 2.0},
            {0.0, 0.0, -3.0},
            ian::CollisionWorld::PlayerRadius);
    const auto stoppedByUpperWall =
        modularCollision.moveCircle(
            {18.5, 4.0, 2.0},
            {0.0, 0.0, -3.0},
            ian::CollisionWorld::PlayerRadius);
    require(
        belowUpperWall.z < 0.0 &&
            stoppedByUpperWall.z > 0.8,
        "upper wall blocks its storey but not space below");
    modularCollision.syncModularBuildings({
        std::span<const ian::PlatformFrameInstance>{},
        std::span<const ian::WallInstance>{},
        std::span<const ian::RampInstance>{},
        1.0,
    });
    require(
        !modularCollision.modularSurfaceHeight(
            15.5, 0.5, 3.0) &&
            modularCollision.modularSurfaceHeight(
                8.5, 8.5, 3.1) ==
                std::optional<double>{3.0} &&
            modularCollision
                    .moveCircle(
                        {18.5, 4.0, 2.0},
                        {0.0, 0.0, -3.0},
                        ian::CollisionWorld::PlayerRadius)
                    .z < 0.0,
        "modular resync removes stale modular data without erasing building foundations");
}
