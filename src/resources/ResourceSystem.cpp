#include "resources/ResourceSystem.hpp"
#include "core/DeterministicRandom.hpp"
#include "core/Geometry.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <utility>

namespace ian {
namespace {

constexpr double BuildingClearance = 2.5;
constexpr double ActiveResourceBuildingClearance = 0.08;
constexpr double PlayerClearance = 3.0;
constexpr double ResourceClearance = 0.8;
constexpr std::uint32_t MaximumRelocationAttempts = 96;

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

struct ResourceCapacity {
    double health;
    int yield;
};

std::pair<double, double> resourceVisualTransform(
    EntityId id, ResourceType type,
    std::uint32_t respawnGeneration) {
    const std::uint64_t seed =
        (static_cast<std::uint64_t>(id.index) << 32U) ^
        (static_cast<std::uint64_t>(id.generation) << 8U) ^
        respawnGeneration;
    constexpr double TwoPi = 6.28318530717958647692;
    const double yaw = unitRandom(seed ^ 0x9e3779b97f4a7c15ULL) * TwoPi;
    const double scaleRoll =
        unitRandom(seed ^ 0xd1b54a32d192ed03ULL);
    const double scale = type == ResourceType::Wood
        ? 0.80 + scaleRoll * 0.45
        : type == ResourceType::Stone
            ? 0.90 + scaleRoll * 0.20
            : (0.88 + scaleRoll * 0.22) * 1.40;
    return {yaw, scale};
}

std::size_t resourceVisualVariant(
    EntityId id, ResourceType type,
    std::size_t authoredVariant,
    std::uint32_t respawnGeneration) {
    if (type != ResourceType::Stone) {
        return authoredVariant % TreeVisualVariantCount;
    }
    const std::uint64_t seed =
        (static_cast<std::uint64_t>(id.index) << 32U) ^
        (static_cast<std::uint64_t>(id.generation) << 8U) ^
        static_cast<std::uint64_t>(respawnGeneration) ^
        0xa24baed4963ee407ULL;
    return static_cast<std::size_t>(
        mixBits64(seed) % StoneVisualVariantCount);
}

ResourceCapacity variedCapacity(
    double baseHealth, int baseYield, EntityId id,
    std::uint32_t respawnGeneration) {
    const std::uint64_t seed =
        (static_cast<std::uint64_t>(id.index) << 32U) ^
        static_cast<std::uint64_t>(respawnGeneration);
    const double roll = unitRandom(seed);
    double extraHealth = 0.0;
    if (roll >= 0.95) {
        extraHealth = 4.0;
    } else if (roll >= 0.84) {
        extraHealth = 2.0;
    } else if (roll >= 0.62) {
        extraHealth = 1.0;
    }
    const double health = baseHealth + extraHealth;
    const int yield = baseYield <= 0
        ? 0
        : std::max(
              1, static_cast<int>(std::lround(
                     static_cast<double>(baseYield) *
                     health / baseHealth)));
    return {health, yield};
}

} // namespace

ResourceSystem::ResourceSystem() : ResourceSystem(defaultDefinitions()) {}

ResourceSystem::ResourceSystem(
    std::vector<ResourceNodeDefinition> definitions,
    GroundHeightProvider groundHeight,
    GroundSafetyProvider groundSafety)
    : definitions_(std::move(definitions)),
      groundHeight_(std::move(groundHeight)),
      groundSafety_(std::move(groundSafety)),
      nodes_(makeNodes()) {}

std::vector<ResourceNode> ResourceSystem::makeNodes() const {
    std::vector<ResourceNode> nodes;
    nodes.reserve(definitions_.size());
    for (std::size_t index = 0; index < definitions_.size(); ++index) {
        const auto& definition = definitions_[index];
        const EntityId id{
            static_cast<std::uint32_t>(index),
            runGeneration_};
        const ResourceCapacity capacity =
            variedCapacity(
                definition.health, definition.yield,
                id, 0);
        const auto [visualYaw, visualScale] =
            resourceVisualTransform(
                id, definition.type, 0U);
        const double groundOffset =
            groundHeight_
                ? definition.position.y -
                      groundHeight_(
                          definition.position.x,
                          definition.position.z)
                : definition.position.y;
        nodes.push_back({
            .id = id,
            .type = definition.type,
            .position = definition.position,
            .radius = definition.radius,
            .groundOffset = groundOffset,
            .health = capacity.health,
            .maxHealth = capacity.health,
            .yield = capacity.yield,
            .yieldRemaining = capacity.yield,
            .respawnSeconds = definition.respawnSeconds,
            .respawnRemaining = 0.0,
            .respawnGeneration = 0,
            .visualYaw = visualYaw,
            .visualScale = visualScale,
            .visualVariant = resourceVisualVariant(
                id, definition.type,
                definition.visualVariant, 0U),
            .active = true,
        });
    }
    return nodes;
}

void ResourceSystem::reset() {
    ++runGeneration_;
    if (runGeneration_ == 0U) {
        ++runGeneration_;
    }
    woodYieldMultiplier_ = 1.0;
    nodes_ = makeNodes();
    collisionGeometryDirty_ = true;
}

void ResourceSystem::setWoodYieldMultiplier(double multiplier) {
    const double next = std::max(0.0, multiplier);
    if (std::abs(next - woodYieldMultiplier_) <= 1e-9) {
        return;
    }
    const double ratio = woodYieldMultiplier_ > 0.0
        ? next / woodYieldMultiplier_ : next;
    for (ResourceNode& node : nodes_) {
        if (node.type != ResourceType::Wood) {
            continue;
        }
        const int previousYield = node.yield;
        const int nextYield = std::max(
            1,
            static_cast<int>(std::lround(
                static_cast<double>(previousYield) * ratio)));
        node.yield = nextYield;
        node.yieldRemaining = std::clamp(
            node.yieldRemaining + nextYield - previousYield,
            0, nextYield);
    }
    woodYieldMultiplier_ = next;
}

void ResourceSystem::tick(
    double deltaSeconds,
    std::span<const BuildingInstance> buildings,
    double worldLimit,
    std::optional<Vec3> playerPosition) {
    const bool validateActivePositions = deltaSeconds <= 0.0;
    for (auto& node : nodes_) {
        if (validateActivePositions && node.active &&
            !positionIsSafe(
                node, node.position, buildings, worldLimit,
                std::nullopt,
                ActiveResourceBuildingClearance)) {
            node.active = false;
            node.respawnRemaining = 0.0;
            ++node.respawnGeneration;
            collisionGeometryDirty_ = true;
        }
        if (node.active) {
            continue;
        }

        node.respawnRemaining = std::max(0.0, node.respawnRemaining - deltaSeconds);
        if (node.respawnRemaining <= 0.0) {
            const auto position = findSafePosition(
                node, buildings, worldLimit, playerPosition);
            if (!position) {
                continue;
            }
            node.position = *position;
            const auto [visualYaw, visualScale] =
                resourceVisualTransform(
                    node.id, node.type,
                    node.respawnGeneration);
            node.visualYaw = visualYaw;
            node.visualScale = visualScale;
            node.visualVariant = resourceVisualVariant(
                node.id, node.type, node.visualVariant,
                node.respawnGeneration);
            const auto definitionIndex =
                static_cast<std::size_t>(node.id.index);
            if (definitionIndex < definitions_.size()) {
                const auto& definition =
                    definitions_[definitionIndex];
                const ResourceCapacity capacity =
                    variedCapacity(
                        definition.health,
                        definition.yield, node.id,
                        node.respawnGeneration);
                node.maxHealth = capacity.health;
                node.yield = capacity.yield;
                if (node.type == ResourceType::Wood) {
                    node.yield = std::max(
                        1,
                        static_cast<int>(std::lround(
                            static_cast<double>(node.yield) *
                            woodYieldMultiplier_)));
                }
            }
            node.health = node.maxHealth;
            node.yieldRemaining = node.yield;
            node.active = true;
            collisionGeometryDirty_ = true;
        }
    }
}

bool ResourceSystem::consumeCollisionGeometryDirty() {
    const bool dirty = collisionGeometryDirty_;
    collisionGeometryDirty_ = false;
    return dirty;
}

std::optional<Vec3> ResourceSystem::findSafePosition(
    const ResourceNode& node,
    std::span<const BuildingInstance> buildings,
    double worldLimit,
    std::optional<Vec3> playerPosition) const {
    const double usableLimit =
        std::max(1.0, worldLimit - node.radius - 1.0);
    for (std::uint32_t attempt = 0;
         attempt < MaximumRelocationAttempts; ++attempt) {
        const std::uint64_t seed =
            (static_cast<std::uint64_t>(node.id.index) << 32U) ^
            (static_cast<std::uint64_t>(
                 node.respawnGeneration)
             << 8U) ^
            static_cast<std::uint64_t>(attempt);
        const double candidateX =
            (unitRandom(seed) * 2.0 - 1.0) * usableLimit;
        const double candidateZ =
            (unitRandom(
                 seed ^ 0xd1b54a32d192ed03ULL) *
                 2.0 - 1.0) *
            usableLimit;
        const Vec3 candidate{
            candidateX,
            groundHeight_
                ? groundHeight_(candidateX, candidateZ) +
                      node.groundOffset
                : node.position.y,
            candidateZ,
        };
        if (positionIsSafe(
                node, candidate, buildings, worldLimit,
                playerPosition, BuildingClearance)) {
            return candidate;
        }
    }
    return std::nullopt;
}

bool ResourceSystem::positionIsSafe(
    const ResourceNode& node, Vec3 position,
    std::span<const BuildingInstance> buildings,
    double worldLimit,
    std::optional<Vec3> playerPosition,
    double buildingClearance) const {
    const double boundary =
        worldLimit - node.radius - 1.0;
    if (std::abs(position.x) > boundary ||
        std::abs(position.z) > boundary) {
        return false;
    }
    if (groundSafety_ &&
        !groundSafety_(position.x, position.z, node.radius)) {
        return false;
    }

    for (const auto& building : buildings) {
        const Vec3 center = buildingWorldPosition(building);
        const double halfExtent =
            buildingFootprintHalfExtent(building.type);
        const double distanceX = std::max(
            0.0, std::abs(position.x - center.x) -
                     halfExtent);
        const double distanceZ = std::max(
            0.0, std::abs(position.z - center.z) -
                     halfExtent);
        const double required =
            node.radius + buildingClearance;
        if (distanceX * distanceX +
                distanceZ * distanceZ <
            required * required) {
            return false;
        }
    }

    if (playerPosition) {
        const double distanceX =
            position.x - playerPosition->x;
        const double distanceZ =
            position.z - playerPosition->z;
        const double required =
            node.radius + PlayerClearance;
        if (distanceX * distanceX +
                distanceZ * distanceZ <
            required * required) {
            return false;
        }
    }

    for (const auto& other : nodes_) {
        if (!other.active || other.id == node.id) {
            continue;
        }
        const double distanceX =
            position.x - other.position.x;
        const double distanceZ =
            position.z - other.position.z;
        const double required =
            node.radius + other.radius +
            ResourceClearance;
        if (distanceX * distanceX +
                distanceZ * distanceZ <
            required * required) {
            return false;
        }
    }
    return true;
}

std::optional<EntityId> ResourceSystem::raycast(Vec3 origin, Vec3 direction,
                                                double maxDistance) const {
    std::optional<EntityId> result;
    double closestDistance = std::numeric_limits<double>::max();

    for (const auto& node : nodes_) {
        if (!node.active) {
            continue;
        }

        const auto distance = geometry::raySphereDistance(
            origin, direction, node.position, node.radius);
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
    int grantedAmount = 0;
    if (isDestructibleProp(iterator->type)) {
        iterator->yieldRemaining = 0;
    }
    if (collected) {
        grantedAmount = iterator->yieldRemaining;
    } else {
        const double depletedFraction =
            1.0 - iterator->health / iterator->maxHealth;
        const int targetGranted = static_cast<int>(
            std::floor(static_cast<double>(iterator->yield) *
                           depletedFraction +
                       1e-9));
        const int alreadyGranted =
            iterator->yield - iterator->yieldRemaining;
        grantedAmount =
            std::clamp(targetGranted - alreadyGranted, 0,
                       iterator->yieldRemaining);
    }
    iterator->yieldRemaining -= grantedAmount;
    if (collected) {
        iterator->active = false;
        iterator->respawnRemaining = iterator->respawnSeconds;
        ++iterator->respawnGeneration;
        collisionGeometryDirty_ = true;
    }

    return ResourceHit{
        .nodeId = iterator->id,
        .type = iterator->type,
        .position = iterator->position,
        .collected = collected,
        .amount = grantedAmount,
    };
}

const std::vector<ResourceNode>& ResourceSystem::nodes() const {
    return nodes_;
}

} // namespace ian
