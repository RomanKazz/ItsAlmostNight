#include "TestHarness.hpp"
#include "progression/InsightSystem.hpp"
#include "game/Simulation.hpp"

#include <cmath>
#include <limits>

namespace {

using ian::InsightCategory;
using ian::InsightConfig;
using ian::InsightSource;
using ian::InsightSystem;

void lessThanThreshold() {
    InsightSystem insight;
    const auto result = insight.grantInsight(
        25.0, InsightSource::Objective, InsightCategory::Exploration,
        {.bypassDiminishing = true});
    require(result.accepted && result.treePointsGranted == 0,
            "sub-threshold Insight must not grant a point");
    requireNear(insight.progress().currentInsight, 25.0, 1e-9,
                "sub-threshold Insight remains in the bar");
}

void exactThreshold() {
    InsightSystem insight;
    const auto result = insight.grantInsight(
        100.0, InsightSource::Objective, InsightCategory::Exploration,
        {.bypassDiminishing = true});
    require(result.treePointsGranted == 1, "exact threshold grants one point");
    requireNear(insight.progress().currentInsight, 0.0, 1e-9,
                "exact threshold resets the bar");
}

void overflowAndMultiplePoints() {
    InsightSystem insight;
    const auto result = insight.grantInsight(
        250.0, InsightSource::ChallengeCompleted, InsightCategory::Combat,
        {.bypassDiminishing = true});
    require(result.treePointsGranted == 2, "250 Insight grants two points");
    requireNear(insight.progress().currentInsight, 50.0, 1e-9,
                "overflow remainder must be preserved");
    require(insight.progress().totalTreePointsEarned == 2,
            "total earned point telemetry follows overflow");
}

void duplicateEventsAreBlocked() {
    InsightSystem insight;
    const ian::InsightGrantContext context{
        .eventId = 42, .oneTime = true, .bypassDiminishing = true};
    require(insight.grantInsight(20.0, InsightSource::Objective,
            InsightCategory::Exploration, context).accepted,
            "first unique event is accepted");
    const auto duplicate = insight.grantInsight(20.0, InsightSource::Objective,
        InsightCategory::Exploration, context);
    require(duplicate.duplicate && !duplicate.accepted,
            "same event ID is rejected");
    require(insight.blockedDuplicateEvents() == 1,
            "duplicate telemetry increments");
}

void diminishingReturnsArePiecewise() {
    InsightSystem insight;
    const auto full = insight.grantInsight(
        20.0, InsightSource::EnemyKilled, InsightCategory::Combat);
    const auto half = insight.grantInsight(
        20.0, InsightSource::EnemyKilled, InsightCategory::Combat);
    const auto tail = insight.grantInsight(
        10.0, InsightSource::EnemyKilled, InsightCategory::Combat);
    requireNear(full.finalAmount, 20.0, 1e-9, "first category budget is full rate");
    requireNear(half.finalAmount, 10.0, 1e-9, "second category budget is half rate");
    requireNear(tail.finalAmount, 2.0, 1e-9, "exhausted category retains soft progress");
    insight.beginNewDiminishingCycle();
    requireNear(insight.grantInsight(5.0, InsightSource::EnemyKilled,
                    InsightCategory::Combat).finalAmount,
                5.0, 1e-9, "new cycle restores the category budget");
}

void uniqueRewardsBypassDiminishing() {
    InsightSystem insight;
    static_cast<void>(insight.grantInsight(
        100.0, InsightSource::EnemyKilled, InsightCategory::Combat));
    const auto objective = insight.grantInsight(
        40.0, InsightSource::Objective, InsightCategory::Combat,
        {.eventId = 77, .oneTime = true, .bypassDiminishing = true});
    requireNear(objective.finalAmount, 40.0, 1e-9,
                "unique objective ignores category exhaustion");
}

void constructionAndDeathsCannotRepeat() {
    InsightSystem insight;
    const auto construction = ian::InsightGrantContext{.eventId = 501, .oneTime = true};
    static_cast<void>(insight.grantInsight(2.0, InsightSource::StructureBuilt,
        InsightCategory::Building, construction));
    require(insight.grantInsight(2.0, InsightSource::StructureBuilt,
                InsightCategory::Building, construction).duplicate,
            "construction transaction cannot reward after refund/replay");
    const auto death = ian::InsightGrantContext{.eventId = 901, .oneTime = true};
    static_cast<void>(insight.grantInsight(1.0, InsightSource::EnemyKilled,
        InsightCategory::Combat, death));
    require(insight.grantInsight(1.0, InsightSource::EnemyKilled,
                InsightCategory::Combat, death).duplicate,
            "one enemy death cannot reward twice");
}

void saveLoadAndReset() {
    InsightSystem original;
    static_cast<void>(original.grantInsight(35.0, InsightSource::Objective,
        InsightCategory::Exploration,
        {.eventId = 123, .oneTime = true, .bypassDiminishing = true}));
    static_cast<void>(original.grantInsight(10.0, InsightSource::ResourceGathered,
        InsightCategory::Gathering));
    const auto saved = original.saveState();
    InsightSystem loaded;
    require(loaded.loadState(saved), "valid Insight state loads");
    requireNear(loaded.progress().currentInsight, 45.0, 1e-9,
                "load restores exact bar remainder");
    require(loaded.grantInsight(35.0, InsightSource::Objective,
                InsightCategory::Exploration,
                {.eventId = 123, .oneTime = true, .bypassDiminishing = true}).duplicate,
            "load restores consumed one-time IDs");
    requireNear(loaded.grantInsight(15.0, InsightSource::ResourceGathered,
                    InsightCategory::Gathering).finalAmount,
                12.5, 1e-9, "load restores current diminishing budget");
    loaded.reset();
    requireNear(loaded.progress().currentInsight, 0.0, 1e-9,
                "new run resets Insight");
    require(loaded.progress().totalTreePointsEarned == 0 &&
                loaded.blockedDuplicateEvents() == 0,
            "new run resets progression telemetry and dedupe");
}

void invalidAmountsAreIgnored() {
    InsightSystem insight;
    for (double value : {0.0, -1.0,
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity()}) {
        require(!insight.grantInsight(value, InsightSource::Other,
                    InsightCategory::Exploration).accepted,
                "invalid Insight amount is ignored");
    }
    requireNear(insight.progress().totalInsightEarned, 0.0, 1e-9,
                "invalid values cannot poison totals");
}

void growingRequirementIsSupportedButDisabledByDefault() {
    InsightConfig config;
    config.requirementGrowth = 10.0;
    InsightSystem insight{config};
    const auto result = insight.grantInsight(215.0, InsightSource::Objective,
        InsightCategory::Exploration, {.bypassDiminishing = true});
    require(result.treePointsGranted == 2, "growing requirement uses each new threshold");
    requireNear(insight.progress().currentInsight, 5.0, 1e-9,
                "growing thresholds preserve the final remainder");
    requireNear(insight.progress().requiredInsight, 120.0, 1e-9,
                "next growing threshold is derived from total earned points");
}

void simulationProgressionStateRoundTripsAndResets() {
    ian::Simulation simulation;
    ian::ProgressionRunState state;
    state.skillTree.points = 2;
    state.insight.progress.currentInsight = 47.5;
    state.insight.progress.requiredInsight = 100.0;
    state.insight.progress.totalTreePointsEarned = 2;
    state.insight.progress.totalInsightEarned = 247.5;
    state.insight.consumedEventIds = {45, 90};
    require(simulation.loadProgressionState(state),
            "combined run progression state loads atomically");
    const auto saved = simulation.saveProgressionState();
    require(saved.skillTree.points == 2 &&
                saved.insight.progress.currentInsight == 47.5 &&
                saved.insight.consumedEventIds.size() == 2,
            "combined save preserves SkillTree and Insight state");
    simulation.startRun();
    require(simulation.skillTree().points() == 0 &&
                simulation.insightSystem().progress().currentInsight == 0.0 &&
                simulation.insightSystem().saveState().consumedEventIds.empty(),
            "starting a new run resets both progression halves");
}

} // namespace

void runInsightSystemTests() {
    lessThanThreshold();
    exactThreshold();
    overflowAndMultiplePoints();
    duplicateEventsAreBlocked();
    diminishingReturnsArePiecewise();
    uniqueRewardsBypassDiminishing();
    constructionAndDeathsCannotRepeat();
    saveLoadAndReset();
    invalidAmountsAreIgnored();
    growingRequirementIsSupportedButDisabledByDefault();
    simulationProgressionStateRoundTripsAndResets();
}
