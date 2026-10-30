#pragma once

#include "core/Types.hpp"
#include <span>
#include <vector>

namespace ian {
class TerrainHeightfield;
struct WorldSceneryInstance;
struct ResourceNode;
struct LootChestInstance;

enum class SceneryLootKind { Chest, DestructibleProp };
struct SceneryLootPlacement {
    SceneryLootKind kind;
    EntityId id;
    Vec3 groundPosition;
    double yaw;
    std::size_t site;
};

// Relocate existing exploration loot; preserve item budgets, IDs and save format.
[[nodiscard]] std::vector<SceneryLootPlacement> generateWorldSceneryLoot(
    const TerrainHeightfield& terrain, double worldLimit,
    std::span<const WorldSceneryInstance> scenery,
    std::span<const ResourceNode> resources,
    std::span<const LootChestInstance> chests);
} // namespace ian
