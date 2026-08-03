#include "buildings/PlacementValidator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

struct TerrainRange {
    double lowest{std::numeric_limits<double>::infinity()};
    double highest{-std::numeric_limits<double>::infinity()};
};

TerrainRange sampleTerrainFootprint(
    const TerrainHeightfield& terrain,
    double minimumX, double maximumX,
    double minimumZ, double maximumZ) {
    constexpr int SamplesPerAxis = 5;
    TerrainRange range;
    for (int zIndex = 0; zIndex < SamplesPerAxis; ++zIndex) {
        const double zAmount =
            static_cast<double>(zIndex) /
            static_cast<double>(SamplesPerAxis - 1);
        const double z = minimumZ +
            (maximumZ - minimumZ) * zAmount;
        for (int xIndex = 0; xIndex < SamplesPerAxis; ++xIndex) {
            const double xAmount =
                static_cast<double>(xIndex) /
                static_cast<double>(SamplesPerAxis - 1);
            const double x = minimumX +
                (maximumX - minimumX) * xAmount;
            const double height = terrain.getHeight(x, z);
            range.lowest = std::min(range.lowest, height);
            range.highest = std::max(range.highest, height);
        }
    }
    return range;
}

} // namespace

PlacementValidator::PlacementValidator(
    const TerrainHeightfield& terrain,
    const BuildGrid& grid)
    : terrain_(terrain), grid_(grid) {}

PlatformFramePlacement
PlacementValidator::validateGroundPlatformFrame(
    Vec3 terrainHit, Vec3 playerPosition) const {
    GridCoord anchor = grid_.worldToGrid(terrainHit);
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);
    const WorldConfig& config = grid_.config();
    const double minimumX =
        static_cast<double>(anchor.x) *
        config.cellSize;
    const double minimumZ =
        static_cast<double>(anchor.z) *
        config.cellSize;
    const double maximumX =
        minimumX +
        PlatformFrameWidthCells * config.cellSize;
    const double maximumZ =
        minimumZ +
        PlatformFrameWidthCells * config.cellSize;
    const std::array<Vec3, 4> corners{{
        {minimumX, 0.0, minimumZ},
        {maximumX, 0.0, minimumZ},
        {minimumX, 0.0, maximumZ},
        {maximumX, 0.0, maximumZ},
    }};
    if (std::any_of(
            corners.begin(), corners.end(),
            [this](Vec3 corner) {
                return !terrain_.isInside(
                    corner.x, corner.z);
            })) {
        return PlatformFramePlacement{
            .error =
                ModularPlacementError::OutOfBounds,
            .anchor = anchor,
        };
    }

    const TerrainRange terrainRange =
        sampleTerrainFootprint(
            terrain_, minimumX, maximumX,
            minimumZ, maximumZ);
    if (terrainRange.highest - terrainRange.lowest >
        config.maximumFoundationHeightDifference) {
        return PlatformFramePlacement{
            .error = ModularPlacementError::TerrainIntersection,
            .anchor = anchor,
        };
    }
    const double floorHeight =
        std::ceil(
            (terrainRange.highest +
             config.minimumGroundClearance) /
            config.verticalGridStep) *
        config.verticalGridStep;
    return validateGroundPlatformFrameAt(
        anchor, floorHeight, playerPosition);
}

PlatformFramePlacement
PlacementValidator::validateGroundPlatformFrameAt(
    GridCoord anchor, double floorHeight,
    Vec3 playerPosition) const {
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);
    PlatformFramePlacement placement{
        .anchor = anchor,
        .floorHeight = floorHeight,
        .storey = 0,
    };
    const WorldConfig& config = grid_.config();
    placement.anchor.yLevel =
        static_cast<int>(std::lround(
            placement.floorHeight /
            config.verticalGridStep));
    const double minimumX =
        static_cast<double>(placement.anchor.x) *
        config.cellSize;
    const double minimumZ =
        static_cast<double>(placement.anchor.z) *
        config.cellSize;
    const double maximumX =
        minimumX +
        PlatformFrameWidthCells * config.cellSize;
    const double maximumZ =
        minimumZ +
        PlatformFrameWidthCells * config.cellSize;
    const std::array<Vec3, 4> corners{{
        {minimumX, floorHeight, minimumZ},
        {maximumX, floorHeight, minimumZ},
        {minimumX, floorHeight, maximumZ},
        {maximumX, floorHeight, maximumZ},
    }};

    if (std::any_of(
            corners.begin(), corners.end(),
            [this](Vec3 corner) {
                return !terrain_.isInside(
                    corner.x, corner.z);
            })) {
        placement.error =
            ModularPlacementError::OutOfBounds;
        return placement;
    }
    const Vec3 center =
        grid_.worldCenter(
            placement.anchor, Footprint::TwoByTwo);
    const bool tooFar =
        std::hypot(
            center.x - playerPosition.x,
            center.z - playerPosition.z) >
        config.buildPreviewDistance;

    const bool occupied = !grid_.canOccupy(
        placement.anchor, Footprint::TwoByTwo, 1,
        OccupancyLayer::Floor);

    const TerrainRange terrainRange =
        sampleTerrainFootprint(
            terrain_, minimumX, maximumX,
            minimumZ, maximumZ);
    const bool terrainTooSteep =
        terrainRange.highest - terrainRange.lowest >
        config.maximumFoundationHeightDifference;

    bool supportTooLong = false;
    bool terrainIntersection =
        terrainRange.highest >
        placement.floorHeight -
                config.minimumGroundClearance +
            1e-6;
    for (std::size_t index = 0;
         index < corners.size(); ++index) {
        const double groundHeight =
            terrain_.getHeight(
                corners[index].x,
                corners[index].z);
        const double length =
            placement.floorHeight -
            groundHeight;
        placement.supports[index] = {
            .top = {
                corners[index].x,
                placement.floorHeight,
                corners[index].z,
            },
            .bottom = {
                corners[index].x,
                groundHeight,
                corners[index].z,
            },
            .length = length,
        };
        if (length >
            config.maxWoodSupportLength) {
            supportTooLong = true;
        }
        if (length <
            config.minimumGroundClearance - 1e-6) {
            terrainIntersection = true;
        }
    }
    if (tooFar) {
        placement.error =
            ModularPlacementError::TooFar;
    } else if (occupied) {
        placement.error =
            ModularPlacementError::Occupied;
    } else if (terrainTooSteep) {
        placement.error =
            ModularPlacementError::TerrainIntersection;
    } else if (supportTooLong) {
        placement.error =
            ModularPlacementError::SupportTooLong;
    } else if (terrainIntersection) {
        placement.error =
            ModularPlacementError::
                TerrainIntersection;
    }
    return placement;
}

} // namespace ian
