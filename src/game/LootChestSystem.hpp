#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ian {

class TerrainHeightfield;
struct ResourceNode;

enum class LootChestType { Wooden, Stone };
enum class LootChestState { Closed, Opening, Open };
enum class LootRarity { Common, Uncommon, Rare };
enum class LootUpgradeEffect {
    Damage,
    MoveSpeed,
    MaximumHealth,
    Apple,
    Bread,
};
constexpr std::size_t LootUpgradeEffectCount = 5U;

[[nodiscard]] constexpr std::size_t lootUpgradeIndex(
    LootUpgradeEffect effect) {
    return static_cast<std::size_t>(effect);
}

struct ChestLoot {
    EntityId id;
    LootRarity rarity{LootRarity::Common};
    LootUpgradeEffect effect{LootUpgradeEffect::Damage};
    Vec3 position;
    double revealProgress{};
    double hoverTime{};
    bool available{};
    bool collected{};
};

struct LootChestInstance {
    EntityId id;
    LootChestType type{LootChestType::Wooden};
    LootChestState state{LootChestState::Closed};
    Vec3 position;
    double yaw{};
    int goldCost{};
    double openingProgress{};
    ChestLoot loot;
};

enum class ChestOpenResult { None, Opened, InsufficientGold, AlreadyOpen };

struct LootPickup {
    EntityId lootId;
    LootRarity rarity;
    LootUpgradeEffect effect;
    Vec3 position;
};

class LootChestSystem {
  public:
    void reset(std::uint32_t terrainSeed, double worldLimit,
               const TerrainHeightfield& terrain,
               std::span<const ResourceNode> resources,
               Vec3 playerSpawn);
    void tick(double deltaSeconds);

    [[nodiscard]] std::optional<EntityId> raycastChest(
        Vec3 origin, Vec3 direction, double maximumDistance) const;
    [[nodiscard]] std::optional<EntityId> raycastLoot(
        Vec3 origin, Vec3 direction, double maximumDistance) const;
    [[nodiscard]] ChestOpenResult open(EntityId id, int& gold);
    [[nodiscard]] std::optional<LootPickup> collect(EntityId id);
    [[nodiscard]] std::optional<LootPickup> collectNearby(
        Vec3 playerPosition, double radius);

    [[nodiscard]] const std::vector<LootChestInstance>& chests() const;

  private:
    std::vector<LootChestInstance> chests_;
    std::uint32_t runGeneration_{};
};

[[nodiscard]] const char* lootRarityName(LootRarity rarity);
[[nodiscard]] const char* lootUpgradeName(LootUpgradeEffect effect);
[[nodiscard]] const char* lootUpgradeDescription(LootUpgradeEffect effect);

} // namespace ian
