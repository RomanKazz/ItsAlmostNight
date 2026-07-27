#include "resources/ResourceSystem.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <utility>

namespace ian {
namespace {

std::vector<ResourceNodeDefinition> defaultDefinitions() {
    return {
        {ResourceType::Wood, {0.0, 1.0, 2.5}, 1.0, 3.0, 15, 12.0},
        {ResourceType::Wood, {-4.0, 1.0, -2.0}, 1.0, 3.0, 15, 12.0},
        {ResourceType::Wood, {5.0, 1.0, -5.0}, 1.0, 3.0, 15, 12.0},
        {ResourceType::Wood, {-8.0, 1.0, -9.0}, 1.0, 3.0, 15, 12.0},
        {ResourceType::Stone, {3.0, 0.8, 4.0}, 0.9, 4.0, 12, 15.0},
        {ResourceType::Stone, {8.0, 0.8, -1.0}, 0.9, 4.0, 12, 15.0},
        {ResourceType::Stone, {-6.0, 0.8, -5.0}, 0.9, 4.0, 12, 15.0},
    };
}

double dot(Vec3 left, Vec3 right) {
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

Vec3 subtract(Vec3 left, Vec3 right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

std::optional<double> raySphereDistance(Vec3 origin, Vec3 direction, const ResourceNode& node) {
    const Vec3 offset = subtract(origin, node.position);
    const double halfB = dot(offset, direction);
    const double c = dot(offset, offset) - (node.radius * node.radius);
    const double discriminant = (halfB * halfB) - c;
    if (discriminant < 0.0) {
        return std::nullopt;
    }

    const double root = std::sqrt(discriminant);
    const double nearDistance = -halfB - root;
    if (nearDistance >= 0.0) {
        return nearDistance;
    }

    const double farDistance = -halfB + root;
    return farDistance >= 0.0 ? std::optional<double>{farDistance} : std::nullopt;
}

} // namespace

ResourceSystem::ResourceSystem() : ResourceSystem(defaultDefinitions()) {}

ResourceSystem::ResourceSystem(std::vector<ResourceNodeDefinition> definitions)
    : definitions_(std::move(definitions)), nodes_(makeNodes()) {}

std::vector<ResourceNode> ResourceSystem::makeNodes() const {
    std::vector<ResourceNode> nodes;
    nodes.reserve(definitions_.size());
    for (std::size_t index = 0; index < definitions_.size(); ++index) {
        const auto& definition = definitions_[index];
        nodes.push_back({
            .id = {static_cast<std::uint32_t>(index), 1},
            .type = definition.type,
            .position = definition.position,
            .radius = definition.radius,
            .health = definition.health,
            .maxHealth = definition.health,
            .yield = definition.yield,
            .respawnSeconds = definition.respawnSeconds,
            .respawnRemaining = 0.0,
            .active = true,
        });
    }
    return nodes;
}

void ResourceSystem::reset() {
    nodes_ = makeNodes();
}

void ResourceSystem::tick(double deltaSeconds) {
    for (auto& node : nodes_) {
        if (node.active) {
            continue;
        }

        node.respawnRemaining = std::max(0.0, node.respawnRemaining - deltaSeconds);
        if (node.respawnRemaining <= 0.0) {
            node.health = node.maxHealth;
            node.active = true;
        }
    }
}

std::optional<EntityId> ResourceSystem::raycast(Vec3 origin, Vec3 direction,
                                                double maxDistance) const {
    std::optional<EntityId> result;
    double closestDistance = std::numeric_limits<double>::max();

    for (const auto& node : nodes_) {
        if (!node.active) {
            continue;
        }

        const auto distance = raySphereDistance(origin, direction, node);
        if (distance && *distance <= maxDistance && *distance < closestDistance) {
            result = node.id;
            closestDistance = *distance;
        }
    }
    return result;
}

std::optional<ResourceHit> ResourceSystem::damage(EntityId id, double amount) {
    const auto iterator = std::find_if(nodes_.begin(), nodes_.end(),
                                       [id](const ResourceNode& node) { return node.id == id; });
    if (iterator == nodes_.end() || !iterator->active || amount <= 0.0) {
        return std::nullopt;
    }

    iterator->health = std::max(0.0, iterator->health - amount);
    const bool collected = iterator->health <= 0.0;
    if (collected) {
        iterator->active = false;
        iterator->respawnRemaining = iterator->respawnSeconds;
    }

    return ResourceHit{
        .nodeId = iterator->id,
        .type = iterator->type,
        .position = iterator->position,
        .collected = collected,
        .amount = collected ? iterator->yield : 0,
    };
}

const std::vector<ResourceNode>& ResourceSystem::nodes() const {
    return nodes_;
}

} // namespace ian
