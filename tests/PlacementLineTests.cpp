#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/PlacementLine.hpp"

void runPlacementLineTests() {
    using ian::PlacementLineAxis;

    auto axis = ian::stabilizePlacementLineAxis(
        4, 3, std::nullopt, 2);
    require(
        axis == PlacementLineAxis::X,
        "line initially chooses dominant X axis");
    axis = ian::stabilizePlacementLineAxis(
        3, 4, axis, 2);
    require(
        axis == PlacementLineAxis::X,
        "near-diagonal cursor noise keeps current axis");
    axis = ian::stabilizePlacementLineAxis(
        2, 4, axis, 2);
    require(
        axis == PlacementLineAxis::Z,
        "switch occurs at one modular grid step");
    axis = ian::stabilizePlacementLineAxis(
        2, 0, axis, 2);
    require(
        axis == PlacementLineAxis::X,
        "axis can change beside blocked neighboring cells");

    const auto negative = ian::placementLine(
        ian::GridPosition{3, 2},
        ian::GridPosition{-3, 1}, 2,
        PlacementLineAxis::X);
    require(
        negative.size() == 4U &&
            negative.front() ==
                ian::GridPosition{3, 2} &&
            negative.back() ==
                ian::GridPosition{-3, 2},
        "line handles negative direction and spacing");

    const auto capped = ian::placementLine(
        ian::GridPosition{0, 0},
        ian::GridPosition{1000, 0}, 1,
        PlacementLineAxis::X);
    require(
        capped.size() ==
            ian::MaximumPlacementLineLength &&
            capped.back() ==
                ian::GridPosition{47, 0},
        "line length cap applies to preview and placement");

    const auto empty = ian::placementLine(
        ian::GridPosition{0, 0},
        ian::GridPosition{3, 0}, 0);
    require(
        empty.empty(),
        "invalid spacing produces no placements");
}
