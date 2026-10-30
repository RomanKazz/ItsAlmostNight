#include "TestHarness.hpp"
#include "core/FixedStep.hpp"

#include <cstddef>
#include <limits>

void runSimulationTests();
void runRunShopTests();
void runPlayerClassAbilityTests();
void runRunUpgradeTests();
void runPowerupTests();
void runResourceSystemTests();
void runBuildingSystemTests();
void runMenuBackgroundSceneTests();
void runEnemySystemTests();
void runEnvironmentSystemTests();
void runCollisionWorldTests();
void runFlowFieldTests();
void runSpatialHashTests();
void runTowerSystemTests();
void runCrystalMineSystemTests();
void runCoinPickupSystemTests();
void runWaveDirectorTests();
void runCannonSystemTests();
void runTrapSystemTests();
void runPlayerWeaponSystemTests();
void runBombSystemTests();
void runIceWandSystemTests();
void runGameBalanceTests();
void runMapDefinitionTests();
void runTerrainHeightfieldTests();
void runWorldSceneryTests();
void runBuildGridTests();
void runPlacementValidatorTests();
void runFoundationSystemTests();
void runModularCombatTests();
void runPresentationEffectQueryTests();
void runResourceWorldTests();
void runBuildingStatsTests();
void runPresentationTimelineTests();
void runUiLabelsTests();
void runDeterministicRandomTests();
void runDecorationExclusionMapTests();
void runPlacementLineTests();
void runGlbCollisionTests();
void runGlbNodeAnimationTests();
void runSaturatingArithmeticTests();
void runGeometryTests();
void runRendererMathTests();
void runResourceCostTests();
void runUserSettingsTests();
void runLootChestSystemTests();
void runChestItemCombatTests();
void runRelicTests();
void runInsightSystemTests();
void runObjectiveSystemTests();
void runQuestSystemTests();

namespace {

void fixedStepProducesSixtyTicksPerSecond() {
    ian::FixedStep fixedStep;
    std::size_t ticks = 0;

    for (int frame = 0; frame < 120; ++frame) {
        fixedStep.advance(1.0 / 120.0, [&ticks](double deltaSeconds) {
            requireNear(deltaSeconds, 1.0 / 60.0, 1e-12, "tick duration must stay fixed");
            ++ticks;
        });
    }

    require(ticks == 60, "120 frames at 120 FPS must produce 60 ticks");
}

void fixedStepClampsLongFrames() {
    ian::FixedStep fixedStep;
    std::size_t ticks = 0;
    fixedStep.advance(10.0, [&ticks](double) { ++ticks; });
    require(ticks == 15, "long frame must be clamped to 250 ms");
}

void fixedStepIgnoresNonFiniteFrames() {
    ian::FixedStep fixedStep;
    std::size_t ticks = 0;
    fixedStep.advance(
        std::numeric_limits<double>::quiet_NaN(),
        [&ticks](double) { ++ticks; });
    fixedStep.advance(
        std::numeric_limits<double>::infinity(),
        [&ticks](double) { ++ticks; });
    fixedStep.advance(
        ian::FixedStep::TickSeconds,
        [&ticks](double) { ++ticks; });
    require(ticks == 1,
            "non-finite frame times do not poison the accumulator");
}

} // namespace

int main() {
    runWorldSceneryTests();
    fixedStepProducesSixtyTicksPerSecond();
    fixedStepClampsLongFrames();
    fixedStepIgnoresNonFiniteFrames();
    runGeometryTests();
    runRendererMathTests();
    runResourceCostTests();
    runSaturatingArithmeticTests();
    runUserSettingsTests();
    runGameBalanceTests();
    runMapDefinitionTests();
    runTerrainHeightfieldTests();
    runBuildGridTests();
    runMenuBackgroundSceneTests();
    runPlacementValidatorTests();
    runFoundationSystemTests();
    runModularCombatTests();
    runPresentationEffectQueryTests();
    runResourceWorldTests();
    runBuildingStatsTests();
    runPresentationTimelineTests();
    runUiLabelsTests();
    runDeterministicRandomTests();
    runDecorationExclusionMapTests();
    runPlacementLineTests();
    runGlbCollisionTests();
    runGlbNodeAnimationTests();
    runInsightSystemTests();
    runObjectiveSystemTests();
    runQuestSystemTests();
    runBuildingSystemTests();
    runCollisionWorldTests();
    runEnemySystemTests();
    runEnvironmentSystemTests();
    runFlowFieldTests();
    runSpatialHashTests();
    runTowerSystemTests();
    runCrystalMineSystemTests();
    runCoinPickupSystemTests();
    runWaveDirectorTests();
    runCannonSystemTests();
    runTrapSystemTests();
    runPlayerWeaponSystemTests();
    runBombSystemTests();
    runIceWandSystemTests();
    runLootChestSystemTests();
    runChestItemCombatTests();
    runRelicTests();
    runResourceSystemTests();
    runRunShopTests();
    runSimulationTests();
    runPlayerClassAbilityTests();
    runRunUpgradeTests();
    runPowerupTests();
    return 0;
}
