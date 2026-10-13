#pragma once

#include <raymath.h>

#include <array>
#include <algorithm>
#include <cmath>

namespace ian::core_crystal_model_detail {

inline constexpr int MeshCount = 13;
// core_levels.glb: original central crystal = 0, satellites = 1..4,
// authored pedestals for levels 1..8 = 5..12. One textured pedestal draw
// replaces the old stack of scaled/tinted copies of the original geometry.
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

// All passes reuse the authored meshes: no per-frame geometry or GPU allocations.
template <typename Emit>
void forEachLevelPart(int requestedLevel, double seconds, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    if (level == 1) {
        for (int mesh = 0; mesh < 6; ++mesh)
            emit(mesh, meshTransform(mesh, seconds), WHITE, false);
        return;
    }
    const float progression = static_cast<float>(level - 1);
    const float growth = 1.0F + progression * 0.045F;
    const float lift = progression * 0.035F;
    constexpr float CrystalBottom = 0.267762F;
    const Matrix centerPose = MatrixMultiply(
        MatrixTranslate(0, -CrystalBottom, 0),
        MatrixMultiply(MatrixScale(1.0F + (growth - 1.0F) * 0.5F, growth,
                                  1.0F + (growth - 1.0F) * 0.5F),
            MatrixTranslate(0, CrystalBottom + lift, 0)));
    emit(0, centerPose, WHITE, false);
    emit(level + 4, MatrixIdentity(), WHITE, false);

    // Mesh 1 already contains its GLB node offset. Recenter before placing
    // copies on the orbit, rather than rotating each crystal around itself.
    const auto orbit = [&](int count, float radius, float height, float size, double speed) {
        for (int crystal = 0; crystal < count; ++crystal) {
            const double phase = crystal * (2.0 * PI / count);
            const float angle = static_cast<float>(std::fmod(seconds * speed + phase, 2.0 * PI));
            const float bob = 0.015F * static_cast<float>(std::sin(seconds * (2.0 * PI / 6.0) + phase));
            const Matrix pose = MatrixMultiply(MatrixTranslate(0, -0.441512F, 0.25F),
                MatrixMultiply(MatrixScale(size, size, size),
                    MatrixMultiply(MatrixRotateY(angle),
                        MatrixTranslate(std::sin(angle) * radius, height + bob,
                                        -std::cos(angle) * radius))));
            emit(1, pose, WHITE, false);
        }
    };
    const int satelliteCount = level >= 6 ? 8 : (level >= 3 ? 6 : 4);
    orbit(satelliteCount, 0.29F + progression * 0.008F,
          0.491512F + lift, 1.0F + progression * 0.025F, 0.28);
}

[[nodiscard]] inline BoundingBox levelBounds(int requestedLevel) {
    const int level = std::clamp(requestedLevel, 1, 8);
    // visualBounds() covers all eight stored variants, not only the selected
    // one. Use the original dimensions and the authored animation envelope.
    // The whole pedestal family fits the same half-metre horizontal bounds.
    constexpr float CrystalBottom = 0.267762F;
    const float progression = static_cast<float>(level - 1);
    const float lift = progression * 0.035F;
    return {{-0.5F, -0.002238F, -0.5F},
            {0.5F, CrystalBottom + (0.815262F - CrystalBottom) *
                (1.0F + progression * 0.045F) + lift, 0.5F}};
}

} // namespace ian::core_crystal_model_detail
