#include "buildings/BuildGrid.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ian {

BuildGrid::BuildGrid(WorldConfig config)
    : config_(config) {}

GridCoord BuildGrid::worldToGrid(
    Vec3 position) const {
    return {
        static_cast<int>(std::floor(
            position.x / config_.cellSize)),
        static_cast<int>(std::lround(
            position.y / config_.verticalGridStep)),
        static_cast<int>(std::floor(
            position.z / config_.cellSize)),
    };
}

Vec3 BuildGrid::worldCenter(
    GridCoord anchor, Footprint footprint) const {
    const double halfWidth =
        static_cast<double>(
            footprintWidth(footprint)) *
        0.5;
    return {
        (static_cast<double>(anchor.x) + halfWidth) *
            config_.cellSize,
        static_cast<double>(anchor.yLevel) *
            config_.verticalGridStep,
        (static_cast<double>(anchor.z) + halfWidth) *
            config_.cellSize,
    };
}

std::vector<GridCoord> BuildGrid::occupiedCells(
    GridCoord anchor, Footprint footprint,
    int heightLevels) const {
    const int width = footprintWidth(footprint);
    return occupiedRectangleCells(
        anchor, width, width, heightLevels);
}

std::vector<GridCoord>
BuildGrid::occupiedRectangleCells(
    GridCoord anchor, int widthCells,
    int depthCells, int heightLevels) const {
    std::vector<GridCoord> cells;
    if (widthCells <= 0 || depthCells <= 0 ||
        heightLevels <= 0) {
        return cells;
    }
    cells.reserve(
        static_cast<std::size_t>(
            widthCells * depthCells * heightLevels));
    for (int y = 0; y < heightLevels; ++y) {
        for (int z = 0; z < depthCells; ++z) {
            for (int x = 0; x < widthCells; ++x) {
                cells.push_back({
                    anchor.x + x,
                    anchor.yLevel + y,
                    anchor.z + z,
                });
            }
        }
    }
    return cells;
}

bool BuildGrid::canOccupy(
    GridCoord anchor, Footprint footprint,
    int heightLevels, OccupancyLayer layer,
    EntityId ignoredOwner) const {
    const int width = footprintWidth(footprint);
    return canOccupyRectangle(
        anchor, width, width, heightLevels,
        layer, ignoredOwner);
}

bool BuildGrid::canOccupyRectangle(
    GridCoord anchor, int widthCells,
    int depthCells, int heightLevels,
    OccupancyLayer layer,
    EntityId ignoredOwner) const {
    if (widthCells <= 0 || depthCells <= 0 ||
        heightLevels <= 0) {
        return false;
    }
    for (const GridCoord cell :
         occupiedRectangleCells(
             anchor, widthCells, depthCells,
             heightLevels)) {
        const auto iterator = occupancy_.find(cell);
        if (iterator == occupancy_.end()) {
            continue;
        }
        const bool conflict = std::any_of(
            iterator->second.begin(),
            iterator->second.end(),
            [layer, ignoredOwner](
                const OccupancyRecord& record) {
                return record.owner != ignoredOwner &&
                       layersConflict(
                           layer, record.layer);
            });
        if (conflict) {
            return false;
        }
    }
    return true;
}

bool BuildGrid::occupy(
    EntityId owner, GridCoord anchor,
    Footprint footprint, int heightLevels,
    OccupancyLayer layer) {
    const int width = footprintWidth(footprint);
    return occupyRectangle(
        owner, anchor, width, width,
        heightLevels, layer);
}

bool BuildGrid::occupyRectangle(
    EntityId owner, GridCoord anchor,
    int widthCells, int depthCells,
    int heightLevels, OccupancyLayer layer) {
    if (!canOccupyRectangle(
            anchor, widthCells, depthCells,
            heightLevels, layer, owner)) {
        return false;
    }
    release(owner);
    for (const GridCoord cell :
         occupiedRectangleCells(
             anchor, widthCells, depthCells,
             heightLevels)) {
        occupancy_[cell].push_back({owner, layer});
    }
    return true;
}

void BuildGrid::release(EntityId owner) {
    for (auto iterator = occupancy_.begin();
         iterator != occupancy_.end();) {
        std::erase_if(
            iterator->second,
            [owner](const OccupancyRecord& record) {
                return record.owner == owner;
            });
        if (iterator->second.empty()) {
            iterator = occupancy_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

bool BuildGrid::isOccupied(
    GridCoord coord, OccupancyLayer layer) const {
    const auto iterator = occupancy_.find(coord);
    if (iterator == occupancy_.end()) {
        return false;
    }
    return std::any_of(
        iterator->second.begin(),
        iterator->second.end(),
        [layer](const OccupancyRecord& record) {
            return layersConflict(
                layer, record.layer);
        });
}

std::span<const OccupancyRecord>
BuildGrid::occupants(GridCoord coord) const {
    const auto iterator = occupancy_.find(coord);
    if (iterator == occupancy_.end()) {
        return {};
    }
    return iterator->second;
}

std::size_t BuildGrid::occupiedCellCount() const {
    return occupancy_.size();
}

const WorldConfig& BuildGrid::config() const {
    return config_;
}

std::size_t BuildGrid::GridCoordHash::operator()(
    const GridCoord& coord) const {
    const auto x = static_cast<std::uint32_t>(coord.x);
    const auto y =
        static_cast<std::uint32_t>(coord.yLevel);
    const auto z = static_cast<std::uint32_t>(coord.z);
    std::uint32_t hash = x * 0x9e3779b9U;
    hash ^= y * 0x85ebca6bU +
            0x9e3779b9U + (hash << 6U) +
            (hash >> 2U);
    hash ^= z * 0xc2b2ae35U +
            0x9e3779b9U + (hash << 6U) +
            (hash >> 2U);
    return static_cast<std::size_t>(hash);
}

int BuildGrid::footprintWidth(
    Footprint footprint) {
    return footprint == Footprint::TwoByTwo
               ? 2
               : 1;
}

bool BuildGrid::layersConflict(
    OccupancyLayer left, OccupancyLayer right) {
    if (left == OccupancyLayer::StructuralNode ||
        right == OccupancyLayer::StructuralNode) {
        return left == right;
    }
    if (left == OccupancyLayer::Wall ||
        right == OccupancyLayer::Wall) {
        return left == right ||
               left == OccupancyLayer::Volume ||
               right == OccupancyLayer::Volume;
    }
    return true;
}

} // namespace ian
