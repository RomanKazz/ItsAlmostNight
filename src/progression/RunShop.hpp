#pragma once
#include "progression/RunUpgrade.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace ian {
inline constexpr std::size_t RunShopSlotCount = 4;
struct RunShopOffer {
    RunUpgradeEffect effect{RunUpgradeEffect::Damage};
    RunUpgradeRarity rarity{RunUpgradeRarity::Common};
    std::uint64_t rollSeed{};
    int price{};
    bool present{};
    bool locked{};
    bool operator==(const RunShopOffer&) const = default;
};
struct RunShopState {
    bool pending{};
    int wave{};
    int rerolls{};
    std::uint32_t generation{};
    std::array<RunShopOffer, RunShopSlotCount> offers{};
    bool operator==(const RunShopState&) const = default;
};
[[nodiscard]] inline int runUpgradeOfferWeight(RunUpgradeEffect effect) {
    // Every eligible tome has the same acquisition/upgrade weight. Ownership
    // removes new tomes when slots are full. Level does not bias within a group.
    if (runTomeDefinition(effect) && !retiredRunUpgrade(effect)) return 2;
        switch (effect) {
        case RunUpgradeEffect::ProjectileCount:
        case RunUpgradeEffect::ClusterPayload:
        case RunUpgradeEffect::ReactiveTraps:
        case RunUpgradeEffect::Bounces:
        case RunUpgradeEffect::Pierce:
        case RunUpgradeEffect::GunShrapnel:
            return 6;
        case RunUpgradeEffect::BuildingHealth:
        case RunUpgradeEffect::DefenseDamage:
        case RunUpgradeEffect::DefenseFireRate:
        case RunUpgradeEffect::ProductionSpeed:
        case RunUpgradeEffect::LumberMillProduction:
        case RunUpgradeEffect::QuarryProduction:
        case RunUpgradeEffect::CrystalMineProduction:
            return 4;
        case RunUpgradeEffect::Damage:
        case RunUpgradeEffect::AttackSpeed:
        case RunUpgradeEffect::MoveSpeed:
        case RunUpgradeEffect::Overkill:
            return 1;
        default:
            return 2;
        }
}

// Prices use run coins, not resources or level-up reroll tokens.
[[nodiscard]] inline int runShopPrice(RunUpgradeEffect effect, RunUpgradeRarity rarity, int wave) {
    const int base = runUpgradeBuilding(effect) || runUpgradeWeapon(effect) ? 18 : 12;
    const long long basePrice=base+8*static_cast<int>(rarity);
    const long long completed=std::max(0LL,static_cast<long long>(wave)-1);
    // Wave scaling applies equally to rarity and blueprints. Integer ceiling
    // preserves exact prices; check before multiplying large late-run values.
    const long long percent=100+25*completed+completed*completed;
    const long long maximumScaled=static_cast<long long>(std::numeric_limits<int>::max())*100;
    if(percent>maximumScaled/basePrice) return std::numeric_limits<int>::max();
    return static_cast<int>((basePrice*percent+99)/100);
}
[[nodiscard]] inline int runShopRerollPrice(const RunShopState& shop) {
    const long long completed=std::max(0LL,static_cast<long long>(shop.wave)-1);
    const long long step=1+completed/4;
    const long long cost=2+completed+step*std::max(0LL,static_cast<long long>(shop.rerolls));
    return static_cast<int>(std::min(cost, static_cast<long long>(std::numeric_limits<int>::max())));
}
} // namespace ian
