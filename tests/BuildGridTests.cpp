#include "TestHarness.hpp"
#include "buildings/BuildGrid.hpp"
#include "buildings/ModularBuildingConstants.hpp"

void runBuildGridTests() {
    ian::WorldConfig config =
        ian::WorldConfig::defaults();
    config.cellSize = 1.0;
    config.verticalGridStep = 0.5;
    ian::BuildGrid grid{config};

    const ian::GridCoord negative =
        grid.worldToGrid({-0.01, 1.0, -1.01});
    require(
        negative ==
            ian::GridCoord{-1, 2, -2},
        "world to build grid floors negative XZ coordinates");
    const ian::Vec3 oneCenter =
        grid.worldCenter(
            {-1, 2, -2},
            ian::Footprint::OneByOne);
    requireNear(
        oneCenter.x, -0.5, 1e-12,
        "one by one uses minimum-cell anchor");
    requireNear(
        oneCenter.y, 1.0, 1e-12,
        "vertical level converts to world height");
    requireNear(
        oneCenter.z, -1.5, 1e-12,
        "one by one center round trips");

    const auto twoCells = grid.occupiedCells(
        {3, 0, 4}, ian::Footprint::TwoByTwo);
    require(
        twoCells.size() == 4 &&
            twoCells[0] == ian::GridCoord{3, 0, 4} &&
            twoCells[3] == ian::GridCoord{4, 0, 5},
        "two by two footprint occupies four cells");
    const ian::Vec3 twoCenter =
        grid.worldCenter(
            {3, 0, 4},
            ian::Footprint::TwoByTwo);
    requireNear(
        twoCenter.x, 4.0, 1e-12,
        "two by two shares minimum-cell anchor rule");

    const ian::EntityId first{1U, 1U};
    const ian::EntityId second{2U, 1U};
    require(
        grid.occupy(
            first, {0, 0, 0},
            ian::Footprint::TwoByTwo, 2,
            ian::OccupancyLayer::Volume),
        "volume occupancy succeeds");
    require(
        !grid.canOccupy(
            {1, 1, 1},
            ian::Footprint::OneByOne, 1,
            ian::OccupancyLayer::Floor),
        "overlapping occupancy is rejected");
    require(
        grid.canOccupy(
            {2, 0, 0},
            ian::Footprint::OneByOne, 1,
            ian::OccupancyLayer::Floor),
        "adjacent occupancy remains available");
    require(
        grid.occupy(
            second, {2, 0, 0},
            ian::Footprint::OneByOne, 1,
            ian::OccupancyLayer::Floor),
        "adjacent floor occupancy succeeds");
    require(
        grid.occupiedCellCount() == 9,
        "occupancy stores footprint volume cells");

    grid.release(first);
    require(
        grid.canOccupy(
            {0, 0, 0},
            ian::Footprint::TwoByTwo, 2,
            ian::OccupancyLayer::Volume) &&
            grid.isOccupied(
                {2, 0, 0},
                ian::OccupancyLayer::Floor),
        "release frees only the selected owner");
    grid.release(second);
    require(
        grid.occupiedCellCount() == 0,
        "releasing all owners clears occupancy");

    require(
        grid.occupy(
            first, {-4, 0, -4},
            ian::Footprint::TwoByTwo, 1,
            ian::OccupancyLayer::Floor) &&
        grid.occupy(
            first, {6, 0, 6},
            ian::Footprint::OneByOne, 1,
            ian::OccupancyLayer::Floor) &&
        grid.canOccupy(
            {-4, 0, -4}, ian::Footprint::TwoByTwo, 1,
            ian::OccupancyLayer::Floor) &&
        grid.isOccupied({6, 0, 6}, ian::OccupancyLayer::Floor) &&
        grid.occupiedCellCount() == 1U,
        "moving one owner releases its indexed old footprint");
    grid.release(first);

    const ian::EntityId rampOwner{8U, 1U};
    require(
        grid.occupyRectangle(
            rampOwner, {4, 1, 5},
            ian::ModularRampWidthCells,
            ian::ModularRampRunCells, 3,
            ian::OccupancyLayer::Volume) &&
            grid.occupiedCellCount() ==
                static_cast<std::size_t>(
                    ian::ModularRampWidthCells *
                    ian::ModularRampRunCells * 3),
        "rectangular occupancy stores a 2x4 ramp volume");
    require(
        !grid.canOccupyRectangle(
            {5, 1, 7},
            ian::ModularRampRunCells,
            ian::ModularRampWidthCells, 3,
            ian::OccupancyLayer::Volume),
        "rotated ramp rectangle detects overlap");
    grid.release(rampOwner);
}
