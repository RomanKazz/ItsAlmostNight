#pragma once

#include "world/WorldConfig.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <optional>

namespace ian {

enum class ModularBuildPiece {
    Foundation,
    FloorPlatform,
    Wall,
    Ramp,
};

inline constexpr std::size_t ModularBuildPieceCount = 4;
inline constexpr std::array<ModularBuildPiece, 2>
    ModularBuildHotbarOrder{
        ModularBuildPiece::Foundation,
        ModularBuildPiece::Ramp,
    };

[[nodiscard]] constexpr std::optional<std::size_t>
modularBuildHotbarIndex(ModularBuildPiece piece) {
    if (piece == ModularBuildPiece::FloorPlatform) {
        piece = ModularBuildPiece::Foundation;
    }
    for (std::size_t index = 0;
         index < ModularBuildHotbarOrder.size(); ++index) {
        if (ModularBuildHotbarOrder[index] == piece) {
            return index;
        }
    }
    return std::nullopt;
}
inline constexpr int PlatformFrameWidthCells = 2;
// Placement advances one cell even when a piece occupies several cells.
inline constexpr int PlacementGridStepCells = 1;
inline constexpr int ModularStoreyHeightCells = 4;
inline constexpr int ModularRampWidthCells = 2;
inline constexpr int ModularRampRunCells = 4;
// Ground platforms use broad construction planes.  Five-centimetre height
// steps made one visually flat patch of terrain produce slightly mismatched
// decks, especially when separate drag rows met later.  A quarter-metre band
// absorbs those small terrain changes while still allowing real slopes to
// move foundations onto a new level.  Upper storeys remain locked to
// modularStoreyHeight().
inline constexpr double GroundPlatformHeightStep = 0.25;
inline constexpr std::array<bool, 4> AllPlatformSupportsVisible{
    true, true, true, true};
// World-space order: minimum X, maximum X, minimum Z, maximum Z.
// Each edge has one segment per grid cell, from its minimum coordinate.
using PlatformEdgeVisibilityMask =
    std::array<std::array<bool, PlatformFrameWidthCells>, 4>;
inline constexpr PlatformEdgeVisibilityMask AllPlatformEdgesVisible = [] {
    PlatformEdgeVisibilityMask mask{};
    for (auto& edge : mask) edge.fill(true);
    return mask;
}();
inline constexpr double PlatformFrameMaxHealth = 300.0;
inline constexpr double ModularWallMaxHealth = 220.0;
inline constexpr double ModularRampMaxHealth = 260.0;

[[nodiscard]] inline int snapPlatformFrameAxis(
    int cell) {
    const int quotient =
        cell / PlacementGridStepCells;
    const int remainder =
        cell % PlacementGridStepCells;
    return (remainder < 0 ? quotient - 1 : quotient) *
           PlacementGridStepCells;
}

// Snap manual foundation aim to the nearest anchor. At an exact midpoint,
// retain the lower anchor so synthetic cell-center hits still round-trip.
[[nodiscard]] inline int snapPlatformFrameWorldAxis(
    double coordinate, double cellSize) {
    const double step = cellSize * PlacementGridStepCells;
    return static_cast<int>(std::ceil(coordinate / step - 0.5)) *
           PlacementGridStepCells;
}

// Convert aim at a frame's visible center into its lower-corner anchor.
[[nodiscard]] inline int platformAnchorAxisFromCenter(
    double coordinate, double cellSize) {
    const double halfWidth = PlatformFrameWidthCells * cellSize * 0.5;
    return snapPlatformFrameWorldAxis(coordinate - halfWidth, cellSize);
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
