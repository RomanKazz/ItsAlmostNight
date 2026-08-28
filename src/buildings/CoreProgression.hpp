#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"

#include <string_view>

namespace ian {

inline constexpr int ModularBuildingRequiredCoreLevel = 2;

[[nodiscard]] constexpr int playerWeaponRequiredCoreLevel(
    PlayerWeapon weapon) {
    switch (weapon) {
    case PlayerWeapon::Axe:
    case PlayerWeapon::Pickaxe:
        return 0;
    case PlayerWeapon::Club:
        return 1;
    case PlayerWeapon::Rifle:
        return 2;
    case PlayerWeapon::Bomb:
        return 3;
    case PlayerWeapon::IceWand:
        return 4;
    case PlayerWeapon::FireWand:
        return 5;
    case PlayerWeapon::LegacyBareHands:
    case PlayerWeapon::Hammer:
        return MaxBuildingLevel + 1;
    }
    return MaxBuildingLevel + 1;
}

[[nodiscard]] constexpr int buildingRequiredCoreLevel(
    BuildingType type) {
    switch (type) {
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
    case BuildingType::Turret:
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
        return 2;
    case BuildingType::CrystalMine:
    case BuildingType::Cannon:
        return 3;
    case BuildingType::Catapult:
        return 4;
    case BuildingType::Core:
    case BuildingType::Wall:
    case BuildingType::Gate:
    case BuildingType::GunTurret:
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
        return 1;
    }
    return 1;
}

[[nodiscard]] constexpr std::string_view coreLevelUnlockSummary(
    int level) {
    switch (level) {
    case 2:
        return "UNLOCKS  RIFLE · CROSSBOW · LUMBER MILL · QUARRY · TRAPS · MODULAR";
    case 3:
        return "UNLOCKS  BOMBS · CRYSTAL MINE · CANNON";
    case 4:
        return "UNLOCKS  ICE WAND · CATAPULT";
    case 5:
        return "UNLOCKS  FIRE WAND";
    default:
        return {};
    }
}

} // namespace ian
