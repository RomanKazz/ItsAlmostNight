#pragma once

#include "core/PerformanceStats.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace ian {

enum class WorldObjectStage : std::size_t {
    Decorations, Resources, Rocks, Trees, LootProps, Pickups,
    DestroyedResources, Buildings, ProjectilesPortals, Enemies, Grass, Count
};

inline constexpr std::array<std::string_view,
    static_cast<std::size_t>(WorldObjectStage::Count)> WorldObjectColumns{
    "objects_decorations_ms", "objects_resources_ms", "objects_rocks_ms",
    "objects_trees_ms", "objects_loot_props_ms", "objects_pickups_ms",
    "objects_destroyed_resources_ms", "objects_buildings_ms",
    "objects_projectiles_portals_ms", "objects_enemies_ms", "objects_grass_ms",
};
using WorldObjectTimings = std::array<double, WorldObjectColumns.size()>;

// CPU submission intervals, including any driver wait. Do not flush the GPU
// at boundaries: doing so would change batching and the stall being measured.
class WorldObjectProfiler {
public:
    WorldObjectProfiler(WorldObjectTimings& timings, bool enabled)
        : timings_(timings), enabled_(enabled) {
        if (enabled_) start_ = PerformanceClock::now();
    }
    ~WorldObjectProfiler() { finish(); }
    WorldObjectProfiler(const WorldObjectProfiler&) = delete;
    WorldObjectProfiler& operator=(const WorldObjectProfiler&) = delete;

    void next(WorldObjectStage stage) {
        finish();
        stage_ = stage;
    }

private:
    void finish() {
        if (!enabled_) return;
        const auto now = PerformanceClock::now();
        timings_[static_cast<std::size_t>(stage_)] +=
            performanceMilliseconds(start_, now);
        start_ = now;
    }
    WorldObjectTimings& timings_;
    bool enabled_{};
    WorldObjectStage stage_{WorldObjectStage::Decorations};
    PerformanceClock::time_point start_{};
};

} // namespace ian
