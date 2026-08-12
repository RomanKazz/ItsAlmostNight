#include "ui/UiLabels.hpp"

#include <algorithm>
#include <cmath>

namespace ian {

std::string_view buildingDisplayName(BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return "Core";
    case BuildingType::Wall:
        return "Wall";
    case BuildingType::Turret:
        return "Turret";
    case BuildingType::CrystalMine:
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
    case BuildingType::WoodStorage:
        return "Wood Storage";
    case BuildingType::StoneStorage:
        return "Stone Storage";
    case BuildingType::CrystalStorage:
        return "Crystal Storage";
    }
    return {};
}

std::string introGatherRewardMessage(double insightReward) {
    return "Gathering objective complete: +" +
        std::to_string(static_cast<int>(std::lround(
            std::max(0.0, insightReward)))) +
        " Insight";
}

} // namespace ian
