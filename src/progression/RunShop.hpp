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
    const auto cost = static_cast<long long>(base + 8 * static_cast<int>(rarity)) +
        3LL * std::max(0LL, static_cast<long long>(wave) - 1);
    return static_cast<int>(std::min(cost, static_cast<long long>(std::numeric_limits<int>::max())));
}
[[nodiscard]] inline int runShopRerollPrice(const RunShopState& shop) {
    const long long step = std::max(1LL, shop.wave * 2LL / 5);
    const long long cost = std::max(1LL, shop.wave * 3LL / 4) + step * (1LL + shop.rerolls);
    return static_cast<int>(std::min(cost, static_cast<long long>(std::numeric_limits<int>::max())));
}
} // namespace ian
