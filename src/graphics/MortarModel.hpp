#pragma once
#include "buildings/MortarRig.hpp"
#include "graphics/MortarUpgradeData.hpp"
#include <algorithm>
#include <raymath.h>

namespace ian::mortar_model {
template <typename Emit>
void forEachUpgradePart(int requestedLevel, Emit&& emit) {
    const int level=std::clamp(requestedLevel,1,8);
    if(level==1)return;
    constexpr std::array<int,3> Roles{0,2,1}; // fixed base, yaw carriage, pitch tube
    const auto& parts=UpgradeMeshes[static_cast<std::size_t>(level-2)];
    for(std::size_t index=0;index<parts.size();++index)emit(parts[index],Roles[index]);
}
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
