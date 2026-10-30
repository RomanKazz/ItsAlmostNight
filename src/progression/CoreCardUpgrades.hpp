#pragma once

#include "core/DeterministicRandom.hpp"
#include "progression/RunUpgrade.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace ian {

enum class CoreCardStat : std::size_t {
    Health,
    BuildRadius,
    Buildings,
    Storage,
};

inline constexpr std::size_t CoreCardStatCount = 4;
inline constexpr std::array CoreCardOfferedStats{
    CoreCardStat::Health, CoreCardStat::BuildRadius, CoreCardStat::Storage};
inline constexpr double CoreStorageBonusPerPoint = 0.20;

struct CoreCardRoll {
    std::array<CoreCardStat, 2> stats{};
    std::uint8_t count{1};
    double power{1.0};
};

[[nodiscard]] inline CoreCardRoll rollCoreCardUpgrade(
    RunUpgradeRarity rarity, std::uint64_t seed) {
    const bool twoStats = rarity != RunUpgradeRarity::Common ||
        (mixBits64(seed ^ 0x43b0d7e5ULL) & 1ULL) != 0;
    CoreCardRoll result;
    result.count = twoStats ? 2 : 1;
    result.power = runUpgradeRarityMultiplier(rarity);
    const auto first = mixBits64(seed ^ 0xa0761d6478bd642fULL) % CoreCardOfferedStats.size();
    result.stats[0] = CoreCardOfferedStats[first];
    if (twoStats) {
        const auto other = mixBits64(seed ^ 0xe7037ed1a0b428dbULL) % (CoreCardOfferedStats.size() - 1);
        result.stats[1] = CoreCardOfferedStats[other >= first ? other + 1 : other];
    }
    return result;
}

[[nodiscard]] constexpr std::string_view coreCardStatName(CoreCardStat stat) {
    switch (stat) {
    case CoreCardStat::Health: return "Core HP";
    case CoreCardStat::BuildRadius: return "Build range";
    case CoreCardStat::Buildings: return "Buildings";
    case CoreCardStat::Storage: return "All storage";
    }
    return "Core";
}

} // namespace ian
