#pragma once

#include "buildings/BuildingSystem.hpp"

#include <algorithm>
#include <span>

namespace ian {

inline constexpr double RepairStationInterval = 2.0;
inline constexpr double RepairStationVisualDuration = 1.8;
inline constexpr std::size_t MaximumRepairStationVisualLinks = 64;

[[nodiscard]] inline double repairStationRadius(int level) {
    return 5.0 + .25 * (std::clamp(level, 1, 8) - 1);
}
[[nodiscard]] inline double repairStationRate(int level) {
    return 2.0 + .5 * (std::clamp(level, 1, 8) - 1);
}
[[nodiscard]] inline bool repairStationTarget(BuildingType type) {
    switch (type) {
    case BuildingType::Wall:
    case BuildingType::Gate:
    case BuildingType::Turret:
    case BuildingType::GunTurret:
    case BuildingType::Cannon:
    case BuildingType::Catapult:
    case BuildingType::Flamethrower:
    case BuildingType::MissileLauncher:
    case BuildingType::TeslaTower:
    case BuildingType::LaserTower:
    case BuildingType::Mortar:
    case BuildingType::WindBlower: return true;
    case BuildingType::DecoyBeacon: return true;
    default: return false;
    }
}

struct RepairStationSource {
    EntityId id;
    Vec3 position;
    int level{1};
};
struct RepairStationLink {
    EntityId stationId;
    EntityId targetId;
    Vec3 origin;
    Vec3 target;
};
struct RepairStationDose {
    const RepairStationSource* source{};
    double healing{};
};

// Choose the strongest effective source; overlapping stations never stack.
// A shared run-wide pulse means overlapping stations cannot alternate to stack.
[[nodiscard]] inline RepairStationDose repairStationDose(
    std::span<const RepairStationSource> sources, Vec3 target,
    double pulseCount = 1) {
    RepairStationDose result;
    for (const auto& source : sources) {
        const double x = source.position.x - target.x;
        const double y = source.position.y - target.y;
        const double z = source.position.z - target.z;
        const double radius = repairStationRadius(source.level);
        if (x*x + y*y + z*z > radius*radius + 1e-9) continue;
        const double healing = repairStationRate(source.level) * RepairStationInterval * pulseCount;
        if (healing > result.healing) result = {&source, healing};
    }
    return result;
}

} // namespace ian
