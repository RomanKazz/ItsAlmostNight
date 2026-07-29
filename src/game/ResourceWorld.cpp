#include "game/ResourceWorld.hpp"

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
    if (configured.size() < 7U || worldLimit < 16.0) {
        for (auto& definition : result) {
            definition.position.y += terrain.getHeight(
                definition.position.x,
                definition.position.z);
        }
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

    constexpr std::size_t AdditionalCount = 48;
    constexpr double GoldenAngle = 2.39996322972865332;
    const double innerRadius =
        std::min(10.0, worldLimit * 0.3);
    const double outerRadius =
        std::max(innerRadius + 1.0, worldLimit - 3.0);
    result.reserve(result.size() + AdditionalCount);
    for (std::size_t index = 0;
         index < AdditionalCount; ++index) {
        const ResourceType type =
            index % 5U < 3U
                ? ResourceType::Wood
                : ResourceType::Stone;
        ResourceNodeDefinition definition = templateFor(type);
        const double radialProgress =
            (static_cast<double>(index) + 0.5) /
            static_cast<double>(AdditionalCount);
        const double radius = std::sqrt(
            innerRadius * innerRadius +
            radialProgress *
                (outerRadius * outerRadius -
                 innerRadius * innerRadius));
        const double angle =
            static_cast<double>(index) * GoldenAngle +
            (type == ResourceType::Wood ? 0.31 : 0.87);
        definition.position = {
            std::cos(angle) * radius,
            type == ResourceType::Wood ? 1.0 : 0.8,
            std::sin(angle) * radius,
        };
        result.push_back(definition);
    }
    for (auto& definition : result) {
        definition.position.y += terrain.getHeight(
            definition.position.x,
            definition.position.z);
    }
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
