#pragma once

#include "buildings/BuildingOrientation.hpp"

#include <algorithm>
#include <cmath>

namespace ian {

// Critically damped aiming retains velocity across target changes. Its exact
// update accelerates from rest and slows near the goal without oscillation.
[[nodiscard]] inline double advanceDefenseAim(
    double current, double target, double& velocity, double deltaSeconds,
    double response, double maximumSpeed, bool wrapped) {
    if (deltaSeconds <= 0.0) return current;
    const double offset = wrapped ? wrapBuildingAngle(current - target)
                                  : current - target;
    const double decay = std::exp(-response * deltaSeconds);
    const double tangent = velocity + response * offset;
    const double nextOffset = (offset + tangent * deltaSeconds) * decay;
    velocity = std::clamp(
        (velocity - response * tangent * deltaSeconds) * decay,
        -maximumSpeed, maximumSpeed);
    const double next = current + std::clamp(
        nextOffset - offset, -maximumSpeed * deltaSeconds,
        maximumSpeed * deltaSeconds);
    return wrapped ? wrapBuildingAngle(next) : next;
}

} // namespace ian
