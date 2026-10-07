#pragma once

#include <raymath.h>

#include <array>
#include <algorithm>
#include <cmath>

namespace ian::core_crystal_model_detail {

// core.glb: central crystal = 0, four satellites = 1..4, pedestal = 5,
// reusable metal band = 6 (hidden at level 1).
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
    const float lift = level >= 5 ? 0.16F : (level >= 3 ? 0.08F : 0.0F);
    constexpr float CrystalBottom = 0.267762F;
    const Matrix centerPose = MatrixMultiply(
        MatrixTranslate(0, -CrystalBottom, 0),
        MatrixMultiply(MatrixScale(1.0F + (growth - 1.0F) * 0.5F, growth,
                                  1.0F + (growth - 1.0F) * 0.5F),
            MatrixTranslate(0, CrystalBottom + lift, 0)));
    emit(0, centerPose, WHITE, false);
    emit(5, MatrixIdentity(), WHITE, false);
    const auto pedestalPart = [&](float radius, float height, float y, Color color, bool solid) {
        emit(solid ? 6 : 5, MatrixMultiply(MatrixScale(radius, height, radius),
                               MatrixTranslate(0, y, 0)), color, solid);
    };
    const Color metal = level >= 5 ? Color{239, 188, 77, 255} : Color{151, 181, 206, 255};
    pedestalPart(1.025F, 1.2F, 0.12F, metal, true);
    if (level >= 3) pedestalPart(0.80F, 0.55F, 0.18F, WHITE, false);
    if (level >= 4) pedestalPart(0.825F, 1.0F, 0.265F, metal, true);
    if (level >= 5) pedestalPart(0.66F, 0.45F, 0.27F, WHITE, false);
    if (level >= 6) pedestalPart(0.685F, 1.0F, 0.345F, metal, true);
    if (level == 8) pedestalPart(0.53F, 1.4F, 0.365F, {255, 217, 116, 255}, true);

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
    if (level >= 7) orbit(level == 8 ? 6 : 4, 0.22F,
        0.90F + static_cast<float>(level - 7) * 0.04F, 0.48F, -0.20);
}

[[nodiscard]] inline BoundingBox levelBounds(BoundingBox authored, int requestedLevel) {
    const int level = std::clamp(requestedLevel, 1, 8);
    if (level == 1) return authored;
    constexpr float CrystalBottom = 0.267762F;
    const float lift = level >= 5 ? 0.16F : (level >= 3 ? 0.08F : 0.0F);
    authored.min.x *= 1.025F;
    authored.min.z *= 1.025F;
    authored.max.x *= 1.025F;
    authored.max.z *= 1.025F;
    authored.max.y = CrystalBottom + (authored.max.y - CrystalBottom) *
        (1.0F + static_cast<float>(level - 1) * 0.045F) + lift;
    return authored;
}

} // namespace ian::core_crystal_model_detail
