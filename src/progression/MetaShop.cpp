#include "progression/MetaShop.hpp"
#include "progression/MetaProgression.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ian {
namespace {
const MetaShopDefinition* definition(MetaShopUpgrade upgrade) {
    const auto i = static_cast<std::size_t>(upgrade);
    return i < MetaShopDefinitions.size() ? &MetaShopDefinitions[i] : nullptr;
}
}
int metaShopLevel(const MetaProgression& profile, MetaShopUpgrade upgrade) {
    const auto* d = definition(upgrade);
    return d ? std::clamp(profile.shopLevels[static_cast<std::size_t>(upgrade)], 0, d->maximumLevel) : 0;
}
int metaShopCost(const MetaProgression& profile, MetaShopUpgrade upgrade) {
    const auto* d = definition(upgrade);
    if (!d || metaShopLevel(profile, upgrade) >= d->maximumLevel) return 0;
    return static_cast<int>(std::ceil(static_cast<double>(d->baseCost) * std::pow(1.5, metaShopLevel(profile, upgrade))));
}
bool metaShopAvailable(const MetaProgression& profile, MetaShopUpgrade upgrade) {
    const auto* d = definition(upgrade);
    return d && (d->requiredUnlock.empty() || profile.quests.ownedUnlocks.contains(d->requiredUnlock));
}
MetaShopPurchaseResult purchaseMetaShopUpgrade(MetaProgression& profile, MetaShopUpgrade upgrade) {
    const auto* d = definition(upgrade);
    if (!d) return MetaShopPurchaseResult::InvalidUpgrade;
    if (!metaShopAvailable(profile, upgrade)) return MetaShopPurchaseResult::Locked;
    if (metaShopLevel(profile, upgrade) >= d->maximumLevel) return MetaShopPurchaseResult::MaximumLevel;
    const int price = metaShopCost(profile, upgrade);
    if (profile.silver < price) return MetaShopPurchaseResult::InsufficientSilver;
    profile.silver -= price;
    profile.shopLevels[static_cast<std::size_t>(upgrade)] = metaShopLevel(profile, upgrade) + 1;
    return MetaShopPurchaseResult::Purchased;
}
MetaRunBonuses metaRunBonuses(const MetaProgression& p) {
    const auto percent = [&](MetaShopUpgrade u) {
        return static_cast<double>(metaShopLevel(p, u) * MetaShopDefinitions[static_cast<std::size_t>(u)].bonusPerLevel) / 100;
    };
    return {.luck = percent(MetaShopUpgrade::Luck),
        .defenseDamage = 1 + percent(MetaShopUpgrade::DefenseDamage),
        .defenseFireRate = 1 + percent(MetaShopUpgrade::DefenseFireRate),
        .buildingHealth = 1 + percent(MetaShopUpgrade::BuildingHealth),
        .productionSpeed = 1 + percent(MetaShopUpgrade::ProductionSpeed),
        .resourceYield = 1 + percent(MetaShopUpgrade::ResourceYield),
        .rerolls = metaShopLevel(p, MetaShopUpgrade::Rerolls) + (p.quests.ownedUnlocks.contains("meta.refresh") ? 1 : 0),
        .additionalTomeSlots = metaShopLevel(p, MetaShopUpgrade::TomeSlots),
        .additionalWeaponSlots = metaShopLevel(p, MetaShopUpgrade::WeaponSlots)};
}
bool validMetaRunBonuses(const MetaRunBonuses& b) {
    if (!std::isfinite(b.luck) || b.luck < 0 || b.luck > .20 || b.rerolls < 0 || b.rerolls > 6) return false;
    for (double value : {b.defenseDamage, b.defenseFireRate, b.buildingHealth, b.productionSpeed, b.resourceYield})
        if (!std::isfinite(value) || value < 1 || value > 1.5) return false;
    return b.defenseFireRate <= 1.3 && b.additionalTomeSlots >= 0 &&
        b.additionalTomeSlots <= MaximumRunTomeSlots - BaseRunTomeSlots &&
        b.additionalWeaponSlots >= 0 && b.additionalWeaponSlots <= MaximumRunWeaponSlots - BaseRunWeaponSlots;
}
int creditMetaSilver(MetaProgression& p, int baseAmount) {
    if (baseAmount <= 0) return 0;
    const int bonusPercent = metaShopLevel(p, MetaShopUpgrade::SilverGain) * 5;
    const std::int64_t scaled = static_cast<std::int64_t>(baseAmount) * (100 + bonusPercent) +
        std::clamp(p.silverRewardRemainder, 0, 99);
    const int balance = std::max(0, p.silver);
    const auto increase = std::min<std::int64_t>(scaled / 100, std::numeric_limits<int>::max() - balance);
    p.silver = balance + static_cast<int>(increase);
    p.silverRewardRemainder = static_cast<int>(scaled % 100);
    return static_cast<int>(increase);
}
} // namespace ian
