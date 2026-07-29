#pragma once

#include "buildings/BuildGrid.hpp"
#include "buildings/ModularBuildingConstants.hpp"

#include <cmath>

namespace ian {

[[nodiscard]] inline Rotation rampRotationFromDirection(
    double directionX, double directionZ) {
    if (std::abs(directionX) >
        std::abs(directionZ)) {
        return directionX >= 0.0
                   ? Rotation::Deg270
                   : Rotation::Deg90;
    }
    return directionZ >= 0.0
               ? Rotation::Deg0
               : Rotation::Deg180;
}

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

} // namespace ian
