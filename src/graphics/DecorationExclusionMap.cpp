#include "graphics/DecorationExclusionMap.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {

void DecorationExclusionMap::rebuild(
    double worldHalfExtent,
    std::span<const DecorationExclusion> exclusions,
    double decorationPadding, double cellSize) {
    cells_.clear();
    dimension_ = 0;
    blockedCellCount_ = 0U;
    if (!std::isfinite(worldHalfExtent) || worldHalfExtent <= 0.0 ||
        !std::isfinite(decorationPadding) || decorationPadding < 0.0 ||
        !std::isfinite(cellSize) || cellSize <= 0.0) {
        return;
    }

    constexpr int MaximumDimension = 4096;
    dimension_ = std::clamp(
        static_cast<int>(std::ceil(worldHalfExtent * 2.0 / cellSize)),
        1, MaximumDimension);
    minimumCoordinate_ = -worldHalfExtent;
    cellSize_ = worldHalfExtent * 2.0 /
        static_cast<double>(dimension_);
    cells_.assign(
        static_cast<std::size_t>(dimension_) *
            static_cast<std::size_t>(dimension_),
        0U);

    // Expand by half a cell diagonal as well as the largest decorative prop
    // radius. This makes point sampling conservative at raster boundaries.
    const double padding = decorationPadding +
        cellSize_ * 0.7071067811865476;
    const auto coordinateToCell = [this](double value) {
        return static_cast<int>(std::floor(
            (value - minimumCoordinate_) / cellSize_));
    };

    for (const DecorationExclusion& exclusion : exclusions) {
        if (!std::isfinite(exclusion.centerX) ||
            !std::isfinite(exclusion.centerZ)) {
            continue;
        }
        const bool circle =
            exclusion.shape == DecorationExclusionShape::Circle;
        const double extentX = circle
            ? std::max(exclusion.radius, 0.0) + padding
            : std::max(exclusion.halfWidth, 0.0) + padding;
        const double extentZ = circle
            ? std::max(exclusion.radius, 0.0) + padding
            : std::max(exclusion.halfDepth, 0.0) + padding;
        if (!std::isfinite(extentX) || !std::isfinite(extentZ)) {
            continue;
        }
        const int minimumX = std::clamp(
            coordinateToCell(exclusion.centerX - extentX),
            0, dimension_ - 1);
        const int maximumX = std::clamp(
            coordinateToCell(exclusion.centerX + extentX),
            0, dimension_ - 1);
        const int minimumZ = std::clamp(
            coordinateToCell(exclusion.centerZ - extentZ),
            0, dimension_ - 1);
        const int maximumZ = std::clamp(
            coordinateToCell(exclusion.centerZ + extentZ),
            0, dimension_ - 1);
        const double circleRadiusSquared = extentX * extentX;
        for (int z = minimumZ; z <= maximumZ; ++z) {
            for (int x = minimumX; x <= maximumX; ++x) {
                const double worldX = minimumCoordinate_ +
                    (static_cast<double>(x) + 0.5) * cellSize_;
                const double worldZ = minimumCoordinate_ +
                    (static_cast<double>(z) + 0.5) * cellSize_;
                const double deltaX = worldX - exclusion.centerX;
                const double deltaZ = worldZ - exclusion.centerZ;
                const bool inside = circle
                    ? deltaX * deltaX + deltaZ * deltaZ <=
                          circleRadiusSquared
                    : std::abs(deltaX) <= extentX &&
                          std::abs(deltaZ) <= extentZ;
                if (!inside) {
                    continue;
                }
                std::uint8_t& cell = cells_[indexOf(x, z)];
                if (cell == 0U) {
                    cell = 1U;
                    ++blockedCellCount_;
                }
            }
        }
    }
}

bool DecorationExclusionMap::blocked(double x, double z) const {
    if (dimension_ <= 0 || cells_.empty() ||
        !std::isfinite(x) || !std::isfinite(z)) {
        return false;
    }
    const int cellX = static_cast<int>(std::floor(
        (x - minimumCoordinate_) / cellSize_));
    const int cellZ = static_cast<int>(std::floor(
        (z - minimumCoordinate_) / cellSize_));
    if (cellX < 0 || cellZ < 0 ||
        cellX >= dimension_ || cellZ >= dimension_) {
        return true;
    }
    return cells_[indexOf(cellX, cellZ)] != 0U;
}

std::size_t DecorationExclusionMap::cellCount() const {
    return cells_.size();
}

std::size_t DecorationExclusionMap::blockedCellCount() const {
    return blockedCellCount_;
}

std::size_t DecorationExclusionMap::indexOf(int x, int z) const {
    return static_cast<std::size_t>(z) *
        static_cast<std::size_t>(dimension_) +
        static_cast<std::size_t>(x);
}

} // namespace ian
