#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace ian {
void Simulation::applyLootPickup(const LootPickup& pickup) {
    const double rarityStrength =
        pickup.rarity == LootRarity::Legendary
            ? 4.0
            : pickup.rarity == LootRarity::Rare
            ? 2.5
            : pickup.rarity == LootRarity::Uncommon ? 1.6 : 1.0;
    switch (pickup.effect) {
    case LootUpgradeEffect::Damage:
        playerDamageMultiplier_ = std::min(
            1.60, playerDamageMultiplier_ + 0.12 * rarityStrength);
        break;
    case LootUpgradeEffect::MoveSpeed:
        playerMoveSpeedMultiplier_ = std::min(
            1.35, playerMoveSpeedMultiplier_ + 0.07 * rarityStrength);
        break;
    case LootUpgradeEffect::MaximumHealth: {
        const double previousMaximum = playerPermanentMaxHealth();
        playerMaxHealthMultiplier_ += 0.15 * rarityStrength;
        const double newMaximum = playerPermanentMaxHealth();
        playerHealth_ += newMaximum - previousMaximum;
        break;
    }
    case LootUpgradeEffect::Apple:
        playerBonusMaxHealth_ += 12.0;
        playerHealth_ += 12.0;
        break;
    case LootUpgradeEffect::Bread:
        break;
    case LootUpgradeEffect::IronBar:
        playerArmorMultiplier_ += 0.03;
        break;
    case LootUpgradeEffect::FuelJerrycan:
        productionSpeedMultiplier_ = std::min(
            1.40, productionSpeedMultiplier_ + 0.08);
        crystalMines_.setProductionSpeedMultiplier(
            productionSpeedMultiplier_);
        break;
    case LootUpgradeEffect::Compass:
        break;
    case LootUpgradeEffect::Nail:
        buildingMaxHealthMultiplier_ = std::min(
            1.40, buildingMaxHealthMultiplier_ + 0.08);
        buildings_.setMaxHealthMultiplier(
            buildingMaxHealthMultiplier_);
        foundations_.setMaxHealthMultiplier(
            buildingMaxHealthMultiplier_);
        break;
    case LootUpgradeEffect::Key:
        chestOpeningCostMultiplier_ = std::max(
            0.75, chestOpeningCostMultiplier_ - 0.05);
        lootChests_.setCoinCostMultiplier(
            chestOpeningCostMultiplier_);
        break;
    case LootUpgradeEffect::Map:
        break;
    case LootUpgradeEffect::Anvil:
        buildings_.setNewTowerBonusStacks(
            lootStacks_[lootUpgradeIndex(
                LootUpgradeEffect::Anvil)] + 1);
        break;
    case LootUpgradeEffect::Saw:
        break;
    case LootUpgradeEffect::Potion:
        break;
    case LootUpgradeEffect::Blueprint:
        grantBlueprintInsightForExistingBuildings(
            lootStacks_[lootUpgradeIndex(
                LootUpgradeEffect::Blueprint)] + 1);
        break;
    case LootUpgradeEffect::Hourglass:
    case LootUpgradeEffect::Rope:
        break;
    }
    ++lootStacks_[lootUpgradeIndex(pickup.effect)];
    refreshSkillRuntimeEffects();
    events_.push_back({
        .type = GameEventType::LootCollected,
        .entityId = pickup.lootId,
        .position = pickup.position,
        .lootRarity = pickup.rarity,
        .lootUpgradeEffect = pickup.effect,
    });
    aimedLoot_.reset();
}

double Simulation::playerPermanentMaxHealth() const {
    return gameplay_.playerMaxHealth * playerMaxHealthMultiplier_ +
        playerBonusMaxHealth_;
}

void Simulation::applyPotionWaveStart() {
    const int stacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Potion)];
    if (stacks <= 0) {
        playerTemporaryHealth_ = 0.0;
        return;
    }
    playerTemporaryHealth_ = 10.0 * static_cast<double>(stacks);
    const double maximum =
        playerPermanentMaxHealth() + playerTemporaryHealth_;
    playerHealth_ = std::min(
        maximum,
        playerHealth_ + 20.0 * static_cast<double>(stacks));
}

void Simulation::grantLootUpgrade(
    LootUpgradeEffect effect, LootRarity rarity) {
    invalidateSnapshotCache();
    applyLootPickup({
        .lootId = {std::numeric_limits<std::uint32_t>::max(), 0U},
        .rarity = rarity,
        .effect = effect,
        .position = playerPosition_,
    });
}

void Simulation::updateLootEffects(double deltaSeconds) {
    if (playerRespawning_) {
        secondsSincePlayerDamage_ = 0.0;
        return;
    }
    const double previousSeconds = secondsSincePlayerDamage_;
    secondsSincePlayerDamage_ += deltaSeconds;
    const int breadStacks =
        lootStacks_[lootUpgradeIndex(LootUpgradeEffect::Bread)];
    if (breadStacks <= 0 || secondsSincePlayerDamage_ < 6.0) {
        return;
    }
    const double maximumHealth =
        playerPermanentMaxHealth() + playerTemporaryHealth_;
    const double regeneratingSeconds = std::max(
        0.0, secondsSincePlayerDamage_ -
            std::max(previousSeconds, 6.0));
    playerHealth_ = std::min(
        maximumHealth,
        playerHealth_ + regeneratingSeconds * 0.4 *
            static_cast<double>(breadStacks));
}

void Simulation::updatePendingResourceGrants(
    double deltaSeconds) {
    for (auto& grant : pendingResourceGrants_) {
        grant.remaining -= deltaSeconds;
        if (grant.remaining > 0.0) {
            continue;
        }
        if (grant.type == ResourceType::Wood) {
            addWood(grant.amount);
        } else {
            addStone(grant.amount);
        }
        events_.push_back({
            .type = GameEventType::ResourceGranted,
            .resourceType = grant.type,
            .position = grant.position,
            .amount = grant.amount,
        });
    }
    std::erase_if(
        pendingResourceGrants_,
        [](const PendingResourceGrant& grant) {
            return grant.remaining <= 0.0;
        });
}

void Simulation::launchSawSplinters(
    EntityId sourceId, Vec3 origin, int chainDepth) {
    const int stacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Saw)];
    if (stacks <= 0 || chainDepth > 1) {
        return;
    }

    const int baseCount = std::min(7, 2 + stacks);
    const int splinterCount = chainDepth == 0
        ? baseCount : std::max(1, baseCount / 2);
    const double radius = std::min(
        16.0, 10.0 + static_cast<double>(stacks - 1) * 1.5);
    const double radiusSquared = radius * radius;
    const double damageFraction = std::min(
        0.375, 0.225 + static_cast<double>(stacks - 1) * 0.05);

    std::vector<const ResourceNode*> candidates;
    candidates.reserve(resources_.nodes().size());
    for (const ResourceNode& node : resources_.nodes()) {
        if (!node.active || node.id == sourceId ||
            !isHarvestableResource(node.type)) {
            continue;
        }
        if (std::ranges::any_of(
                pendingSawSplinters_, [&node](const auto& splinter) {
                    return splinter.targetId == node.id;
                })) {
            continue;
        }
        const double dx = node.position.x - origin.x;
        const double dz = node.position.z - origin.z;
        if (dx * dx + dz * dz <= radiusSquared) {
            candidates.push_back(&node);
        }
    }
    std::ranges::sort(
        candidates, [origin](const ResourceNode* left,
                             const ResourceNode* right) {
            const double leftX = left->position.x - origin.x;
            const double leftZ = left->position.z - origin.z;
            const double rightX = right->position.x - origin.x;
            const double rightZ = right->position.z - origin.z;
            return leftX * leftX + leftZ * leftZ <
                rightX * rightX + rightZ * rightZ;
        });

    const int launchCount = std::min(
        splinterCount, static_cast<int>(candidates.size()));
    for (int index = 0; index < launchCount; ++index) {
        const ResourceNode& target = *candidates[
            static_cast<std::size_t>(index)];
        const double dx = target.position.x - origin.x;
        const double dz = target.position.z - origin.z;
        const double distance = std::sqrt(dx * dx + dz * dz);
        const double travelSeconds = std::clamp(
            distance / 24.0, 0.24, 0.52);
        const Vec3 launchPosition{
            origin.x, origin.y + 1.1, origin.z};
        const Vec3 targetPosition{
            target.position.x,
            target.position.y + std::max(0.35, target.radius * 0.55),
            target.position.z};
        pendingSawSplinters_.push_back({
            .sourceId = sourceId,
            .targetId = target.id,
            .origin = launchPosition,
            .targetPosition = targetPosition,
            .damage = target.maxHealth * damageFraction,
            .remaining = travelSeconds,
            .chainDepth = chainDepth,
        });
        events_.push_back({
            .type = GameEventType::SawSplinterLaunched,
            .entityId = target.id,
            .sourceId = sourceId,
            .position = launchPosition,
            .targetPosition = targetPosition,
            .amount = chainDepth,
            .damage = target.maxHealth * damageFraction,
            .intensity = travelSeconds,
        });
    }
}

void Simulation::updateSawSplinters(double deltaSeconds) {
    struct ChainLaunch {
        EntityId sourceId;
        Vec3 position;
        int depth;
    };
    std::vector<ChainLaunch> chainLaunches;
    for (PendingSawSplinter& splinter : pendingSawSplinters_) {
        splinter.remaining -= deltaSeconds;
        if (splinter.remaining > 0.0) {
            continue;
        }
        const auto target = std::ranges::find(
            resources_.nodes(), splinter.targetId, &ResourceNode::id);
        if (target == resources_.nodes().end() || !target->active ||
            !hasStorageSpace(target->type)) {
            continue;
        }
        const bool largeDeposit = target->yield >= 30;
        const auto hit = resources_.damage(
            splinter.targetId, splinter.damage);
        if (!hit) {
            continue;
        }
        events_.push_back({
            .type = hit->collected
                ? GameEventType::ResourceCollected
                : GameEventType::ResourceHit,
            .entityId = hit->nodeId,
            .sourceId = splinter.sourceId,
            .resourceType = hit->type,
            .position = splinter.targetPosition,
            .amount = hit->amount,
            .damage = splinter.damage,
            .largeDeposit = largeDeposit,
            .night = state_ == RunState::Sunset ||
                state_ == RunState::Wave,
        });
        if (hit->amount > 0) {
            pendingResourceGrants_.push_back({
                .type = hit->type,
                .position = splinter.targetPosition,
                .amount = hit->amount,
                .remaining = ResourcePickupFlightSeconds,
            });
        }
        if (hit->collected && hit->type == ResourceType::Wood) {
            chainLaunches.push_back({
                hit->nodeId, hit->position,
                splinter.chainDepth + 1});
        }
    }
    std::erase_if(
        pendingSawSplinters_, [](const PendingSawSplinter& splinter) {
            return splinter.remaining <= 0.0;
        });
    for (const ChainLaunch& launch : chainLaunches) {
        launchSawSplinters(
            launch.sourceId, launch.position, launch.depth);
    }
}

} // namespace ian
