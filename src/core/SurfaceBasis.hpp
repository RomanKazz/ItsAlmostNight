#pragma once

#include "core/Types.hpp"

#include <cmath>

namespace ian {

// Deterministic right-handed frame for an object standing on a surface.
// yaw 0 points along +Z, matching gameplay movement.
struct SurfaceBasis {
    Vec3 right{1.0, 0.0, 0.0};
    Vec3 up{0.0, 1.0, 0.0};
    Vec3 forward{0.0, 0.0, 1.0};
};

[[nodiscard]] inline double surfaceDot(Vec3 left, Vec3 right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] inline Vec3 surfaceCross(Vec3 left, Vec3 right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

[[nodiscard]] inline double surfaceLengthSquared(Vec3 value) {
    return surfaceDot(value, value);
}

[[nodiscard]] inline Vec3 surfaceNormalize(
    Vec3 value, Vec3 fallback = {0.0, 1.0, 0.0}) {
    const double lengthSquared = surfaceLengthSquared(value);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12) {
        return fallback;
    }
    const double inverseLength = 1.0 / std::sqrt(lengthSquared);
    return {
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength,
    };
}

[[nodiscard]] inline SurfaceBasis makeSurfaceBasis(
    Vec3 normal, double yawRadians) {
    SurfaceBasis basis{};
    basis.up = surfaceNormalize(normal);
    if (!std::isfinite(yawRadians)) {
        yawRadians = 0.0;
    }

    const Vec3 authoredForward{
        std::sin(yawRadians), 0.0, std::cos(yawRadians)};
    const double forwardAlongNormal =
        surfaceDot(authoredForward, basis.up);
    Vec3 projectedForward{
        authoredForward.x - basis.up.x * forwardAlongNormal,
        authoredForward.y - basis.up.y * forwardAlongNormal,
        authoredForward.z - basis.up.z * forwardAlongNormal,
    };
    if (surfaceLengthSquared(projectedForward) <= 1.0e-12) {
        const Vec3 fallbackAxis =
            std::abs(basis.up.y) < 0.85
                ? Vec3{0.0, 1.0, 0.0}
                : Vec3{1.0, 0.0, 0.0};
        const double fallbackAlongNormal =
            surfaceDot(fallbackAxis, basis.up);
        projectedForward = {
            fallbackAxis.x - basis.up.x * fallbackAlongNormal,
            fallbackAxis.y - basis.up.y * fallbackAlongNormal,
            fallbackAxis.z - basis.up.z * fallbackAlongNormal,
        };
    }
    basis.forward = surfaceNormalize(projectedForward, {0.0, 0.0, 1.0});
    basis.right = surfaceNormalize(
        surfaceCross(basis.up, basis.forward), {1.0, 0.0, 0.0});
    basis.forward = surfaceNormalize(
        surfaceCross(basis.right, basis.up), basis.forward);
    return basis;
}

} // namespace ian
