#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"

#include <string_view>

namespace ian {

inline constexpr int ModularBuildingRequiredCoreLevel = 2;

[[nodiscard]] constexpr int modularBuildPieceRequiredCoreLevel(
    ModularBuildPiece piece) {
    switch (piece) {
    case ModularBuildPiece::Foundation:
    case ModularBuildPiece::Wall:
        return 2;
    case ModularBuildPiece::FloorPlatform:
    case ModularBuildPiece::Ramp:
        return 5;
    }
    return MaxBuildingLevel + 1;
}

[[nodiscard]] constexpr int playerWeaponRequiredCoreLevel(
    PlayerWeapon weapon) {
    switch (weapon) {
    case PlayerWeapon::Axe:
    case PlayerWeapon::Pickaxe:
        return 0;
    case PlayerWeapon::Club:
        return 2;
    case PlayerWeapon::Rifle:
        return 3;
    case PlayerWeapon::Bomb:
        return 4;
    case PlayerWeapon::IceWand:
        return 5;
    case PlayerWeapon::FireWand:
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
        return 3;
    case BuildingType::CrystalMine:
    case BuildingType::Wall:
    case BuildingType::Gate:
        return 2;
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
    case BuildingType::Cannon:
        return 4;
    case BuildingType::Catapult:
        return 5;
    case BuildingType::Core:
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
        return "UNLOCKS  WALLS · GATE · CRYSTAL MINE · CLUB";
    case 3:
        return "UNLOCKS  QUARRY · LUMBER MILL · CROSSBOW · RIFLE";
    case 4:
        return "UNLOCKS  SLOW TRAP · SPIKES · CANNON · BOMBS";
    case 5:
        return "UNLOCKS  FLOOR · RAMP · CATAPULT · ICE WAND";
    case 6:
    case 7:
    case 8:
        return "EXPANDS  DEFENSES · PRODUCERS · STORAGE";
    default:
        return {};
    }
}

} // namespace ian
