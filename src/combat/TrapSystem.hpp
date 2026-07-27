#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"

#include <span>
#include <vector>

namespace ian {

struct TrapRuntime {
    EntityId buildingId;
    double cooldownRemaining{};
};

struct TrapActivation {
    EntityId trapId;
    Vec3 position;
    int affectedCount;
    double wearDamage;
};

class TrapSystem {
  public:
    static constexpr double TriggerRadius = 1.6;
    static constexpr double Cooldown = 3.0;
    static constexpr double SlowMultiplier = 0.45;
    static constexpr double SlowDuration = 2.0;
    static constexpr double WearDamage = 10.0;

    TrapSystem();

    void reset();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const TrapActivation> tick(double deltaSeconds,
                                         const std::vector<BuildingInstance>& buildings,
                                         EnemySystem& enemies);

  private:
    std::vector<TrapRuntime> traps_;
    std::vector<TrapActivation> activationBuffer_;
};

} // namespace ian
