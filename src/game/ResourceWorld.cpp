#include "game/ResourceWorld.hpp"

#include "core/DeterministicRandom.hpp"
#include "world/MapDefinition.hpp"
#include "world/SpatialHash.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr double ResourcePlacementClearance = 0.08;
constexpr double PropPlacementClearance = 0.80;
constexpr double StarterCrystalMaximumDistance = 11.0;
constexpr int StarterCrystalMinimumYield = 10;

std::size_t clusteredTreeVariant(
    std::size_t cluster, std::size_t member,
    std::uint64_t worldSeed) {
    const std::uint64_t clusterSeed = mixBits64(
        0x8f3f73b5cf1c9adeULL + worldSeed +
        static_cast<std::uint64_t>(cluster) *
            0x9e3779b97f4a7c15ULL);
    const std::size_t style = static_cast<std::size_t>(
        clusterSeed % TreeVisualStyleCount);
    const std::size_t variation = static_cast<std::size_t>(
        mixBits64(
            clusterSeed +
            static_cast<std::uint64_t>(member) *
                0xd1b54a32d192ed03ULL) %
        TreeVisualVariantsPerStyle);
    return style * TreeVisualVariantsPerStyle + variation;
}

} // namespace

bool resourceOverlapsRectangle(
    std::span<const ResourceNode> nodes,
    double minimumX, double maximumX,
    double minimumZ, double maximumZ) {
    return std::any_of(
        nodes.begin(), nodes.end(),
        [=](const ResourceNode& node) {
            if (!node.active) {
                return false;
            }
            const double distanceX = std::max(
                0.0,
                std::max(
                    minimumX - node.position.x,
                    node.position.x - maximumX));
            const double distanceZ = std::max(
                0.0,
                std::max(
                    minimumZ - node.position.z,
                    node.position.z - maximumZ));
            const double required =
                node.radius + ResourcePlacementClearance;
            return distanceX * distanceX +
                       distanceZ * distanceZ <
                   required * required;
        });
}

std::vector<ResourceNodeDefinition> scatterResources(
    const std::vector<ResourceNodeDefinition>& configured,
    double worldLimit,
    const TerrainHeightfield& terrain,
    std::span<const MapObstacle> obstacles,
    std::uint32_t layoutSeed,
    std::optional<Vec3> playerSpawn) {
    std::vector<ResourceNodeDefinition> result = configured;
    const auto placeOnDryTerrain = [&terrain](
        std::vector<ResourceNodeDefinition>& definitions) {
        for (auto& definition : definitions) {
            definition.position.y += terrain.getHeight(
                definition.position.x,
                definition.position.z);
        }
        std::erase_if(
            definitions,
            [&terrain](const ResourceNodeDefinition& definition) {
                return terrain.waterSignedDistance(
                           definition.position.x,
                           definition.position.z) <
                    definition.radius + 0.8;
            });
    };
    if (configured.size() < 7U || worldLimit < 16.0) {
        placeOnDryTerrain(result);
        return result;
    }

    const std::uint64_t worldSeed = layoutSeed == 0U
        ? 0ULL
        : mixBits64(
              static_cast<std::uint64_t>(layoutSeed) ^
              0x6a09e667f3bcc909ULL);
    constexpr double Tau = 6.28318530717958647692;
    // A zero layout seed is the authored first-run layout. Keep it bit-for-bit
    // compatible with the original placement so tutorials and deterministic
    // fixtures do not move; restarted runs receive a seeded rotation.
    const double worldRotation = layoutSeed == 0U
        ? 0.0
        : unitRandom(worldSeed) * Tau;
    const double worldCosine = std::cos(worldRotation);
    const double worldSine = std::sin(worldRotation);
    // Authored entries are useful as a balanced starter ring, but their
    // absolute coordinates must not survive a restart. Rotating the whole
    // ring preserves its distances from the spawn while producing a new
    // layout for every run seed.
    for (ResourceNodeDefinition& definition : result) {
        const double x = definition.position.x;
        const double z = definition.position.z;
        definition.position.x = x * worldCosine - z * worldSine;
        definition.position.z = x * worldSine + z * worldCosine;
        const double coordinateLimit = std::max(1.0, worldLimit - 3.0);
        const double maximumCoordinate = std::max(
            std::abs(definition.position.x),
            std::abs(definition.position.z));
        if (maximumCoordinate > coordinateLimit) {
            const double fit = coordinateLimit / maximumCoordinate;
            definition.position.x *= fit;
            definition.position.z *= fit;
        }
    }

    if (playerSpawn) {
        const auto distanceToSpawn = [&playerSpawn](
                                         const ResourceNodeDefinition& node) {
            return std::hypot(
                node.position.x - playerSpawn->x,
                node.position.z - playerSpawn->z);
        };
        auto nearbyCrystal = std::find_if(
            result.begin(), result.end(),
            [&distanceToSpawn](const ResourceNodeDefinition& node) {
                return node.type == ResourceType::Crystal &&
                    distanceToSpawn(node) <=
                        StarterCrystalMaximumDistance;
            });
        if (nearbyCrystal != result.end()) {
            nearbyCrystal->yield = std::max(
                nearbyCrystal->yield,
                StarterCrystalMinimumYield);
        } else {
            ResourceNodeDefinition starterCrystal{
                ResourceType::Crystal, {}, 0.72, 12.0,
                StarterCrystalMinimumYield, 28.0};
            const auto configuredCrystal = std::find_if(
                configured.begin(), configured.end(),
                [](const ResourceNodeDefinition& node) {
                    return node.type == ResourceType::Crystal;
                });
            if (configuredCrystal != configured.end()) {
                starterCrystal = *configuredCrystal;
                starterCrystal.yield = std::max(
                    starterCrystal.yield,
                    StarterCrystalMinimumYield);
            }
            const double startingAngle = worldRotation + 0.47;
            constexpr std::size_t CandidateCount = 64U;
            for (std::size_t attempt = 0;
                 attempt < CandidateCount; ++attempt) {
                const double radius = 5.5 +
                    static_cast<double>(attempt / 16U) * 1.5;
                const double angle = startingAngle +
                    static_cast<double>(attempt % 16U) *
                        Tau / 16.0;
                starterCrystal.position = {
                    playerSpawn->x + std::cos(angle) * radius,
                    CrystalVisualGroundOffset,
                    playerSpawn->z + std::sin(angle) * radius,
                };
                // The default Core tutorial is anchored around world origin.
                // Keep the guaranteed deposit close to the player without
                // letting it invalidate the initial Core footprint.
                if (std::hypot(
                        starterCrystal.position.x,
                        starterCrystal.position.z) < 5.0) {
                    continue;
                }
                if (std::abs(starterCrystal.position.x) >
                        worldLimit - starterCrystal.radius - 1.0 ||
                    std::abs(starterCrystal.position.z) >
                        worldLimit - starterCrystal.radius - 1.0 ||
                    terrain.waterSignedDistance(
                        starterCrystal.position.x,
                        starterCrystal.position.z) <
                        starterCrystal.radius + 0.8) {
                    continue;
                }
                const bool obstacleBlocked = std::any_of(
                    obstacles.begin(), obstacles.end(),
                    [&starterCrystal](const MapObstacle& obstacle) {
                        const double distanceX = std::max(
                            0.0,
                            std::max(
                                obstacle.collision.minX -
                                    starterCrystal.position.x,
                                starterCrystal.position.x -
                                    obstacle.collision.maxX));
                        const double distanceZ = std::max(
                            0.0,
                            std::max(
                                obstacle.collision.minZ -
                                    starterCrystal.position.z,
                                starterCrystal.position.z -
                                    obstacle.collision.maxZ));
                        const double required =
                            starterCrystal.radius + 0.30;
                        return distanceX * distanceX +
                                   distanceZ * distanceZ <
                               required * required;
                    });
                const bool resourceBlocked = std::any_of(
                    result.begin(), result.end(),
                    [&starterCrystal](
                        const ResourceNodeDefinition& other) {
                        const double distanceX =
                            starterCrystal.position.x -
                            other.position.x;
                        const double distanceZ =
                            starterCrystal.position.z -
                            other.position.z;
                        const double required =
                            starterCrystal.radius + other.radius +
                            PropPlacementClearance;
                        return distanceX * distanceX +
                                   distanceZ * distanceZ <
                               required * required;
                    });
                if (!obstacleBlocked && !resourceBlocked) {
                    result.push_back(starterCrystal);
                    break;
                }
            }
        }
    }

    const auto templateFor =
        [&configured](ResourceType type) {
            const auto found = std::find_if(
                configured.begin(), configured.end(),
                [type](
                    const ResourceNodeDefinition& definition) {
                    return definition.type == type;
                });
            if (found != configured.end()) {
                return *found;
            }
            if (type == ResourceType::Wood) {
                return ResourceNodeDefinition{
                    type, {}, 1.0, 3.0, 15, 12.0};
            }
            if (type == ResourceType::Stone) {
                return ResourceNodeDefinition{
                    type, {}, 0.9, 4.0, 12, 15.0};
            }
            return ResourceNodeDefinition{
                type, {}, 0.72, 12.0, 10, 28.0};
        };

    constexpr std::size_t TreesPerCluster = 5;
    constexpr double GoldenAngle = 2.39996322972865332;
    const double innerRadius =
        std::min(10.0, worldLimit * 0.3);
    const double outerRadius =
        std::max(
            innerRadius + 1.0,
            worldLimit - 3.0);
    const double densityScale = std::clamp(
        (outerRadius * outerRadius) / (45.0 * 45.0),
        1.0, 16.0);
    const std::size_t additionalTreeCount =
        static_cast<std::size_t>(
            std::lround(30.0 * densityScale));
    const std::size_t additionalStoneCount =
        static_cast<std::size_t>(
            std::lround(18.0 * densityScale));
    const std::size_t additionalCrystalCount =
        static_cast<std::size_t>(
            std::lround(5.0 * std::sqrt(densityScale)));
    const std::size_t treeClusterCount =
        std::max<std::size_t>(
            1U, additionalTreeCount / 6U);
    const std::size_t clusteredTreeCount = std::min(
        additionalTreeCount,
        treeClusterCount * TreesPerCluster);
    result.reserve(
        result.size() + additionalTreeCount +
        additionalStoneCount + additionalCrystalCount);

    const double clusterOuterRadius =
        std::max(innerRadius + 1.0, outerRadius - 5.0);
    for (std::size_t index = 0;
         index < additionalTreeCount; ++index) {
        ResourceNodeDefinition definition =
            templateFor(ResourceType::Wood);
        if (index >= clusteredTreeCount) {
            const std::size_t singleIndex =
                index - clusteredTreeCount;
            definition.visualVariant = clusteredTreeVariant(
                singleIndex / TreesPerCluster,
                singleIndex % TreesPerCluster, worldSeed);
            const double progress =
                (static_cast<double>(singleIndex) + 0.5) /
                static_cast<double>(
                    additionalTreeCount - clusteredTreeCount);
            const double radius = std::sqrt(
                innerRadius * innerRadius +
                progress *
                    (outerRadius * outerRadius -
                     innerRadius * innerRadius));
            const double angle =
                static_cast<double>(singleIndex) * GoldenAngle +
                1.73 + worldRotation;
            definition.position = {
                std::cos(angle) * radius,
                1.0,
                std::sin(angle) * radius,
            };
            result.push_back(definition);
            continue;
        }
        const std::size_t cluster = index % treeClusterCount;
        const std::size_t member = index / treeClusterCount;
        definition.visualVariant = clusteredTreeVariant(
            cluster, member, worldSeed);
        const double clusterProgress =
            (static_cast<double>(cluster) + 0.65) /
            static_cast<double>(treeClusterCount);
        const double clusterRadius = std::sqrt(
            innerRadius * innerRadius +
            clusterProgress *
                (clusterOuterRadius * clusterOuterRadius -
                 innerRadius * innerRadius));
        const double clusterAngle =
            static_cast<double>(cluster) * GoldenAngle + 0.31 +
            worldRotation;
        const std::uint64_t seed =
            0x51f15e5dULL + worldSeed +
            static_cast<std::uint64_t>(cluster) * 131U +
            static_cast<std::uint64_t>(member) * 977U;
        const double localRadius = member == 0U
                                       ? unitRandom(seed) * 0.55
                                       : 1.55 +
                                             static_cast<double>(member) *
                                                 0.62 +
                                             unitRandom(seed) * 0.38;
        const double localAngle =
            static_cast<double>(member) * GoldenAngle +
            unitRandom(seed ^ 0x9e3779b97f4a7c15ULL) * 0.7;
        definition.position = {
            std::cos(clusterAngle) * clusterRadius +
                std::cos(localAngle) * localRadius,
            1.0,
            std::sin(clusterAngle) * clusterRadius +
                std::sin(localAngle) * localRadius,
        };
        result.push_back(definition);
    }

    for (std::size_t index = 0;
         index < additionalStoneCount; ++index) {
        ResourceNodeDefinition definition =
            templateFor(ResourceType::Stone);
        const double radialProgress =
            (static_cast<double>(index) + 0.5) /
            static_cast<double>(additionalStoneCount);
        const double radius = std::sqrt(
            innerRadius * innerRadius +
            radialProgress *
                (outerRadius * outerRadius -
                 innerRadius * innerRadius));
        const double angle =
            static_cast<double>(index) * GoldenAngle + 0.87 +
            worldRotation +
            (unitRandom(
                 0x8f3f73b5ULL + worldSeed +
                 static_cast<std::uint64_t>(index)) -
             0.5) *
                0.42;
        definition.position = {
            std::cos(angle) * radius,
            0.8,
            std::sin(angle) * radius,
        };
        result.push_back(definition);
    }

    for (std::size_t index = 0;
         index < additionalCrystalCount; ++index) {
        ResourceNodeDefinition definition =
            templateFor(ResourceType::Crystal);
        const double progress =
            (static_cast<double>(index) + 0.55) /
            static_cast<double>(additionalCrystalCount);
        const double radius = std::sqrt(
            innerRadius * innerRadius + progress *
                (outerRadius * outerRadius -
                 innerRadius * innerRadius));
        const double angle =
            static_cast<double>(index) * GoldenAngle + 2.17 +
            worldRotation;
        definition.position = {
            std::cos(angle) * radius,
            CrystalVisualGroundOffset,
            std::sin(angle) * radius,
        };
        result.push_back(definition);
    }

    // Sparse interactive clutter. These use the resource target pipeline for
    // precise hover, health bars and collision, but have their own rewards.
    const std::size_t propCount = static_cast<std::size_t>(
        std::lround(12.0 * std::sqrt(densityScale)));
    SpatialHash placementIndex;
    double maximumResourceRadius = 0.0;
    for (std::size_t index = 0; index < result.size(); ++index) {
        placementIndex.insert(
            {static_cast<std::uint32_t>(index), 0U},
            result[index].position);
        maximumResourceRadius = std::max(
            maximumResourceRadius, result[index].radius);
    }
    for (std::size_t index = 0; index < propCount; ++index) {
        const std::uint64_t seed =
            0xa0761d6478bd642fULL + worldSeed +
            index * 0xe7037ed1a0b428dbULL;
        const double progress =
            (static_cast<double>(index) + 0.6) /
            static_cast<double>(propCount);
        const double radius = std::sqrt(
            (innerRadius + 2.0) * (innerRadius + 2.0) + progress *
            (outerRadius * outerRadius -
             (innerRadius + 2.0) * (innerRadius + 2.0)));
        const double roll = unitRandom(seed ^ 0xd1b54a32d192ed03ULL);
        const ResourceType type = roll < 0.46
            ? ResourceType::Barrel
            : roll < 0.82 ? ResourceType::Crate
                          : ResourceType::ItemCrate;
        const double health = type == ResourceType::Barrel ? 4.0 : 5.0;
        ResourceNodeDefinition definition{
            type, {}, 0.68, health, 0, 75.0,
        };
        constexpr std::size_t MaximumPropPlacementAttempts = 32U;
        for (std::size_t attempt = 0;
             attempt < MaximumPropPlacementAttempts; ++attempt) {
            const std::uint64_t attemptSeed = seed +
                attempt * 0x9e3779b97f4a7c15ULL;
            const double candidateRadius = std::clamp(
                radius +
                    (unitRandom(attemptSeed ^ 0x243f6a8885a308d3ULL) -
                     0.5) * 7.0,
                innerRadius + 2.0, outerRadius);
            const double angle =
                static_cast<double>(index) * GoldenAngle +
                worldRotation + unitRandom(seed) * 0.9 +
                static_cast<double>(attempt) * GoldenAngle * 0.41;
            definition.position = {
                std::cos(angle) * candidateRadius,
                0.62,
                std::sin(angle) * candidateRadius,
            };
            if (terrain.waterSignedDistance(
                    definition.position.x,
                    definition.position.z) <
                definition.radius + 0.8) {
                continue;
            }
            const bool obstacleBlocked = std::any_of(
                obstacles.begin(), obstacles.end(),
                [&definition](const MapObstacle& obstacle) {
                    const double distanceX = std::max(
                        0.0,
                        std::max(
                            obstacle.collision.minX - definition.position.x,
                            definition.position.x - obstacle.collision.maxX));
                    const double distanceZ = std::max(
                        0.0,
                        std::max(
                            obstacle.collision.minZ - definition.position.z,
                            definition.position.z - obstacle.collision.maxZ));
                    const double required = definition.radius + 0.30;
                    return distanceX * distanceX + distanceZ * distanceZ <
                        required * required;
                });
            if (obstacleBlocked) {
                continue;
            }
            bool resourceBlocked = false;
            placementIndex.forEachNearby(
                definition.position,
                definition.radius + maximumResourceRadius +
                    PropPlacementClearance,
                [&](const SpatialEntry& entry) {
                    const std::size_t otherIndex =
                        static_cast<std::size_t>(entry.id.index);
                    if (otherIndex >= result.size()) {
                        return;
                    }
                    const ResourceNodeDefinition& other = result[otherIndex];
                    const double deltaX =
                        definition.position.x - other.position.x;
                    const double deltaZ =
                        definition.position.z - other.position.z;
                    const double required = definition.radius + other.radius +
                        PropPlacementClearance;
                    if (deltaX * deltaX + deltaZ * deltaZ <
                        required * required) {
                        resourceBlocked = true;
                    }
                });
            if (resourceBlocked) {
                continue;
            }
            const std::size_t acceptedIndex = result.size();
            result.push_back(definition);
            placementIndex.insert(
                {static_cast<std::uint32_t>(acceptedIndex), 0U},
                definition.position);
            maximumResourceRadius = std::max(
                maximumResourceRadius, definition.radius);
            break;
        }
    }
    placeOnDryTerrain(result);
    return result;
}

Vec3 resourceImpactPosition(
    std::span<const ResourceNode> nodes, EntityId id,
    Vec3 origin, Vec3 direction) {
    const auto node = std::find_if(
        nodes.begin(), nodes.end(),
        [id](const ResourceNode& candidate) {
            return candidate.id == id;
        });
    if (node == nodes.end()) {
        return origin;
    }
    const Vec3 offset{
        origin.x - node->position.x,
        origin.y - node->position.y,
        origin.z - node->position.z,
    };
    const double halfB =
        offset.x * direction.x +
        offset.y * direction.y +
        offset.z * direction.z;
    const double c =
        offset.x * offset.x + offset.y * offset.y +
        offset.z * offset.z -
        node->radius * node->radius;
    const double discriminant = halfB * halfB - c;
    if (discriminant < 0.0) {
        return node->position;
    }
    const double root = std::sqrt(discriminant);
    double distance = -halfB - root;
    if (distance < 0.0) {
        distance = -halfB + root;
    }
    if (distance < 0.0) {
        return node->position;
    }
    return {
        origin.x + direction.x * distance,
        origin.y + direction.y * distance,
        origin.z + direction.z * distance,
    };
}

} // namespace ian
