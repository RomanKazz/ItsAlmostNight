#include "game/ModularCombat.hpp"

#include <algorithm>

namespace ian {

Vec3 modularBaseCenter(
    const PlatformFrameInstance& frame,
    const WorldConfig& config) {
    return {
        (frame.anchor.x +
         PlatformFrameWidthCells * 0.5) *
            config.cellSize,
        frame.floorHeight,
        (frame.anchor.z +
         PlatformFrameWidthCells * 0.5) *
            config.cellSize,
    };
}

Vec3 modularBaseCenter(
    const WallInstance& wall,
    const WorldConfig& config) {
    return {
        (wall.anchor.x + 0.5) * config.cellSize,
        wall.bottomHeight,
        (wall.anchor.z + 0.5) * config.cellSize,
    };
}

Vec3 modularBaseCenter(
    const RampInstance& ramp,
    const WorldConfig& config) {
    const bool alongZ =
        ramp.rotation == Rotation::Deg0 ||
        ramp.rotation == Rotation::Deg180;
    const int widthCells =
        alongZ ? ModularRampWidthCells
               : ModularRampRunCells;
    const int depthCells =
        alongZ ? ModularRampRunCells
               : ModularRampWidthCells;
    return {
        (ramp.anchor.x + widthCells * 0.5) *
            config.cellSize,
        ramp.bottomHeight,
        (ramp.anchor.z + depthCells * 0.5) *
            config.cellSize,
    };
}

Vec3 modularBaseCenter(
    const ModularBuildingDamageResult& result,
    const WorldConfig& config) {
    if (result.platformFrame) {
        return modularBaseCenter(
            *result.platformFrame, config);
    }
    if (result.wall) {
        return modularBaseCenter(*result.wall, config);
    }
    if (result.ramp) {
        return modularBaseCenter(*result.ramp, config);
    }
    return {};
}

Vec3 modularBaseCenter(
    const ModularBuildingRepairResult& result,
    const WorldConfig& config) {
    if (result.platformFrame) {
        return modularBaseCenter(
            *result.platformFrame, config);
    }
    if (result.wall) {
        return modularBaseCenter(*result.wall, config);
    }
    if (result.ramp) {
        return modularBaseCenter(*result.ramp, config);
    }
    return {};
}

std::vector<EnemyStructureTarget>
buildModularEnemyTargets(
    const FoundationSystem& foundations,
    const WorldConfig& config) {
    std::vector<EnemyStructureTarget> targets;
    buildModularEnemyTargets(
        foundations, config, targets);
    return targets;
}

void buildModularEnemyTargets(
    const FoundationSystem& foundations,
    const WorldConfig& config,
    std::vector<EnemyStructureTarget>& targets) {
    targets.clear();
    targets.reserve(
        foundations.platformFrames().size() * 5U +
        foundations.walls().size() +
        foundations.ramps().size());

    for (const PlatformFrameInstance& frame :
         foundations.platformFrames()) {
        if (frame.supportState !=
            StructuralSupportState::Supported) {
            continue;
        }
        Vec3 position = modularBaseCenter(frame, config);
        if (frame.storey == 0) {
            for (const FoundationSupport& support :
                 frame.supports) {
                position.y = std::min(
                    position.y, support.bottom.y);
            }
        }
        targets.push_back({
            .id = frame.id,
            .position = position,
            .radius =
                PlatformFrameWidthCells *
                config.cellSize * 0.55,
            .buildingType = std::nullopt,
            .modular = true,
            .structuralImpact =
                1U +
                foundations.structuralGraph()
                    .dependentCount(frame.id),
            .traversable = true,
            .attackable = false,
        });
        if (frame.storey == 0) {
            for (const FoundationSupport& support : frame.supports) {
                if (support.length <= 1e-4) {
                    continue;
                }
                targets.push_back({
                    .id = frame.id,
                    .position = {
                        (support.top.x + support.bottom.x) * 0.5,
                        support.bottom.y,
                        (support.top.z + support.bottom.z) * 0.5,
                    },
                    .radius = config.cellSize * 0.14,
                    .buildingType = std::nullopt,
                    .modular = true,
                    .structuralImpact = 0U,
                    .traversable = false,
                    .minimumEnemySurfaceHeight = support.bottom.y,
                    .maximumEnemySurfaceHeight =
                        frame.floorHeight - 0.10,
                    .attackable = false,
                });
            }
        }
    }

    for (const WallInstance& wall :
         foundations.walls()) {
        if (wall.supportState !=
            StructuralSupportState::Supported) {
            continue;
        }
        Vec3 position = modularBaseCenter(wall, config);
        position.y = wall.bottomHeight;
        targets.push_back({
            .id = wall.id,
            .position = position,
            .radius = config.cellSize * 0.65,
            .buildingType = std::nullopt,
            .modular = true,
            .structuralImpact = 0U,
            .traversable = false,
        });
    }

    for (const RampInstance& ramp :
         foundations.ramps()) {
        if (ramp.supportState !=
            StructuralSupportState::Supported) {
            continue;
        }
        Vec3 position = modularBaseCenter(ramp, config);
        position.y = ramp.bottomHeight;
        targets.push_back({
            .id = ramp.id,
            .position = position,
            .radius = config.cellSize * 2.1,
            .buildingType = std::nullopt,
            .modular = true,
            .structuralImpact = 0U,
            .traversable = true,
        });
    }
}

} // namespace ian
