#pragma once

#include "core/SurfaceBasis.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace ian::world_transforms {

[[nodiscard]] inline Vector3 toRaylib(Vec3 value) {
    return {
        static_cast<float>(value.x),
        static_cast<float>(value.y),
        static_cast<float>(value.z),
    };
}

[[nodiscard]] inline Matrix surfaceRotation(
    Vector3 normal, float yawRadians) {
    const SurfaceBasis basis = makeSurfaceBasis(
        {normal.x, normal.y, normal.z},
        static_cast<double>(yawRadians));
    const Vector3 right = toRaylib(basis.right);
    const Vector3 up = toRaylib(basis.up);
    const Vector3 forward = toRaylib(basis.forward);
    Matrix result = MatrixIdentity();
    result.m0 = right.x;
    result.m1 = right.y;
    result.m2 = right.z;
    result.m4 = up.x;
    result.m5 = up.y;
    result.m6 = up.z;
    result.m8 = forward.x;
    result.m9 = forward.y;
    result.m10 = forward.z;
    return result;
}

[[nodiscard]] inline bool finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

[[nodiscard]] inline bool finite(Matrix value) {
    const float* values = &value.m0;
    for (int index = 0; index < 16; ++index) {
        if (!std::isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool finite(BoundingBox bounds) {
    return finite(bounds.min) && finite(bounds.max) &&
           bounds.min.x <= bounds.max.x &&
           bounds.min.y <= bounds.max.y &&
           bounds.min.z <= bounds.max.z &&
           (bounds.max.x - bounds.min.x > 0.000001F ||
            bounds.max.y - bounds.min.y > 0.000001F ||
            bounds.max.z - bounds.min.z > 0.000001F);
}

[[nodiscard]] inline BoundingBox transformBounds(
    const BoundingBox& bounds, Matrix transform) {
    if (!finite(bounds) || !finite(transform)) {
        return {};
    }
    BoundingBox result{};
    for (int corner = 0; corner < 8; ++corner) {
        const Vector3 point{
            (corner & 1) != 0 ? bounds.max.x : bounds.min.x,
            (corner & 2) != 0 ? bounds.max.y : bounds.min.y,
            (corner & 4) != 0 ? bounds.max.z : bounds.min.z,
        };
        const Vector3 transformed = Vector3Transform(point, transform);
        if (!finite(transformed)) {
            return {};
        }
        if (corner == 0) {
            result.min = transformed;
            result.max = transformed;
            continue;
        }
        result.min.x = std::min(result.min.x, transformed.x);
        result.min.y = std::min(result.min.y, transformed.y);
        result.min.z = std::min(result.min.z, transformed.z);
        result.max.x = std::max(result.max.x, transformed.x);
        result.max.y = std::max(result.max.y, transformed.y);
        result.max.z = std::max(result.max.z, transformed.z);
    }
    return finite(result) ? result : BoundingBox{};
}

inline void expandBounds(BoundingBox& destination,
                         const BoundingBox& addition,
                         bool& initialized) {
    if (!finite(addition)) {
        return;
    }
    if (!initialized) {
        destination = addition;
        initialized = true;
        return;
    }
    destination.min.x = std::min(destination.min.x, addition.min.x);
    destination.min.y = std::min(destination.min.y, addition.min.y);
    destination.min.z = std::min(destination.min.z, addition.min.z);
    destination.max.x = std::max(destination.max.x, addition.max.x);
    destination.max.y = std::max(destination.max.y, addition.max.y);
    destination.max.z = std::max(destination.max.z, addition.max.z);
}

} // namespace ian::world_transforms
