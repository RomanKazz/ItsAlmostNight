#pragma once
#include <raylib.h>
#include <raymath.h>
namespace ian::wind_blower_model {
// Loader bakes glTF nodes: base=0, rotor=1, yaw housing=2.
[[nodiscard]] inline Matrix meshTransform(int mesh, Vector3 position,
    float baseYaw, float headYaw, float rotor, float scale=1) {
    Matrix local=MatrixIdentity();
    if (mesh==1) local=MatrixMultiply(MatrixMultiply(
        MatrixTranslate(0,-.960F,.300F),MatrixRotateZ(-rotor)),MatrixTranslate(0,.960F,-.300F));
    return MatrixMultiply(MatrixMultiply(MatrixMultiply(local,MatrixScale(scale,scale,scale)),
        MatrixRotateY(mesh==0 ? baseYaw : headYaw)),MatrixTranslate(position.x,position.y,position.z));
}
}
