#include "TestHarness.hpp"
#include "game/ResourceWorld.hpp"
#include "world/WorldConfig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

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

    config.terrainWorldSize = 96.0;
    ian::TerrainHeightfield largeTerrain{config};
    std::vector<ian::ResourceNodeDefinition> clusterInput;
    for (int index = 0; index < 7; ++index) {
        clusterInput.push_back({
            index < 4 ? ian::ResourceType::Wood
                      : ian::ResourceType::Stone,
            {static_cast<double>(index), 1.0, 0.0},
            1.0, 10.0, 10, 5.0,
        });
    }
    const auto clustered =
        ian::scatterResources(clusterInput, 48.0, largeTerrain);
    const auto repeated =
        ian::scatterResources(clusterInput, 48.0, largeTerrain);
    require(clustered.size() == clusterInput.size() + 48U,
            "large worlds retain configured resource density");
    requireNear(clustered.back().position.x,
                repeated.back().position.x, 1e-12,
                "resource scattering stays deterministic");
    int nearbyTreePairs = 0;
    for (std::size_t left = clusterInput.size();
         left < clustered.size(); ++left) {
        if (clustered[left].type != ian::ResourceType::Wood) {
            continue;
        }
        for (std::size_t right = left + 1U;
             right < clustered.size(); ++right) {
            if (clustered[right].type != ian::ResourceType::Wood) {
                continue;
            }
            const double deltaX = clustered[left].position.x -
                                  clustered[right].position.x;
            const double deltaZ = clustered[left].position.z -
                                  clustered[right].position.z;
            if (deltaX * deltaX + deltaZ * deltaZ < 36.0) {
                ++nearbyTreePairs;
            }
        }
    }
    require(nearbyTreePairs >= 25,
            "generated trees form several visible local clusters");

    config.terrainWorldSize = 384.0;
    config.terrainBoundaryRiseWidth = 48.0;
    ian::TerrainHeightfield expandedTerrain{config};
    const auto expanded =
        ian::scatterResources(clusterInput, 144.0, expandedTerrain);
    require(
        expanded.size() > clustered.size() * 6U,
        "expanded world scales resource population with usable area");
    require(
        std::all_of(
            expanded.begin() +
                static_cast<std::ptrdiff_t>(clusterInput.size()),
            expanded.end(),
            [](const ian::ResourceNodeDefinition& resource) {
                return std::hypot(
                           resource.position.x,
                           resource.position.z) <= 142.0;
            }),
        "generated resources stay below raised boundary terrain");
    require(
        std::all_of(
            expanded.begin(), expanded.end(),
            [&expandedTerrain](
                const ian::ResourceNodeDefinition& resource) {
                return expandedTerrain.waterSignedDistance(
                           resource.position.x,
                           resource.position.z) >=
                    resource.radius + 0.8;
            }),
        "resource bounds stay clear of pond shorelines");
}
