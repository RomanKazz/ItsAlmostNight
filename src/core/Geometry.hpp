#pragma once

#include "core/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace ian::geometry {

[[nodiscard]] constexpr Vec3 add(Vec3 left, Vec3 right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] constexpr Vec3 subtract(Vec3 left, Vec3 right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] constexpr Vec3 scale(Vec3 value, double factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] constexpr double dot(Vec3 left, Vec3 right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] constexpr Vec3 cross(Vec3 left, Vec3 right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

[[nodiscard]] constexpr Vec3 lerpClamped(
    Vec3 from, Vec3 to, double amount) {
    const double blend = std::clamp(amount, 0.0, 1.0);
    return add(from, scale(subtract(to, from), blend));
}

[[nodiscard]] constexpr double lengthSquared(Vec3 value) {
    return dot(value, value);
}

[[nodiscard]] constexpr double distanceSquared(Vec3 left, Vec3 right) {
    return lengthSquared(subtract(left, right));
}

[[nodiscard]] inline double length(Vec3 value) {
    return std::sqrt(lengthSquared(value));
}

[[nodiscard]] inline Vec3 normalizedOr(
    Vec3 value, Vec3 fallback = {0.0, 0.0, -1.0},
    double epsilon = 1e-9) {
    const double magnitude = length(value);
    return magnitude > epsilon ? scale(value, 1.0 / magnitude)
                               : fallback;
}

// Direction is expected to be normalized. The returned value is the nearest
// non-negative distance along the ray, including zero when the origin is
// inside the sphere.
[[nodiscard]] inline std::optional<double> raySphereDistance(
    Vec3 origin, Vec3 direction, Vec3 center, double radius) {
    const Vec3 offset = subtract(origin, center);
    const double halfB = dot(offset, direction);
    const double c = lengthSquared(offset) - radius * radius;
    const double discriminant = halfB * halfB - c;
    if (discriminant < 0.0) {
        return std::nullopt;
    }
    const double root = std::sqrt(discriminant);
    const double nearDistance = -halfB - root;
    if (nearDistance >= 0.0) {
        return nearDistance;
    }
    const double farDistance = -halfB + root;
    return farDistance >= 0.0
        ? std::optional<double>{farDistance}
        : std::nullopt;
}

[[nodiscard]] inline std::optional<double> rayVerticalCapsuleDistance(
    Vec3 origin, Vec3 direction, Vec3 center,
    double radius, double segmentHalfHeight) {
    double closest = std::numeric_limits<double>::max();
    const auto accept = [&closest](std::optional<double> distance) {
        if (distance && *distance >= 0.0 && *distance < closest) {
            closest = *distance;
        }
    };

    const double offsetX = origin.x - center.x;
    const double offsetZ = origin.z - center.z;
    const double horizontalDirectionSquared =
        direction.x * direction.x + direction.z * direction.z;
    const double horizontalOffsetSquared =
        offsetX * offsetX + offsetZ * offsetZ;
    const double minimumY = center.y - segmentHalfHeight;
    const double maximumY = center.y + segmentHalfHeight;
    if (horizontalOffsetSquared <= radius * radius &&
        origin.y >= minimumY && origin.y <= maximumY) {
        closest = 0.0;
    }
    if (horizontalDirectionSquared > 1e-12) {
        const double halfB =
            offsetX * direction.x + offsetZ * direction.z;
        const double c = horizontalOffsetSquared - radius * radius;
        const double discriminant =
            halfB * halfB - horizontalDirectionSquared * c;
        if (discriminant >= 0.0) {
            const double root = std::sqrt(discriminant);
            const std::array distances{
                (-halfB - root) / horizontalDirectionSquared,
                (-halfB + root) / horizontalDirectionSquared,
            };
            for (const double distance : distances) {
                if (distance < 0.0) {
                    continue;
                }
                const double hitY = origin.y + direction.y * distance;
                if (hitY >= minimumY && hitY <= maximumY) {
                    closest = std::min(closest, distance);
                }
            }
        }
    }

    Vec3 lower = center;
    lower.y = minimumY;
    Vec3 upper = center;
    upper.y = maximumY;
    accept(raySphereDistance(origin, direction, lower, radius));
    accept(raySphereDistance(origin, direction, upper, radius));
    return closest == std::numeric_limits<double>::max()
        ? std::nullopt
        : std::optional<double>{closest};
}

} // namespace ian::geometry
