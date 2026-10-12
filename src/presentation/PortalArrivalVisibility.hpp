#pragma once

#include "enemies/EnemySystem.hpp"

#include <cmath>

namespace ian::presentation {
inline bool portalArrivalVisible(const EnemyInstance& enemy,Vec3 observer) {
    if(!enemy.arrivalPortal) return true;
    const auto side=[&](Vec3 point) {
        return (point.x-enemy.arrivalPortal->x)*std::sin(enemy.portalExitYaw)+
            (point.z-enemy.arrivalPortal->z)*std::cos(enemy.portalExitYaw);
    };
    // Spawn formations may extend behind the gate. They become visible only
    // after crossing its forward plane; the rear observer never sees the
    // birth animation through/around the back of the frame.
    return side(enemy.position)>=-.02 && (enemy.portalArrivalRemaining<=0 || side(observer)>=0);
}
} // namespace ian::presentation
