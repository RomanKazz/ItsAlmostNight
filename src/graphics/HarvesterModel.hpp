#pragma once

#include "buildings/BuildingSystem.hpp"
#include <raymath.h>

namespace ian::harvester_model {

// ModelResource bakes neutral GLB node transforms into the visible meshes.
// Wood: base/module/resource/saw. Stone: base/drill/module/resource.
// Crystal: base/module/resource. All variants use their final metre scale.
[[nodiscard]] inline Matrix meshPose(BuildingType type, int mesh, double seconds) {
    if (type == BuildingType::LumberMill && mesh == 3) {
        constexpr Vector3 pivot{-.100F, .630F, .060F};
        return MatrixMultiply(MatrixMultiply(
            MatrixTranslate(-pivot.x, -pivot.y, -pivot.z),
            MatrixRotateZ(static_cast<float>(-seconds * 4.0))),
            MatrixTranslate(pivot.x, pivot.y, pivot.z));
    }
    if (type == BuildingType::Quarry && mesh == 1) {
        constexpr Vector3 pivot{0, .610F, .030F};
        return MatrixMultiply(MatrixMultiply(
            MatrixTranslate(-pivot.x, -pivot.y, -pivot.z),
            MatrixRotateY(static_cast<float>(seconds * 2.0))),
            MatrixTranslate(pivot.x, pivot.y, pivot.z));
    }
    return MatrixIdentity();
}

} // namespace ian::harvester_model
