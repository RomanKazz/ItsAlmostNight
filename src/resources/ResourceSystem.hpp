#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"

#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace ian {

enum class ResourceType {
    Wood,
    Stone,
};

struct ResourceNode {
    EntityId id;
    ResourceType type;
    Vec3 position;
    double radius;
    double groundOffset{};
    double health;
    double maxHealth;
    int yield;
    int yieldRemaining;
    double respawnSeconds;
    double respawnRemaining;
    std::uint32_t respawnGeneration{};
    bool active;
};

struct ResourceNodeDefinition {
    ResourceType type;
    Vec3 position;
    double radius;
    double health;
    int yield;
    double respawnSeconds;
};

struct ResourceHit {
    EntityId nodeId;
    ResourceType type;
    Vec3 position;
    bool collected;
    int amount;
};

class ResourceSystem {
  public:
    using GroundHeightProvider =
        std::function<double(double, double)>;

    ResourceSystem();
    explicit ResourceSystem(
        std::vector<ResourceNodeDefinition> definitions,
        GroundHeightProvider groundHeight = {});

    void reset();
    void tick(
        double deltaSeconds,
        std::span<const BuildingInstance> buildings = {},
        double worldLimit = 48.0,
        std::optional<Vec3> playerPosition = std::nullopt);

    [[nodiscard]] std::optional<EntityId> raycast(Vec3 origin, Vec3 direction,
                                                  double maxDistance) const;
    std::optional<ResourceHit> damage(EntityId id, double amount);

    [[nodiscard]] const std::vector<ResourceNode>& nodes() const;

  private:
    [[nodiscard]] std::vector<ResourceNode> makeNodes() const;
    [[nodiscard]] std::optional<Vec3> findSafePosition(
        const ResourceNode& node,
        std::span<const BuildingInstance> buildings,
        double worldLimit,
        std::optional<Vec3> playerPosition) const;
    [[nodiscard]] bool positionIsSafe(
        const ResourceNode& node, Vec3 position,
        std::span<const BuildingInstance> buildings,
        double worldLimit,
        std::optional<Vec3> playerPosition,
        double buildingClearance) const;

    std::vector<ResourceNodeDefinition> definitions_;
    GroundHeightProvider groundHeight_;
    std::vector<ResourceNode> nodes_;
};

} // namespace ian
