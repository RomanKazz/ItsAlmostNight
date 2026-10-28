#pragma once

#include "game/PlayerClass.hpp"

#include <algorithm>

namespace ian {

// Derived from the run's earned levels, never accumulated into saved stat
// multipliers. Reloading or refreshing equipment cannot apply growth twice.
struct PlayerClassAbilityState {
    double rageRemaining{};
    double exhaustionRemaining{};
    int stumpCombo{};
    double stumpPulseRemaining{};
};

struct PlayerClassLevelBonuses {
    double maximumHealth{};
    double rangedDamage{1};
    double rangedSpeed{1};
    double productionSpeed{1};
    double woodYield{1};
    double meleeDamage{1};
    double rootWaveScale{1};
    double moveSpeed{1};
    double meleeSpeed{1};
    double defenseDamage{1};
    double gatheringYield{1};
    double carryCapacity{1};
    double movingDamage{1.2};
    double missingHealthDamage{0.75};
    double armorBreakRadius{4.5};
    double fortificationDuration{10};
    double killHealing{2};
    double bloodShieldCapacity{15};
    double appleHealing{1};
    double potionDurationBonus{};
    double rageDuration{4};
    double chainHarvestRadius{4};
};

[[nodiscard]] inline PlayerClassLevelBonuses playerClassLevelBonuses(PlayerClass type, int level) {
    const double earned = static_cast<double>(std::max(1, level)) - 1.0;
    PlayerClassLevelBonuses result;
    switch (type) {
    case PlayerClass::Vanguard:
        result.armorBreakRadius = std::min(8.0, 4.5 + 0.05*earned);
        break;
    case PlayerClass::Ranger:
        result.movingDamage = 1.2 + std::min(0.5, 0.01*earned);
        result.moveSpeed = 1 + std::min(0.5, 0.005*earned);
        break;
    case PlayerClass::Engineer:
        result.defenseDamage = 1 + 0.02*earned;
        result.fortificationDuration = 20 + 0.25*earned;
        break;
    case PlayerClass::Prospector:
        result.gatheringYield = 1.35 + 0.02*earned;
        result.carryCapacity = 1.25 + 0.02*earned;
        result.chainHarvestRadius = std::min(7.0,4+0.03*earned);
        break;
    case PlayerClass::Berserker:
        result.missingHealthDamage = 0.75 + 0.02*earned;
        result.meleeSpeed = 1 + 0.01*earned;
        result.rageDuration = std::min(6.0,4+0.04*earned);
        break;
    case PlayerClass::Vampire:
        result.killHealing = 2 + 0.1*earned;
        result.bloodShieldCapacity = 15 + earned;
        break;
    case PlayerClass::Alchemist:
        result.appleHealing = 1.5 + 0.02*earned;
        result.potionDurationBonus = 3 + 0.1*earned;
        break;
    case PlayerClass::Chronomancer:
        result.moveSpeed = 1 + std::min(0.5, 0.005*earned);
        break;
    case PlayerClass::Stump:
        result.meleeDamage = 1 + 0.02*earned;
        result.rootWaveScale = 1 + 0.005*earned;
        break;
    case PlayerClass::Lumberjack:
        result.meleeDamage = 1.20 + .02 * earned;
        result.woodYield = 1.20;
        result.maximumHealth = 3 * earned;
        break;
    case PlayerClass::Sharpshooter:
        result.rangedDamage = 1 + .02 * earned;
        result.rangedSpeed = 1.15;
        result.maximumHealth = 2 * earned;
        break;
    case PlayerClass::Mechanic:
        result.defenseDamage = 1 + .02 * earned;
        result.maximumHealth = 2 * earned;
        break;
    case PlayerClass::Miner:
        result.gatheringYield = 1.20;
        result.productionSpeed = 1 + .02 * earned;
        result.maximumHealth = 2 * earned;
        break;
    case PlayerClass::None: break;
    }
    return result;
}

} // namespace ian
