#pragma once

#include "buildings/TowerRig.hpp"
#include "graphics/FlamethrowerUpgradeData.hpp"
#include <algorithm>
#include <raymath.h>

namespace ian::flamethrower_model {

template <typename Emit>
void forEachUpgradePart(int requestedLevel, Emit&& emit) {
    const int level = std::clamp(requestedLevel, 1, 8);
    if (level == 1) return;
    constexpr std::array<int, 3> Roles{0, 2, 1}; // base, yaw, pitch
    const auto& parts = UpgradeMeshes[static_cast<std::size_t>(level - 2)];
    for (std::size_t index = 0; index < parts.size(); ++index) emit(parts[index], Roles[index]);
}

// Collider-stripped GLB: base=0, pitch=1, yaw/tank/hose=2.
[[nodiscard]] inline Matrix meshTransform(int mesh, Vector3 position,
    float baseYaw, float headYaw, float pitch, float scale = 1.0F) {
    Matrix local = MatrixIdentity();
    if (mesh == 1) {
        const float pivot = static_cast<float>(FlamethrowerPitchPivotY);
        local = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -pivot, 0),
            MatrixRotateX(pitch)), MatrixTranslate(0, pivot, 0));
    }
    return MatrixMultiply(MatrixMultiply(MatrixMultiply(local,
        MatrixScale(scale, scale, scale)), MatrixRotateY(mesh == 0 ? baseYaw : headYaw)),
        MatrixTranslate(position.x, position.y, position.z));
}

} // namespace ian::flamethrower_model
