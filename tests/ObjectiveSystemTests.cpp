#include "TestHarness.hpp"
#include "progression/ObjectiveSystem.hpp"

#include <algorithm>

namespace {

const ian::ObjectiveStatus& status(
    const ian::ObjectiveSystem& objectives, const char* id) {
    const auto found = std::ranges::find_if(
        objectives.statuses(), [id](const ian::ObjectiveStatus& value) {
            return value.definition.id == id;
        });
    require(found != objectives.statuses().end(), "objective ID must exist");
    return *found;
}

void milestoneChainsTrackConfirmedDepletions() {
    ian::ObjectiveSystem objectives;
    auto completed = objectives.onResourceEvent({
        .wood = true, .amount = 15, .depleted = true,
        .elapsedSeconds = 1.0});
    require(status(objectives, "trees_1").completed,
            "first tree milestone completes on depletion");
    require(!status(objectives, "trees_10").completed,
            "later tree milestone remains incomplete");
    require(completed.size() == 1 && completed.front().id == "trees_1",
            "completion result contains only crossed milestone");
    for (int index = 1; index < 10; ++index)
        static_cast<void>(objectives.onResourceEvent({
            .wood = true, .depleted = true,
            .elapsedSeconds = static_cast<double>(index + 1)}));
    require(status(objectives, "trees_10").completed,
            "tree chain advances at ten confirmed nodes");
}

void gatheringChallengesUseTimeAndMisses() {
    ian::ObjectiveSystem objectives;
    static_cast<void>(objectives.onResourceEvent({
        .wood = true, .amount = 25, .depleted = true,
        .bareHands = true, .elapsedSeconds = 10.0}));
    static_cast<void>(objectives.onResourceEvent({
        .wood = false, .amount = 25, .depleted = true,
        .elapsedSeconds = 50.0}));
    require(status(objectives, "bare_hands").completed,
            "bare-hands challenge sees the actual finishing tool");
    require(status(objectives, "workaholic").completed,
            "rolling sixty-second window sums actual yield");
    objectives.onGatheringMiss();
    require(status(objectives, "no_stopping").progress == 0.0,
            "gathering miss resets the depletion streak");
}

void dayConditionsAndChallengeRotationWork() {
    ian::ObjectiveSystem objectives;
    static_cast<void>(objectives.onResourceEvent({
        .wood = true, .amount = 1, .night = true,
        .elapsedSeconds = 1.0}));
    static_cast<void>(objectives.onResourceEvent({
        .wood = false, .amount = 1, .night = true,
        .elapsedSeconds = 2.0}));
    static_cast<void>(objectives.onCrystalsGathered(1, 3.0, true));
    require(status(objectives, "all_resources_day").completed,
            "wood, stone and crystals complete same-day milestone");
    const bool nightInitiallyActive = status(objectives, "night_shift").active;
    static_cast<void>(objectives.beginNewDay());
    require(!nightInitiallyActive && status(objectives, "night_shift").active,
            "challenge selection rotates on a new day");
    require(status(objectives, "night_shift").progress == 0.0,
            "new day clears temporary challenge progress");
}

void worldEventAndSaveStateWork() {
    ian::ObjectiveSystem objectives;
    static_cast<void>(objectives.onResourceEvent({
        .wood = true, .amount = 2, .hasCore = true,
        .distanceFromCore = 181.0, .elapsedSeconds = 5.0}));
    require(status(objectives, "far_from_home").completed,
            "remote gathering completes world event");
    const auto saved = objectives.saveState();
    ian::ObjectiveSystem loaded;
    require(loaded.loadState(saved), "valid objective state loads");
    require(status(loaded, "far_from_home").completed &&
                status(loaded, "resources_100").progress ==
                    status(objectives, "resources_100").progress,
            "objective save preserves completion and aggregate progress");
    loaded.reset();
    require(!status(loaded, "far_from_home").completed,
            "new run clears objective state");
}

void gameplayEventsFeedSmallObjectiveChains() {
    ian::ObjectiveSystem objectives;
    const auto enemyCompletions = objectives.onGameplayEvent(
        ian::ObjectiveMetric::EnemiesKilled, 3, 5.0);
    require(
        status(objectives, "enemies_3").completed &&
            !status(objectives, "enemies_10").completed &&
            enemyCompletions.size() == 1 &&
            enemyCompletions.front().insightReward == 4.0,
        "small combat objective completes from event counters");
    static_cast<void>(objectives.onGameplayEvent(
        ian::ObjectiveMetric::BuildingsPlaced, 1, 6.0));
    static_cast<void>(objectives.onGameplayEvent(
        ian::ObjectiveMetric::CoinsCollected, 12, 7.0));
    require(
        status(objectives, "buildings_1").completed &&
            status(objectives, "coins_10").completed,
        "construction and economy events use the same objective path");

    const auto saved = objectives.saveState();
    ian::ObjectiveSystem loaded;
    require(
        loaded.loadState(saved) &&
            status(loaded, "enemies_10").progress == 3.0 &&
            status(loaded, "coins_50").progress == 12.0,
        "generic gameplay objective counters survive save and load");
}

void dataDrivenDefinitionsLoad() {
    require(
        ian::ObjectiveSystem::defaultDefinitions().size() == 85,
        "fallback objective pool matches data-driven content");
    const auto definitions = ian::loadObjectiveDefinitions(
        IAN_SOURCE_DIR "/assets/data/objectives.json");
    require(definitions.size() == 85,
            "objective data file exposes the expanded objective pool");
    require(std::ranges::any_of(definitions, [](const ian::ObjectiveDefinition& value) {
                return value.id == "far_from_home" &&
                       value.kind == ian::ObjectiveKind::WorldEvent &&
                       value.insightReward == 30.0;
            }),
            "world-event definition and reward load from data");
    require(std::ranges::any_of(definitions, [](const ian::ObjectiveDefinition& value) {
                return value.id == "elemental_hits_10" &&
                       value.metric == ian::ObjectiveMetric::ElementalHits &&
                       value.insightReward == 4.0;
            }),
            "new gameplay objective metrics load from data");
}

} // namespace

void runObjectiveSystemTests() {
    milestoneChainsTrackConfirmedDepletions();
    gatheringChallengesUseTimeAndMisses();
    dayConditionsAndChallengeRotationWork();
    worldEventAndSaveStateWork();
    gameplayEventsFeedSmallObjectiveChains();
    dataDrivenDefinitionsLoad();
}
