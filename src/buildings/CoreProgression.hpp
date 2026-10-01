#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"

#include <string_view>

namespace ian {

inline constexpr int ModularBuildingRequiredCoreLevel = 1;

[[nodiscard]] constexpr int modularBuildPieceRequiredCoreLevel(
    ModularBuildPiece piece) {
    (void)piece;
    return 1;
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
    (void)type;
    return 1;
}

[[nodiscard]] constexpr std::string_view coreLevelUnlockSummary(
    int level) {
    switch (level) {
    case 2:
        return "UNLOCKS  CLUB · EXPANDS CAPACITY";
    case 3:
        return "UNLOCKS  RIFLE · EXPANDS CAPACITY";
    case 4:
        return "UNLOCKS  BOMBS · EXPANDS CAPACITY";
    case 5:
        return "UNLOCKS  ICE WAND · EXPANDS CAPACITY";
    case 6:
    case 7:
    case 8:
        return "EXPANDS  DEFENSES · PRODUCERS · STORAGE";
    default:
        return {};
    }
}

} // namespace ian
