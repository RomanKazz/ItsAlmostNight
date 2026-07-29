#pragma once

#include "core/Types.hpp"
#include "world/WorldConfig.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace ian {

struct GridCoord {
    int x{};
    int yLevel{};
    int z{};

    friend bool operator==(
        const GridCoord&,
        const GridCoord&) = default;
};

enum class Footprint : std::uint8_t {
    OneByOne,
    TwoByTwo,
};

enum class Rotation : std::uint8_t {
    Deg0,
    Deg90,
    Deg180,
    Deg270,
};

enum class OccupancyLayer : std::uint8_t {
    Volume = 1U << 0U,
    Floor = 1U << 1U,
    Wall = 1U << 2U,
    StructuralNode = 1U << 3U,
};

struct OccupancyRecord {
    EntityId owner;
    OccupancyLayer layer;
};

class BuildGrid {
  public:
    explicit BuildGrid(
        WorldConfig config = WorldConfig::defaults());

    // Anchor is always the minimum occupied X/Z cell.
    // Vertical placement uses the bottom yLevel.
    [[nodiscard]] GridCoord worldToGrid(
        Vec3 position) const;
    [[nodiscard]] Vec3 worldCenter(
        GridCoord anchor, Footprint footprint) const;
    [[nodiscard]] std::vector<GridCoord> occupiedCells(
        GridCoord anchor, Footprint footprint,
        int heightLevels = 1) const;
    [[nodiscard]] std::vector<GridCoord>
    occupiedRectangleCells(
        GridCoord anchor, int widthCells,
        int depthCells, int heightLevels = 1) const;

    [[nodiscard]] bool canOccupy(
        GridCoord anchor, Footprint footprint,
        int heightLevels, OccupancyLayer layer,
        EntityId ignoredOwner = {}) const;
    [[nodiscard]] bool occupy(
        EntityId owner, GridCoord anchor,
        Footprint footprint, int heightLevels,
        OccupancyLayer layer);
    [[nodiscard]] bool canOccupyRectangle(
        GridCoord anchor, int widthCells,
        int depthCells, int heightLevels,
        OccupancyLayer layer,
        EntityId ignoredOwner = {}) const;
    [[nodiscard]] bool occupyRectangle(
        EntityId owner, GridCoord anchor,
        int widthCells, int depthCells,
        int heightLevels, OccupancyLayer layer);
    void release(EntityId owner);

    [[nodiscard]] bool isOccupied(
        GridCoord coord, OccupancyLayer layer) const;
    [[nodiscard]] std::span<const OccupancyRecord>
    occupants(GridCoord coord) const;
    [[nodiscard]] std::size_t occupiedCellCount() const;
    [[nodiscard]] const WorldConfig& config() const;

  private:
    struct GridCoordHash {
        [[nodiscard]] std::size_t operator()(
            const GridCoord& coord) const;
    };

    [[nodiscard]] static int footprintWidth(
        Footprint footprint);
    [[nodiscard]] static bool layersConflict(
        OccupancyLayer left, OccupancyLayer right);

    WorldConfig config_;
    std::unordered_map<
        GridCoord, std::vector<OccupancyRecord>,
        GridCoordHash>
        occupancy_;
};

} // namespace ian
