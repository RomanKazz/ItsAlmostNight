#pragma once

#include "core/Types.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <span>
#include <vector>

namespace ian {

[[nodiscard]] bool resourceOverlapsRectangle(
    std::span<const ResourceNode> nodes,
    double minimumX, double maximumX,
    double minimumZ, double maximumZ);

[[nodiscard]] std::vector<ResourceNodeDefinition> scatterResources(
    const std::vector<ResourceNodeDefinition>& configured,
    double worldLimit,
    const TerrainHeightfield& terrain);

[[nodiscard]] Vec3 resourceImpactPosition(
    std::span<const ResourceNode> nodes, EntityId id,
    Vec3 origin, Vec3 direction);

} // namespace ian
