#pragma once

#include "progression/RunUpgrade.hpp"

#include <string_view>

namespace ian {

// Acquisition cards explain the role; subsequent cards show actual stat gains.
[[nodiscard]] constexpr std::string_view runUpgradeRoleDescription(RunUpgradeEffect effect) {
    switch (effect) {
    case RunUpgradeEffect::UnlockGunTurret: return "Rapid fire against ground and air.";
    case RunUpgradeEffect::UnlockTurret: return "Piercing bolts cut through enemy lines.";
    case RunUpgradeEffect::UnlockCannon: return "Explosive shots punish grouped enemies.";
    case RunUpgradeEffect::UnlockCatapult: return "Heavy shots stagger enemy groups.";
    case RunUpgradeEffect::UnlockMortar: return "Artillery blasts distant enemy groups.";
    case RunUpgradeEffect::UnlockFlamethrower: return "Burns crowds at close range.";
    case RunUpgradeEffect::UnlockMissileLauncher: return "Splash rockets hit ground and air.";
    case RunUpgradeEffect::UnlockTeslaTower: return "Lightning chains between nearby enemies.";
    case RunUpgradeEffect::UnlockLaserTower: return "Focus on one target to build damage.";
    case RunUpgradeEffect::UnlockWindBlower: return "Pushes groups of enemies back.";
    case RunUpgradeEffect::UnlockSpikeTrap: return "Damages enemies crossing the trap.";
    case RunUpgradeEffect::UnlockSlowTrap: return "Slows enemies crossing the trap.";
    case RunUpgradeEffect::UnlockRepairStation: return "Repairs nearby walls and defenses.";
    case RunUpgradeEffect::UnlockDecoyBeacon: return "Lures enemies away from your defenses.";
    case RunUpgradeEffect::UnlockWall: return "Blocks the enemy advance.";
    case RunUpgradeEffect::UnlockLumberMill: return "Produces wood over time.";
    case RunUpgradeEffect::UnlockQuarry: return "Produces stone over time.";
    case RunUpgradeEffect::UnlockCrystalMine: return "Produces crystals over time.";
    case RunUpgradeEffect::UnlockClub: return "Sweeping melee blows knock enemies back.";
    case RunUpgradeEffect::UnlockRifle: return "Accurate shots against distant enemies.";
    case RunUpgradeEffect::UnlockBomb: return "Thrown explosives damage enemy groups.";
    case RunUpgradeEffect::UnlockIceWand: return "Ice shots slow approaching enemies.";
    case RunUpgradeEffect::UnlockFireWand: return "Fireballs ignite groups of enemies.";
    case RunUpgradeEffect::UpgradeAxe: return "Chops trees for wood.";
    case RunUpgradeEffect::UpgradePickaxe: return "Mines stone and crystals.";
    default: return {};
    }
}

} // namespace ian
