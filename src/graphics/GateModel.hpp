#pragma once

#include <raylib.h>
#include <raymath.h>

#include <algorithm>

namespace ian::gate_model_detail {

inline constexpr int DoorMeshIndex = 0;
inline constexpr float DoorHingeX = -0.436F;
inline constexpr float OpenAngle = -PI * 0.5F;

[[nodiscard]] inline Matrix doorHingeTransform(float openAmount) {
    // The sign selects which side of the gate the door swings towards.
    const float clamped = std::clamp(openAmount, -1.0F, 1.0F);
    return MatrixMultiply(
        MatrixTranslate(-DoorHingeX, 0.0F, 0.0F),
        MatrixMultiply(
            MatrixRotateY(OpenAngle * clamped),
            MatrixTranslate(DoorHingeX, 0.0F, 0.0F)));
}

} // namespace ian::gate_model_detail
