#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace ian {
namespace {
constexpr double IronBarArmor = 12.0;
constexpr double ArmorRechargeDelay = 5.0;
constexpr double ArmorMinimumRechargePerSecond = 5.0;
constexpr double ArmorRechargeFractionPerSecond = 0.22;
constexpr double BerserkHealthThreshold = 0.35;
constexpr double BerserkBaseDuration = 6.0;
constexpr double BerserkDurationPerExtraStack = 2.0;
constexpr double BerserkMaximumDuration = 12.0;
constexpr double BerserkAttackSpeedMultiplier = 1.35;
constexpr double BerserkLifestealFraction = 0.10;
}

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
    case LootUpgradeEffect::IronBar: {
        const double gainedArmor = IronBarArmor;
        playerMaxRecoverableArmor_ += gainedArmor;
        playerRecoverableArmor_ += gainedArmor;
        break;
    }
    case LootUpgradeEffect::FuelJerrycan:
        productionSpeedMultiplier_ = std::min(
            1.40, productionSpeedMultiplier_ + 0.08);
        crystalMines_.setProductionSpeedMultiplier(
            productionSpeedMultiplier_);
        break;
    case LootUpgradeEffect::Compass:
        break;
    case LootUpgradeEffect::Nail:
        break;
    case LootUpgradeEffect::Key: {
        const int previousStacks = lootStacks_[
            lootUpgradeIndex(LootUpgradeEffect::Key)];
        if (previousStacks == 0) {
            freeChestOpeningAvailable_ = true;
        }
        const int discountStacks = std::max(0, previousStacks);
        chestOpeningCostMultiplier_ = std::max(
            0.75, 1.0 - 0.05 * static_cast<double>(discountStacks));
        lootChests_.setCoinCostMultiplier(
            chestOpeningCostMultiplier_);
        break;
    }
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
        if (state_ == RunState::Wave &&
            battlePotionBerserkRemaining_ <= 0.0) {
            battlePotionAvailable_ = true;
        }
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
    if (playerTemporaryHealth_ > 0.0) {
        playerTemporaryHealth_ = 0.0;
        playerHealth_ = std::min(
            playerHealth_, playerPermanentMaxHealth());
    }
    battlePotionAvailable_ = stacks > 0;
    battlePotionBerserkRemaining_ = 0.0;
    battlePotionBerserkDuration_ = 0.0;
    battlePotionLifestealRemaining_ = 0.0;
    iceWand_.setCastSpeedMultiplier(1.0);
    fireWand_.setCastSpeedMultiplier(1.0);
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

void Simulation::updateLootEffects(
    double deltaSeconds, std::size_t firstGameplayEvent) {
    if (playerRespawning_) {
        secondsSincePlayerDamage_ = 0.0;
        battlePotionAvailable_ = false;
        battlePotionBerserkRemaining_ = 0.0;
        battlePotionBerserkDuration_ = 0.0;
        battlePotionLifestealRemaining_ = 0.0;
        iceWand_.setCastSpeedMultiplier(1.0);
        fireWand_.setCastSpeedMultiplier(1.0);
        return;
    }

    const int potionStacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Potion)];
    if (battlePotionAvailable_ && potionStacks > 0 &&
        state_ == RunState::Wave && playerHealth_ > 0.0) {
        const double maximumHealth =
            playerPermanentMaxHealth() + playerTemporaryHealth_;
        if (maximumHealth > 0.0 &&
            playerHealth_ / maximumHealth <= BerserkHealthThreshold) {
            battlePotionAvailable_ = false;
            battlePotionBerserkDuration_ = std::min(
                BerserkMaximumDuration,
                BerserkBaseDuration +
                    static_cast<double>(potionStacks - 1) *
                        BerserkDurationPerExtraStack);
            battlePotionBerserkRemaining_ =
                battlePotionBerserkDuration_;
            battlePotionLifestealRemaining_ = std::min(
                40.0, 20.0 + 5.0 *
                    static_cast<double>(potionStacks - 1));
            iceWand_.setCastSpeedMultiplier(
                BerserkAttackSpeedMultiplier);
            fireWand_.setCastSpeedMultiplier(
                BerserkAttackSpeedMultiplier);
            events_.push_back({
                .type = GameEventType::BattlePotionActivated,
                .position = playerPosition_,
                .amount = potionStacks,
                .intensity = battlePotionBerserkDuration_,
            });
        }
    }

    if (battlePotionBerserkRemaining_ > 0.0) {
        double dealtDamage = 0.0;
        for (std::size_t index = firstGameplayEvent;
             index < events_.size(); ++index) {
            const GameEvent& event = events_[index];
            const bool playerHit =
                event.type == GameEventType::PickaxeHit ||
                event.type == GameEventType::IceWandHit ||
                event.type == GameEventType::FireWandHit ||
                event.type == GameEventType::ChainLightningHit ||
                (event.type == GameEventType::ProjectileHit &&
                 !event.sourceId.has_value());
            if (playerHit) {
                dealtDamage += std::max(0.0, event.damage);
            }
        }
        const double maximumHealth =
            playerPermanentMaxHealth() + playerTemporaryHealth_;
        const double healing = std::min({
            dealtDamage * BerserkLifestealFraction,
            battlePotionLifestealRemaining_,
            std::max(0.0, maximumHealth - playerHealth_),
        });
        playerHealth_ += healing;
        battlePotionLifestealRemaining_ -= healing;

        const double previousRemaining =
            battlePotionBerserkRemaining_;
        battlePotionBerserkRemaining_ = std::max(
            0.0, battlePotionBerserkRemaining_ - deltaSeconds);
        if (previousRemaining > 0.0 &&
            battlePotionBerserkRemaining_ <= 0.0) {
            const double temporaryHealth = std::min(
                30.0, 10.0 + 5.0 *
                    static_cast<double>(potionStacks - 1));
            playerTemporaryHealth_ += temporaryHealth;
            playerHealth_ += temporaryHealth;
            battlePotionLifestealRemaining_ = 0.0;
            iceWand_.setCastSpeedMultiplier(1.0);
            fireWand_.setCastSpeedMultiplier(1.0);
        }
    }
    const double previousSeconds = secondsSincePlayerDamage_;
    secondsSincePlayerDamage_ += deltaSeconds;
    if (playerRecoverableArmor_ < playerMaxRecoverableArmor_ &&
        secondsSincePlayerDamage_ >= ArmorRechargeDelay) {
        const double regeneratingSeconds = std::max(
            0.0, secondsSincePlayerDamage_ -
                std::max(previousSeconds, ArmorRechargeDelay));
        const double rechargePerSecond = std::max(
            ArmorMinimumRechargePerSecond,
            playerMaxRecoverableArmor_ *
                ArmorRechargeFractionPerSecond);
        playerRecoverableArmor_ = std::min(
            playerMaxRecoverableArmor_,
            playerRecoverableArmor_ +
                regeneratingSeconds * rechargePerSecond);
    }
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
        // Give the blade enough screen time to read as an actual projectile.
        // It still accelerates over longer hops so chain harvesting stays
        // responsive rather than turning into a long waiting sequence.
        const double travelSeconds = std::clamp(
            distance / 13.0, 0.55, 0.95);
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
