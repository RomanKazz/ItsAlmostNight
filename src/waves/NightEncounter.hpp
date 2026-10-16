#pragma once

#include "core/DeterministicRandom.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace ian {

enum class NightPattern : std::uint8_t { Horde, Blitz, Siege, AirRaid, Crossfire, RoyalCourt };

struct WaveContext {
    std::uint32_t runSeed{};
    int act{};
    std::uint32_t cycle{};
};

struct NightForecast {
    NightPattern pattern{NightPattern::Horde};
    std::array<int, GameBalance::EnemyTypeCount> enemies{};
};

[[nodiscard]] inline std::array<int, GameBalance::EnemyTypeCount> waveEnemyCounts(const WaveDefinition& wave) {
    return {wave.basic, wave.fast, wave.heavy, wave.boss ? 1 : 0,
            wave.ranged, 0, wave.flying, wave.splitter, 0};
}

[[nodiscard]] inline NightPattern nightPattern(const WaveDefinition& base, int wave, std::uint32_t seed) {
    if (base.boss) return NightPattern::RoyalCourt;
    std::array<NightPattern, 5> choices{NightPattern::Horde};
    std::size_t count = 1;
    if (base.fast > 0) choices[count++] = NightPattern::Blitz;
    if (base.heavy > 0) choices[count++] = NightPattern::Siege;
    if (base.flying > 0) choices[count++] = NightPattern::AirRaid;
    if (base.ranged > 0) choices[count++] = NightPattern::Crossfire;
    const auto hash = mixBits64(static_cast<std::uint64_t>(seed) ^
        static_cast<std::uint64_t>(std::max(1, wave)) * 0x9e3779b97f4a7c15ULL);
    return choices[hash % count];
}

[[nodiscard]] inline WaveDefinition nightComposition(const WaveDefinition& configuredBase, NightPattern pattern, int requestedAct) {
    WaveDefinition base = configuredBase;
    base.sapper = 0; // Retired role; old wave data must not bring it back.
    // The tutorial opening stays exactly the configured first-night roster.
    const auto available = waveEnemyCounts(base);
    if (base.fast == 0 && base.heavy == 0 && base.ranged == 0 && base.flying == 0 && base.splitter == 0)
        return base;
    const int act = std::clamp(requestedAct, 0, 2);
    std::array<int, GameBalance::EnemyTypeCount> weights{};
    const auto weight = [&](EnemyType type, int value) { weights[static_cast<std::size_t>(type)] = value; };
    weight(EnemyType::Basic, 25);
    switch (pattern) {
    case NightPattern::Horde:
        weight(EnemyType::Basic, act == 0 ? 85 : 55);
        weight(EnemyType::Splitter, act == 0 ? 5 : 35);
        weight(EnemyType::Fast, act == 2 ? 25 : 10);
        break;
    case NightPattern::Blitz:
        weight(EnemyType::Fast, act == 2 ? 75 : 60);
        weight(EnemyType::Ranged, act == 1 ? 25 : 5);
        break;
    case NightPattern::Siege:
        weight(EnemyType::Heavy, act == 0 ? 45 : 60);
        weight(EnemyType::Ranged, act == 1 ? 30 : 10);
        break;
    case NightPattern::AirRaid:
        weight(EnemyType::Flying, act == 0 ? 65 : 55);
        weight(EnemyType::Fast, act == 2 ? 40 : 15);
        weight(EnemyType::Ranged, act == 1 ? 25 : 0);
        break;
    case NightPattern::Crossfire:
        weight(EnemyType::Ranged, act == 0 ? 60 : 45);
        weight(EnemyType::Fast, act == 0 ? 10 : 40);
        weight(EnemyType::Heavy, act == 2 ? 25 : 0);
        break;
    case NightPattern::RoyalCourt:
        weight(EnemyType::Heavy, act == 1 ? 10 : 40);
        weight(EnemyType::Ranged, act == 1 ? 10 : 35);
        weight(EnemyType::Fast, act == 1 ? 40 : 10);
        weight(EnemyType::Flying, act == 1 ? 35 : 0);
        break;
    }
    constexpr std::array<int, GameBalance::EnemyTypeCount> Costs{1, 2, 5, 0, 3, 4, 3, 4, 0};
    std::int64_t originalBudget = 0;
    int totalWeight = 0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        originalBudget += static_cast<std::int64_t>(std::max(0, available[i])) * Costs[i];
        if (available[i] <= 0 || Costs[i] == 0) weights[i] = 0;
        totalWeight += weights[i];
    }
    if (totalWeight == 0) return base;
    const int budget = static_cast<int>(std::clamp<std::int64_t>(originalBudget, 0, 2560));
    auto counts = available;
    int spent = 0;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        if (Costs[i] == 0) continue;
        counts[i] = budget * weights[i] / totalWeight / Costs[i];
        spent += counts[i] * Costs[i];
    }
    // Fractional budget remainders go to the already-unlocked cheapest role.
    for (std::size_t i = 0; i < counts.size(); ++i) {
        if (Costs[i] == 0 || available[i] <= 0) continue;
        const int extra = (budget - spent) / Costs[i];
        counts[i] += extra;
        spent += extra * Costs[i];
    }
    if (spent != budget) return base;
    WaveDefinition result = base;
    result.basic = counts[0]; result.fast = counts[1]; result.heavy = counts[2];
    result.ranged = counts[4]; result.sapper = counts[5]; result.flying = counts[6]; result.splitter = counts[7];
    result.budget = spent;
    return result;
}

[[nodiscard]] inline int nightAct(double progress) { return progress >= .80 ? 2 : progress >= .40 ? 1 : 0; }
[[nodiscard]] inline bool nightRegrouping(double progress) {
    return (progress >= .36 && progress < .40) || (progress >= .76 && progress < .80);
}
[[nodiscard]] inline double nightPopulationScale(NightPattern pattern, int wave) {
    if (wave <= 1) return 1.0;
    switch (pattern) {
    case NightPattern::Horde: return 1.0;
    case NightPattern::Blitz: return .95;
    case NightPattern::Siege: return .72;
    case NightPattern::AirRaid: return .90;
    case NightPattern::Crossfire: return .85;
    case NightPattern::RoyalCourt: return .85;
    }
    return 1.0;
}

} // namespace ian
