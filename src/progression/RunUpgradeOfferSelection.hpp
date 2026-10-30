#pragma once
#include "progression/RunShop.hpp"
#include "core/DeterministicRandom.hpp"
#include <span>

namespace ian {
// Ordinary level-up cards choose the ownership category first. Each item
// within that category has equal chance, regardless of pool size or level.
[[nodiscard]] inline int runLevelUpOfferWeight(
    RunUpgradeEffect effect,
    const std::array<int, RunUpgradeEffectCount>& levels) {
    (void)levels;
    return runUpgradeOfferWeight(effect);
}

[[nodiscard]] inline std::size_t chooseRunUpgradeOffer(
    std::span<const RunUpgradeEffect> pool,
    const std::array<int, RunUpgradeEffectCount>& levels,
    std::uint64_t& random) {
    if (pool.empty()) return 0;
    std::size_t existingCount = 0;
    std::size_t newCount = 0;
    for (const auto effect : pool) {
        if (levels[runUpgradeIndex(effect)] > 0) ++existingCount;
        else ++newCount;
    }
    random = mixBits64(random + 0x9e3779b97f4a7c15ULL);
    const bool chooseExisting = newCount == 0 ||
        (existingCount > 0 && random % 100ULL < 45ULL);
    const std::size_t count = chooseExisting ? existingCount : newCount;
    random = mixBits64(random + 0xa0761d6478bd642fULL);
    std::size_t roll = static_cast<std::size_t>(random % count);
    for (std::size_t i = 0; i < pool.size(); ++i) {
        if ((levels[runUpgradeIndex(pool[i])] > 0) != chooseExisting) continue;
        if (roll-- == 0) return i;
    }
    return pool.size() - 1;
}
} // namespace ian
