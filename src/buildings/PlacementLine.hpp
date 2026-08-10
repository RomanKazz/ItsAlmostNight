#pragma once

#include "core/Types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace ian {

enum class PlacementLineAxis {
    X,
    Z,
};

inline constexpr std::size_t MaximumPlacementLineLength = 48U;

// Intersect only the active construction plane. Terrain must not affect an
// elevated drag: player height and view pitch determine which side of the
// plane can be aimed. The horizontal bound prevents a near-parallel ray from
// producing a horizon-sized placement line.
[[nodiscard]] inline std::optional<Vec3>
elevatedPlatformDragAim(
    Vec3 viewer, Vec3 lookDirection,
    double planeHeight, double maximumDistance) {
    if (std::abs(lookDirection.y) <= 1e-9) {
        return std::nullopt;
    }
    const double rayDistance =
        (planeHeight - viewer.y) / lookDirection.y;
    if (rayDistance <= 0.0) {
        return std::nullopt;
    }
    Vec3 planeAim{
        viewer.x + lookDirection.x * rayDistance,
        planeHeight,
        viewer.z + lookDirection.z * rayDistance,
    };
    const double deltaX = planeAim.x - viewer.x;
    const double deltaZ = planeAim.z - viewer.z;
    const double distance = std::hypot(deltaX, deltaZ);
    if (maximumDistance > 0.0 && distance > maximumDistance) {
        const double scale = maximumDistance / distance;
        planeAim.x = viewer.x + deltaX * scale;
        planeAim.z = viewer.z + deltaZ * scale;
    }
    return planeAim;
}

// Multi-placement must remain one connected run. Once required support is
// absent, later cells are unreachable even if support exists farther away.
template <typename GridPoint, typename IsSupported>
[[nodiscard]] std::vector<GridPoint>
contiguousPlacementPrefix(
    std::vector<GridPoint> line,
    IsSupported&& isSupported) {
    const auto firstUnsupported = std::find_if(
        line.begin(), line.end(),
        [&isSupported](const GridPoint& point) {
            return !isSupported(point);
        });
    line.erase(firstUnsupported, line.end());
    return line;
}

[[nodiscard]] inline std::optional<PlacementLineAxis>
stabilizePlacementLineAxis(
    double deltaX, double deltaZ,
    std::optional<PlacementLineAxis> current,
    double switchMargin) {
    const double distanceX = std::abs(deltaX);
    const double distanceZ = std::abs(deltaZ);
    constexpr double Epsilon = 1e-6;
    if (distanceX <= Epsilon &&
        distanceZ <= Epsilon) {
        return current;
    }
    if (!current) {
        return distanceX >= distanceZ
                   ? PlacementLineAxis::X
                   : PlacementLineAxis::Z;
    }
    // Never let hysteresis keep an axis whose projected line has
    // collapsed to one cell. This was the main source of a drag
    // visibly continuing in the direction perpendicular to the aim.
    if (*current == PlacementLineAxis::X &&
        distanceX <= Epsilon &&
        distanceZ > Epsilon) {
        return PlacementLineAxis::Z;
    }
    if (*current == PlacementLineAxis::Z &&
        distanceZ <= Epsilon &&
        distanceX > Epsilon) {
        return PlacementLineAxis::X;
    }
    if (*current == PlacementLineAxis::X &&
        distanceZ >= distanceX + switchMargin) {
        return PlacementLineAxis::Z;
    }
    if (*current == PlacementLineAxis::Z &&
        distanceX >= distanceZ + switchMargin) {
        return PlacementLineAxis::X;
    }
    return current;
}

template <typename GridPoint>
[[nodiscard]] std::vector<GridPoint> placementLine(
    GridPoint start, GridPoint end, int spacing,
    std::optional<PlacementLineAxis> preferredAxis =
        std::nullopt,
    std::size_t maximumLength =
        MaximumPlacementLineLength) {
    if (spacing <= 0 || maximumLength == 0U) {
        return {};
    }
    const long long deltaX =
        static_cast<long long>(end.x) -
        static_cast<long long>(start.x);
    const long long deltaZ =
        static_cast<long long>(end.z) -
        static_cast<long long>(start.z);
    const PlacementLineAxis axis =
        preferredAxis.value_or(
            std::abs(deltaX) >= std::abs(deltaZ)
                ? PlacementLineAxis::X
                : PlacementLineAxis::Z);
    const long long axisDelta =
        axis == PlacementLineAxis::X ? deltaX : deltaZ;
    const std::size_t count = std::min(
        static_cast<std::size_t>(
            std::abs(axisDelta) / spacing + 1),
        maximumLength);
    const int direction = axisDelta >= 0 ? 1 : -1;

    std::vector<GridPoint> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        GridPoint point = start;
        const int offset =
            static_cast<int>(index) * spacing *
            direction;
        if (axis == PlacementLineAxis::X) {
            point.x += offset;
        } else {
            point.z += offset;
        }
        result.push_back(point);
    }
    return result;
}

} // namespace ian
