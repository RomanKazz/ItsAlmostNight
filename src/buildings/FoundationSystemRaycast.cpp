#include "buildings/FoundationSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

struct SelectionBox {
    EntityId id;
    Vec3 minimum;
    Vec3 maximum;
};

std::optional<double> rayBoxDistance(
    Vec3 origin, Vec3 direction,
    const SelectionBox& box,
    double maximumDistance) {
    double nearDistance = 0.0;
    double farDistance = maximumDistance;
    const std::array<double, 3> origins{
        origin.x, origin.y, origin.z};
    const std::array<double, 3> directions{
        direction.x, direction.y, direction.z};
    const std::array<double, 3> minimums{
        box.minimum.x, box.minimum.y, box.minimum.z};
    const std::array<double, 3> maximums{
        box.maximum.x, box.maximum.y, box.maximum.z};
    for (std::size_t axis = 0; axis < 3U; ++axis) {
        if (std::abs(directions[axis]) < 1e-9) {
            if (origins[axis] < minimums[axis] ||
                origins[axis] > maximums[axis]) {
                return std::nullopt;
            }
            continue;
        }
        const double inverse = 1.0 / directions[axis];
        double first =
            (minimums[axis] - origins[axis]) * inverse;
        double second =
            (maximums[axis] - origins[axis]) * inverse;
        if (first > second) {
            std::swap(first, second);
        }
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) {
            return std::nullopt;
        }
    }
    return nearDistance <= maximumDistance
               ? std::optional<double>{nearDistance}
               : std::nullopt;
}

} // namespace

std::optional<EntityId> FoundationSystem::raycast(
    Vec3 origin, Vec3 direction,
    double maximumDistance) const {
    if (maximumDistance <= 0.0) {
        return std::nullopt;
    }
    const double directionLength = std::sqrt(
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z);
    if (directionLength <= 1e-9) {
        return std::nullopt;
    }
    direction.x /= directionLength;
    direction.y /= directionLength;
    direction.z /= directionLength;

    std::optional<EntityId> closest;
    double closestDistance = maximumDistance;
    const auto consider =
        [&](const SelectionBox& box) {
            const auto distance = rayBoxDistance(
                origin, direction, box,
                closestDistance);
            if (distance &&
                *distance <= closestDistance) {
                closestDistance = *distance;
                closest = box.id;
            }
        };
    const double cellSize = grid_.config().cellSize;
    const auto frameBox =
        [cellSize](EntityId id, GridCoord anchor,
                   double height) {
            return SelectionBox{
                .id = id,
                .minimum = {
                    anchor.x * cellSize,
                    height - 0.20,
                    anchor.z * cellSize,
                },
                .maximum = {
                    (anchor.x + PlatformFrameWidthCells) *
                        cellSize,
                    height + 0.06,
                    (anchor.z + PlatformFrameWidthCells) *
                        cellSize,
                },
            };
        };
    for (const PlatformFrameInstance& frame :
         platformFrames_) {
        consider(frameBox(
            frame.id, frame.anchor,
            frame.floorHeight));
        for (const FoundationSupport& support :
             frame.supports) {
            constexpr double HalfSupportWidth = 0.10;
            consider({
                .id = frame.id,
                .minimum = {
                    support.top.x - HalfSupportWidth,
                    support.bottom.y,
                    support.top.z - HalfSupportWidth,
                },
                .maximum = {
                    support.top.x + HalfSupportWidth,
                    support.top.y,
                    support.top.z + HalfSupportWidth,
                },
            });
        }
    }
    for (const WallInstance& wall : walls_) {
        const bool alongX =
            wall.rotation == Rotation::Deg0 ||
            wall.rotation == Rotation::Deg180;
        const double centerX =
            (wall.anchor.x + 0.5) * cellSize;
        const double centerZ =
            (wall.anchor.z + 0.5) * cellSize;
        const double halfX =
            alongX ? cellSize * 0.5 : 0.09;
        const double halfZ =
            alongX ? 0.09 : cellSize * 0.5;
        consider({
            .id = wall.id,
            .minimum = {
                centerX - halfX, wall.bottomHeight,
                centerZ - halfZ},
            .maximum = {
                centerX + halfX, wall.topHeight,
                centerZ + halfZ},
        });
    }
    for (const RampInstance& ramp : ramps_) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const int widthCells =
            alongZ ? ModularRampWidthCells
                   : ModularRampRunCells;
        const int depthCells =
            alongZ ? ModularRampRunCells
                   : ModularRampWidthCells;
        consider({
            .id = ramp.id,
            .minimum = {
                ramp.anchor.x * cellSize,
                ramp.bottomHeight - 0.10,
                ramp.anchor.z * cellSize},
            .maximum = {
                (ramp.anchor.x + widthCells) *
                    cellSize,
                ramp.topHeight + 0.10,
                (ramp.anchor.z + depthCells) *
                    cellSize},
        });
    }
    return closest;
}

std::optional<Vec3>
FoundationSystem::raycastPlatformSurface(
    Vec3 origin, Vec3 direction,
    double maximumDistance) const {
    if (maximumDistance <= 0.0) {
        return std::nullopt;
    }
    const double directionLength = std::sqrt(
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z);
    if (directionLength <= 1e-9) {
        return std::nullopt;
    }
    direction.x /= directionLength;
    direction.y /= directionLength;
    direction.z /= directionLength;

    const double cellSize = grid_.config().cellSize;
    const PlatformFrameInstance* closestFrame =
        nullptr;
    double closestDistance = maximumDistance;
    for (const PlatformFrameInstance& frame :
         platformFrames_) {
        const double minimumX =
            frame.anchor.x * cellSize;
        const double maximumX =
            (frame.anchor.x +
             PlatformFrameWidthCells) *
            cellSize;
        const double minimumZ =
            frame.anchor.z * cellSize;
        const double maximumZ =
            (frame.anchor.z +
             PlatformFrameWidthCells) *
            cellSize;
        const auto considerDistance =
            [&](std::optional<double> distance) {
                if (distance &&
                    *distance <= closestDistance) {
                    closestDistance = *distance;
                    closestFrame = &frame;
                }
            };
        considerDistance(rayBoxDistance(
            origin, direction,
            {
                .id = frame.id,
                .minimum = {
                    minimumX,
                    frame.floorHeight - 0.20,
                    minimumZ,
                },
                .maximum = {
                    maximumX,
                    frame.floorHeight + 0.06,
                    maximumZ,
                },
            },
            closestDistance));
        for (const FoundationSupport& support :
             frame.supports) {
            constexpr double HalfSupportWidth =
                0.10;
            considerDistance(rayBoxDistance(
                origin, direction,
                {
                    .id = frame.id,
                    .minimum = {
                        support.top.x -
                            HalfSupportWidth,
                        support.bottom.y,
                        support.top.z -
                            HalfSupportWidth,
                    },
                    .maximum = {
                        support.top.x +
                            HalfSupportWidth,
                        support.top.y,
                        support.top.z +
                            HalfSupportWidth,
                    },
                },
                closestDistance));
        }
    }
    if (!closestFrame) {
        return std::nullopt;
    }
    const double minimumX =
        closestFrame->anchor.x * cellSize;
    const double maximumX =
        (closestFrame->anchor.x +
         PlatformFrameWidthCells) *
        cellSize;
    const double minimumZ =
        closestFrame->anchor.z * cellSize;
    const double maximumZ =
        (closestFrame->anchor.z +
         PlatformFrameWidthCells) *
        cellSize;
    return Vec3{
        std::clamp(
            origin.x +
                direction.x * closestDistance,
            minimumX, maximumX),
        closestFrame->floorHeight,
        std::clamp(
            origin.z +
                direction.z * closestDistance,
            minimumZ, maximumZ),
    };
}

} // namespace ian
