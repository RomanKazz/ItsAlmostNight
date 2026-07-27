#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "world/CollisionWorld.hpp"

void runCollisionWorldTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "collision fixture creates core");

    ian::CollisionWorld collision;
    collision.syncBuildings(buildings.buildings());

    const auto stopped =
        collision.moveCircle({0.0, 1.7, 4.0}, {0.0, 0.0, -4.0}, ian::CollisionWorld::PlayerRadius);
    require(stopped.z > 1.3, "player cannot cross core collider");

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

    require(collision.overlapsBox({-8.5, -7.5, -7.5, -6.5}),
            "box overlap detects static graybox obstacle");
    require(!collision.overlapsBox({20.0, 21.0, 20.0, 21.0}),
            "box overlap accepts free world area");

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
}
