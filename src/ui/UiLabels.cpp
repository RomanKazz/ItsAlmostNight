#include "ui/UiLabels.hpp"

#include <algorithm>
#include <cmath>

namespace ian {

std::string_view placementErrorMessage(PlacementError error) {
    switch (error) {
    case PlacementError::None:
        return "";
    case PlacementError::CoreAlreadyPlaced:
        return "Core already placed";
    case PlacementError::CoreRequired:
        return "Place Core first";
    case PlacementError::InsufficientResources:
        return "Not enough resources";
    case PlacementError::Occupied:
        return "Space occupied";
    case PlacementError::OutsideCoreArea:
        return "Outside Core area";
    case PlacementError::PlayerOverlap:
        return "Player blocks placement";
    case PlacementError::WorldCollision:
        return "Terrain or structure blocks placement";
    case PlacementError::LimitReached:
        return "Building limit reached";
    case PlacementError::OutOfRange:
        return "Placement is too far";
    case PlacementError::BlueprintRequired:
        return "Choose this building on level-up";
    case PlacementError::CoreLevelRequired:
        return "Upgrade Core to unlock";
    case PlacementError::ResourceBlocked:
        return "Clear tree or stone first";
    }
    return "";
}


std::string_view buildingDisplayName(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return "Core";
    case BuildingType::Wall:
        return "Wall";
    case BuildingType::Turret:
        return "Crossbow";
    case BuildingType::GunTurret:
        return "Turret";
    case BuildingType::CrystalMine:
        return "Crystal Mine";
    case BuildingType::Cannon:
        return "Cannon";
    case BuildingType::Catapult:
        return "Catapult";
    case BuildingType::TeslaTower: return "Tesla";
    case BuildingType::LaserTower: return "Laser";
    case BuildingType::Mortar: return "Mortar";
    case BuildingType::DecoyBeacon: return "Decoy";
    case BuildingType::WindBlower: return "Wind Blower";
    case BuildingType::RepairStation: return "Repair Station";
    case BuildingType::MissileLauncher: return "Missile Launcher";
    case BuildingType::Flamethrower:
        return "Flamethrower";
    case BuildingType::SlowTrap:
        return "Slow Trap";
    case BuildingType::Gate:
        return "Gate";
    case BuildingType::LumberMill:
        return "Lumber Mill";
    case BuildingType::Quarry:
        return "Quarry";
    case BuildingType::SpikeTrap:
        return "Spike Trap";
    case BuildingType::WoodStorage:
        return "Wood Storage";
    case BuildingType::StoneStorage:
        return "Stone Storage";
    case BuildingType::CrystalStorage:
        return "Crystal Storage";
    }
    return {};
}

} // namespace ian
