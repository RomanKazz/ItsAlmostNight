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
#include <unordered_set>

namespace ian {
namespace {

constexpr std::size_t ChestCount = 14;
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

LootUpgradeEffect chooseAllowedLoot(LootRarity& rarity,
    const std::array<bool, LootUpgradeEffectCount>& allowed, std::uint64_t roll) {
    constexpr std::array tiers{LootRarity::Common, LootRarity::Rare, LootRarity::Epic, LootRarity::Legendary};
    const auto wanted = std::find(tiers.begin(), tiers.end(), rarity);
    const int target = wanted == tiers.end() ? 1 : static_cast<int>(wanted - tiers.begin());
    std::array<LootUpgradeEffect, ActiveLootEffects.size()> pool{};
    // Empty locked tiers downgrade to owned content and retain its real rarity.
    // A defensive second pass also handles a custom pool with no common items.
    for (int pass = 0; pass < 2; ++pass) {
        for (int tier = pass == 0 ? target : 3; tier >= 0; --tier) {
            std::size_t count = 0;
            for (const auto item : ActiveLootEffects)
                if (allowed[lootUpgradeIndex(item)] && lootUpgradeRarity(item) == tiers[static_cast<std::size_t>(tier)])
                    pool[count++] = item;
            if (count > 0) { rarity = tiers[static_cast<std::size_t>(tier)]; return pool[roll % count]; }
        }
    }
    // setAllowedLoot guarantees a nonempty active pool.
    rarity = LootRarity::Common;
    return LootUpgradeEffect::Acorn;
}

ChestLoot makeLoot(EntityId chestId, Vec3 position,
                   std::uint64_t reroll = 0U,
                   LootChestPurpose purpose =
                       LootChestPurpose::Exploration,
                   const std::array<bool, LootUpgradeEffectCount>& allowed = {}) {
    const std::uint64_t seed =
        (static_cast<std::uint64_t>(chestId.generation) << 32U) |
        chestId.index;
    const std::uint64_t roll =
        mixBits64(seed ^ 0xe7037ed1a0b428dbULL ^
                  mixBits64(reroll + 0x9e3779b97f4a7c15ULL));
    // Four fixed item tiers; reward chests retain their lower high-tier chance.
    const auto percentile = roll % 1000ULL;
    const bool exploration = purpose == LootChestPurpose::Exploration;
    LootRarity rarity = percentile < (exploration ? 600ULL : 720ULL) ? LootRarity::Common
        : percentile < (exploration ? 870ULL : 920ULL) ? LootRarity::Rare
        : percentile < (exploration ? 970ULL : 985ULL) ? LootRarity::Epic : LootRarity::Legendary;
    const auto effect = chooseAllowedLoot(rarity, allowed, roll / 1000ULL);
    return {
        .id = {chestId.index, chestId.generation},
        .rarity = rarity,
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

void LootChestSystem::setAllowedLoot(const std::array<bool, LootUpgradeEffectCount>& allowed) {
    allowedLoot_ = allowed;
    if (std::ranges::none_of(ActiveLootEffects, [this](auto effect) { return allowedLoot_[lootUpgradeIndex(effect)]; }))
        allowedLoot_[lootUpgradeIndex(LootUpgradeEffect::Acorn)] = true;
}

void LootChestSystem::reset(
    std::uint32_t terrainSeed, double worldLimit,
    const TerrainHeightfield& terrain,
    std::span<const ResourceNode> resources, Vec3 playerSpawn) {
    ++runGeneration_;
    if (runGeneration_ == 0U) ++runGeneration_;
    nextEntityIndex_ = 0U;
    openedPaidChestCount_ = 0;
    chests_.clear();
    chests_.reserve(ChestCount);
    spawnAdditionalChests(
        static_cast<int>(ChestCount), terrainSeed, worldLimit,
        terrain, resources, playerSpawn, playerSpawn,
        ExplorationMaximumRadius, ExplorationMinimumRadius,
        LootChestPurpose::Exploration);
}

bool LootChestSystem::restoreChests(
    std::span<const LootChestInstance> chests) {
    constexpr std::size_t MaximumRestoredChests = 2048U;
    constexpr std::uint32_t ReservedIdHeadroom = 4096U;
    if (chests.size() > MaximumRestoredChests) {
        return false;
    }
    const auto finiteVector = [](Vec3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
               std::isfinite(value.z);
    };
    const auto validProgress = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    };
    const auto validTimer = [](double value) {
        return std::isfinite(value) && value >= 0.0 &&
               value <= 1'000'000.0;
    };
    const auto key = [](EntityId id) {
        return
            (static_cast<std::uint64_t>(id.generation) << 32U) |
            static_cast<std::uint64_t>(id.index);
    };

    std::unordered_set<std::uint64_t> ids;
    ids.reserve(chests.size());
    std::uint32_t generation = 0U;
    std::uint32_t nextIndex = 0U;
    for (const LootChestInstance& chest : chests) {
        const int type = static_cast<int>(chest.type);
        const int purpose = static_cast<int>(chest.purpose);
        const int state = static_cast<int>(chest.state);
        const int targetEffect = static_cast<int>(chest.rerollTargetEffect);
        const int targetRarity = static_cast<int>(chest.rerollTargetRarity);
        const int effect = static_cast<int>(chest.loot.effect);
        const int rarity = static_cast<int>(chest.loot.rarity);
        const double normalLengthSquared =
            chest.surfaceNormal.x * chest.surfaceNormal.x +
            chest.surfaceNormal.y * chest.surfaceNormal.y +
            chest.surfaceNormal.z * chest.surfaceNormal.z;
        if (chest.id.generation == 0U ||
            chest.id.index >=
                std::numeric_limits<std::uint32_t>::max() -
                    ReservedIdHeadroom ||
            chest.loot.id != chest.id ||
            !ids.insert(key(chest.id)).second ||
            (generation != 0U && chest.id.generation != generation) ||
            type < 0 || type > static_cast<int>(LootChestType::Stone) ||
            purpose < 0 ||
            purpose > static_cast<int>(LootChestPurpose::Reward) ||
            state < 0 || state > static_cast<int>(LootChestState::Open) ||
            targetEffect < 0 ||
            targetEffect >= static_cast<int>(LootUpgradeEffectCount) ||
            targetRarity < 0 ||
            targetRarity > static_cast<int>(LootRarity::Epic) ||
            effect < 0 || effect >= static_cast<int>(LootUpgradeEffectCount) ||
            rarity < 0 || rarity > static_cast<int>(LootRarity::Epic) ||
            !finiteVector(chest.position) ||
            !finiteVector(chest.surfaceNormal) ||
            normalLengthSquared < 0.25 || normalLengthSquared > 2.25 ||
            !std::isfinite(chest.yaw) || chest.coinCost < 0 ||
            chest.coinCost > 1'000'000 || chest.rerollCount > 3U ||
            !validProgress(chest.rerollProgress) ||
            !validProgress(chest.openingProgress) ||
            !validTimer(chest.disappearanceDelayRemaining) ||
            !validProgress(chest.disappearanceProgress) ||
            !finiteVector(chest.loot.position) ||
            !validProgress(chest.loot.revealProgress) ||
            !validTimer(chest.loot.hoverTime) ||
            !validTimer(chest.loot.pickupDelayRemaining) ||
            !std::isfinite(chest.loot.proximityPickupRadius) ||
            chest.loot.proximityPickupRadius < 0.1 ||
            chest.loot.proximityPickupRadius > 100.0) {
            return false;
        }
        generation = chest.id.generation;
        nextIndex = std::max(nextIndex, chest.id.index + 1U);
    }

    chests_.assign(chests.begin(), chests.end());
    if (generation != 0U) {
        runGeneration_ = generation;
    }
    nextEntityIndex_ = nextIndex;
    return true;
}

bool LootChestSystem::relocateExplorationChest(
    EntityId id, Vec3 position, const TerrainHeightfield& terrain, double yaw) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z) || !std::isfinite(yaw)) return false;
    const auto chest = std::ranges::find(chests_, id, &LootChestInstance::id);
    if (chest == chests_.end() || chest->looseLoot ||
        chest->purpose != LootChestPurpose::Exploration ||
        chest->state != LootChestState::Closed) return false;
    position.y = terrain.getHeight(position.x, position.z);
    chest->position = position;
    chest->surfaceNormal = terrain.getNormal(position.x, position.z);
    chest->yaw = yaw;
    chest->loot.position = position;
    return true;
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
            .loot = makeLoot(id, position, 0U, purpose, allowedLoot_),
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
            id, position, 0U, LootChestPurpose::Reward, allowedLoot_),
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

ChestOpenResult LootChestSystem::open(
    EntityId id, int& coins, bool freeOpening) {
    const auto chest = std::find_if(
        chests_.begin(), chests_.end(),
        [id](const LootChestInstance& value) { return value.id == id; });
    if (chest == chests_.end()) return ChestOpenResult::None;
    if (chest->state != LootChestState::Closed)
        return ChestOpenResult::AlreadyOpen;
    const int cost = freeOpening ? 0 : openingCost(*chest);
    if (coins < cost)
        return ChestOpenResult::InsufficientCoins;
    coins -= cost;
    chest->state = LootChestState::Opening;
    chest->openingProgress = 0.0;
    if (chest->coinCost > 0 && openedPaidChestCount_ < 1'000'000)
        ++openedPaidChestCount_;
    return ChestOpenResult::Opened;
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

void LootChestSystem::setOpenedPaidChestCount(int count) {
    openedPaidChestCount_ = std::clamp(count, 0, 1'000'000);
}

int LootChestSystem::openedPaidChestCount() const {
    return openedPaidChestCount_;
}

int scaledChestOpeningCost(
    int baseCost, int surcharge, double discountMultiplier,
    int previouslyOpenedPaidChests) {
    if (baseCost <= 0) return 0;
    const double growth = std::pow(
        1.15, std::min(std::max(0, previouslyOpenedPaidChests), 100));
    const double scaled = std::min(
        1'000'000.0,
        (static_cast<double>(baseCost) +
         static_cast<double>(std::max(0, surcharge))) *
            growth * std::clamp(discountMultiplier, 0.01, 1.0));
    return std::max(5, static_cast<int>(std::round(scaled / 5.0)) * 5);
}

int LootChestSystem::openingCost(
    const LootChestInstance& chest) const {
    return scaledChestOpeningCost(
        chest.coinCost, openingCostSurcharge_,
        coinCostMultiplier_, openedPaidChestCount_);
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
    if (rarity == LootRarity::Uncommon) rarity = LootRarity::Rare;
    const auto effect = chooseAllowedLoot(rarity, allowedLoot_, mixBits64(seed ^ 0x8ebc6af09c88c6e3ULL));
    spawnLooseLootEffect(position, effect, rarity, seed);
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
    case LootRarity::Epic: return "EPIC";
    }
    return "";
}

const char* lootUpgradeName(LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Acorn: return "Acorn";
    case LootUpgradeEffect::CrystalPan: return "Crystal Pan";
    case LootUpgradeEffect::RepairNail: return "Repair Nail";
    case LootUpgradeEffect::LumberBoot: return "Lumberjack Boot";
    case LootUpgradeEffect::Abacus: return "Abacus";
    case LootUpgradeEffect::WeaponToken: return "Weapon Token";
    case LootUpgradeEffect::IdleSpring: return "Coiled Spring";
    case LootUpgradeEffect::RepairMagnet: return "Repair Magnet";
    case LootUpgradeEffect::Siren: return "Warning Siren";
    case LootUpgradeEffect::Clover: return "Lucky Clover";
    case LootUpgradeEffect::MiningHammer: return "Mining Hammer";
    case LootUpgradeEffect::DawnCoin: return "Dawn Coin";
    case LootUpgradeEffect::GhostCandle: return "Last Light";
    case LootUpgradeEffect::CoreFuse: return "Core Fuse";
    case LootUpgradeEffect::CrystalSafe: return "Crystal Safe";
    case LootUpgradeEffect::RebuildBlueprint: return "Rebuild Blueprint";
    case LootUpgradeEffect::LuckyDie: return "Loaded Die";
    case LootUpgradeEffect::UpgradeMirror: return "Upgrade Mirror";
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
    case LootUpgradeEffect::Magnet: return "Pickup Magnet";
    case LootUpgradeEffect::HealthAid: return "Field Medkit";
    case LootUpgradeEffect::CoalBell: return "Coal Bell";
    case LootUpgradeEffect::PowderPouch: return "Powder Pouch";
    case LootUpgradeEffect::PlagueMushroom: return "Plague Mushroom";
    case LootUpgradeEffect::InfectedPouch: return "Infected Pouch";
    case LootUpgradeEffect::GasMask: return "Gas Mask";
    case LootUpgradeEffect::IcePrism: return "Ice Prism";
    case LootUpgradeEffect::TargetingLens: return "Targeting Lens";
    }
    return "Unknown Item";
}

const char* lootUpgradeDescription(LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Acorn: return "Felled trees have a 10% regrowth chance per stack.";
    case LootUpgradeEffect::CrystalPan: return "+5% chance per stack to find a crystal when mining stone.";
    case LootUpgradeEffect::RepairNail: return "10% chance per stack to repair by hand for free.";
    case LootUpgradeEffect::LumberBoot: return "Felling a tree grants +10% speed per stack for 5s.";
    case LootUpgradeEffect::Abacus: return "Each depleted resource adds +1% matching production per stack, up to +100% per stack.";
    case LootUpgradeEffect::WeaponToken: return "First hit after switching weapons deals +25% damage per stack.";
    case LootUpgradeEffect::IdleSpring: return "After 3s idle, a tower's next hit deals +40% damage per stack.";
    case LootUpgradeEffect::RepairMagnet: return "Hand repairs also heal the nearest structure for 30% of the repair per stack.";
    case LootUpgradeEffect::Siren: return "Towers below 40% HP attack 15% faster per stack.";
    case LootUpgradeEffect::Clover: return "Opening a barrel grants 10% of each resource capacity per stack.";
    case LootUpgradeEffect::MiningHammer: return "Breaking stone deals 50% of the mining hit to nearby stones per stack.";
    case LootUpgradeEffect::DawnCoin: return "At dawn gain 5% of stored resources per stack.";
    case LootUpgradeEffect::GhostCandle: return "Destroyed towers keep firing for 8s; +5s per extra stack.";
    case LootUpgradeEffect::CoreFuse: return "Once per night saves the Core and stops enemies for 5s; +2s per extra stack.";
    case LootUpgradeEffect::CrystalSafe: return "Each 25 crystals held at nightfall grant +10% tower damage per stack; up to 3 deposits.";
    case LootUpgradeEffect::RebuildBlueprint: return "At dawn restores 1 destroyed tower per stack with 40% HP.";
    case LootUpgradeEffect::LuckyDie: return "+1 independent roll for chance effects of hand actions per stack.";
    case LootUpgradeEffect::UpgradeMirror: return "Every 5th chosen stat card applies twice. Extra stacks reduce the interval, down to 2.";
    case LootUpgradeEffect::Damage: return "+12% damage (max +60%)";
    case LootUpgradeEffect::MoveSpeed: return "+7% movement speed (max +35%)";
    case LootUpgradeEffect::MaximumHealth: return "+15% maximum health";
    case LootUpgradeEffect::Apple:
        return "Below 35% HP: automatically restores health once per night";
    case LootUpgradeEffect::Bread:
        return "Regenerates after 6s safe; full health grants attack speed until hit";
    case LootUpgradeEffect::IronBar:
        return "+12 regenerating armor; breaking it knocks back and briefly stuns enemies";
    case LootUpgradeEffect::FuelJerrycan:
        return "Hits can ignite enemies; repeated burns deal more damage";
    case LootUpgradeEffect::Compass:
        return "Points to nearby chests; range grows per stack";
    case LootUpgradeEffect::Nail:
        return "Every 5th hit chains lightning; stacks improve frequency, damage and jumps";
    case LootUpgradeEffect::Key:
        return "First chest each day is free; extra stacks reduce chest prices by 15%";
    case LootUpgradeEffect::Map:
        return "Reveals chests; boss waves add one per stack";
    case LootUpgradeEffect::Anvil:
        return "Fully repairing a structure releases a knockback shockwave";
    case LootUpgradeEffect::Saw:
        return "Destroyed trees launch spinning saw blades into nearby resources";
    case LootUpgradeEffect::Potion:
        return "Below 35% HP: Berserk with speed and lifesteal; stacks extend duration";
    case LootUpgradeEffect::Blueprint:
        return "First building of each type grants XP; retroactive";
    case LootUpgradeEffect::Hourglass:
        return "Early night converts remaining time into Coins and XP";
    case LootUpgradeEffect::Rope:
        return "Reduces fall damage; consumes one to prevent a fatal fall";
    case LootUpgradeEffect::Magnet:
        return "Coins and hearts attract from 6m farther per stack (max 20m)";
    case LootUpgradeEffect::HealthAid:
        return "Destroying a tree or stone restores 4 health per stack (max 16)";
    case LootUpgradeEffect::CoalBell: return "Burning kills explode and ignite nearby enemies";
    case LootUpgradeEffect::PowderPouch: return "Critical hits explode, damaging nearby enemies";
    case LootUpgradeEffect::PlagueMushroom: return "Hits can poison; repeated doses strengthen poison";
    case LootUpgradeEffect::InfectedPouch: return "Poisoned kills leave a poison cloud";
    case LootUpgradeEffect::GasMask: return "Nearby poisoned enemies reduce damage taken (max 35%)";
    case LootUpgradeEffect::IcePrism: return "Frozen kills shatter; shards hit slowed enemies harder";
    case LootUpgradeEffect::TargetingLens: return "Player hits mark targets; defenses prioritize them and deal more damage";
    }
    return "";
}

LootRarity lootUpgradeRarity(LootUpgradeEffect effect) {
    const auto index = lootUpgradeIndex(effect);
    if (index >= 39U) return LootRarity::Legendary;
    if (index >= 36U) return LootRarity::Epic;
    if (index >= 31U) return LootRarity::Rare;
    return LootRarity::Common;
}
const char* lootUpgradeModelPath(LootUpgradeEffect effect) {
    switch (effect) {
    case LootUpgradeEffect::Acorn: return "assets/models/items/relics/acorn.glb";
    case LootUpgradeEffect::CrystalPan: return "assets/models/items/relics/crystal_pan.glb";
    case LootUpgradeEffect::RepairNail: return "assets/models/items/relics/repair_nail.glb";
    case LootUpgradeEffect::LumberBoot: return "assets/models/items/relics/lumber_boot.glb";
    case LootUpgradeEffect::Abacus: return "assets/models/items/relics/abacus.glb";
    case LootUpgradeEffect::WeaponToken: return "assets/models/items/relics/weapon_token.glb";
    case LootUpgradeEffect::IdleSpring: return "assets/models/items/relics/idle_spring.glb";
    case LootUpgradeEffect::RepairMagnet: return "assets/models/items/relics/repair_magnet.glb";
    case LootUpgradeEffect::Siren: return "assets/models/items/relics/siren.glb";
    case LootUpgradeEffect::Clover: return "assets/models/items/relics/clover.glb";
    case LootUpgradeEffect::MiningHammer: return "assets/models/items/relics/mining_hammer.glb";
    case LootUpgradeEffect::DawnCoin: return "assets/models/items/relics/dawn_coin.glb";
    case LootUpgradeEffect::GhostCandle: return "assets/models/items/relics/ghost_candle.glb";
    case LootUpgradeEffect::CoreFuse: return "assets/models/items/relics/core_fuse.glb";
    case LootUpgradeEffect::CrystalSafe: return "assets/models/items/relics/crystal_safe.glb";
    case LootUpgradeEffect::RebuildBlueprint: return "assets/models/items/relics/rebuild_blueprint.glb";
    case LootUpgradeEffect::LuckyDie: return "assets/models/items/relics/lucky_die.glb";
    case LootUpgradeEffect::UpgradeMirror: return "assets/models/items/relics/upgrade_mirror.glb";
    default: return "";
    }
}

} // namespace ian
