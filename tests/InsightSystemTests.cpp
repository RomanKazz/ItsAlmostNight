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
    require(result.accepted && result.levelsGranted == 0,
            "sub-threshold XP must not grant a level");
    requireNear(insight.progress().currentInsight, 25.0, 1e-9,
                "sub-threshold Insight remains in the bar");
}

void exactThreshold() {
    InsightSystem insight;
    const auto result = insight.grantInsight(
        30.0, InsightSource::Objective, InsightCategory::Exploration,
        {.bypassDiminishing = true});
    require(result.levelsGranted == 1, "exact threshold grants one level");
    requireNear(insight.progress().currentInsight, 0.0, 1e-9,
                "exact threshold resets the bar");
    requireNear(insight.progress().requiredInsight, 65.0, 1e-9,
                "next level requires more XP");
}

void overflowAndMultiplePoints() {
    InsightSystem insight;
    const auto result = insight.grantInsight(
        250.0, InsightSource::ChallengeCompleted, InsightCategory::Combat,
        {.bypassDiminishing = true});
    require(result.levelsGranted == 3, "250 XP grants three early levels");
    requireNear(insight.progress().currentInsight, 60.0, 1e-9,
                "overflow remainder must be preserved");
    require(insight.progress().totalLevelsEarned == 3,
            "total earned level telemetry follows overflow");
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
    InsightConfig config = InsightConfig::defaults();
    config.fullRateBudget = 20.0;
    config.reducedRateBudget = 20.0;
    InsightSystem insight{config};
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
    InsightConfig config = InsightConfig::defaults();
    config.fullRateBudget = 20.0;
    config.reducedRateBudget = 20.0;
    InsightSystem original{config};
    static_cast<void>(original.grantInsight(20.0, InsightSource::Objective,
        InsightCategory::Exploration,
        {.eventId = 123, .oneTime = true, .bypassDiminishing = true}));
    static_cast<void>(original.grantInsight(10.0, InsightSource::ResourceGathered,
        InsightCategory::Gathering));
    const auto saved = original.saveState();
    InsightSystem loaded{config};
    require(loaded.loadState(saved), "valid Insight state loads");
    requireNear(loaded.progress().currentInsight, 0.0, 1e-9,
                "load restores exact bar remainder");
    require(loaded.grantInsight(35.0, InsightSource::Objective,
                InsightCategory::Exploration,
                {.eventId = 123, .oneTime = true, .bypassDiminishing = true}).duplicate,
            "load restores consumed one-time IDs");
    requireNear(loaded.grantInsight(15.0, InsightSource::ResourceGathered,
                    InsightCategory::Gathering).finalAmount,
                12.5, 1e-9, "load restores current diminishing budget");

    auto invalid = saved;
    invalid.consumedEventIds.push_back(123);
    require(!loaded.loadState(invalid),
            "load rejects duplicate consumed event IDs");
    invalid = saved;
    invalid.progress.requiredInsight += 1.0;
    require(!loaded.loadState(invalid),
            "load rejects a requirement inconsistent with earned points");
    invalid = saved;
    invalid.consumedEventIds.assign(100'001U, 1U);
    require(!loaded.loadState(invalid),
            "load bounds consumed event history");
    loaded.reset();
    requireNear(loaded.progress().currentInsight, 0.0, 1e-9,
                "new run resets Insight");
    require(loaded.progress().totalLevelsEarned == 0 &&
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

void earlyRewardsJoinTheNormalCurve() {
    InsightSystem insight;
    for (double threshold : {30.0, 65.0, 95.0, 125.0, 160.0, 175.0}) {
        requireNear(insight.progress().requiredInsight, threshold, 1e-9,
                    "early discounts end at the original fifth threshold");
        const auto result = insight.grantInsight(threshold, InsightSource::Objective,
            InsightCategory::Exploration, {.bypassDiminishing = true});
        require(result.levelsGranted == 1,
                "each exact threshold grants one reward");
        requireNear(insight.progress().currentInsight, 0.0, 1e-9,
                    "each exact threshold clears the bar");
    }
    insight.reset();
    requireNear(insight.progress().requiredInsight, 30.0, 1e-9,
                "new runs restore the quick first reward");
}

void previousEarlySavesPreserveProgress() {
    InsightConfig previousConfig = InsightConfig::defaults();
    previousConfig.earlyRequirementDiscounts.fill(0.0);
    for (int level = 0; level < 6; ++level) {
        InsightSystem previous{previousConfig};
        for (int earned = 0; earned < level; ++earned) {
            static_cast<void>(previous.grantInsight(previous.progress().requiredInsight,
                InsightSource::Objective, InsightCategory::Exploration,
                {.bypassDiminishing = true}));
        }
        static_cast<void>(previous.grantInsight(previous.progress().requiredInsight*0.9,
            InsightSource::Objective, InsightCategory::Exploration,
            {.eventId = 777, .oneTime = true, .bypassDiminishing = true}));
        const auto saved = previous.saveState();
        InsightSystem loaded;
        require(loaded.loadState(saved), "previous early and late saves must load");
        require(loaded.progress().totalLevelsEarned == level,
                "migration must preserve earned rewards");
        requireNear(loaded.progress().currentInsight/loaded.progress().requiredInsight,
                    0.9, 1e-9, "migration preserves the filled bar fraction");
        requireNear(loaded.progress().totalInsightEarned, saved.progress.totalInsightEarned,
                    1e-9, "migration preserves lifetime XP telemetry");
        InsightSystem reloaded;
        require(reloaded.loadState(loaded.saveState()), "migrated saves round trip");
        requireNear(reloaded.progress().currentInsight, loaded.progress().currentInsight,
                    1e-9, "migration must not scale progress twice");
        require(loaded.grantInsight(1.0, InsightSource::Objective,
                    InsightCategory::Exploration,
                    {.eventId = 777, .oneTime = true, .bypassDiminishing = true}).duplicate,
                "migration preserves consumed event IDs");
        const double remaining = loaded.progress().requiredInsight -
            loaded.progress().currentInsight;
        require(loaded.grantInsight(remaining, InsightSource::Objective,
                    InsightCategory::Exploration, {.bypassDiminishing = true}).levelsGranted == 1,
                "the next earned reward still works after migration");
    }
}

void previousFirstRewardPreservesFraction() {
    auto oldConfig = InsightConfig::defaults();
    oldConfig.earlyRequirementDiscounts[0] = 60.0;
    InsightSystem previous{oldConfig};
    static_cast<void>(previous.grantInsight(20.0, InsightSource::Objective,
        InsightCategory::Exploration,
        {.eventId = 1234, .oneTime = true, .bypassDiminishing = true}));
    InsightSystem loaded;
    require(loaded.loadState(previous.saveState()), "old forty-XP first level still loads");
    requireNear(loaded.progress().currentInsight, 15.0, 1e-9,
                "old first-level save preserves half-filled bar");
    require(loaded.progress().totalLevelsEarned == 0,
            "loading shorter first level does not silently grant a reward");
    InsightSystem reloaded;
    require(reloaded.loadState(loaded.saveState()), "migrated first-level save round trips");
    requireNear(reloaded.progress().currentInsight, 15.0, 1e-9,
                "first-level migration is applied only once");
    require(reloaded.grantInsight(1.0, InsightSource::Objective,
                InsightCategory::Exploration,
                {.eventId = 1234, .oneTime = true, .bypassDiminishing = true}).duplicate,
            "first-level migration retains consumed reward IDs");
    auto customConfig = InsightConfig::defaults();
    customConfig.baseRequirement = 30;
    customConfig.earlyRequirementDiscounts.fill(0);
    InsightSystem custom{customConfig};
    require(!custom.loadState(previous.saveState()),
            "forty-XP migration does not accept unrelated custom thresholds");
}

void simulationProgressionStateRoundTripsAndResets() {
    ian::Simulation simulation;
    ian::ProgressionRunState state;
    state.insight.progress.currentInsight = 47.5;
    state.insight.progress.requiredInsight = 95.0;
    state.insight.progress.totalLevelsEarned = 2;
    state.insight.progress.totalInsightEarned = 262.5;
    state.insight.consumedEventIds = {45, 90};
    require(simulation.loadProgressionState(state),
            "combined run progression state loads atomically");
    const auto saved = simulation.saveProgressionState();
    require(saved.insight.progress.currentInsight == 47.5 &&
                saved.insight.consumedEventIds.size() == 2,
            "run progression save preserves Insight state");
    state.insight.progress.requiredInsight = 100.0;
    require(simulation.loadProgressionState(state) &&
                simulation.snapshot().requiredInsight == 95.0,
            "flat-threshold saves migrate to the early requirement");
    requireNear(simulation.insightSystem().progress().currentInsight, 45.125, 1e-9,
                "flat-threshold early saves preserve their bar fraction");
    simulation.startRun();
    require(simulation.insightSystem().progress().currentInsight == 0.0 &&
                simulation.insightSystem().saveState().consumedEventIds.empty(),
            "starting a new run resets progression");
}

} // namespace

void runInsightSystemTests() {
    {
        InsightSystem base;
        InsightSystem boosted;
        for (int grant = 0; grant < 3; ++grant) {
            const auto ordinary = base.grantInsight(28.0,
                InsightSource::ResourceGathered, InsightCategory::Gathering);
            const auto bonus = boosted.grantInsight(28.0,
                InsightSource::ResourceGathered, InsightCategory::Gathering,
                {.rewardMultiplier = 1.1});
            requireNear(bonus.finalAmount, ordinary.finalAmount * 1.1, 1e-12,
                "experience bonus remains exact across diminishing-return budgets");
            requireNear(bonus.diminishingMultiplier, ordinary.diminishingMultiplier, 1e-12,
                "experience bonus never exhausts a category budget sooner");
        }
        for (const auto invalid : {0.0, -1.0, std::numeric_limits<double>::infinity()}) {
            const auto rejected = boosted.grantInsight(1.0,
                InsightSource::Other, InsightCategory::Exploration,
                {.rewardMultiplier = invalid});
            require(!rejected.accepted, "invalid reward multipliers never award experience");
        }
    }
    lessThanThreshold();
    exactThreshold();
    overflowAndMultiplePoints();
    duplicateEventsAreBlocked();
    diminishingReturnsArePiecewise();
    uniqueRewardsBypassDiminishing();
    constructionAndDeathsCannotRepeat();
    saveLoadAndReset();
    invalidAmountsAreIgnored();
    earlyRewardsJoinTheNormalCurve();
    previousEarlySavesPreserveProgress();
    previousFirstRewardPreservesFraction();
    simulationProgressionStateRoundTripsAndResets();
}
