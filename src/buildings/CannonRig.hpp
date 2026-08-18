#pragma once

#include "core/Types.hpp"

#include <cmath>

namespace ian {

inline constexpr double CannonPitchPivotY = 0.14931100606918335;
inline constexpr Vec3 CannonAuthoredMuzzle{
    0.0, 0.7002042531967163, -0.5782914683222771};

[[nodiscard]] inline Vec3 cannonMuzzleWorldPosition(
    Vec3 position, double yawRadians, double pitchRadians) {
    const double relativeY =
        CannonAuthoredMuzzle.y - CannonPitchPivotY;
    const double cosinePitch = std::cos(pitchRadians);
    const double sinePitch = std::sin(pitchRadians);
    const double pitchedY =
        CannonPitchPivotY + relativeY * cosinePitch -
        CannonAuthoredMuzzle.z * sinePitch;
    const double pitchedZ =
        relativeY * sinePitch + CannonAuthoredMuzzle.z * cosinePitch;
    return {
        position.x + pitchedZ * std::sin(yawRadians),
        position.y + pitchedY,
        position.z + pitchedZ * std::cos(yawRadians),
    };
}

} // namespace ian
