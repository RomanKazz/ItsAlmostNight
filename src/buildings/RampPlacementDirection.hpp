#pragma once

#include "buildings/BuildGrid.hpp"
#include "buildings/ModularBuildingConstants.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace ian {

inline constexpr double RampSocketLostGraceSeconds = 0.65;
// rampSocketAimScore() is approximately tan(angle)^2. This keeps an
// edge selected within roughly 23 degrees of the crosshair.
inline constexpr double RampSocketRetentionAimScore = 0.18;
inline constexpr double RampSocketDirectionSwitchMargin = 0.28;
inline constexpr double RampSocketSafeZoneHalfCells = 0.5;

[[nodiscard]] inline GridCoord platformEdgeNeighborAnchor(
    GridCoord frameAnchor, Rotation direction) {
    switch (direction) {
    case Rotation::Deg0:
        frameAnchor.z += PlatformFrameWidthCells;
        break;
    case Rotation::Deg90:
        frameAnchor.x -= PlatformFrameWidthCells;
        break;
    case Rotation::Deg180:
        frameAnchor.z -= PlatformFrameWidthCells;
        break;
    case Rotation::Deg270:
        frameAnchor.x += PlatformFrameWidthCells;
        break;
    }
    return frameAnchor;
}

struct RampEdgeSocket {
    Rotation rotation;
    GridCoord neighborAnchor;
    Vec3 position;
};

[[nodiscard]] inline Vec3 rampSocketOutwardDirection(
    Rotation rotation) {
    switch (rotation) {
    case Rotation::Deg0:
        return {0.0, 0.0, 1.0};
    case Rotation::Deg90:
        return {-1.0, 0.0, 0.0};
    case Rotation::Deg180:
        return {0.0, 0.0, -1.0};
    case Rotation::Deg270:
        return {1.0, 0.0, 0.0};
    }
    return {};
}

[[nodiscard]] inline double rampSocketViewAlignment(
    const RampEdgeSocket& socket, Vec3 lookDirection) {
    const double horizontalLength = std::sqrt(
        lookDirection.x * lookDirection.x +
        lookDirection.z * lookDirection.z);
    if (horizontalLength <= 1e-9) {
        return -1.0;
    }
    const Vec3 outward =
        rampSocketOutwardDirection(socket.rotation);
    return
        (outward.x * lookDirection.x +
         outward.z * lookDirection.z) /
        horizontalLength;
}

[[nodiscard]] inline std::optional<Vec3>
rampSocketAimOnFloor(
    Vec3 playerPosition, Vec3 lookDirection,
    double floorHeight) {
    if (std::abs(lookDirection.y) <= 1e-9) {
        return std::nullopt;
    }
    const double distance =
        (floorHeight - playerPosition.y) /
        lookDirection.y;
    if (distance <= 0.0) {
        return std::nullopt;
    }
    return Vec3{
        playerPosition.x +
            lookDirection.x * distance,
        floorHeight,
        playerPosition.z +
            lookDirection.z * distance,
    };
}

[[nodiscard]] inline double rampSocketOutwardOffset(
    const RampEdgeSocket& socket, Vec3 point) {
    const Vec3 outward =
        rampSocketOutwardDirection(socket.rotation);
    return
        (point.x - socket.position.x) * outward.x +
        (point.z - socket.position.z) * outward.z;
}

[[nodiscard]] inline bool rampSocketContainsFloorAim(
    const RampEdgeSocket& socket, Vec3 floorAim,
    double cellSize) {
    const double halfSafeZone =
        RampSocketSafeZoneHalfCells * cellSize;
    const double outwardOffset =
        rampSocketOutwardOffset(socket, floorAim);
    return outwardOffset >= -halfSafeZone &&
           outwardOffset <= halfSafeZone;
}

[[nodiscard]] inline std::array<RampEdgeSocket, 4>
platformRampEdgeSockets(
    GridCoord frameAnchor, double floorHeight,
    double cellSize) {
    const double minimumX =
        frameAnchor.x * cellSize;
    const double minimumZ =
        frameAnchor.z * cellSize;
    const double maximumX =
        (frameAnchor.x + PlatformFrameWidthCells) *
        cellSize;
    const double maximumZ =
        (frameAnchor.z + PlatformFrameWidthCells) *
        cellSize;
    const double centerX =
        (minimumX + maximumX) * 0.5;
    const double centerZ =
        (minimumZ + maximumZ) * 0.5;
    return {{
        {
            Rotation::Deg0,
            platformEdgeNeighborAnchor(
                frameAnchor, Rotation::Deg0),
            {centerX, floorHeight, maximumZ},
        },
        {
            Rotation::Deg90,
            platformEdgeNeighborAnchor(
                frameAnchor, Rotation::Deg90),
            {minimumX, floorHeight, centerZ},
        },
        {
            Rotation::Deg180,
            platformEdgeNeighborAnchor(
                frameAnchor, Rotation::Deg180),
            {centerX, floorHeight, minimumZ},
        },
        {
            Rotation::Deg270,
            platformEdgeNeighborAnchor(
                frameAnchor, Rotation::Deg270),
            {maximumX, floorHeight, centerZ},
        },
    }};
}

[[nodiscard]] inline double rampSocketAimScore(
    const RampEdgeSocket& socket,
    Vec3 playerPosition, Vec3 lookDirection) {
    const double directionLength = std::sqrt(
        lookDirection.x * lookDirection.x +
        lookDirection.y * lookDirection.y +
        lookDirection.z * lookDirection.z);
    if (directionLength <= 1e-9) {
        return std::numeric_limits<double>::infinity();
    }
    lookDirection.x /= directionLength;
    lookDirection.y /= directionLength;
    lookDirection.z /= directionLength;
    const Vec3 toSocket{
        socket.position.x - playerPosition.x,
        socket.position.y - playerPosition.y,
        socket.position.z - playerPosition.z,
    };
    const double forwardDistance =
        toSocket.x * lookDirection.x +
        toSocket.y * lookDirection.y +
        toSocket.z * lookDirection.z;
    if (forwardDistance <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    const Vec3 rejection{
        toSocket.x -
            lookDirection.x * forwardDistance,
        toSocket.y -
            lookDirection.y * forwardDistance,
        toSocket.z -
            lookDirection.z * forwardDistance,
    };
    const double rejectionSquared =
        rejection.x * rejection.x +
        rejection.y * rejection.y +
        rejection.z * rejection.z;
    return rejectionSquared /
           std::max(
               forwardDistance * forwardDistance,
               0.25);
}

[[nodiscard]] inline std::optional<RampEdgeSocket>
nearestRampEdgeSocket(
    GridCoord frameAnchor, double floorHeight,
    double cellSize, Vec3 playerPosition,
    Vec3 lookDirection) {
    const auto sockets = platformRampEdgeSockets(
        frameAnchor, floorHeight, cellSize);
    const RampEdgeSocket* best = nullptr;
    double bestScore =
        std::numeric_limits<double>::infinity();
    for (const RampEdgeSocket& socket : sockets) {
        const double score = rampSocketAimScore(
            socket, playerPosition, lookDirection);
        if (score < bestScore) {
            best = &socket;
            bestScore = score;
        }
    }
    return best ? std::optional<RampEdgeSocket>{*best}
                : std::nullopt;
}

[[nodiscard]] inline std::optional<RampEdgeSocket>
mostViewAlignedRampEdgeSocket(
    GridCoord frameAnchor, double floorHeight,
    double cellSize, Vec3 lookDirection) {
    const auto sockets = platformRampEdgeSockets(
        frameAnchor, floorHeight, cellSize);
    const RampEdgeSocket* best = nullptr;
    double bestAlignment = -2.0;
    for (const RampEdgeSocket& socket : sockets) {
        const double alignment =
            rampSocketViewAlignment(socket, lookDirection);
        if (alignment > bestAlignment) {
            best = &socket;
            bestAlignment = alignment;
        }
    }
    return best ? std::optional<RampEdgeSocket>{*best}
                : std::nullopt;
}

} // namespace ian
