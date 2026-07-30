#include "TestHarness.hpp"
#include "game/ModularCombat.hpp"

#include <algorithm>
#include <cmath>

void runModularCombatTests() {
    ian::WorldConfig config =
        ian::WorldConfig::defaults();
    config.terrainResolution = 33;
    config.terrainWorldSize = 32.0;
    config.coreFlatRadius = 12.0;
    config.buildPreviewDistance = 20.0;
    ian::TerrainHeightfield terrain{config};
    ian::FoundationSystem foundations{terrain, config};
    const ian::Vec3 player{-4.0, 1.7, 0.0};

    const auto frame = foundations.placePlatformFrame(
        foundations.previewFoundation(
            {0.2, 0.0, 0.2}, player));
    require(frame.has_value(),
            "modular combat fixture places frame");
    const auto wall = foundations.placeWall(
        foundations.previewWall(
            {0.2, 0.0, 1.2}, player,
            ian::Rotation::Deg90));
    require(wall.has_value(),
            "modular combat fixture places wall");

    const auto targets =
        ian::buildModularEnemyTargets(
            foundations, config);
    require(
        targets.size() == 2U,
        "supported modular pieces become enemy targets");
    const auto frameTarget = std::find_if(
        targets.begin(), targets.end(),
        [frame](const ian::EnemyStructureTarget& target) {
            return target.id == frame->id;
        });
    require(
        frameTarget != targets.end() &&
            frameTarget->modular &&
            frameTarget->structuralImpact >= 2U,
        "frame target carries recursive structural impact");

    const ian::Vec3 frameCenter =
        ian::modularBaseCenter(*frame, config);
    require(
        std::abs(
            frameCenter.x -
            (frame->anchor.x + 1.0) *
                config.cellSize) < 1e-9 &&
            std::abs(
                frameCenter.z -
                (frame->anchor.z + 1.0) *
                    config.cellSize) < 1e-9,
        "modular base center uses shared grid geometry");

    static_cast<void>(foundations.remove(frame->id));
    const auto unsupportedTargets =
        ian::buildModularEnemyTargets(
            foundations, config);
    require(
        unsupportedTargets.empty(),
        "unsupported modular pieces are not combat targets");
}
