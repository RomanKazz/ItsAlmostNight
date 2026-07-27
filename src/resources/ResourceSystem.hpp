#pragma once

#include "core/Types.hpp"

#include <optional>
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
    double health;
    double maxHealth;
    int yield;
    double respawnSeconds;
    double respawnRemaining;
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
    ResourceSystem();
    explicit ResourceSystem(std::vector<ResourceNodeDefinition> definitions);

    void reset();
    void tick(double deltaSeconds);

    [[nodiscard]] std::optional<EntityId> raycast(Vec3 origin, Vec3 direction,
                                                  double maxDistance) const;
    std::optional<ResourceHit> damage(EntityId id, double amount);

    [[nodiscard]] const std::vector<ResourceNode>& nodes() const;

  private:
    [[nodiscard]] std::vector<ResourceNode> makeNodes() const;

    std::vector<ResourceNodeDefinition> definitions_;
    std::vector<ResourceNode> nodes_;
};

} // namespace ian
