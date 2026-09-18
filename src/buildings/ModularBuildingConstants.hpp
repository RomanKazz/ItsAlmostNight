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
inline constexpr std::array<ModularBuildPiece, 3>
    ModularBuildHotbarOrder{
        ModularBuildPiece::Foundation,
        ModularBuildPiece::FloorPlatform,
        ModularBuildPiece::Ramp,
    };

[[nodiscard]] constexpr std::optional<std::size_t>
modularBuildHotbarIndex(ModularBuildPiece piece) {
    for (std::size_t index = 0;
         index < ModularBuildHotbarOrder.size(); ++index) {
        if (ModularBuildHotbarOrder[index] == piece) {
            return index;
        }
    }
    return std::nullopt;
}
inline constexpr int PlatformFrameWidthCells = 2;
inline constexpr int ModularStoreyHeightCells = 4;
inline constexpr int ModularRampWidthCells = 2;
inline constexpr int ModularRampRunCells = 4;
// Ground platforms follow terrain closely while upper storeys remain locked
// to modularStoreyHeight(). Core/building plinths and hand-placed foundations
// must use the same ground quantization so switching build modes cannot lift
// the construction plane by a full vertical-grid unit.
inline constexpr double GroundPlatformHeightStep = 0.05;
inline constexpr std::array<bool, 4> AllPlatformSupportsVisible{
    true, true, true, true};
// World-space order: minimum X, maximum X, minimum Z, maximum Z.
inline constexpr std::array<bool, 4> AllPlatformEdgesVisible{
    true, true, true, true};
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
