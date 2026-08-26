#pragma once

#include "buildings/BuildingSystem.hpp"

#include <string_view>

namespace ian {

inline constexpr int ModularBuildingRequiredCoreLevel = 2;

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
        return "UNLOCKS  CROSSBOW · LUMBER MILL · QUARRY · TRAPS · MODULAR";
    case 3:
        return "UNLOCKS  CRYSTAL MINE · CANNON";
    case 4:
        return "UNLOCKS  CATAPULT";
    default:
        return {};
    }
}

} // namespace ian
