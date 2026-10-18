#pragma once

#include "buildings/BuildingSystem.hpp"
#include <algorithm>

namespace ian {
// Copy the authored level/orientation before damage erases a lethal target.
// The existing GameEvent::building payload carries it to presentation only.
inline std::optional<BuildingInstance> buildingBeforeLethalHit(
    const BuildingSystem& buildings, EntityId id, double damage) {
    const auto& all=buildings.buildings();
    const auto found=std::ranges::find(all,id,&BuildingInstance::id);
    if (found==all.end() || damage<=0 || found->health>damage) return std::nullopt;
    return *found;
}
} // namespace ian
