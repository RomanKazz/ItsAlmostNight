#pragma once
#include "progression/RunShop.hpp"
#include "core/DeterministicRandom.hpp"
#include <span>

namespace ian {
// Pool is already filtered for eligibility, slot limits and duplicates.
// Keep category weights. Within tomes or combat cards, split owned/new 50/50
// whenever both groups remain eligible, independent of group size or levels.
[[nodiscard]] inline std::size_t chooseRunUpgradeOffer(
    std::span<const RunUpgradeEffect> pool,
    const std::array<int, RunUpgradeEffectCount>& stacks,
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
    const bool defense = runDefenseCard(pool[selected]);
    if (!defense && !runTomeDefinition(pool[selected])) return selected;
    const auto sameCategory = [defense](RunUpgradeEffect effect) {
        return defense ? runDefenseCard(effect) : runTomeDefinition(effect) != nullptr;
    };
    std::array<std::size_t,2> counts{};
    for (const auto effect : pool) if (sameCategory(effect))
        ++counts[runTomeLevel(effect,stacks)>0 ? 1U : 0U];
    random = mixBits64(random + 0xd1b54a32d192ed03ULL);
    const std::size_t group = !counts[0] ? 1U : !counts[1] ? 0U : random % 2U;
    random = mixBits64(random + 0xa0761d6478bd642fULL);
    std::size_t offset = random % counts[group];
    for (std::size_t i=0;i<pool.size();++i) {
        if (!sameCategory(pool[i]) || (runTomeLevel(pool[i],stacks)>0 ? 1U : 0U)!=group) continue;
        if (offset-- == 0) return i;
    }
    return selected;
}
} // namespace ian
