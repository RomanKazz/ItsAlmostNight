#pragma once

#include "core/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace ian {

enum class AttackStat : std::size_t {
    Damage, AttackSpeed, Quantity, ProjectileSpeed, Duration, Size,
    Bounces, Pierce, CritChance, CritDamage, Knockback, Range,
    BurstInterval, Spread, BounceRange, Prediction, Homing, FanSpread, Count
};

struct StatModifier {
    double flat{};
    double multiplier{1.0};
};

// Independent sources multiply their factors; flat bonuses sum. Recompute
// from immutable base data, never mutate the weapon when applying a card.
struct AttackModifiers {
    std::array<StatModifier, static_cast<std::size_t>(AttackStat::Count)> values{};
    StatModifier& operator[](AttackStat stat) { return values[static_cast<std::size_t>(stat)]; }
    const StatModifier& operator[](AttackStat stat) const { return values[static_cast<std::size_t>(stat)]; }
};

struct AttackStats {
    double damage{};
    double attackSpeed{1.0}; // Frequency multiplier of Attack Events.
    double attackInterval{1.0}; // Seconds between event starts, not after their last instance.
    double quantity{1.0}; // Attack Instances per Event; pattern is weapon-specific.
    double projectileSpeed{22.0};
    double duration{3.0};
    double size{1.0};
    // Additional targets AFTER the first hit, independently per instance.
    double bounces{};
    double pierce{};
    double critChance{};
    double critDamage{2.0};
    double knockback{};
    double range{9.0};
    double burstInterval{0.08}; // Authored intra-event spacing before pattern compression.
    double spreadDegrees{2.0};
    double bounceRange{6.0};
    double prediction{};
    double homing{};
    double fanStepDegrees{5.0};
};

struct FinalAttackStats : AttackStats {};
// Stored stats retain fractions. Only a consumer creating/spending instances
// converts the accumulated total to a whole count; fractions are never RNG.
[[nodiscard]] inline int effectiveAttackCount(double value, int minimum = 0) {
    if (!std::isfinite(value)) return minimum;
    return static_cast<int>(std::floor(std::clamp(value, static_cast<double>(minimum),
        static_cast<double>(std::numeric_limits<int>::max() - 1))));
}
inline constexpr double MinimumAttackCooldown = 0.025;
[[nodiscard]] constexpr double defenseProjectileHitRadius(bool bolt) { return bolt ? 0.09 : 0.13; }

[[nodiscard]] inline FinalAttackStats resolveAttackStats(
    const AttackStats& base, std::span<const AttackModifiers> sources = {}) {
    const auto value = [&](AttackStat stat, double initial) {
        double factor = 1.0;
        double flat = 0.0;
        for (const auto& source : sources) {
            if (std::isfinite(source[stat].multiplier)) factor *= source[stat].multiplier;
            if (std::isfinite(source[stat].flat)) flat += source[stat].flat;
        }
        const double result = initial * factor + flat;
        return std::isfinite(result) ? result : initial;
    };
    const auto count = [&](AttackStat stat, double initial, int minimum) {
        return std::clamp(value(stat, initial), static_cast<double>(minimum),
            static_cast<double>(std::numeric_limits<int>::max() - 1));
    };
    FinalAttackStats result;
    result.damage = std::max(0.0, value(AttackStat::Damage, base.damage));
    result.attackSpeed = std::max(0.001, value(AttackStat::AttackSpeed, base.attackSpeed));
    result.attackInterval = std::max(MinimumAttackCooldown,
        base.attackInterval / result.attackSpeed);
    result.quantity = count(AttackStat::Quantity, base.quantity, 1);
    result.projectileSpeed = std::max(0.0, value(AttackStat::ProjectileSpeed, base.projectileSpeed));
    result.duration = std::max(0.001, value(AttackStat::Duration, base.duration));
    result.size = std::max(0.001, value(AttackStat::Size, base.size));
    result.bounces = count(AttackStat::Bounces, base.bounces, 0);
    result.pierce = count(AttackStat::Pierce, base.pierce, 0);
    result.critChance = std::clamp(value(AttackStat::CritChance, base.critChance), 0.0, 1.0);
    result.critDamage = std::max(1.0, value(AttackStat::CritDamage, base.critDamage));
    result.knockback = std::max(0.0, value(AttackStat::Knockback, base.knockback));
    result.range = std::max(0.0, value(AttackStat::Range, base.range));
    result.burstInterval = std::max(0.001, value(AttackStat::BurstInterval, base.burstInterval));
    result.spreadDegrees = std::clamp(value(AttackStat::Spread, base.spreadDegrees), 0.0, 180.0);
    result.bounceRange = std::max(0.0, value(AttackStat::BounceRange, base.bounceRange));
    result.prediction = std::clamp(value(AttackStat::Prediction, base.prediction), 0.0, 1.0);
    result.homing = std::max(0.0, value(AttackStat::Homing, base.homing));
    result.fanStepDegrees = std::max(0.0, value(AttackStat::FanSpread, base.fanStepDegrees));
    return result;
}

// Count determines instances; a weapon chooses fan, sequence, areas, etc.
[[nodiscard]] inline double fanAngleDegrees(int index, int quantity, double stepDegrees) {
    return (static_cast<double>(index) - (quantity - 1) * 0.5) * stepDegrees;
}

struct AttackEvent {
    EntityId sourceId;
    FinalAttackStats stats;
};

} // namespace ian
