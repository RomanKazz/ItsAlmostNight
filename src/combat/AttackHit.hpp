#pragma once
#include "combat/AttackStats.hpp"
#include "enemies/EnemySystem.hpp"
#include "core/DeterministicRandom.hpp"
#include <optional>

namespace ian {
struct AttackHit {
    EnemyDamageResult result;
    bool critical{};
    double payloadDamage{};
};

// Shared by travelling instances, explosions and traps. Armor/elite health
// rules remain exclusively in EnemySystem::damage.
[[nodiscard]] inline std::optional<AttackHit> applyAttackHit(
    EnemySystem& enemies, EntityId target, const AttackStats& stats,
    Vec3 direction, std::uint64_t seed) {
    const double roll = static_cast<double>(mixBits64(seed) >> 11U) / 9007199254740992.0;
    const bool critical = roll < stats.critChance;
    const auto result = enemies.damage(target,
        stats.damage * (critical ? stats.critDamage : 1.0) * enemies.markedDefenseDamage(target));
    if (!result) return std::nullopt;
    if (!result->killed) enemies.applyKnockback(target, direction, stats.knockback);
    return AttackHit{*result, critical, stats.damage};
}
} // namespace ian
