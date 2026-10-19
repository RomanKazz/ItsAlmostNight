#pragma once
#include "progression/RunShop.hpp"
#include "core/DeterministicRandom.hpp"
#include <span>

namespace ian {
// Pool is already filtered for eligibility and slot limits. Draw every card
// from the same weighted pool, then remove it before the next choice.
[[nodiscard]] inline std::size_t chooseRunUpgradeOffer(
    std::span<const RunUpgradeEffect> pool,
    const std::array<int, RunUpgradeEffectCount>&,
    std::uint64_t& random) {
    if (pool.empty()) return 0;
    int total = 0;
    for (const auto effect : pool) total += runUpgradeOfferWeight(effect);
    random = mixBits64(random + 0x9e3779b97f4a7c15ULL);
    int roll = static_cast<int>(random % static_cast<std::uint64_t>(total));
    std::size_t selected = pool.size()-1;
    for (std::size_t i=0; i<pool.size(); ++i) {
        roll -= runUpgradeOfferWeight(pool[i]);
        if (roll < 0) { selected=i; break; }
    }
    return selected;
}
} // namespace ian
