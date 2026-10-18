#pragma once
#include "buildings/BuildingSystem.hpp"
#include "combat/AttackStats.hpp"

namespace ian {
struct DefenseTomeScaling {
    double extraInstanceDamage{1.0};
    double bounceRetention{1.0};
    double pierceRetention{1.0};
};
// Damage on authored attacks stays intact. Only additional tome interactions
// lose energy; counts and their fractional accumulation are never transformed.
[[nodiscard]] constexpr DefenseTomeScaling defenseTomeScaling(BuildingType type) {
    switch (type) {
    case BuildingType::GunTurret: return {.50, .75, .85};
    case BuildingType::Turret: return {.50, .75, .85};
    case BuildingType::Cannon:
    case BuildingType::Catapult:
    case BuildingType::Mortar: return {.25, .50, 1.0};
    case BuildingType::MissileLauncher: return {.50, 1.0, 1.0};
    case BuildingType::TeslaTower: return {.35, .75, 1.0};
    case BuildingType::LaserTower: return {.25, .50, 1.0};
    case BuildingType::SpikeTrap: return {.35, 1.0, 1.0};
    default: return {};
    }
}
[[nodiscard]] inline FinalAttackStats defenseInstanceStats(
    const FinalAttackStats& stats, BuildingType type, int instance) {
    auto result = stats;
    if (instance >= effectiveAttackCount(stats.nativeQuantity, 1))
        result.damage *= defenseTomeScaling(type).extraInstanceDamage;
    return result;
}
[[nodiscard]] inline FinalAttackStats defenseContactStats(
    const FinalAttackStats& stats, BuildingType type, int bouncesUsed, int pierceUsed) {
    const auto response = defenseTomeScaling(type);
    auto result = stats;
    const int bonusBounces = std::max(0, bouncesUsed - effectiveAttackCount(stats.nativeBounces));
    const int bonusPierce = std::max(0, pierceUsed - effectiveAttackCount(stats.nativePierce));
    if (bonusBounces) result.damage *= std::pow(response.bounceRetention, bonusBounces);
    if (bonusPierce) result.damage *= std::pow(response.pierceRetention, bonusPierce);
    return result;
}
} // namespace ian
