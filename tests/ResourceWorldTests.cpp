#include "TestHarness.hpp"
#include "game/ResourceWorld.hpp"
#include "world/MapDefinition.hpp"
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
    require(clustered.size() > clusterInput.size() + 48U,
            "large worlds retain configured resource density");
    require(
        std::count_if(
            clustered.begin(), clustered.end(),
            [](const ian::ResourceNodeDefinition& definition) {
                return ian::isDestructibleProp(definition.type);
            }) >= 10,
        "large worlds scatter destructible barrels and crates");
    for (std::size_t propIndex = 0;
         propIndex < clustered.size(); ++propIndex) {
        const auto& prop = clustered[propIndex];
        if (!ian::isDestructibleProp(prop.type)) {
            continue;
        }
        for (std::size_t otherIndex = 0;
             otherIndex < clustered.size(); ++otherIndex) {
            if (propIndex == otherIndex) {
                continue;
            }
            const auto& other = clustered[otherIndex];
            const double distance = std::hypot(
                prop.position.x - other.position.x,
                prop.position.z - other.position.z);
            require(
                distance + 1e-9 >=
                    prop.radius + other.radius + 0.8,
                "destructible props avoid nearby world resources");
        }
    }
    const std::array<ian::MapObstacle, 1> coveringObstacle{{{
        .collision = {-48.0, 48.0, -48.0, 48.0},
        .height = 4.0,
    }}};
    const auto obstacleBlocked = ian::scatterResources(
        clusterInput, 48.0, largeTerrain, coveringObstacle);
    require(
        std::none_of(
            obstacleBlocked.begin(), obstacleBlocked.end(),
            [](const ian::ResourceNodeDefinition& definition) {
                return ian::isDestructibleProp(definition.type);
            }),
        "destructible props avoid static map obstacles during spawn");
    requireNear(clustered.back().position.x,
                repeated.back().position.x, 1e-12,
                "resource scattering stays deterministic");

    const ian::Vec3 playerSpawn{0.0, 0.0, 6.0};
    for (std::uint32_t seed = 1U; seed <= 32U; ++seed) {
        const auto starterResources = ian::scatterResources(
            clusterInput, 48.0, largeTerrain, {}, seed,
            playerSpawn);
        const auto starterCrystal = std::find_if(
            starterResources.begin(), starterResources.end(),
            [&playerSpawn](
                const ian::ResourceNodeDefinition& resource) {
                return resource.type == ian::ResourceType::Crystal &&
                    std::hypot(
                        resource.position.x - playerSpawn.x,
                        resource.position.z - playerSpawn.z) <= 11.0 &&
                    std::hypot(
                        resource.position.x,
                        resource.position.z) >= 5.0 &&
                    resource.yield >= 10;
            });
        require(
            starterCrystal != starterResources.end(),
            "every run places a ten-yield crystal near spawn and clear of the Core");
    }
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
    constexpr std::size_t GeneratedTreeCount = 30U;
    constexpr std::size_t GeneratedTreeClusterCount = 5U;
    constexpr std::size_t TreesPerGeneratedCluster = 5U;
    static_assert(
        GeneratedTreeClusterCount * TreesPerGeneratedCluster ==
        25U);
    for (std::size_t clusterIndex = 0;
         clusterIndex < GeneratedTreeClusterCount; ++clusterIndex) {
        const std::size_t firstTreeIndex =
            clusterInput.size() + clusterIndex;
        const std::size_t expectedStyle = ian::treeVisualStyle(
            clustered[firstTreeIndex].visualVariant);
        for (std::size_t member = 1U;
             member < TreesPerGeneratedCluster; ++member) {
            const std::size_t treeIndex =
                firstTreeIndex +
                member * GeneratedTreeClusterCount;
            require(
                ian::treeVisualStyle(
                    clustered[treeIndex].visualVariant) ==
                    expectedStyle,
                "each generated cluster keeps one tree visual style");
        }
    }
    require(
        std::all_of(
            clustered.begin() + static_cast<std::ptrdiff_t>(
                clusterInput.size()),
            clustered.begin() + static_cast<std::ptrdiff_t>(
                clusterInput.size() + GeneratedTreeCount),
            [](const ian::ResourceNodeDefinition& tree) {
                return tree.visualVariant <
                    ian::TreeVisualVariantCount;
            }),
        "generated trees select one of nine visual variants");

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
