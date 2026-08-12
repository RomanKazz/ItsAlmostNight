#include "navigation/FlowField.hpp"

#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
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

FlowField::FlowField(
    std::vector<CollisionBox> staticObstacles,
    const TerrainHeightfield* terrain)
    : staticObstacles_(std::move(staticObstacles)), terrain_(terrain) {}

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
                    building.type == BuildingType::CrystalMine ||
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

    constexpr int MaximumTraversalCost = 25;
    constexpr std::size_t BucketCount =
        static_cast<std::size_t>(MaximumTraversalCost + 1);
    std::array<std::deque<std::size_t>, BucketCount> frontier;
    cells_[indexOf(target_)].distanceToCore = 0.0;
    frontier[0].push_back(indexOf(target_));
    std::size_t queuedCells = 1U;
    int currentDistance = 0;

    while (queuedCells > 0U) {
        auto& bucket = frontier[
            static_cast<std::size_t>(currentDistance) %
            BucketCount];
        if (bucket.empty()) {
            ++currentDistance;
            continue;
        }
        const std::size_t index = bucket.front();
        bucket.pop_front();
        --queuedCells;
        if (cells_[index].distanceToCore !=
            static_cast<double>(currentDistance)) {
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

            const int traversalCost = std::clamp(
                static_cast<int>(std::lround(
                    cells_[index].terrainCost)),
                1, MaximumTraversalCost);
            const int candidate =
                currentDistance + traversalCost;
            if (static_cast<double>(candidate) <
                neighborCell.distanceToCore) {
                neighborCell.distanceToCore =
                    static_cast<double>(candidate);
                frontier[
                    static_cast<std::size_t>(candidate) %
                    BucketCount]
                    .push_back(indexOf(neighbor));
                ++queuedCells;
            }
        }
    }

    ready_ = true;
}

std::optional<Vec3> FlowField::directionAt(Vec3 worldPosition) const {
    if (!ready_ || !std::isfinite(worldPosition.x) ||
        !std::isfinite(worldPosition.z)) {
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
            if (terrain_ != nullptr && !cell.blocked) {
                const double depth = terrain_->waterDepth(
                    static_cast<double>(x), static_cast<double>(z));
                if (depth >= terrain_->config().pondDeepWaterDepth) {
                    cell.blocked = true;
                } else if (depth > 0.02) {
                    const double multiplier =
                        terrain_->waterMovementMultiplier(x, z);
                    cell.terrainCost = std::max(
                        cell.terrainCost,
                        std::min(8.0, 1.0 / std::max(multiplier, 0.01)));
                }
            }
        }
    }
}

} // namespace ian
