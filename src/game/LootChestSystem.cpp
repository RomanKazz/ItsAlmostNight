#include "game/LootChestSystem.hpp"

#include "core/DeterministicRandom.hpp"
#include "core/Geometry.hpp"
#include "core/SurfaceBasis.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr std::size_t ChestCount = 10;
constexpr double ExplorationMinimumRadius = 48.0;
constexpr double ExplorationMaximumRadius = 120.0;
constexpr double OpeningDuration = 1.05;
constexpr double LooseLootRevealDuration = 0.38;
constexpr double ChestDisappearanceDelay = 1.15;
constexpr double ChestDisappearanceDuration = 0.35;
constexpr double RerollDuration = 0.48;
// These values are shared by the simulation position, raycast, prompt and
// render passes. Keep the reward above the chest without adding a permanent
// forward displacement that becomes visible on sloped chests.
constexpr double LootExitHeight = 0.56;
// Previous final offset was 1.62. Keep the authored lid exit, then reduce the
// complete hovering height to exactly 80%: 0.56 + 0.736 = 1.296.
constexpr double LootRiseHeight = 0.736;
constexpr double LootBobAmplitude = 0.072;
// A slightly generous silhouette makes a hovering reward easy to target
// without making nearby chest interactions ambiguous.
constexpr double LootRaycastRadius = 0.72;

ChestLoot makeLoot(EntityId chestId, Vec3 position,
                   std::uint64_t reroll = 0U,
                   LootChestPurpose purpose =
                       LootChestPurpose::Exploration) {
    const std::uint64_t seed =
        (static_cast<std::uint64_t>(chestId.generation) << 32U) |
        chestId.index;
    const std::uint64_t roll =
        mixBits64(seed ^ 0xe7037ed1a0b428dbULL ^
                  mixBits64(reroll + 0x9e3779b97f4a7c15ULL));
    constexpr std::array<LootUpgradeEffect, 7> CommonLoot{{
        LootUpgradeEffect::Apple,
        LootUpgradeEffect::Bread,
        LootUpgradeEffect::IronBar,
        LootUpgradeEffect::FuelJerrycan,
        LootUpgradeEffect::Compass,
        LootUpgradeEffect::Nail,
        LootUpgradeEffect::Key,
    }};
    constexpr std::array<LootUpgradeEffect, 7> RareLoot{{
        LootUpgradeEffect::Map,
        LootUpgradeEffect::Anvil,
        LootUpgradeEffect::Saw,
        LootUpgradeEffect::Potion,
        LootUpgradeEffect::Blueprint,
        LootUpgradeEffect::Hourglass,
        LootUpgradeEffect::Rope,
    }};
    // Distant exploration chests demand a meaningful detour during the short
    // day, so they deserve a noticeably better rare-item chance than the
    // safely delivered post-wave rewards.
    const std::uint64_t rareThreshold =
        purpose == LootChestPurpose::Exploration ? 60ULL : 72ULL;
    const bool rare = roll % 100ULL >= rareThreshold;
    const std::span<const LootUpgradeEffect> pool = rare
        ? std::span<const LootUpgradeEffect>{RareLoot}
        : std::span<const LootUpgradeEffect>{CommonLoot};
    const LootUpgradeEffect effect = pool[
        (roll / 100ULL) % pool.size()];
    return {
        .id = {chestId.index, chestId.generation},
        .rarity = rare ? LootRarity::Rare : LootRarity::Common,
        .effect = effect,
        .position = position,
    };
}

Vec3 surfaceVector(const SurfaceBasis& basis, Vec3 local) {
    return {
        basis.right.x * local.x + basis.up.x * local.y +
            basis.forward.x * local.z,
        basis.right.y * local.x + basis.up.y * local.y +
            basis.forward.y * local.z,
        basis.right.z * local.x + basis.up.z * local.y +
            basis.forward.z * local.z,
    };
}

Vec3 lootVisualPositionImpl(const LootChestInstance& chest) {
    const double reveal = std::clamp(chest.loot.revealProgress, 0.0, 1.0);
    // Loose crate drops use revealProgress for the same scale-in animation as
    // chest loot, but they have no lid to fly out of. Keep their established
    // final hover position while only their visual scale is revealed.
    const double motionReveal = chest.looseLoot ? 1.0 : reveal;
    const double eased = 1.0 -
        std::pow(1.0 - motionReveal, 3.0);
    const SurfaceBasis basis = makeSurfaceBasis(
        chest.surfaceNormal, chest.yaw);
    const Vec3 localExit{0.0, LootExitHeight, 0.0};
    const Vec3 localImpulse{0.0, 0.26, 1.0};
    const double impulseLength = std::sqrt(
        localImpulse.x * localImpulse.x +
        localImpulse.y * localImpulse.y +
        localImpulse.z * localImpulse.z);
    const Vec3 impulse = surfaceVector(
        basis, {localImpulse.x / impulseLength,
                localImpulse.y / impulseLength,
                localImpulse.z / impulseLength});
    const Vec3 exitOffset = surfaceVector(basis, localExit);
    const double arc = eased * (1.0 - eased) * 0.62;
    const double rise = eased * LootRiseHeight +
        std::sin(chest.loot.hoverTime * 2.4) * LootBobAmplitude;
    return {
        chest.position.x + exitOffset.x + impulse.x * arc +
            basis.up.x * rise,
        chest.position.y + exitOffset.y + impulse.y * arc +
            basis.up.y * rise,
        chest.position.z + exitOffset.z + impulse.z * arc +
            basis.up.z * rise,
    };
}

} // namespace

Vec3 lootVisualPosition(const LootChestInstance& chest) {
    return lootVisualPositionImpl(chest);
}

bool lootChestOverlapsRectangle(
    std::span<const LootChestInstance> chests,
    double minimumX, double maximumX,
    double minimumZ, double maximumZ) {
    constexpr double PlacementRadius = 0.82;
    return std::any_of(
        chests.begin(), chests.end(),
        [=](const LootChestInstance& chest) {
            if (chest.looseLoot) {
                return false;
            }
            const double distanceX = std::max(
                0.0,
                std::max(
                    minimumX - chest.position.x,
                    chest.position.x - maximumX));
            const double distanceZ = std::max(
                0.0,
                std::max(
                    minimumZ - chest.position.z,
                    chest.position.z - maximumZ));
            return distanceX * distanceX +
                       distanceZ * distanceZ <=
                   PlacementRadius * PlacementRadius;
        });
}

void LootChestSystem::reset(
    std::uint32_t terrainSeed, double worldLimit,
    const TerrainHeightfield& terrain,
    std::span<const ResourceNode> resources, Vec3 playerSpawn) {
    ++runGeneration_;
    if (runGeneration_ == 0U) ++runGeneration_;
    nextEntityIndex_ = 0U;
    chests_.clear();
    chests_.reserve(ChestCount);
    spawnAdditionalChests(
        static_cast<int>(ChestCount), terrainSeed, worldLimit,
        terrain, resources, playerSpawn, playerSpawn,
        ExplorationMaximumRadius, ExplorationMinimumRadius,
        LootChestPurpose::Exploration);
}

void LootChestSystem::spawnAdditionalChests(
    int count, std::uint32_t terrainSeed, double worldLimit,
    const TerrainHeightfield& terrain,
    std::span<const ResourceNode> resources, Vec3 playerSpawn,
    std::optional<Vec3> preferredCenter,
    double preferredRadius,
    double preferredMinimumRadius,
    LootChestPurpose purpose) {
    if (count <= 0) return;
    constexpr std::size_t MaximumAttempts = 4096;
    const std::size_t targetCount = chests_.size() +
        static_cast<std::size_t>(count);
    for (std::size_t attempt = 0;
         attempt < MaximumAttempts && chests_.size() < targetCount;
         ++attempt) {
        const std::size_t chestIndex = chests_.size();
        const std::uint64_t seed =
            static_cast<std::uint64_t>(terrainSeed) ^
            (static_cast<std::uint64_t>(attempt + 1U) *
             0x9e3779b97f4a7c15ULL) ^
            (static_cast<std::uint64_t>(chestIndex + 1U) *
             0xd1b54a32d192ed03ULL);
        const double limit = std::max(1.0, worldLimit - 3.0);
        Vec3 position{};
        const double minimumRadius = std::max(
            0.0, preferredMinimumRadius);
        if (preferredCenter && preferredRadius > minimumRadius) {
            constexpr double TwoPi =
                6.28318530717958647692;
            const double angle =
                unitRandom(seed ^ 0x243f6a8885a308d3ULL) *
                TwoPi;
            const double outerRadius = std::max(
                minimumRadius,
                std::min(preferredRadius, limit));
            const double radius = std::sqrt(
                minimumRadius * minimumRadius +
                unitRandom(seed ^ 0x13198a2e03707344ULL) *
                    (outerRadius * outerRadius -
                     minimumRadius * minimumRadius));
            position.x = preferredCenter->x +
                std::cos(angle) * radius;
            position.z = preferredCenter->z +
                std::sin(angle) * radius;
        } else {
            position.x =
                (unitRandom(seed ^ 0x243f6a8885a308d3ULL) *
                     2.0 -
                 1.0) *
                limit;
            position.z =
                (unitRandom(seed ^ 0x13198a2e03707344ULL) *
                     2.0 -
                 1.0) *
                limit;
        }
        if (!terrain.isInside(position.x, position.z)) {
            continue;
        }
        position.y = terrain.getHeight(position.x, position.z);
        if (terrain.waterSignedDistance(position.x, position.z) < 2.5 ||
            terrain.getNormal(position.x, position.z).y < 0.82 ||
            geometry::distanceSquared(position, playerSpawn) < 100.0) {
            continue;
        }
        const bool resourceBlocked = std::any_of(
            resources.begin(), resources.end(),
            [position](const ResourceNode& resource) {
                return resource.active &&
                    geometry::distanceSquared(
                        position, resource.position) < 12.25;
            });
        const bool chestBlocked = std::any_of(
            chests_.begin(), chests_.end(),
            [position](const LootChestInstance& chest) {
                return geometry::distanceSquared(
                    position, chest.position) < 64.0;
            });
        if (resourceBlocked || chestBlocked) continue;

        const std::uint32_t index = nextEntityIndex_++;
        const LootChestType type = index % 3U == 2U
            ? LootChestType::Stone
            : LootChestType::Wooden;
        const EntityId id{index, runGeneration_};
        const Vec3 surfaceNormal = terrain.getNormal(
            position.x, position.z);
        chests_.push_back({
            .id = id,
            .type = type,
            .purpose = purpose,
            .position = position,
            .surfaceNormal = surfaceNormal,
            .yaw = unitRandom(seed ^ 0x452821e638d01377ULL) *
                6.28318530717958647692,
            .coinCost = purpose == LootChestPurpose::Reward ? 0 : 20,
            // Exploration chests stay hidden from the minimap until the
            // player reveals one or acquires an effect that exposes chests.
            .revealed = false,
            .loot = makeLoot(id, position, 0U, purpose),
        });
    }
}

void LootChestSystem::spawnRewardChest(
    Vec3 position, const TerrainHeightfield& terrain,
    LootChestType type) {
    position.y = terrain.getHeight(position.x, position.z);
    const std::uint32_t index = nextEntityIndex_++;
    const EntityId id{index, runGeneration_};
    const std::uint64_t seed = mixBits64(
        (static_cast<std::uint64_t>(terrain.seed()) << 32U) ^
        static_cast<std::uint64_t>(index));
    chests_.push_back({
        .id = id,
        .type = type,
        .purpose = LootChestPurpose::Reward,
        .position = position,
        .surfaceNormal = terrain.getNormal(position.x, position.z),
        .yaw = unitRandom(seed) * 6.28318530717958647692,
        .coinCost = 0,
        .revealed = true,
        .loot = makeLoot(
            id, position, 0U, LootChestPurpose::Reward),
    });
}

void LootChestSystem::tick(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) return;
    for (LootChestInstance& chest : chests_) {
        chest.loot.pickupDelayRemaining = std::max(
            0.0, chest.loot.pickupDelayRemaining - deltaSeconds);
        if (chest.state == LootChestState::Opening) {
            chest.openingProgress = std::min(
                1.0, chest.openingProgress + deltaSeconds / OpeningDuration);
            chest.loot.revealProgress = std::clamp(
                (chest.openingProgress - 0.14) / 0.44, 0.0, 1.0);
            chest.loot.available = chest.openingProgress >= 0.58;
            if (chest.openingProgress >= 1.0)
                chest.state = LootChestState::Open;
        }
        if (chest.rerolling) {
            chest.rerollProgress = std::min(
                1.0, chest.rerollProgress +
                         deltaSeconds / RerollDuration);
            if (chest.rerollProgress >= 1.0) {
                chest.rerolling = false;
                chest.loot.effect = chest.rerollTargetEffect;
                chest.loot.rarity = chest.rerollTargetRarity;
                chest.loot.available = true;
            }
        }
        if (chest.looseLoot && chest.loot.available &&
            !chest.loot.collected) {
            chest.loot.revealProgress = std::min(
                1.0,
                chest.loot.revealProgress +
                    deltaSeconds / LooseLootRevealDuration);
        }
        if (chest.loot.available && !chest.loot.collected)
            chest.loot.hoverTime += deltaSeconds;
        if (chest.loot.collected) {
            if (chest.looseLoot) {
                chest.disappearanceProgress = 1.0;
                continue;
            }
            double disappearanceDelta = deltaSeconds;
            if (chest.disappearanceDelayRemaining > 0.0) {
                const double delayStep = std::min(
                    chest.disappearanceDelayRemaining,
                    disappearanceDelta);
                chest.disappearanceDelayRemaining -= delayStep;
                disappearanceDelta -= delayStep;
            }
            if (disappearanceDelta > 0.0) {
                chest.disappearanceProgress = std::min(
                    1.0,
                    chest.disappearanceProgress +
                        disappearanceDelta /
                            ChestDisappearanceDuration);
            }
        }
    }
    std::erase_if(
        chests_, [](const LootChestInstance& chest) {
            return chest.disappearanceProgress >= 1.0;
        });
}

std::optional<EntityId> LootChestSystem::raycastChest(
    Vec3 origin, Vec3 direction, double maximumDistance) const {
    std::optional<EntityId> closest;
    double closestDistance = maximumDistance;
    for (const LootChestInstance& chest : chests_) {
        if (chest.state != LootChestState::Closed) continue;
        Vec3 center = chest.position;
        center.y += 0.55;
        const auto distance = geometry::raySphereDistance(
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
        if (!chest.loot.available || chest.loot.collected ||
            chest.loot.pickupDelayRemaining > 0.0) continue;
        const Vec3 center = lootVisualPositionImpl(chest);
        const auto distance = geometry::raySphereDistance(
            origin, direction, center, LootRaycastRadius);
        if (distance && *distance <= closestDistance) {
            closestDistance = *distance;
            closest = chest.loot.id;
        }
    }
    return closest;
}

ChestOpenResult LootChestSystem::open(EntityId id, int& coins) {
    const auto chest = std::find_if(
        chests_.begin(), chests_.end(),
        [id](const LootChestInstance& value) { return value.id == id; });
    if (chest == chests_.end()) return ChestOpenResult::None;
    if (chest->state != LootChestState::Closed)
        return ChestOpenResult::AlreadyOpen;
    const int cost = openingCost(*chest);
    if (coins < cost)
        return ChestOpenResult::InsufficientCoins;
    coins -= cost;
    chest->state = LootChestState::Opening;
    chest->openingProgress = 0.0;
    return ChestOpenResult::Opened;
}

ChestRerollResult LootChestSystem::reroll(
    EntityId id, int& coins, int cost) {
    const auto chest = std::find_if(
        chests_.begin(), chests_.end(),
        [id](const LootChestInstance& value) { return value.id == id; });
    if (chest == chests_.end()) return ChestRerollResult::None;
    if (chest->looseLoot || chest->state != LootChestState::Open ||
        !chest->loot.available || chest->loot.collected ||
        chest->rerolling)
        return ChestRerollResult::NotReady;
    if (chest->rerollCount >= 3U)
        return ChestRerollResult::AlreadyRerolled;
    cost = std::max(0, cost);
    if (coins < cost) return ChestRerollResult::InsufficientCoins;
    coins -= cost;
    const LootUpgradeEffect previous = chest->loot.effect;
    ChestLoot target{};
    std::uint64_t candidateRoll = chest->rerollCount + 1U;
    do {
        target = makeLoot(
            chest->id, chest->position, candidateRoll++,
            chest->purpose);
    } while (target.effect == previous && candidateRoll < 32U);
    ++chest->rerollCount;
    chest->rerollTargetEffect = target.effect;
    chest->rerollTargetRarity = target.rarity;
    chest->rerollProgress = 0.0;
    chest->rerolling = true;
    chest->loot.available = false;
    return ChestRerollResult::Rerolled;
}

std::optional<Vec3> LootChestSystem::revealNearest(
    Vec3 playerPosition) {
    LootChestInstance* nearest = nullptr;
    double nearestDistance = std::numeric_limits<double>::infinity();
    for (LootChestInstance& chest : chests_) {
        if (chest.looseLoot || chest.state != LootChestState::Closed ||
            chest.loot.collected) continue;
        const double distance = geometry::distanceSquared(
            chest.position, playerPosition);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = &chest;
        }
    }
    if (!nearest) return std::nullopt;
    nearest->revealed = true;
    return nearest->position;
}

void LootChestSystem::setCoinCostMultiplier(double multiplier) {
    coinCostMultiplier_ = std::clamp(multiplier, 0.01, 1.0);
}

void LootChestSystem::setOpeningCostSurcharge(int surcharge) {
    openingCostSurcharge_ = std::max(0, surcharge);
}

int LootChestSystem::openingCost(
    const LootChestInstance& chest) const {
    if (chest.coinCost <= 0) return 0;
    return std::max(
        1,
        static_cast<int>(std::lround(
            static_cast<double>(chest.coinCost + openingCostSurcharge_) *
            coinCostMultiplier_)));
}

std::optional<LootPickup> LootChestSystem::collect(EntityId id) {
    for (LootChestInstance& chest : chests_) {
        if (chest.loot.id != id || !chest.loot.available ||
            chest.loot.collected ||
            chest.loot.pickupDelayRemaining > 0.0) continue;
        chest.loot.collected = true;
        chest.disappearanceDelayRemaining =
            chest.looseLoot ? 0.0 : ChestDisappearanceDelay;
        return LootPickup{
            chest.loot.id, chest.loot.rarity,
            chest.loot.effect, lootVisualPositionImpl(chest),
        };
    }
    return std::nullopt;
}

std::optional<LootPickup> LootChestSystem::collectNearby(
    Vec3 playerPosition, double radius) {
    for (LootChestInstance& chest : chests_) {
        if (!chest.looseLoot) continue;
        const Vec3 itemPosition = lootVisualPositionImpl(chest);
        if (chest.loot.available && !chest.loot.collected &&
            chest.loot.pickupDelayRemaining <= 0.0 &&
            geometry::distanceSquared(
                playerPosition, itemPosition) <=
                std::min(radius, chest.loot.proximityPickupRadius) *
                std::min(radius, chest.loot.proximityPickupRadius))
            return collect(chest.loot.id);
    }
    return std::nullopt;
}

void LootChestSystem::spawnLooseLoot(
    Vec3 position, LootRarity rarity, std::uint64_t seed) {
    constexpr std::array<LootUpgradeEffect, 7> CommonLoot{{
        LootUpgradeEffect::Apple, LootUpgradeEffect::Bread,
        LootUpgradeEffect::IronBar, LootUpgradeEffect::FuelJerrycan,
        LootUpgradeEffect::Compass, LootUpgradeEffect::Nail,
        LootUpgradeEffect::Key,
    }};
    constexpr std::array<LootUpgradeEffect, 7> RareLoot{{
        LootUpgradeEffect::Map, LootUpgradeEffect::Anvil,
        LootUpgradeEffect::Saw, LootUpgradeEffect::Potion,
        LootUpgradeEffect::Blueprint, LootUpgradeEffect::Hourglass,
        LootUpgradeEffect::Rope,
    }};
    const auto pool = rarity == LootRarity::Common
        ? std::span<const LootUpgradeEffect>{CommonLoot}
        : std::span<const LootUpgradeEffect>{RareLoot};
    spawnLooseLootEffect(
        position,
        pool[mixBits64(seed ^ 0x8ebc6af09c88c6e3ULL) % pool.size()],
        rarity, seed);
}

void LootChestSystem::spawnLooseLootEffect(
    Vec3 position, LootUpgradeEffect effect,
    LootRarity rarity, std::uint64_t seed,
    double pickupDelay, double proximityPickupRadius) {
    const EntityId id{nextEntityIndex_++, runGeneration_};
    LootChestInstance loose{
        .id = id,
        .type = LootChestType::Wooden,
        .state = LootChestState::Open,
        .position = position,
        .surfaceNormal = {0.0, 1.0, 0.0},
        .yaw = unitRandom(seed) * 6.28318530717958647692,
        .openingProgress = 1.0,
        .looseLoot = true,
        .loot = {
            .id = id,
            .rarity = rarity,
            .effect = effect,
            .position = position,
            .revealProgress = 0.0,
            .pickupDelayRemaining = std::max(0.0, pickupDelay),
            .proximityPickupRadius = std::max(
                0.1, proximityPickupRadius),
            .available = true,
        },
    };
    // Loose items start close to the floor but still use the established
    // hover/pickup pipeline.
    loose.position.y -= LootExitHeight + LootRiseHeight;
    chests_.push_back(loose);
}

const std::vector<LootChestInstance>& LootChestSystem::chests() const {
    return chests_;
}

const char* lootRarityName(LootRarity rarity) {
    switch (rarity) {
    case LootRarity::Common: return "COMMON";
    case LootRarity::Uncommon: return "UNCOMMON";
    case LootRarity::Rare: return "RARE";
    case LootRarity::Legendary: return "LEGENDARY";
    }
    return "";
}

const char* lootUpgradeName(LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Damage: return "Razor Charm";
    case LootUpgradeEffect::MoveSpeed: return "Wind Feather";
    case LootUpgradeEffect::MaximumHealth: return "Heartwood Seed";
    case LootUpgradeEffect::Apple: return "Apple";
    case LootUpgradeEffect::Bread: return "Bread";
    case LootUpgradeEffect::IronBar: return "Iron Bar";
    case LootUpgradeEffect::FuelJerrycan: return "Fuel Jerrycan";
    case LootUpgradeEffect::Compass: return "Compass";
    case LootUpgradeEffect::Nail: return "Nail";
    case LootUpgradeEffect::Key: return "Chest Key";
    case LootUpgradeEffect::Map: return "Treasure Map";
    case LootUpgradeEffect::Anvil: return "Anvil";
    case LootUpgradeEffect::Saw: return "Saw";
    case LootUpgradeEffect::Potion: return "Battle Potion";
    case LootUpgradeEffect::Blueprint: return "Blueprint";
    case LootUpgradeEffect::Hourglass: return "Hourglass";
    case LootUpgradeEffect::Rope: return "Safety Rope";
    }
    return "Unknown Item";
}

const char* lootUpgradeDescription(LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Damage: return "+12% damage (max +60%)";
    case LootUpgradeEffect::MoveSpeed: return "+7% movement speed (max +35%)";
    case LootUpgradeEffect::MaximumHealth: return "+15% maximum health";
    case LootUpgradeEffect::Apple: return "+12 maximum health per stack";
    case LootUpgradeEffect::Bread:
        return "After 6s without damage: +0.4 HP/s per stack";
    case LootUpgradeEffect::IronBar: return "+3% armor";
    case LootUpgradeEffect::FuelJerrycan:
        return "+8% producer speed per stack (max +40%)";
    case LootUpgradeEffect::Compass:
        return "Points to nearby chests; range grows per stack";
    case LootUpgradeEffect::Nail:
        return "+8% maximum health for buildings (max +40%)";
    case LootUpgradeEffect::Key:
        return "-5% chest cost per stack (max -25%)";
    case LootUpgradeEffect::Map:
        return "Reveals chests; boss waves add one per stack";
    case LootUpgradeEffect::Anvil:
        return "New towers: +10% damage and health";
    case LootUpgradeEffect::Saw:
        return "+25% wood gathering and lumber production";
    case LootUpgradeEffect::Potion:
        return "Wave start: +20 HP and +10 temporary health";
    case LootUpgradeEffect::Blueprint:
        return "First building of each type grants Insight; retroactive";
    case LootUpgradeEffect::Hourglass:
        return "Early night converts remaining time into Coins and Insight";
    case LootUpgradeEffect::Rope:
        return "Reduces fall damage; consumes one to prevent a fatal fall";
    }
    return "";
}

} // namespace ian
