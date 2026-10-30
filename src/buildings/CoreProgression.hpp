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

// Historical gates are retained only to migrate pre-blueprint saved runs.
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
    case PlayerWeapon::LegacyHammer:
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
    return level > 1 && level <= MaxBuildingLevel
        ? "EXPANDS  BUILD RANGE · CAPACITY" : std::string_view{};
}

} // namespace ian
