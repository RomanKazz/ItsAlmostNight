#include "TestHarness.hpp"
#include "game/ResourceWorld.hpp"
#include "world/WorldConfig.hpp"

#include <array>

void runResourceWorldTests() {
    const std::array<ian::ResourceNode, 2> nodes{{
        {
            .id = ian::EntityId{1, 0},
            .type = ian::ResourceType::Wood,
            .position = {2.0, 1.0, 0.0},
            .radius = 1.0,
            .health = 10.0,
            .maxHealth = 10.0,
            .yield = 10,
            .yieldRemaining = 10,
            .respawnSeconds = 5.0,
            .respawnRemaining = 0.0,
            .active = true,
        },
        {
            .id = ian::EntityId{2, 0},
            .type = ian::ResourceType::Stone,
            .position = {8.0, 1.0, 0.0},
            .radius = 1.0,
            .health = 10.0,
            .maxHealth = 10.0,
            .yield = 10,
            .yieldRemaining = 10,
            .respawnSeconds = 5.0,
            .respawnRemaining = 0.0,
            .active = false,
        },
    }};

    require(
        ian::resourceOverlapsRectangle(
            nodes, 0.0, 1.1, -0.5, 0.5),
        "active resource blocks overlapping placement");
    require(
        !ian::resourceOverlapsRectangle(
            nodes, 6.5, 7.5, -0.5, 0.5),
        "inactive resource does not block placement");

    const ian::Vec3 hit = ian::resourceImpactPosition(
        nodes, ian::EntityId{1, 0},
        {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0});
    requireNear(
        hit.x, 1.0, 1e-9,
        "resource impact returns ray-sphere surface");
    const ian::Vec3 missing = ian::resourceImpactPosition(
        nodes, ian::EntityId{99, 0},
        {3.0, 4.0, 5.0}, {1.0, 0.0, 0.0});
    requireNear(
        missing.x, 3.0, 1e-9,
        "missing resource impact falls back to origin");

    ian::WorldConfig config = ian::WorldConfig::defaults();
    config.terrainResolution = 33;
    config.terrainWorldSize = 32.0;
    config.coreFlatRadius = 3.0;
    ian::TerrainHeightfield terrain{config};
    const std::vector<ian::ResourceNodeDefinition> configured{
        {ian::ResourceType::Wood, {0.0, 1.0, 0.0},
         1.0, 10.0, 10, 5.0},
    };
    const auto scattered =
        ian::scatterResources(configured, 12.0, terrain);
    require(
        scattered.size() == configured.size(),
        "small worlds preserve configured resource count");
    requireNear(
        scattered.front().position.y,
        configured.front().position.y +
            terrain.getHeight(0.0, 0.0),
        1e-9,
        "resource scattering follows terrain height");
}
