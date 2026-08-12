#include "TestHarness.hpp"
#include "core/FixedStep.hpp"

#include <cstddef>
#include <limits>

void runSimulationTests();
void runResourceSystemTests();
void runBuildingSystemTests();
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
void runSkillTreeTests();
void runSaturatingArithmeticTests();
void runGeometryTests();
void runResourceCostTests();
void runUserSettingsTests();
void runLootChestSystemTests();
void runInsightSystemTests();
void runObjectiveSystemTests();

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
    fixedStepProducesSixtyTicksPerSecond();
    fixedStepClampsLongFrames();
    fixedStepIgnoresNonFiniteFrames();
    runGeometryTests();
    runResourceCostTests();
    runSaturatingArithmeticTests();
    runUserSettingsTests();
    runGameBalanceTests();
    runMapDefinitionTests();
    runTerrainHeightfieldTests();
    runBuildGridTests();
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
    runSkillTreeTests();
    runInsightSystemTests();
    runObjectiveSystemTests();
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
    runResourceSystemTests();
    runSimulationTests();
    return 0;
}
