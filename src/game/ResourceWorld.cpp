#include "game/ResourceWorld.hpp"

#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr double ResourcePlacementClearance = 0.08;

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
    const TerrainHeightfield& terrain) {
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
            return type == ResourceType::Wood
                       ? ResourceNodeDefinition{
                             type, {}, 1.0, 3.0, 15, 12.0}
                       : ResourceNodeDefinition{
                             type, {}, 0.9, 4.0, 12, 15.0};
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
    const std::size_t treeClusterCount =
        std::max<std::size_t>(
            1U, additionalTreeCount / 6U);
    const std::size_t clusteredTreeCount = std::min(
        additionalTreeCount,
        treeClusterCount * TreesPerCluster);
    result.reserve(
        result.size() + additionalTreeCount +
        additionalStoneCount);

    const double clusterOuterRadius =
        std::max(innerRadius + 1.0, outerRadius - 5.0);
    for (std::size_t index = 0;
         index < additionalTreeCount; ++index) {
        ResourceNodeDefinition definition =
            templateFor(ResourceType::Wood);
        if (index >= clusteredTreeCount) {
            const std::size_t singleIndex =
                index - clusteredTreeCount;
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
                1.73;
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
        const double clusterProgress =
            (static_cast<double>(cluster) + 0.65) /
            static_cast<double>(treeClusterCount);
        const double clusterRadius = std::sqrt(
            innerRadius * innerRadius +
            clusterProgress *
                (clusterOuterRadius * clusterOuterRadius -
                 innerRadius * innerRadius));
        const double clusterAngle =
            static_cast<double>(cluster) * GoldenAngle + 0.31;
        const std::uint64_t seed =
            0x51f15e5dULL +
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
            (unitRandom(
                 0x8f3f73b5ULL +
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

    // Sparse interactive clutter. These use the resource target pipeline for
    // precise hover, health bars and collision, but have their own rewards.
    const std::size_t propCount = static_cast<std::size_t>(
        std::lround(12.0 * std::sqrt(densityScale)));
    for (std::size_t index = 0; index < propCount; ++index) {
        const std::uint64_t seed =
            0xa0761d6478bd642fULL + index * 0xe7037ed1a0b428dbULL;
        const double progress =
            (static_cast<double>(index) + 0.6) /
            static_cast<double>(propCount);
        const double radius = std::sqrt(
            (innerRadius + 2.0) * (innerRadius + 2.0) + progress *
            (outerRadius * outerRadius -
             (innerRadius + 2.0) * (innerRadius + 2.0)));
        const double angle = static_cast<double>(index) * GoldenAngle +
            unitRandom(seed) * 0.9;
        const double roll = unitRandom(seed ^ 0xd1b54a32d192ed03ULL);
        const ResourceType type = roll < 0.46
            ? ResourceType::Barrel
            : roll < 0.82 ? ResourceType::Crate
                          : ResourceType::ItemCrate;
        const double health = type == ResourceType::Barrel ? 4.0 : 5.0;
        result.push_back({
            type,
            {std::cos(angle) * radius, 0.62,
             std::sin(angle) * radius},
            0.68, health, 0, 75.0,
        });
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
