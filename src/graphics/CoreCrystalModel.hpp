#pragma once

#include <raymath.h>

#include <array>
#include <cmath>

namespace ian::core_crystal_model_detail {

// core.glb: central crystal = 0, four satellites = 1..4, pedestal = 5.
// Node offsets are baked into the loaded vertices, so a shared rotation
// around the model origin moves the satellites around the central crystal.
[[nodiscard]] inline Matrix meshTransform(int meshIndex, double seconds) {
    if (meshIndex < 1 || meshIndex > 4) return MatrixIdentity();
    constexpr double RadiansPerSecond = 0.28;
    constexpr float ExtraRadius = 0.04F;
    constexpr std::array<Vector3, 4> Offsets{{
        {0, 0, -ExtraRadius}, {ExtraRadius, 0, 0},
        {0, 0, ExtraRadius}, {-ExtraRadius, 0, 0},
    }};
    const Vector3 offset = Offsets[static_cast<std::size_t>(meshIndex - 1)];
    constexpr float Lift = 0.05F;
    constexpr float BobAmplitude = 0.015F;
    constexpr double BobSeconds = 6.0;
    const double phase = static_cast<double>(meshIndex - 1) * PI * 0.5;
    const float height = Lift + BobAmplitude * static_cast<float>(
        std::sin(seconds * (2.0 * PI / BobSeconds) + phase));
    return MatrixMultiply(MatrixTranslate(offset.x, height, offset.z),
        MatrixRotateY(static_cast<float>(std::fmod(
            seconds * RadiansPerSecond, 2.0 * PI))));
}

} // namespace ian::core_crystal_model_detail
