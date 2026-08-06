#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ian {

enum class DecorationExclusionShape : std::uint8_t {
    Circle,
    Rectangle,
};

struct DecorationExclusion {
    DecorationExclusionShape shape{DecorationExclusionShape::Circle};
    double centerX{};
    double centerZ{};
    double radius{};
    double halfWidth{};
    double halfDepth{};
};

class DecorationExclusionMap {
  public:
    static constexpr double DefaultCellSize = 0.75;
    static constexpr double DefaultDecorationPadding = 1.15;

    void rebuild(
        double worldHalfExtent,
        std::span<const DecorationExclusion> exclusions,
        double decorationPadding = DefaultDecorationPadding,
        double cellSize = DefaultCellSize);

    [[nodiscard]] bool blocked(double x, double z) const;
    [[nodiscard]] std::size_t cellCount() const;
    [[nodiscard]] std::size_t blockedCellCount() const;

  private:
    [[nodiscard]] std::size_t indexOf(int x, int z) const;

    std::vector<std::uint8_t> cells_;
    double minimumCoordinate_{};
    double cellSize_{DefaultCellSize};
    int dimension_{};
    std::size_t blockedCellCount_{};
};

} // namespace ian
