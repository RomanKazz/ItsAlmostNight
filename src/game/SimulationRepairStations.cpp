#include "game/Simulation.hpp"
#include "game/ModularCombat.hpp"

#include <algorithm>
#include <cmath>

namespace ian {

void Simulation::updateRepairStations(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0) return;
    // Cancel stale visuals immediately when a source or target is removed.
    std::erase_if(repairStationLinks_, [this](const auto& link) {
        const auto source = std::ranges::find(buildings_.buildings(), link.stationId, &BuildingInstance::id);
        const bool targetAlive = std::ranges::find(buildings_.buildings(), link.targetId, &BuildingInstance::id) != buildings_.buildings().end() ||
            std::ranges::find(foundations_.walls(), link.targetId, &WallInstance::id) != foundations_.walls().end();
        return source == buildings_.buildings().end() || source->type != BuildingType::RepairStation || !targetAlive;
    });
    repairStationCycleRemaining_ -= deltaSeconds;
    if (repairStationCycleRemaining_ > 1e-9) {
        if (RepairStationInterval - repairStationCycleRemaining_ >= RepairStationVisualDuration)
            repairStationLinks_.clear();
        return;
    }
    const double pulses = 1 + std::floor(std::max(0.0, -repairStationCycleRemaining_) / RepairStationInterval);
    repairStationCycleRemaining_ = std::clamp(repairStationCycleRemaining_ + pulses * RepairStationInterval,
        1e-9, RepairStationInterval);
    repairStationLinks_.clear();
    repairStationSources_.clear();
    for (const auto& building : buildings_.buildings()) {
        if (building.type != BuildingType::RepairStation || building.health <= 0) continue;
        repairStationSources_.push_back({building.id, buildingWorldPosition(building),
            building.level});
    }
    const auto remember = [this](const RepairStationDose& dose, EntityId targetId, Vec3 target) {
        if (repairStationLinks_.size() >= MaximumRepairStationVisualLinks) return;
        Vec3 origin = dose.source->position;
        origin.y += 1.205; // Authored socket_effect_repair.
        repairStationLinks_.push_back({dose.source->id, targetId, origin, target});
    };
    for (const auto& target : buildings_.buildings()) {
        if (!repairStationTarget(target.type) || target.health >= target.maxHealth) continue;
        Vec3 center = buildingWorldPosition(target);
        const auto dose = repairStationDose(repairStationSources_, center, pulses);
        if (!dose.source || buildings_.restoreHealth(target.id, dose.healing) <= 0) continue;
        center.y += target.type == BuildingType::Wall || target.type == BuildingType::Gate ? 1.0 : .8;
        remember(dose, target.id, center);
    }
    for (const auto& wall : foundations_.walls()) {
        if (wall.health >= wall.maxHealth) continue;
        Vec3 center = modularBaseCenter(wall, worldConfig_);
        const auto dose = repairStationDose(repairStationSources_, center, pulses);
        if (!dose.source || foundations_.restoreWallHealth(wall.id, dose.healing) <= 0) continue;
        center.y += .5 * (wall.topHeight - wall.bottomHeight);
        remember(dose, wall.id, center);
    }
    if (RepairStationInterval - repairStationCycleRemaining_ >= RepairStationVisualDuration)
        repairStationLinks_.clear();
}

} // namespace ian
