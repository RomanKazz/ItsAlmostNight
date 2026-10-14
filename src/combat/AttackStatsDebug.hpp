#pragma once
#include "combat/AttackPattern.hpp"
#include "buildings/BuildingSystem.hpp"
#include <iomanip>
#include <sstream>
#include <string>

namespace ian {
[[nodiscard]] inline std::string describeAttackStats(const FinalAttackStats& stats, BuildingType type = BuildingType::GunTurret) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3)
        << "Damage: " << stats.damage << "\nAttack Speed: " << stats.attackSpeed << "x"
        << "\nAttack Interval: " << stats.attackInterval << "s"
        << "\nProjectile Count: " << stats.quantity << " (active " << effectiveAttackCount(stats.quantity, 1) << ")\nProjectile Speed: " << stats.projectileSpeed
        << "\nDuration: " << stats.duration << "s\nSize: " << stats.size
        << "\nBounces: " << stats.bounces << " (active " << effectiveAttackCount(stats.bounces) << ")\nPierce: " << stats.pierce
        << " (active " << effectiveAttackCount(stats.pierce) << ")"
        << "\nCrit Chance: " << stats.critChance * 100.0 << "%\nCrit Damage: " << stats.critDamage << "x"
        << "\nKnockback: " << stats.knockback << "\nSpread: " << stats.spreadDegrees << " deg"
        << "\nBase Burst Interval: " << stats.burstInterval << "s\nBounce Range: " << stats.bounceRange
        << "\nRange: " << stats.range;
    if (type == BuildingType::GunTurret || type == BuildingType::MissileLauncher) {
        const auto timing = gunBurstTiming(stats);
        out << "\nEffective Burst Interval: " << timing.interval << "s"
            << "\nTotal Burst Duration: " << timing.duration << "s";
    }
    if (type == BuildingType::MissileLauncher) out << "\nHoming: " << stats.homing << "\nExplosion Radius: " << stats.size * 1.6;
    return out.str();
}
} // namespace ian
