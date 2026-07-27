#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "world/CollisionWorld.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace ian {

struct NavCell {
    double terrainCost{1.0};
    double distanceToCore{std::numeric_limits<double>::infinity()};
    std::optional<EntityId> buildingId;
    bool blocked{};
};

struct FlowDebugVector {
    Vec3 position;
    Vec3 direction;
    double terrainCost;
    bool blocked;
};

class FlowField {
  public:
    static constexpr int MinimumCoordinate = -48;
    static constexpr int GridSize = 96;
    static constexpr double WallTraversalCost = 25.0;

    FlowField();
    explicit FlowField(std::vector<CollisionBox> staticObstacles);

    void reset();
    void rebuild(GridPosition target, const std::vector<BuildingInstance>& buildings);

    [[nodiscard]] std::optional<Vec3> directionAt(Vec3 worldPosition) const;
    [[nodiscard]] double distanceAt(GridPosition position) const;
    [[nodiscard]] const NavCell* cellAt(GridPosition position) const;
    [[nodiscard]] bool ready() const;
    [[nodiscard]] std::vector<FlowDebugVector> debugVectors(int stride = 3) const;

  private:
    static constexpr std::size_t CellCount =
        static_cast<std::size_t>(GridSize) * static_cast<std::size_t>(GridSize);

    [[nodiscard]] static bool contains(GridPosition position);
    [[nodiscard]] static std::size_t indexOf(GridPosition position);
    void markStaticObstacles();

    std::array<NavCell, CellCount> cells_{};
    std::vector<CollisionBox> staticObstacles_;
    GridPosition target_{};
    bool ready_{};
};

} // namespace ian
