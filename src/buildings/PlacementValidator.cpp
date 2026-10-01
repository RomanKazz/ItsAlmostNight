#include "buildings/PlacementValidator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

struct TerrainRange {
    double lowest{std::numeric_limits<double>::infinity()};
    double highest{-std::numeric_limits<double>::infinity()};
    double total{};
    int sampleCount{};

    [[nodiscard]] double average() const {
        return sampleCount > 0
            ? total / static_cast<double>(sampleCount)
            : 0.0;
    }
};

TerrainRange sampleTerrainFootprint(
    const TerrainHeightfield& terrain,
    double minimumX, double maximumX,
    double minimumZ, double maximumZ,
    bool constructionHeight = false) {
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
            const double height = constructionHeight
                ? terrain.getConstructionHeight(x, z)
                : terrain.getHeight(x, z);
            range.lowest = std::min(range.lowest, height);
            range.highest = std::max(range.highest, height);
            range.total += height;
            ++range.sampleCount;
        }
    }
    return range;
}

double snappedPlatformHeight(
    const TerrainRange& terrainRange,
    const TerrainRange& constructionRange,
    const WorldConfig& config, double heightStep) {
    constexpr double SnapEpsilon = 1e-6;
    const double safeStep = std::max(heightStep, 0.001);
    const auto snapUp = [safeStep](double height) {
        return std::ceil(
                   (height - SnapEpsilon) / safeStep) *
            safeStep;
    };
    const double requiredHeight =
        terrainRange.highest +
        config.minimumGroundClearance;

    // The authored terrain is terraced and then receives a small visual
    // surface-noise pass. On a flat terrace that noise must not define a new
    // construction plane for every grid cell. Recover the underlying terrace
    // level only when the complete footprint is close enough to it; genuine
    // slopes and transitions keep using their actual highest point.
    const double terraceStep =
        std::max(config.terrainTerraceHeight, 0.01);
    const double terraceHeight =
        std::round(
            constructionRange.average() / terraceStep) *
        terraceStep;
    const double maximumTerraceDeviation = std::max(
        std::abs(
            constructionRange.lowest - terraceHeight),
        std::abs(
            constructionRange.highest - terraceHeight));
    constexpr double FlatConstructionTolerance = 0.04;
    if (maximumTerraceDeviation <=
        FlatConstructionTolerance) {
        const double stableHeight = snapUp(
            terraceHeight +
            config.minimumGroundClearance +
            config.terrainSurfaceNoiseAmplitude);
        if (stableHeight + SnapEpsilon >=
            requiredHeight) {
            return stableHeight;
        }
    }
    return snapUp(requiredHeight);
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
    anchor.x = snapPlatformFrameWorldAxis(terrainHit.x, grid_.config().cellSize);
    anchor.z = snapPlatformFrameWorldAxis(terrainHit.z, grid_.config().cellSize);
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
    const double floorHeight = stableGroundPlatformHeight(
        anchor, config.verticalGridStep);
    return validateGroundPlatformFrameAt(
        anchor, floorHeight, playerPosition);
}

double PlacementValidator::stableGroundPlatformHeight(
    GridCoord anchor, double heightStep) const {
    const WorldConfig& config = grid_.config();
    const double minimumX =
        static_cast<double>(anchor.x) *
        config.cellSize;
    const double minimumZ =
        static_cast<double>(anchor.z) *
        config.cellSize;
    const double maximumX = minimumX +
        PlatformFrameWidthCells * config.cellSize;
    const double maximumZ = minimumZ +
        PlatformFrameWidthCells * config.cellSize;
    return snappedPlatformHeight(
        sampleTerrainFootprint(
            terrain_, minimumX, maximumX,
            minimumZ, maximumZ),
        sampleTerrainFootprint(
            terrain_, minimumX, maximumX,
            minimumZ, maximumZ, true),
        config, heightStep);
}

PlatformFramePlacement
PlacementValidator::validateGroundPlatformFrameAt(
    GridCoord anchor, double floorHeight,
    Vec3 playerPosition) const {
    return validateGroundPlatformFrameAtImpl(
        anchor, floorHeight, playerPosition, true);
}

PlatformFramePlacement
PlacementValidator::validateAutomaticBuildingFoundationAt(
    GridCoord anchor, double floorHeight,
    Vec3 playerPosition) const {
    return validateGroundPlatformFrameAtImpl(
        anchor, floorHeight, playerPosition, false);
}

PlatformFramePlacement
PlacementValidator::validateGroundPlatformFrameAtImpl(
    GridCoord anchor, double floorHeight,
    Vec3 playerPosition, bool snapAnchor) const {
    if (snapAnchor) {
        anchor.x = snapPlatformFrameAxis(anchor.x);
        anchor.z = snapPlatformFrameAxis(anchor.z);
    }
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
