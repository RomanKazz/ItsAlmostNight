#pragma once

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian::camera_culling {

// Extract from the actual render matrices, including camera pitch and the
// current target's aspect ratio. Planes face into the OpenGL clip volume.
struct Frustum {
    std::array<Vector4, 6> planes{};

    explicit Frustum(const Matrix& m) : planes{{
        {m.m3 + m.m0, m.m7 + m.m4, m.m11 + m.m8, m.m15 + m.m12},
        {m.m3 - m.m0, m.m7 - m.m4, m.m11 - m.m8, m.m15 - m.m12},
        {m.m3 + m.m1, m.m7 + m.m5, m.m11 + m.m9, m.m15 + m.m13},
        {m.m3 - m.m1, m.m7 - m.m5, m.m11 - m.m9, m.m15 - m.m13},
        {m.m3 + m.m2, m.m7 + m.m6, m.m11 + m.m10, m.m15 + m.m14},
        {m.m3 - m.m2, m.m7 - m.m6, m.m11 - m.m10, m.m15 - m.m14},
    }} {
        for (auto& plane : planes) {
            const float length = std::sqrt(
                plane.x*plane.x + plane.y*plane.y + plane.z*plane.z);
            if (!std::isfinite(length) || length <= 0.000001F) {
                plane = {};
                continue;
            }
            plane.x /= length;
            plane.y /= length;
            plane.z /= length;
            plane.w /= length;
        }
    }

    [[nodiscard]] bool intersects(
        const BoundingBox& bounds, float padding = 0.0F) const {
        for (const auto& plane : planes) {
            const Vector3 point{
                plane.x >= 0.0F ? bounds.max.x : bounds.min.x,
                plane.y >= 0.0F ? bounds.max.y : bounds.min.y,
                plane.z >= 0.0F ? bounds.max.z : bounds.min.z,
            };
            if (plane.x*point.x + plane.y*point.y + plane.z*point.z +
                    plane.w < -padding) {
                return false;
            }
        }
        return true;
    }
};

struct HorizontalView {
    Vector2 forward{0.0F, -1.0F};
    float fovTangent{1.0F};
};

[[nodiscard]] inline HorizontalView horizontalView(
    const Camera3D& camera) {
    const float x = camera.target.x - camera.position.x;
    const float z = camera.target.z - camera.position.z;
    const float lengthSquared = x * x + z * z;
    Vector2 forward{0.0F, -1.0F};
    if (std::isfinite(lengthSquared) && lengthSquared > 0.000001F) {
        const float inverseLength = 1.0F / std::sqrt(lengthSquared);
        forward = {x * inverseLength, z * inverseLength};
    }
    const float aspect =
        static_cast<float>(std::max(GetRenderWidth(), 1)) /
        static_cast<float>(std::max(GetRenderHeight(), 1));
    const float fovTangent = std::tan(
        std::clamp(camera.fovy, 1.0F, 175.0F) * DEG2RAD * 0.5F) *
        aspect;
    return {.forward = forward, .fovTangent = fovTangent};
}

[[nodiscard]] inline bool visibleInHorizontalCone(
    float offsetX, float offsetZ, float distanceSquared,
    const HorizontalView& view, float objectRadius,
    float nearPadding = 2.0F) {
    const float nearRadius = objectRadius + nearPadding;
    if (distanceSquared <= nearRadius * nearRadius) {
        return true;
    }
    const float forwardDistance =
        offsetX * view.forward.x + offsetZ * view.forward.y;
    if (forwardDistance < -objectRadius) {
        return false;
    }
    const float sideDistance = std::abs(
        offsetX * view.forward.y - offsetZ * view.forward.x);
    return sideDistance <=
        std::max(forwardDistance, 0.0F) * view.fovTangent +
            objectRadius;
}

[[nodiscard]] inline bool visibleInHorizontalRange(
    float offsetX, float offsetZ, const HorizontalView& view,
    float maximumDistance, float objectRadius,
    float nearPadding = 2.0F) {
    const float safeDistance = std::max(maximumDistance, 0.0F);
    const float distanceSquared =
        offsetX * offsetX + offsetZ * offsetZ;
    if (!std::isfinite(distanceSquared) ||
        distanceSquared > safeDistance * safeDistance) {
        return false;
    }
    return visibleInHorizontalCone(
        offsetX, offsetZ, distanceSquared, view,
        objectRadius, nearPadding);
}

} // namespace ian::camera_culling
