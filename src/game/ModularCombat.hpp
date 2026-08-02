#pragma once

#include "buildings/FoundationSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "world/WorldConfig.hpp"

#include <vector>

namespace ian {

[[nodiscard]] Vec3 modularBaseCenter(
    const PlatformFrameInstance& frame,
    const WorldConfig& config);
[[nodiscard]] Vec3 modularBaseCenter(
    const WallInstance& wall,
    const WorldConfig& config);
[[nodiscard]] Vec3 modularBaseCenter(
    const RampInstance& ramp,
    const WorldConfig& config);
[[nodiscard]] Vec3 modularBaseCenter(
    const ModularBuildingDamageResult& result,
    const WorldConfig& config);
[[nodiscard]] Vec3 modularBaseCenter(
    const ModularBuildingRepairResult& result,
    const WorldConfig& config);

[[nodiscard]] std::vector<EnemyStructureTarget>
buildModularEnemyTargets(
    const FoundationSystem& foundations,
    const WorldConfig& config);
void buildModularEnemyTargets(
    const FoundationSystem& foundations,
    const WorldConfig& config,
    std::vector<EnemyStructureTarget>& targets);

} // namespace ian
