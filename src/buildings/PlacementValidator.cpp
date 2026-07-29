#include "buildings/PlacementValidator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {

PlacementValidator::PlacementValidator(
    const TerrainHeightfield& terrain,
    const BuildGrid& grid)
    : terrain_(terrain), grid_(grid) {}

PlatformFramePlacement
PlacementValidator::validateGroundPlatformFrame(
    Vec3 terrainHit, Vec3 playerPosition) const {
    PlatformFramePlacement placement{
        .anchor = grid_.worldToGrid(terrainHit),
    };
    placement.anchor.x =
        snapPlatformFrameAxis(placement.anchor.x);
    placement.anchor.z =
        snapPlatformFrameAxis(placement.anchor.z);
    placement.anchor.yLevel = 0;
    const WorldConfig& config = grid_.config();
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
        placement.error =
            ModularPlacementError::OutOfBounds;
        return placement;
    }
    const Vec3 center =
        grid_.worldCenter(
            placement.anchor, Footprint::TwoByTwo);
    const double distance = std::hypot(
        center.x - playerPosition.x,
        center.z - playerPosition.z);
    const bool tooFar =
        distance > config.buildPreviewDistance;

    std::array<double, 4> groundHeights{};
    double highestGround =
        -std::numeric_limits<double>::infinity();
    for (std::size_t index = 0;
         index < corners.size(); ++index) {
        groundHeights[index] =
            terrain_.getHeight(
                corners[index].x,
                corners[index].z);
        highestGround =
            std::max(
                highestGround,
                groundHeights[index]);
    }
    placement.floorHeight =
        std::ceil(
            (highestGround +
             config.minimumGroundClearance) /
            config.verticalGridStep) *
        config.verticalGridStep;
    placement.anchor.yLevel =
        static_cast<int>(std::lround(
            placement.floorHeight /
            config.verticalGridStep));

    const bool occupied = !grid_.canOccupy(
        placement.anchor, Footprint::TwoByTwo, 1,
        OccupancyLayer::Floor);

    bool supportTooLong = false;
    bool terrainIntersection = false;
    for (std::size_t index = 0;
         index < corners.size(); ++index) {
        const double length =
            placement.floorHeight -
            groundHeights[index];
        placement.supports[index] = {
            .top = {
                corners[index].x,
                placement.floorHeight,
                corners[index].z,
            },
            .bottom = {
                corners[index].x,
                groundHeights[index],
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
