#pragma once

#include <raymath.h>

#include <cmath>

namespace ian::crystal_mine_model_detail {

// mine.glb has a fixed core/base in mesh 0 and three orbiting shards in mesh 1.
// Rotate the entire shard group about the central crystal, never each shard's
// own center. Keep this transform shared by rendering and interaction rays.
[[nodiscard]] inline Matrix meshTransform(int meshIndex, double seconds) {
    if (meshIndex != 1) return MatrixIdentity();
    constexpr float CenterX = 0.07254255F;
    constexpr float CenterZ = -0.00784531F;
    constexpr double RadiansPerSecond = 0.55;
    const float angle = static_cast<float>(std::fmod(seconds * RadiansPerSecond, 2.0 * PI));
    return MatrixMultiply(
        MatrixTranslate(-CenterX, 0.0F, -CenterZ),
        MatrixMultiply(MatrixRotateY(angle), MatrixTranslate(CenterX, 0.0F, CenterZ)));
}

} // namespace ian::crystal_mine_model_detail
