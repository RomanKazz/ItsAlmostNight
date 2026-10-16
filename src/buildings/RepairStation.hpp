#pragma once

#include "buildings/BuildingSystem.hpp"

#include <algorithm>
#include <span>

namespace ian {

inline constexpr double RepairStationDamagePause = 6.0;
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
    case BuildingType::Mortar: return true;
    default: return false;
    }
}

struct RepairStationSource {
    EntityId id;
    Vec3 position;
    int level{1};
    double delayRemaining{};
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
// Only the part of this tick after both damage pauses can restore health.
[[nodiscard]] inline RepairStationDose repairStationDose(
    std::span<const RepairStationSource> sources, Vec3 target,
    double targetDelay, double deltaSeconds) {
    RepairStationDose result;
    for (const auto& source : sources) {
        const double x = source.position.x - target.x;
        const double y = source.position.y - target.y;
        const double z = source.position.z - target.z;
        const double radius = repairStationRadius(source.level);
        if (x*x + y*y + z*z > radius*radius + 1e-9) continue;
        const double activeSeconds = std::max(0.0, deltaSeconds -
            std::max(targetDelay, source.delayRemaining));
        const double healing = repairStationRate(source.level) * activeSeconds;
        if (healing > result.healing) result = {&source, healing};
    }
    return result;
}

} // namespace ian
