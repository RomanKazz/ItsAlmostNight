#pragma once

#include "core/Types.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/CollisionWorld.hpp"

#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace ian {

struct MapObstacle {
    CollisionBox collision;
    double height;
};

// Horizontal clearance for procedural placement. Tangency is allowed;
// obstacle height does not affect the reserved footprint.
[[nodiscard]] bool mapObstacleNear(
    Vec3 position, double radius, std::span<const MapObstacle> obstacles);

struct MapDefinition {
    Vec3 playerSpawn;
    double worldLimit;
    int coreBuildRadius;
    std::vector<Vec3> enemySpawnAnchors;
    std::vector<ResourceNodeDefinition> resources;
    std::vector<MapObstacle> obstacles;

    [[nodiscard]] static MapDefinition defaults();
};

struct MapLoadResult {
    MapDefinition map;
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const { return errors.empty(); }
};

[[nodiscard]] MapLoadResult parseMapDefinition(std::string_view json);
[[nodiscard]] MapLoadResult loadMapDefinition(std::string_view path);
[[nodiscard]] std::vector<CollisionBox> mapCollisionBoxes(
    const MapDefinition& map);

} // namespace ian
