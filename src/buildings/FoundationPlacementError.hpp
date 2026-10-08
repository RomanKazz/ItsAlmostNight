#pragma once

#include "buildings/BuildingSystem.hpp"

namespace ian {

[[nodiscard]] inline PlacementError foundationPlacementError(ModularPlacementError error) {
    switch (error) {
    case ModularPlacementError::None: return PlacementError::None;
    case ModularPlacementError::ResourceBlocked: return PlacementError::ResourceBlocked;
    case ModularPlacementError::InsufficientResources: return PlacementError::InsufficientResources;
    case ModularPlacementError::OutsideCoreArea: return PlacementError::OutsideCoreArea;
    case ModularPlacementError::TooFar: return PlacementError::OutOfRange;
    case ModularPlacementError::CoreLevelRequired: return PlacementError::CoreLevelRequired;
    case ModularPlacementError::PlayerOverlap: return PlacementError::PlayerOverlap;
    default: return PlacementError::WorldCollision;
    }
}

} // namespace ian
