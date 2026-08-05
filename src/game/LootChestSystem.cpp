#include "game/LootChestSystem.hpp"

#include "core/DeterministicRandom.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr std::size_t ChestCount = 10;
constexpr double OpeningDuration = 1.05;
constexpr double LootHoverHeight = 1.66;

double distanceSquared(Vec3 left, Vec3 right) {
    const double x = left.x - right.x;
    const double y = left.y - right.y;
    const double z = left.z - right.z;
    return x * x + y * y + z * z;
}

std::optional<double> raySphereDistance(
    Vec3 origin, Vec3 direction, Vec3 center, double radius) {
    const Vec3 offset{
        origin.x - center.x,
        origin.y - center.y,
        origin.z - center.z,
    };
    const double halfB = offset.x * direction.x +
        offset.y * direction.y + offset.z * direction.z;
    const double c = distanceSquared(origin, center) - radius * radius;
    const double discriminant = halfB * halfB - c;
    if (discriminant < 0.0) return std::nullopt;
    const double root = std::sqrt(discriminant);
    const double nearDistance = -halfB - root;
    if (nearDistance >= 0.0) return nearDistance;
    const double farDistance = -halfB + root;
    return farDistance >= 0.0 ? std::optional<double>{farDistance}
                              : std::nullopt;
}

ChestLoot makeLoot(EntityId chestId, Vec3 position) {
    const std::uint64_t seed =
        (static_cast<std::uint64_t>(chestId.generation) << 32U) |
        chestId.index;
    const double rarityRoll = unitRandom(seed ^ 0xa0761d6478bd642fULL);
    const LootRarity rarity = rarityRoll < 0.06
        ? LootRarity::Rare
        : rarityRoll < 0.30 ? LootRarity::Uncommon
                            : LootRarity::Common;
    const auto effect = static_cast<LootUpgradeEffect>(
        mixBits64(seed ^ 0xe7037ed1a0b428dbULL) % 3ULL);
    return {
        .id = {chestId.index, chestId.generation},
        .rarity = rarity,
        .effect = effect,
        .position = position,
    };
}

} // namespace

void LootChestSystem::reset(
    std::uint32_t terrainSeed, double worldLimit,
    const TerrainHeightfield& terrain,
    std::span<const ResourceNode> resources, Vec3 playerSpawn) {
    ++runGeneration_;
    if (runGeneration_ == 0U) ++runGeneration_;
    chests_.clear();
    chests_.reserve(ChestCount);
    constexpr std::size_t MaximumAttempts = 4096;
    for (std::size_t attempt = 0;
         attempt < MaximumAttempts && chests_.size() < ChestCount;
         ++attempt) {
        const std::uint64_t seed =
            static_cast<std::uint64_t>(terrainSeed) ^
            (static_cast<std::uint64_t>(attempt + 1U) *
             0x9e3779b97f4a7c15ULL);
        const double limit = std::max(1.0, worldLimit - 3.0);
        Vec3 position{
            (unitRandom(seed ^ 0x243f6a8885a308d3ULL) * 2.0 - 1.0) * limit,
            0.0,
            (unitRandom(seed ^ 0x13198a2e03707344ULL) * 2.0 - 1.0) * limit,
        };
        position.y = terrain.getHeight(position.x, position.z);
        if (terrain.waterSignedDistance(position.x, position.z) < 2.5 ||
            terrain.getNormal(position.x, position.z).y < 0.82 ||
            distanceSquared(position, playerSpawn) < 100.0) {
            continue;
        }
        const bool resourceBlocked = std::any_of(
            resources.begin(), resources.end(),
            [position](const ResourceNode& resource) {
                return resource.active &&
                    distanceSquared(position, resource.position) < 12.25;
            });
        const bool chestBlocked = std::any_of(
            chests_.begin(), chests_.end(),
            [position](const LootChestInstance& chest) {
                return distanceSquared(position, chest.position) < 64.0;
            });
        if (resourceBlocked || chestBlocked) continue;

        const std::uint32_t index =
            static_cast<std::uint32_t>(chests_.size());
        const LootChestType type = index % 3U == 2U
            ? LootChestType::Stone
            : LootChestType::Wooden;
        const EntityId id{index, runGeneration_};
        chests_.push_back({
            .id = id,
            .type = type,
            .position = position,
            .yaw = unitRandom(seed ^ 0x452821e638d01377ULL) *
                6.28318530717958647692,
            .goldCost = type == LootChestType::Wooden ? 15 : 30,
            .loot = makeLoot(id, position),
        });
    }
}

void LootChestSystem::tick(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) return;
    for (LootChestInstance& chest : chests_) {
        if (chest.state == LootChestState::Opening) {
            chest.openingProgress = std::min(
                1.0, chest.openingProgress + deltaSeconds / OpeningDuration);
            chest.loot.revealProgress = std::clamp(
                (chest.openingProgress - 0.14) / 0.44, 0.0, 1.0);
            chest.loot.available = chest.openingProgress >= 0.58;
            if (chest.openingProgress >= 1.0)
                chest.state = LootChestState::Open;
        }
        if (chest.loot.available && !chest.loot.collected)
            chest.loot.hoverTime += deltaSeconds;
    }
}

std::optional<EntityId> LootChestSystem::raycastChest(
    Vec3 origin, Vec3 direction, double maximumDistance) const {
    std::optional<EntityId> closest;
    double closestDistance = maximumDistance;
    for (const LootChestInstance& chest : chests_) {
        if (chest.state != LootChestState::Closed) continue;
        Vec3 center = chest.position;
        center.y += 0.55;
        const auto distance = raySphereDistance(
            origin, direction, center, 0.72);
        if (distance && *distance <= closestDistance) {
            closestDistance = *distance;
            closest = chest.id;
        }
    }
    return closest;
}

std::optional<EntityId> LootChestSystem::raycastLoot(
    Vec3 origin, Vec3 direction, double maximumDistance) const {
    std::optional<EntityId> closest;
    double closestDistance = maximumDistance;
    for (const LootChestInstance& chest : chests_) {
        if (!chest.loot.available || chest.loot.collected) continue;
        Vec3 center = chest.loot.position;
        center.y += LootHoverHeight +
            std::sin(chest.loot.hoverTime * 2.4) * 0.08;
        const auto distance = raySphereDistance(
            origin, direction, center, 0.52);
        if (distance && *distance <= closestDistance) {
            closestDistance = *distance;
            closest = chest.loot.id;
        }
    }
    return closest;
}

ChestOpenResult LootChestSystem::open(EntityId id, int& gold) {
    const auto chest = std::find_if(
        chests_.begin(), chests_.end(),
        [id](const LootChestInstance& value) { return value.id == id; });
    if (chest == chests_.end()) return ChestOpenResult::None;
    if (chest->state != LootChestState::Closed)
        return ChestOpenResult::AlreadyOpen;
    if (gold < chest->goldCost)
        return ChestOpenResult::InsufficientGold;
    gold -= chest->goldCost;
    chest->state = LootChestState::Opening;
    chest->openingProgress = 0.0;
    return ChestOpenResult::Opened;
}

std::optional<LootPickup> LootChestSystem::collect(EntityId id) {
    for (LootChestInstance& chest : chests_) {
        if (chest.loot.id != id || !chest.loot.available ||
            chest.loot.collected) continue;
        chest.loot.collected = true;
        return LootPickup{
            chest.loot.id, chest.loot.rarity,
            chest.loot.effect, chest.loot.position,
        };
    }
    return std::nullopt;
}

std::optional<LootPickup> LootChestSystem::collectNearby(
    Vec3 playerPosition, double radius) {
    for (LootChestInstance& chest : chests_) {
        Vec3 itemPosition = chest.loot.position;
        itemPosition.y += LootHoverHeight;
        if (chest.loot.available && !chest.loot.collected &&
            distanceSquared(playerPosition, itemPosition) <= radius * radius)
            return collect(chest.loot.id);
    }
    return std::nullopt;
}

const std::vector<LootChestInstance>& LootChestSystem::chests() const {
    return chests_;
}

const char* lootRarityName(LootRarity rarity) {
    switch (rarity) {
    case LootRarity::Common: return "COMMON";
    case LootRarity::Uncommon: return "UNCOMMON";
    case LootRarity::Rare: return "RARE";
    }
    return "";
}

const char* lootUpgradeName(LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Damage: return "Razor Charm";
    case LootUpgradeEffect::MoveSpeed: return "Wind Feather";
    case LootUpgradeEffect::MaximumHealth: return "Heartwood Seed";
    }
    return "Unknown Item";
}

} // namespace ian
