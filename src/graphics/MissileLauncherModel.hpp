#pragma once
#include <raylib.h>
#include <raymath.h>
namespace ian::missile_launcher_model {
inline constexpr float PitchPivotY = 1.09F;
[[nodiscard]] inline Matrix meshTransform(int mesh, Vector3 position,
    float baseYaw, float headYaw, float pitch, float scale = 1.0F) {
    Matrix local = MatrixIdentity();
    if (mesh == 1) local = MatrixMultiply(MatrixMultiply(MatrixTranslate(0, -PitchPivotY, 0),
        MatrixRotateX(pitch)), MatrixTranslate(0, PitchPivotY, 0));
    return MatrixMultiply(MatrixMultiply(MatrixMultiply(local, MatrixScale(scale, scale, scale)),
        MatrixRotateY(mesh == 0 ? baseYaw : headYaw)), MatrixTranslate(position.x, position.y, position.z));
}
}
