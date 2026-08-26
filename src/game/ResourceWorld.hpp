#pragma once

#include "core/Types.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <span>
#include <optional>
#include <vector>

namespace ian {

struct MapObstacle;

[[nodiscard]] bool resourceOverlapsRectangle(
    std::span<const ResourceNode> nodes,
    double minimumX, double maximumX,
    double minimumZ, double maximumZ);

[[nodiscard]] std::vector<ResourceNodeDefinition> scatterResources(
    const std::vector<ResourceNodeDefinition>& configured,
    double worldLimit,
    const TerrainHeightfield& terrain,
    std::span<const MapObstacle> obstacles = {},
    std::uint32_t layoutSeed = 0U,
    std::optional<Vec3> playerSpawn = std::nullopt);

[[nodiscard]] Vec3 resourceImpactPosition(
    std::span<const ResourceNode> nodes, EntityId id,
    Vec3 origin, Vec3 direction);

} // namespace ian
