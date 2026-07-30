#pragma once

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
inline constexpr std::size_t MaximumPlacementRectangleArea = 256U;

[[nodiscard]] inline std::optional<PlacementLineAxis>
stabilizePlacementLineAxis(
    long long deltaX, long long deltaZ,
    std::optional<PlacementLineAxis> current,
    long long switchMargin) {
    const long long distanceX = std::abs(deltaX);
    const long long distanceZ = std::abs(deltaZ);
    if (distanceX == 0 && distanceZ == 0) {
        return current;
    }
    if (!current) {
        return distanceX >= distanceZ
                   ? PlacementLineAxis::X
                   : PlacementLineAxis::Z;
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

template <typename GridPoint>
[[nodiscard]] std::vector<GridPoint> placementRectangle(
    GridPoint start, GridPoint end, int spacing,
    std::size_t maximumArea =
        MaximumPlacementRectangleArea,
    std::size_t maximumSideLength =
        MaximumPlacementLineLength) {
    if (spacing <= 0 || maximumArea == 0U ||
        maximumSideLength == 0U) {
        return {};
    }
    const long long deltaX =
        static_cast<long long>(end.x) -
        static_cast<long long>(start.x);
    const long long deltaZ =
        static_cast<long long>(end.z) -
        static_cast<long long>(start.z);
    const std::size_t countX = std::min(
        static_cast<std::size_t>(
            std::abs(deltaX) / spacing + 1),
        maximumSideLength);
    const std::size_t countZ = std::min(
        static_cast<std::size_t>(
            std::abs(deltaZ) / spacing + 1),
        maximumSideLength);
    const int directionX = deltaX >= 0 ? 1 : -1;
    const int directionZ = deltaZ >= 0 ? 1 : -1;

    std::vector<GridPoint> result;
    result.reserve(std::min(
        maximumArea, countX * countZ));
    for (std::size_t distance = 0;
         distance < countX + countZ - 1 &&
         result.size() < maximumArea;
         ++distance) {
        const std::size_t minimumZ =
            distance >= countX
                ? distance - countX + 1
                : 0U;
        const std::size_t maximumZ =
            std::min(distance, countZ - 1);
        for (std::size_t indexZ = minimumZ;
             indexZ <= maximumZ &&
             result.size() < maximumArea;
             ++indexZ) {
            const std::size_t indexX =
                distance - indexZ;
            GridPoint point = start;
            point.x +=
                static_cast<int>(indexX) *
                spacing * directionX;
            point.z +=
                static_cast<int>(indexZ) *
                spacing * directionZ;
            result.push_back(point);
        }
    }
    return result;
}

} // namespace ian
