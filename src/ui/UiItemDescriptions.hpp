#pragma once
#include "buildings/BuildingSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include <string_view>

namespace ian {
inline std::string_view modularDescription(ModularBuildPiece piece) {
    switch (piece) {
    case ModularBuildPiece::Foundation: return "A stable base for buildings on uneven ground.";
    case ModularBuildPiece::FloorPlatform: return "A raised floor for expanding your fort across multiple levels.";
    case ModularBuildPiece::Wall: return "Blocks enemies and protects your defenses.";
    case ModularBuildPiece::Ramp: return "Connects floors at different heights.";
    }
    return {};
}
inline std::string_view weaponDescription(PlayerWeapon weapon) {
    switch (weapon) {
    case PlayerWeapon::Axe: return "Harvest trees and strike enemies in melee.";
    case PlayerWeapon::Pickaxe: return "Mine stone and crystals; strike enemies in melee.";
    case PlayerWeapon::Club: return "A heavy melee strike with knockback.";
    case PlayerWeapon::Rifle: return "Shoot enemies at range.";
    case PlayerWeapon::Bomb: return "Throw an explosive into a group of enemies.";
    case PlayerWeapon::IceWand: return "Freeze enemies with icy projectiles.";
    case PlayerWeapon::FireWand: return "Ignite enemies with fiery projectiles.";
    default: return {};
    }
}
inline std::string_view buildingDescription(BuildingType type) {
    switch (type) {
    case BuildingType::Core: return "The heart of your fort. Upgrade it to expand your defenses.";
    case BuildingType::Wall: return "Blocks enemies and protects your defenses.";
    case BuildingType::Gate: return "An entrance through your fortifications.";
    case BuildingType::Turret: return "Fires bolts at approaching enemies.";
    case BuildingType::GunTurret: return "Rapid bullets against the horde.";
    case BuildingType::Cannon: return "Explosive shells damage groups of enemies.";
    case BuildingType::Mortar: return "High-arc shells. Blasts distant groups. Cannot aim within 4 m.";
    case BuildingType::RepairStation: return "Slowly repairs nearby walls and weapons after 6s without damage. Does not repair the Core.";
    case BuildingType::LaserTower: return "Focused beam. Damage ramps up while holding the same target.";
    case BuildingType::TeslaTower: return "Circular defense. Lightning jumps between nearby enemies.";
    case BuildingType::MissileLauncher: return "Guided rockets hit groups and flyers. Cannot aim within 2.5 m.";
    case BuildingType::Flamethrower: return "Short-range flame cone hits groups and leaves a brief afterburn.";
    case BuildingType::Catapult: return "Lobs heavy projectiles at distant enemies.";
    case BuildingType::SpikeTrap: return "Damages enemies crossing the trap.";
    case BuildingType::SlowTrap: return "Slows enemies crossing the trap.";
    case BuildingType::CrystalMine: return "Produces crystals over time.";
    case BuildingType::LumberMill: return "Produces wood over time.";
    case BuildingType::Quarry: return "Produces stone over time.";
    default: return {};
    }
}
} // namespace ian
