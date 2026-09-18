#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"

#include <string_view>

namespace ian {

inline constexpr int ModularBuildingRequiredCoreLevel = 1;

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
    case BuildingType::CrystalMine:
    case BuildingType::SlowTrap:
        return 2;
    case BuildingType::Turret:
    case BuildingType::SpikeTrap:
        return 3;
    case BuildingType::Cannon:
        return 4;
    case BuildingType::Catapult:
        return 5;
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
        return "UNLOCKS  RIFLE · PRODUCTION · SLOW TRAP";
    case 3:
        return "UNLOCKS  BOMBS · CROSSBOW · SPIKE TRAP";
    case 4:
        return "UNLOCKS  ICE WAND · CANNON";
    case 5:
        return "UNLOCKS  FIRE WAND · CATAPULT";
    case 6:
    case 7:
    case 8:
        return "EXPANDS  DEFENSES · PRODUCERS · STORAGE";
    default:
        return {};
    }
}

} // namespace ian
