#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/PlacementLine.hpp"

void runPlacementLineTests() {
    const ian::Vec3 elevatedViewer{0.0, 5.0, 0.0};
    const auto rightAim = ian::elevatedPlatformDragAim(
        elevatedViewer, {0.8, -0.8, 0.4}, 1.0, 12.0);
    require(
        rightAim &&
            std::abs(rightAim->x - 4.0) < 1e-9 &&
            std::abs(rightAim->y - 1.0) < 1e-9 &&
            std::abs(rightAim->z - 2.0) < 1e-9,
        "elevated floor drag intersects its working plane");
    const auto leftAim = ian::elevatedPlatformDragAim(
        elevatedViewer, {-0.8, -0.8, 0.4},
        1.0, 12.0);
    require(
        leftAim &&
            std::abs(leftAim->x + 4.0) < 1e-9 &&
            std::abs(leftAim->z - 2.0) < 1e-9,
        "working plane preserves world-space drag direction");
    const auto belowAim = ian::elevatedPlatformDragAim(
        {0.0, 1.0, 0.0}, {0.8, 0.8, 0.4},
        5.0, 12.0);
    require(
        belowAim &&
            std::abs(belowAim->x - 4.0) < 1e-9 &&
            std::abs(belowAim->y - 5.0) < 1e-9 &&
            std::abs(belowAim->z - 2.0) < 1e-9,
        "working plane aim accounts for player height");
    require(
        !ian::elevatedPlatformDragAim(
            elevatedViewer, {0.8, 0.8, 0.4},
            1.0, 12.0),
        "working plane rejects a ray aimed away from it");
    const auto boundedAim = ian::elevatedPlatformDragAim(
        elevatedViewer, {1.0, -0.01, 1.0},
        1.0, 12.0);
    require(
        boundedAim &&
            std::hypot(boundedAim->x, boundedAim->z) <=
                12.0 + 1e-9,
        "elevated floor drag endpoint respects build distance");
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
    axis = ian::stabilizePlacementLineAxis(
        0.0, 0.4, PlacementLineAxis::X, 2.0);
    require(
        axis == PlacementLineAxis::Z,
        "axis switches when its projected line collapses");
    axis = ian::stabilizePlacementLineAxis(
        2.0, 2.2, PlacementLineAxis::X, 0.32);
    require(
        axis == PlacementLineAxis::X,
        "small raw aim jitter does not flip drag axis");
    axis = ian::stabilizePlacementLineAxis(
        2.0, 2.4, axis, 0.32);
    require(
        axis == PlacementLineAxis::Z,
        "raw aim switches axis after a clear margin");

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

    const auto clickAcrossBoundary =
        ian::placementGestureLine(
            ian::GridPosition{4, 5},
            ian::GridPosition{5, 5}, 1, false);
    require(
        clickAcrossBoundary.size() == 1U &&
            clickAcrossBoundary.front() ==
                ian::GridPosition{4, 5},
        "unconfirmed click stays on its pressed cell");

    const auto confirmedDrag =
        ian::placementGestureLine(
            ian::GridPosition{4, 5},
            ian::GridPosition{7, 5}, 1, true);
    require(
        confirmedDrag.size() == 4U &&
            confirmedDrag.back() ==
                ian::GridPosition{7, 5},
        "confirmed drag keeps multi-building placement");

    const auto supportedPrefix =
        ian::contiguousPlacementPrefix(
            ian::placementLine(
                ian::GridPosition{0, 0},
                ian::GridPosition{8, 0}, 2,
                PlacementLineAxis::X),
            [](ian::GridPosition cell) {
                return cell.x < 4;
            });
    require(
        supportedPrefix.size() == 2U &&
            supportedPrefix.back() ==
                ian::GridPosition{2, 0},
        "floor line stops at first missing platform support");
}
