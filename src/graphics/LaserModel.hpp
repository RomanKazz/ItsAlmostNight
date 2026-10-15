#pragma once
#include "buildings/TowerRig.hpp"
#include <raylib.h>
#include <raymath.h>
namespace ian::laser_model {
// Collider-stripped mesh order: base=0, energy=1, pitched housing=2, yaw stand=3.
[[nodiscard]] inline Matrix meshTransform(int mesh, Vector3 position,
    float baseYaw, float headYaw, float pitch, float scale=1) {
    Matrix local=MatrixIdentity();
    if (mesh==1 || mesh==2) local=MatrixMultiply(MatrixMultiply(
        MatrixTranslate(0,-static_cast<float>(LaserPitchPivotY),-static_cast<float>(LaserPitchPivotZ)),
        MatrixRotateX(pitch)),MatrixTranslate(0,static_cast<float>(LaserPitchPivotY),static_cast<float>(LaserPitchPivotZ)));
    return MatrixMultiply(MatrixMultiply(MatrixMultiply(local,MatrixScale(scale,scale,scale)),
        MatrixRotateY(mesh==0?baseYaw:headYaw)),MatrixTranslate(position.x,position.y,position.z));
}
}
