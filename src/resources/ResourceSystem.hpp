#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"

#include <functional>
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace ian {

inline constexpr std::size_t TreeVisualVariantCount = 3U;
inline constexpr std::array<double, TreeVisualVariantCount>
    TreeVisualGroundOffsets{0.262, 0.290, 0.208};

enum class ResourceType {
    Wood,
    Stone,
    Barrel,
    Crate,
    ItemCrate,
};

[[nodiscard]] constexpr bool isHarvestableResource(ResourceType type) {
    return type == ResourceType::Wood || type == ResourceType::Stone;
}

[[nodiscard]] constexpr bool isDestructibleProp(ResourceType type) {
    return type == ResourceType::Barrel || type == ResourceType::Crate ||
           type == ResourceType::ItemCrate;
}

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
    double visualYaw{};
    double visualScale{1.0};
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
    using GroundSafetyProvider =
        std::function<bool(double, double, double)>;

    ResourceSystem();
    explicit ResourceSystem(
        std::vector<ResourceNodeDefinition> definitions,
        GroundHeightProvider groundHeight = {},
        GroundSafetyProvider groundSafety = {});

    void reset();
    void setWoodYieldMultiplier(double multiplier);
    void tick(
        double deltaSeconds,
        std::span<const BuildingInstance> buildings = {},
        double worldLimit = 48.0,
        std::optional<Vec3> playerPosition = std::nullopt);

    // Resource collision geometry only changes when a node is relocated,
    // respawned, depleted, or the resource set is reset. Consumers can use
    // this flag to avoid rebuilding all tree colliders every simulation tick.
    [[nodiscard]] bool consumeCollisionGeometryDirty();

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
    GroundSafetyProvider groundSafety_;
    std::uint32_t runGeneration_{1U};
    std::vector<ResourceNode> nodes_;
    bool collisionGeometryDirty_{true};
    double woodYieldMultiplier_{1.0};
};

} // namespace ian
