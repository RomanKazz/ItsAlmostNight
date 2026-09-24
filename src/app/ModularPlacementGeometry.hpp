#pragma once

#include "buildings/FoundationSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"

namespace ian::modular_placement_detail {

inline Vec3 platformFrameCenter(
    const PlatformFrameInstance& frame,
    double cellSize) {
    return {
        (frame.anchor.x + 1.0) * cellSize,
        frame.floorHeight,
        (frame.anchor.z + 1.0) * cellSize,
    };
}

inline Vec3 wallCenter(
    const WallInstance& wall, double cellSize) {
    return {
        (wall.anchor.x + 0.5) * cellSize,
        wall.bottomHeight,
        (wall.anchor.z + 0.5) * cellSize,
    };
}

inline Vec3 rampCenter(
    const RampInstance& ramp, double cellSize) {
    const bool alongZ =
        ramp.rotation == Rotation::Deg0 ||
        ramp.rotation == Rotation::Deg180;
    const int widthCells =
        alongZ ? ModularRampWidthCells
               : ModularRampRunCells;
    const int depthCells =
        alongZ ? ModularRampRunCells
               : ModularRampWidthCells;
    return {
        (ramp.anchor.x + widthCells * 0.5) *
            cellSize,
        ramp.bottomHeight,
        (ramp.anchor.z + depthCells * 0.5) *
            cellSize,
    };
}

inline bool isPlatformBuildPiece(ModularBuildPiece piece) {
    return piece == ModularBuildPiece::Foundation ||
           piece == ModularBuildPiece::FloorPlatform;
}

} // namespace ian::modular_placement_detail
