#pragma once

#include "core/Types.hpp"

#include <cmath>

namespace ian {

inline constexpr double CatapultPitchPivotY = 0.14931100606918335;
inline constexpr Vec3 CatapultAuthoredMuzzle{
    0.0, 0.8762649297714233, 0.6370007395744324};

[[nodiscard]] inline Vec3 catapultMuzzleWorldPosition(
    Vec3 position, double yawRadians, double pitchRadians) {
    const double relativeY =
        CatapultAuthoredMuzzle.y - CatapultPitchPivotY;
    const double cosinePitch = std::cos(pitchRadians);
    const double sinePitch = std::sin(pitchRadians);
    const double pitchedY =
        CatapultPitchPivotY + relativeY * cosinePitch -
        CatapultAuthoredMuzzle.z * sinePitch;
    const double pitchedZ =
        relativeY * sinePitch + CatapultAuthoredMuzzle.z * cosinePitch;
    return {
        position.x + pitchedZ * std::sin(yawRadians),
        position.y + pitchedY,
        position.z + pitchedZ * std::cos(yawRadians),
    };
}

} // namespace ian
