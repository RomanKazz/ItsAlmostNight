#pragma once

#include "world/WorldConfig.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace ian {

enum class ModularBuildPiece {
    Foundation,
    FloorPlatform,
    Wall,
    Ramp,
};

inline constexpr std::size_t ModularBuildPieceCount = 4;
inline constexpr int PlatformFrameWidthCells = 2;
inline constexpr int ModularStoreyHeightCells = 4;
inline constexpr int ModularRampWidthCells = 2;
inline constexpr int ModularRampRunCells = 4;
inline constexpr double PlatformFrameMaxHealth = 300.0;
inline constexpr double ModularWallMaxHealth = 220.0;
inline constexpr double ModularRampMaxHealth = 260.0;

[[nodiscard]] inline int snapPlatformFrameAxis(
    int cell) {
    const int quotient =
        cell / PlatformFrameWidthCells;
    const int remainder =
        cell % PlatformFrameWidthCells;
    return (remainder < 0 ? quotient - 1 : quotient) *
           PlatformFrameWidthCells;
}

[[nodiscard]] inline double modularStoreyHeight(
    const WorldConfig& config) {
    return static_cast<double>(ModularStoreyHeightCells) *
           config.cellSize;
}

[[nodiscard]] inline int modularStoreyHeightLevels(
    const WorldConfig& config) {
    return std::max(
        1,
        static_cast<int>(std::lround(
            modularStoreyHeight(config) /
            config.verticalGridStep)));
}

} // namespace ian
