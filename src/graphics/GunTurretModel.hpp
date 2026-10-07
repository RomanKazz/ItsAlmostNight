#pragma once

#include <raymath.h>

#include <algorithm>

namespace ian::gun_turret_model_detail {

// After import: base = 0, yaw/recoil barrel = 1, reusable armor = 2,
// muzzle collar = 3. The two authored muzzle sockets remain unchanged.
template <typename Emit>
void forEachUpgradePart(int requestedLevel, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    if (level == 1) return;
    const Color steel{133, 159, 181, 255};
    const Color darkSteel{70, 86, 103, 255};
    const Color trim = level >= 5 ? Color{238, 185, 72, 255} : steel;
    const auto armor = [&](Vector3 position, Vector3 size, Color color, bool rotating = true) {
        emit(2, MatrixMultiply(MatrixScale(size.x, size.y, size.z),
                              MatrixTranslate(position.x, position.y, position.z)), color, rotating);
    };
    armor({0, 0.24F, 0}, {0.62F, 0.055F, 0.62F}, trim, false);
    if (level >= 3) {
        for (float side : {-1.0F, 1.0F})
            armor({side * 0.43F, 0.56F, 0.08F}, {0.095F, 0.40F, 0.47F}, darkSteel);
    }
    if (level >= 4) armor({0, 1.00F, 0.05F}, {0.72F, 0.08F, 0.57F}, darkSteel);
    if (level >= 5) {
        for (float side : {-1.0F, 1.0F})
            armor({side * 0.437F, 0.58F, 0.09F}, {0.10F, 0.055F, 0.50F}, trim);
    }
    const auto collars = [&](float z) {
        for (float x : {-0.140388F, 0.140388F}) {
            const Matrix pose = MatrixMultiply(MatrixScale(0.32F, 0.04F, 0.32F),
                MatrixMultiply(MatrixRotateX(-PI * 0.5F),
                    MatrixTranslate(x, 0.685445F, z)));
            emit(3, pose, trim, true);
        }
    };
    if (level >= 6) collars(-0.568F);
    if (level >= 7) {
        for (float x : {-0.22F, 0.0F, 0.22F})
            armor({x, 1.08F, 0.15F}, {0.095F, 0.10F, 0.38F}, steel);
    }
    if (level == 8) {
        collars(-0.46F);
        for (float side : {-1.0F, 1.0F}) {
            armor({side * 0.30F, 1.005F, 0.05F}, {0.075F, 0.085F, 0.59F}, trim);
            armor({side * 0.441F, 0.62F, 0.265F}, {0.10F, 0.31F, 0.065F}, trim);
        }
    }
}

inline constexpr BoundingBox UpgradePartBounds{{-0.5F, 0, -0.5F}, {0.5F, 1.0F, 0.5F}};

[[nodiscard]] inline float height(int requestedLevel) {
    const int level = std::clamp(requestedLevel, 1, 8);
    return level >= 7 ? 1.18F : (level >= 4 ? 1.09F : 1.06F);
}

} // namespace ian::gun_turret_model_detail
