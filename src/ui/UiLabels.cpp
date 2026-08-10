#include "ui/UiLabels.hpp"

namespace ian {

std::string_view buildingDisplayName(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return "Core";
    case BuildingType::Wall:
        return "Wall";
    case BuildingType::Turret:
        return "Turret";
    case BuildingType::GoldMine:
        return "Crystal Mine";
    case BuildingType::Cannon:
        return "Cannon";
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
    }
    return {};
}

} // namespace ian
