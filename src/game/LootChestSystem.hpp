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
enum class LootRarity { Common, Uncommon, Rare, Legendary };
enum class LootUpgradeEffect {
    Damage,
    MoveSpeed,
    MaximumHealth,
    Apple,
    Bread,
    IronBar,
    FuelJerrycan,
    Compass,
    Nail,
    Key,
    Map,
    Anvil,
    Saw,
    Potion,
    Blueprint,
    Hourglass,
    Rope,
};
constexpr std::size_t LootUpgradeEffectCount = 17U;

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
    double pickupDelayRemaining{};
    double proximityPickupRadius{2.0};
    bool available{};
    bool collected{};
};

struct LootChestInstance {
    EntityId id;
    LootChestType type{LootChestType::Wooden};
    LootChestState state{LootChestState::Closed};
    Vec3 position;
    Vec3 surfaceNormal{0.0, 1.0, 0.0};
    double yaw{};
    int coinCost{};
    std::uint32_t rerollCount{};
    double rerollProgress{};
    LootUpgradeEffect rerollTargetEffect{LootUpgradeEffect::Damage};
    LootRarity rerollTargetRarity{LootRarity::Common};
    bool rerolling{};
    double openingProgress{};
    double disappearanceDelayRemaining{};
    double disappearanceProgress{};
    bool looseLoot{};
    bool revealed{};
    ChestLoot loot;
};

enum class ChestOpenResult { None, Opened, InsufficientCoins, AlreadyOpen };
enum class ChestRerollResult {
    None,
    Rerolled,
    InsufficientCoins,
    NotReady,
    AlreadyRerolled,
};

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
    [[nodiscard]] ChestOpenResult open(EntityId id, int& coins);
    [[nodiscard]] ChestRerollResult reroll(
        EntityId id, int& coins, int cost = 10);
    [[nodiscard]] std::optional<Vec3> revealNearest(
        Vec3 playerPosition);
    void setCoinCostMultiplier(double multiplier);
    [[nodiscard]] int openingCost(
        const LootChestInstance& chest) const;
    [[nodiscard]] std::optional<LootPickup> collect(EntityId id);
    [[nodiscard]] std::optional<LootPickup> collectNearby(
        Vec3 playerPosition, double radius);
    void spawnLooseLoot(Vec3 position, LootRarity rarity,
                        std::uint64_t seed);
    void spawnLooseLootEffect(
        Vec3 position, LootUpgradeEffect effect,
        LootRarity rarity, std::uint64_t seed,
        double pickupDelay = 0.0,
        double proximityPickupRadius = 0.72);

    void spawnAdditionalChests(
        int count, std::uint32_t terrainSeed,
        double worldLimit, const TerrainHeightfield& terrain,
        std::span<const ResourceNode> resources,
        Vec3 playerSpawn,
        std::optional<Vec3> preferredCenter = std::nullopt,
        double preferredRadius = 0.0);

    [[nodiscard]] const std::vector<LootChestInstance>& chests() const;

  private:
    std::vector<LootChestInstance> chests_;
    std::uint32_t runGeneration_{};
    std::uint32_t nextEntityIndex_{};
    double coinCostMultiplier_{1.0};
};

[[nodiscard]] const char* lootRarityName(LootRarity rarity);
[[nodiscard]] const char* lootUpgradeName(LootUpgradeEffect effect);
[[nodiscard]] const char* lootUpgradeDescription(LootUpgradeEffect effect);

// The presentation position is derived from the chest's local exit point.
// Keeping this in the simulation makes raycasts, pickup results, UI and
// rendering agree on sloped terrain as well as on flat ground.
[[nodiscard]] Vec3 lootVisualPosition(const LootChestInstance& chest);

// Placement uses a horizontal footprint: chests remain reserved props even
// while opening, and cannot be hidden inside foundations or other structures.
[[nodiscard]] bool lootChestOverlapsRectangle(
    std::span<const LootChestInstance> chests,
    double minimumX, double maximumX,
    double minimumZ, double maximumZ);

} // namespace ian
