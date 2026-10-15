#pragma once
#include "buildings/MortarRig.hpp"
#include <raymath.h>

namespace ian::mortar_model {
// Collider-stripped order: fixed base=0, pitched barrel=1, yaw carriage=2.
[[nodiscard]] inline Matrix meshTransform(int mesh, Vector3 position,
    float baseYaw, float headYaw, float pitch, float scale=1) {
    Matrix local=MatrixIdentity();
    if(mesh==1) local=MatrixMultiply(MatrixMultiply(
        MatrixTranslate(0,-static_cast<float>(MortarPitchPivotY),0),
        MatrixRotateX(pitch)),MatrixTranslate(0,static_cast<float>(MortarPitchPivotY),0));
    return MatrixMultiply(MatrixMultiply(MatrixMultiply(local,MatrixScale(scale,scale,scale)),
        MatrixRotateY(mesh==0?baseYaw:headYaw)),MatrixTranslate(position.x,position.y,position.z));
}
}
