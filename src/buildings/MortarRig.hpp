#pragma once
#include "core/Types.hpp"
#include <cmath>

namespace ian {
inline constexpr double MortarPitchPivotY = 0.870;
inline constexpr double MortarNeutralElevation = 0.7853981633974483;
inline constexpr double MortarMinimumPitch = -0.17453292519943295;
inline constexpr double MortarMaximumPitch = 0.5235987755982988;
inline constexpr Vec3 MortarAuthoredMuzzle{0, 1.20941125497, -0.33941125497};

[[nodiscard]] inline Vec3 mortarMuzzleWorldPosition(Vec3 position, double yaw, double pitch) {
    const double y = MortarAuthoredMuzzle.y - MortarPitchPivotY;
    const double z = y * std::sin(pitch) + MortarAuthoredMuzzle.z * std::cos(pitch);
    return {position.x + z * std::sin(yaw),
        position.y + MortarPitchPivotY + y * std::cos(pitch) - MortarAuthoredMuzzle.z * std::sin(pitch),
        position.z + z * std::cos(yaw)};
}
}
