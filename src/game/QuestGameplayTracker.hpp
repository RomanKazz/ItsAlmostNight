#pragma once

#include "progression/QuestSystem.hpp"
#include "progression/RunUpgrade.hpp"

#include <array>
#include <span>
#include <vector>

namespace ian {

struct GameEvent;
struct SimulationSnapshot;

// Translates authoritative gameplay events; visuals and sandbox never count.
class QuestGameplayTracker {
  public:
    [[nodiscard]] QuestUpdate record(
        const QuestSystem& system, MetaProgression& progression,
        std::span<const GameEvent> events, const SimulationSnapshot& snapshot);

  private:
    void observeStats(const SimulationSnapshot& snapshot);
    std::vector<QuestSignal> signals_;
    std::array<double, 3 + RunUpgradeEffectCount> observedStats_ = [] {
        std::array<double, 3 + RunUpgradeEffectCount> values{};
        values.fill(-1.0);
        return values;
    }();
};

} // namespace ian
