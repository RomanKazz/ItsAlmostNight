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
    double activationRemaining{};
};

struct TrapActivation {
    EntityId trapId;
    Vec3 position;
    int affectedCount;
    double wearDamage;
};

struct TrapHit {
    EntityId trapId;
    EnemyDamageResult result;
};

class TrapSystem {
  public:
    static constexpr double TriggerRadius = 1.6;
    static constexpr double Cooldown = 3.0;
    static constexpr double SlowMultiplier = 0.45;
    static constexpr double SlowDuration = 2.0;
    static constexpr double WearDamage = 10.0;
    static constexpr double SpikeAnimationDuration = 1.2;

    TrapSystem();

    [[nodiscard]] static double triggerRadius(std::uint8_t level);
    [[nodiscard]] static double slowPercent(std::uint8_t level);
    [[nodiscard]] static double slowDuration(std::uint8_t level);
    [[nodiscard]] static double cooldown(std::uint8_t level);
    [[nodiscard]] static double spikeTriggerRadius(std::uint8_t level);
    [[nodiscard]] static double spikeDamage(std::uint8_t level);
    [[nodiscard]] static double spikeCooldown(std::uint8_t level);

    void reset();
    void setSkillModifiers(double damage, double radius,
                           double fireRate, double highGroundDamage,
                           double reactiveShockDamage = 0.0);
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const TrapActivation> tick(double deltaSeconds,
                                         const std::vector<BuildingInstance>& buildings,
                                         EnemySystem& enemies);
    [[nodiscard]] const std::vector<TrapRuntime>& traps() const;
    [[nodiscard]] std::span<const TrapHit> hits() const;

  private:
    std::vector<TrapRuntime> traps_;
    std::vector<TrapActivation> activationBuffer_;
    std::vector<TrapHit> hitBuffer_;
    double damageMultiplier_{1.0};
    double radiusMultiplier_{1.0};
    double fireRateMultiplier_{1.0};
    double highGroundDamageMultiplier_{1.0};
    double reactiveShockDamage_{};
};

} // namespace ian
