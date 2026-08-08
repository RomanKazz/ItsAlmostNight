#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <limits>

namespace ian {
void Simulation::applyLootPickup(const LootPickup& pickup) {
    const double rarityStrength =
        pickup.rarity == LootRarity::Rare
            ? 2.5
            : pickup.rarity == LootRarity::Uncommon ? 1.6 : 1.0;
    switch (pickup.effect) {
    case LootUpgradeEffect::Damage:
        playerDamageMultiplier_ += 0.12 * rarityStrength;
        break;
    case LootUpgradeEffect::MoveSpeed:
        playerMoveSpeedMultiplier_ += 0.07 * rarityStrength;
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
        productionSpeedMultiplier_ += 0.08;
        goldMines_.setProductionSpeedMultiplier(
            productionSpeedMultiplier_);
        break;
    case LootUpgradeEffect::Compass:
        break;
    case LootUpgradeEffect::Nail:
        buildingMaxHealthMultiplier_ += 0.08;
        buildings_.setMaxHealthMultiplier(
            buildingMaxHealthMultiplier_);
        foundations_.setMaxHealthMultiplier(
            buildingMaxHealthMultiplier_);
        break;
    case LootUpgradeEffect::Key:
        chestOpeningCostMultiplier_ = std::max(
            0.75, chestOpeningCostMultiplier_ - 0.05);
        lootChests_.setGoldCostMultiplier(
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
        woodYieldMultiplier_ += 0.25;
        resources_.setWoodYieldMultiplier(woodYieldMultiplier_);
        goldMines_.setWoodYieldMultiplier(woodYieldMultiplier_);
        break;
    case LootUpgradeEffect::Potion:
        break;
    }
    ++lootStacks_[lootUpgradeIndex(pickup.effect)];
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
            wood_ = saturatingAdd(wood_, grant.amount);
        } else {
            stone_ = saturatingAdd(stone_, grant.amount);
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

} // namespace ian
