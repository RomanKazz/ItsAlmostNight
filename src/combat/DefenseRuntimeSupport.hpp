#pragma once

#include "buildings/BuildingOrientation.hpp"
#include "buildings/BuildingSystem.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ian::defense_runtime {

// Share registration and rotation invalidation without coupling hitscan
// attacks to ballistic projectiles or changing saved runtime structures.
template <typename Runtime, typename AcceptsBuilding, typename MatchesRuntime>
void syncBuildings(
    std::vector<Runtime>& runtimes,
    const std::vector<BuildingInstance>& buildings, double searchInterval,
    AcceptsBuilding acceptsBuilding, MatchesRuntime matchesRuntime) {
    std::erase_if(runtimes, [&](const Runtime& runtime) {
        return std::none_of(buildings.begin(), buildings.end(),
            [&](const BuildingInstance& building) {
                return building.id == runtime.buildingId &&
                    acceptsBuilding(building.type) &&
                    matchesRuntime(building, runtime);
            });
    });
    for (const BuildingInstance& building : buildings) {
        if (!acceptsBuilding(building.type)) continue;
        const double restYaw = buildingRotationYaw(building.type, building.rotation);
        const auto runtime = std::find_if(runtimes.begin(), runtimes.end(),
            [&](const Runtime& item) { return item.buildingId == building.id; });
        if (runtime == runtimes.end()) {
            runtimes.push_back({
                .buildingId = building.id,
                .type = building.type,
                .restYaw = restYaw,
                .baseYaw = restYaw,
                .yaw = restYaw,
            });
        } else if (std::abs(wrapBuildingAngle(runtime->restYaw - restYaw)) > 0.0001) {
            runtime->restYaw = restYaw;
            runtime->targetId.reset();
            runtime->targetSearchCooldownRemaining = searchInterval;
        }
    }
}

} // namespace ian::defense_runtime
