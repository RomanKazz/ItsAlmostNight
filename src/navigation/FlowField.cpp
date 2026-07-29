#include "navigation/FlowField.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <queue>
#include <utility>

namespace ian {
namespace {

constexpr std::array<GridPosition, 4> NeighborOffsets{{
    {-1, 0},
    {0, -1},
    {1, 0},
    {0, 1},
}};

std::vector<CollisionBox> defaultStaticObstacles() {
    return {
        {-9.0, -7.0, -8.0, -6.0},
        {7.5, 10.5, -13.5, -10.5},
    };
}

bool cellIntersects(GridPosition position, const CollisionBox& obstacle) {
    const double minX = static_cast<double>(position.x) - 0.5;
    const double maxX = static_cast<double>(position.x) + 0.5;
    const double minZ = static_cast<double>(position.z) - 0.5;
    const double maxZ = static_cast<double>(position.z) + 0.5;
    return minX < obstacle.maxX && maxX > obstacle.minX && minZ < obstacle.maxZ &&
           maxZ > obstacle.minZ;
}

} // namespace

FlowField::FlowField() : FlowField(defaultStaticObstacles()) {}

FlowField::FlowField(std::vector<CollisionBox> staticObstacles)
    : staticObstacles_(std::move(staticObstacles)) {}

void FlowField::reset() {
    for (auto& cell : cells_) {
        cell = {};
    }
    target_ = {};
    ready_ = false;
}

void FlowField::rebuild(GridPosition target, const std::vector<BuildingInstance>& buildings) {
    reset();
    target_ = target;
    markStaticObstacles();

    for (const auto& building : buildings) {
        if (building.platformStorey >= 0) {
            continue;
        }
        const int halfExtent =
            buildingFootprintHalfExtent(building.type) == 1.0
                ? 1
                : 0;
        for (int z = building.gridPosition.z - halfExtent;
             z <= building.gridPosition.z + halfExtent; ++z) {
            for (int x = building.gridPosition.x - halfExtent;
                 x <= building.gridPosition.x + halfExtent; ++x) {
                const GridPosition position{x, z};
                if (!contains(position)) {
                    continue;
                }
                NavCell& cell = cells_[indexOf(position)];
                cell.buildingId = building.id;
                if (building.type == BuildingType::Wall) {
                    cell.terrainCost = WallTraversalCost;
                } else if (building.type == BuildingType::Turret) {
                    cell.terrainCost = 8.0;
                } else if (
                    building.type == BuildingType::GoldMine ||
                    building.type == BuildingType::LumberMill ||
                    building.type == BuildingType::Quarry) {
                    cell.terrainCost = 8.0;
                } else if (building.type == BuildingType::Cannon) {
                    cell.terrainCost = 10.0;
                } else if (building.type == BuildingType::SlowTrap) {
                    cell.terrainCost = 4.0;
                } else if (building.type == BuildingType::Gate) {
                    cell.terrainCost = building.open ? 1.0 : WallTraversalCost;
                }
            }
        }
    }

    if (!contains(target_) || cells_[indexOf(target_)].blocked) {
        return;
    }

    using QueueEntry = std::pair<double, std::size_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> frontier;
    cells_[indexOf(target_)].distanceToCore = 0.0;
    frontier.emplace(0.0, indexOf(target_));

    while (!frontier.empty()) {
        const auto [distance, index] = frontier.top();
        frontier.pop();
        if (distance != cells_[index].distanceToCore) {
            continue;
        }

        const int localX = static_cast<int>(index % static_cast<std::size_t>(GridSize));
        const int localZ = static_cast<int>(index / static_cast<std::size_t>(GridSize));
        const GridPosition current{localX + MinimumCoordinate, localZ + MinimumCoordinate};
        for (const GridPosition offset : NeighborOffsets) {
            const GridPosition neighbor{current.x + offset.x, current.z + offset.z};
            if (!contains(neighbor)) {
                continue;
            }

            NavCell& neighborCell = cells_[indexOf(neighbor)];
            if (neighborCell.blocked) {
                continue;
            }

            const double candidate = distance + cells_[index].terrainCost;
            if (candidate < neighborCell.distanceToCore) {
                neighborCell.distanceToCore = candidate;
                frontier.emplace(candidate, indexOf(neighbor));
            }
        }
    }

    ready_ = true;
}

std::optional<Vec3> FlowField::directionAt(Vec3 worldPosition) const {
    if (!ready_) {
        return std::nullopt;
    }

    const GridPosition position{
        static_cast<int>(std::lround(worldPosition.x)),
        static_cast<int>(std::lround(worldPosition.z)),
    };
    const NavCell* currentCell = cellAt(position);
    if (currentCell == nullptr || currentCell->blocked ||
        !std::isfinite(currentCell->distanceToCore)) {
        return std::nullopt;
    }
    if (position == target_) {
        return Vec3{};
    }

    std::optional<GridPosition> bestNeighbor;
    double bestScore = std::numeric_limits<double>::infinity();
    for (const GridPosition offset : NeighborOffsets) {
        const GridPosition neighbor{position.x + offset.x, position.z + offset.z};
        const NavCell* neighborCell = cellAt(neighbor);
        if (neighborCell == nullptr || neighborCell->blocked ||
            !std::isfinite(neighborCell->distanceToCore)) {
            continue;
        }
        const double score = neighborCell->distanceToCore + neighborCell->terrainCost;
        if (score < bestScore) {
            bestScore = score;
            bestNeighbor = neighbor;
        }
    }

    if (!bestNeighbor) {
        return std::nullopt;
    }
    return Vec3{
        static_cast<double>(bestNeighbor->x - position.x),
        0.0,
        static_cast<double>(bestNeighbor->z - position.z),
    };
}

double FlowField::distanceAt(GridPosition position) const {
    const NavCell* cell = cellAt(position);
    return cell == nullptr ? std::numeric_limits<double>::infinity() : cell->distanceToCore;
}

const NavCell* FlowField::cellAt(GridPosition position) const {
    return contains(position) ? &cells_[indexOf(position)] : nullptr;
}

bool FlowField::ready() const {
    return ready_;
}

std::vector<FlowDebugVector> FlowField::debugVectors(int stride) const {
    std::vector<FlowDebugVector> result;
    if (!ready_ || stride <= 0) {
        return result;
    }
    const int sampleCount = (GridSize + stride - 1) / stride;
    result.reserve(static_cast<std::size_t>(sampleCount * sampleCount));
    for (int z = MinimumCoordinate; z < MinimumCoordinate + GridSize; z += stride) {
        for (int x = MinimumCoordinate; x < MinimumCoordinate + GridSize; x += stride) {
            const GridPosition position{x, z};
            const NavCell* cell = cellAt(position);
            if (cell == nullptr ||
                (!cell->blocked && !std::isfinite(cell->distanceToCore))) {
                continue;
            }
            result.push_back({
                .position = {static_cast<double>(x), 0.04, static_cast<double>(z)},
                .direction =
                    directionAt({static_cast<double>(x), 0.0, static_cast<double>(z)})
                        .value_or(Vec3{}),
                .terrainCost = cell->terrainCost,
                .blocked = cell->blocked,
            });
        }
    }
    return result;
}

bool FlowField::contains(GridPosition position) {
    return position.x >= MinimumCoordinate && position.z >= MinimumCoordinate &&
           position.x < MinimumCoordinate + GridSize &&
           position.z < MinimumCoordinate + GridSize;
}

std::size_t FlowField::indexOf(GridPosition position) {
    const auto localX = static_cast<std::size_t>(position.x - MinimumCoordinate);
    const auto localZ = static_cast<std::size_t>(position.z - MinimumCoordinate);
    return localZ * static_cast<std::size_t>(GridSize) + localX;
}

void FlowField::markStaticObstacles() {
    for (int z = MinimumCoordinate; z < MinimumCoordinate + GridSize; ++z) {
        for (int x = MinimumCoordinate; x < MinimumCoordinate + GridSize; ++x) {
            const GridPosition position{x, z};
            NavCell& cell = cells_[indexOf(position)];
            cell.blocked = std::any_of(
                staticObstacles_.begin(), staticObstacles_.end(),
                [position](const CollisionBox& obstacle) {
                    return cellIntersects(position, obstacle);
                });
        }
    }
}

} // namespace ian
