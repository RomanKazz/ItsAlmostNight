#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "navigation/FlowField.hpp"

#include <cmath>

void runFlowFieldTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "flow field fixture creates core");

    ian::FlowField openField;
    openField.rebuild({0, 0}, buildings.buildings());
    const auto openDirection = openField.directionAt({5.0, 0.0, 0.0});
    require(openDirection.has_value() && openDirection->x < 0.0,
            "open field points toward core");
    requireNear(openField.distanceAt({0, 0}), 0.0, 1e-12, "core distance is zero");
    const auto* obstacleCell = openField.cellAt({-8, -7});
    require(obstacleCell != nullptr && obstacleCell->blocked,
            "graybox obstacle marks navigation cells blocked");

    const auto wall = buildings.place(ian::BuildingType::Wall, {0, -2}, 0, 10, 0);
    require(wall.has_value(), "flow field fixture creates wall");
    ian::FlowField wallField;
    wallField.rebuild({0, 0}, buildings.buildings());
    const auto detour = wallField.directionAt({0.0, 0.0, -3.0});
    require(detour.has_value() && std::abs(detour->x) > 0.0,
            "flow field routes around isolated wall");
    require(wallField.distanceAt({0, -3}) > openField.distanceAt({0, -3}),
            "wall raises route cost");

    ian::BuildingSystem gateBuildings;
    require(gateBuildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0).has_value(),
            "gate flow fixture creates core");
    const auto gate =
        gateBuildings.place(ian::BuildingType::Gate, {0, -2}, 0, 15, 5);
    require(gate.has_value(), "gate flow fixture creates gate");
    ian::FlowField closedGateField;
    closedGateField.rebuild({0, 0}, gateBuildings.buildings());
    const auto closedGateDirection = closedGateField.directionAt({0.0, 0.0, -3.0});
    require(closedGateDirection.has_value() && std::abs(closedGateDirection->x) > 0.0,
            "closed gate makes flow field detour");

    require(gateBuildings.toggleGate(gate->building.id).has_value(),
            "gate flow fixture opens gate");
    ian::FlowField openedGateField;
    openedGateField.rebuild({0, 0}, gateBuildings.buildings());
    const auto openedGateDirection = openedGateField.directionAt({0.0, 0.0, -3.0});
    require(openedGateDirection.has_value() && openedGateDirection->z > 0.0,
            "open gate restores direct flow");
    require(openedGateField.distanceAt({0, -3}) < closedGateField.distanceAt({0, -3}),
            "open gate lowers route cost");

    std::vector<ian::BuildingInstance> sealed = buildings.buildings();
    std::uint32_t id = 4000;
    for (int x = -2; x <= 2; ++x) {
        sealed.push_back({{id++, 1}, ian::BuildingType::Wall, {x, -2}, 0, 1, 100.0, 100.0});
        sealed.push_back({{id++, 1}, ian::BuildingType::Wall, {x, 2}, 0, 1, 100.0, 100.0});
    }
    for (int z = -1; z <= 1; ++z) {
        sealed.push_back({{id++, 1}, ian::BuildingType::Wall, {-2, z}, 0, 1, 100.0, 100.0});
        sealed.push_back({{id++, 1}, ian::BuildingType::Wall, {2, z}, 0, 1, 100.0, 100.0});
    }

    ian::FlowField sealedField;
    sealedField.rebuild({0, 0}, sealed);
    require(std::isfinite(sealedField.distanceAt({0, -4})),
            "finite wall cost preserves route into sealed base");

    ian::FlowField repeatedField;
    repeatedField.rebuild({0, 0}, sealed);
    requireNear(repeatedField.distanceAt({7, -7}), sealedField.distanceAt({7, -7}), 1e-12,
                "same input produces deterministic field");

    ian::FlowField customObstacles{{{-1.0, 1.0, -4.0, -2.0}}};
    customObstacles.rebuild({0, 0}, {});
    const auto* customBlockedCell = customObstacles.cellAt({0, -3});
    require(customBlockedCell != nullptr && customBlockedCell->blocked,
            "flow field consumes configured map obstacle");
    require(!customObstacles.debugVectors().empty(),
            "ready flow field exposes compact debug vectors");
}
