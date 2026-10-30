#pragma once

#include "core/Types.hpp"
#include <raylib.h>
#include <vector>

namespace ian {
struct SimulationSnapshot;
enum class MapIcon { Chest, Wood, Stone, Crystal, Ruins, Supplies, Challenge, Forest, Desert, Snow, Corrupted, Mountain, Base, Boss, Boulders, PlayerShrine, BuildingShrine, Industry, Count };
struct MinimapMarker {
    MapIcon icon;
    Vec3 position;
    int priority{};
    int count{1};
    bool highlighted{};
};
// World-aligned resource groups do not reshuffle when the player moves or turns.
[[nodiscard]] std::vector<MinimapMarker> collectMinimapMarkers(const SimulationSnapshot& snapshot);
} // namespace ian
