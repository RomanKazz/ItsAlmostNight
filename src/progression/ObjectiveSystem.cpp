#include "progression/ObjectiveSystem.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace ian {
namespace {

ObjectiveDefinition milestone(
    std::string id, std::string title, std::string description,
    ObjectiveMetric metric, double target, double reward,
    std::string chain, int order) {
    return {std::move(id), std::move(title), std::move(description),
            ObjectiveKind::Milestone, metric, target, reward,
            std::move(chain), order};
}

ObjectiveDefinition challenge(
    std::string id, std::string title, std::string description,
    ObjectiveMetric metric, double target, double reward) {
    return {std::move(id), std::move(title), std::move(description),
            ObjectiveKind::Challenge, metric, target, reward, {}, 0};
}

} // namespace

std::vector<ObjectiveDefinition> ObjectiveSystem::defaultDefinitions() {
    std::vector<ObjectiveDefinition> result;
    const auto addChain = [&result](const char* prefix, const char* noun,
                                   ObjectiveMetric metric,
                                   std::initializer_list<int> targets,
                                   std::initializer_list<double> rewards) {
        auto reward = rewards.begin();
        int order = 0;
        for (int target : targets) {
            result.push_back(milestone(
                std::string(prefix) + "_" + std::to_string(target),
                std::string(noun) + " " + std::to_string(target),
                std::string("Complete: ") + noun + " " + std::to_string(target),
                metric, target, *reward++, prefix, order++));
        }
    };
    addChain("trees", "Fell trees", ObjectiveMetric::TreesDestroyed,
             {1, 10, 50, 100, 250}, {5, 10, 20, 35, 60});
    addChain("stones", "Break stones", ObjectiveMetric::StonesDestroyed,
             {1, 10, 50, 100, 250}, {5, 10, 20, 35, 60});
    addChain("crystals", "Gather crystals", ObjectiveMetric::CrystalsGathered,
             {10, 50, 200, 500}, {10, 20, 40, 75});
    addChain("resources", "Gather resources", ObjectiveMetric::TotalResourcesGathered,
             {100, 500, 1000}, {10, 30, 60});
    addChain("enemies", "Defeat enemies", ObjectiveMetric::EnemiesKilled,
             {3, 10, 25, 50, 100}, {4, 7, 12, 18, 28});
    addChain("buildings", "Place buildings", ObjectiveMetric::BuildingsPlaced,
             {1, 5, 12, 25}, {4, 8, 14, 22});
    addChain("modular", "Place floor pieces", ObjectiveMetric::ModularPiecesPlaced,
             {5, 15, 40, 100}, {4, 8, 15, 25});
    addChain("upgrades", "Upgrade buildings", ObjectiveMetric::BuildingsUpgraded,
             {1, 3, 8}, {5, 9, 16});
    addChain("repairs", "Repair structures", ObjectiveMetric::StructuresRepaired,
             {1, 5, 15}, {4, 8, 14});
    addChain("waves", "Survive waves", ObjectiveMetric::WavesCompleted,
             {1, 2, 4, 7}, {8, 12, 20, 32});
    addChain("coins", "Collect Coins", ObjectiveMetric::CoinsCollected,
             {10, 50, 150, 400}, {4, 8, 14, 22});
    addChain("chests", "Open chests", ObjectiveMetric::ChestsOpened,
             {1, 3, 8}, {5, 9, 16});
    addChain("loot", "Collect items", ObjectiveMetric::LootCollected,
             {1, 5, 12}, {5, 10, 18});
    addChain("dashes", "Use Dash", ObjectiveMetric::PlayerDashes,
             {5, 20, 50}, {3, 6, 10});
    addChain("rifle_shots", "Fire rifle shots", ObjectiveMetric::RifleShots,
             {10, 30, 75}, {4, 8, 13});
    addChain("elemental_hits", "Hit with wands", ObjectiveMetric::ElementalHits,
             {10, 30, 75}, {4, 8, 13});
    addChain("trap_hits", "Hit with traps", ObjectiveMetric::TrapHits,
             {5, 20, 50}, {4, 8, 14});
    addChain("cannon_shots", "Fire cannon shots", ObjectiveMetric::CannonShots,
             {5, 20, 50}, {4, 8, 14});
    addChain("bombs", "Throw bombs", ObjectiveMetric::BombsThrown,
             {1, 5, 15}, {4, 8, 14});
    addChain("early_waves", "Start waves early", ObjectiveMetric::EarlyWavesStarted,
             {1, 3, 6}, {6, 12, 20});
    addChain("fortifications", "Fortify structures", ObjectiveMetric::StructuresFortified,
             {1, 5}, {5, 10});
    addChain("gates", "Toggle gates", ObjectiveMetric::GatesToggled,
             {3, 10}, {3, 6});
    addChain("sales", "Sell buildings", ObjectiveMetric::BuildingsSold,
             {1, 5}, {3, 7});
    addChain("fall_saves", "Survive lethal falls", ObjectiveMetric::FallsSaved,
             {1}, {12});
    result.push_back(milestone(
        "large_deposit", "Deep excavation",
        "Fully deplete a large resource deposit",
        ObjectiveMetric::LargeDepositDepleted, 1, 25, "large_deposit", 0));
    result.push_back(milestone(
        "all_resources_day", "Varied harvest",
        "Gather wood, stone and crystals in one day",
        ObjectiveMetric::AllResourceTypesInDay, 3, 15, "all_resources_day", 0));

    result.push_back(challenge(
        "bare_hands", "Bare hands",
        "Destroy a tree or stone without a tool",
        ObjectiveMetric::BareHandsDepletion, 1, 20));
    result.push_back(challenge(
        "workaholic", "Workaholic",
        "Gather 50 resources within 60 seconds",
        ObjectiveMetric::ResourcesInSixtySeconds, 50, 30));
    result.push_back(challenge(
        "no_stopping", "No stopping",
        "Destroy 5 resource objects without missing",
        ObjectiveMetric::ConsecutiveDepletions, 5, 30));
    result.push_back(challenge(
        "night_shift", "Night shift",
        "Gather 30 resources during the night",
        ObjectiveMetric::NightResourcesGathered, 30, 25));
    result.push_back({
        "far_from_home", "Far from home",
        "Gather a resource more than 180 meters from the Core",
        ObjectiveKind::WorldEvent, ObjectiveMetric::FarResourceGathered,
        1, 30, {}, 0});
    return result;
}

ObjectiveSystem::ObjectiveSystem()
    : ObjectiveSystem(defaultDefinitions()) {}

ObjectiveSystem::ObjectiveSystem(std::vector<ObjectiveDefinition> definitions)
    : definitions_(std::move(definitions)) {
    if (definitions_.empty()) definitions_ = defaultDefinitions();
    reset();
}

void ObjectiveSystem::reset() {
    statuses_.clear();
    for (const auto& definition : definitions_)
        statuses_.push_back({.definition = definition});
    challengeCycle_ = 0;
    totalTreesDestroyed_ = 0;
    totalStonesDestroyed_ = 0;
    totalCrystalsGathered_ = 0;
    totalResourcesGathered_ = 0;
    dayWoodGathered_ = 0;
    dayStoneGathered_ = 0;
    dayCrystalsGathered_ = 0;
    largeDepositsDepleted_ = 0;
    bareHandsDepletions_ = 0;
    consecutiveDepletions_ = 0;
    nightResourcesGathered_ = 0;
    farResourcesGathered_ = 0;
    recentGathering_.clear();
    eventMetricProgress_.fill(0);
    activateChallenges();
    refreshProgress(0.0);
}

void ObjectiveSystem::activateChallenges() {
    std::vector<std::size_t> challenges;
    for (std::size_t index = 0; index < statuses_.size(); ++index) {
        auto& status = statuses_[index];
        if (status.definition.kind != ObjectiveKind::Challenge) continue;
        status.active = false;
        status.completed = false;
        status.progress = 0.0;
        status.cycle = challengeCycle_;
        challenges.push_back(index);
    }
    constexpr std::size_t ActiveChallenges = 3;
    for (std::size_t offset = 0;
         offset < std::min(ActiveChallenges, challenges.size()); ++offset) {
        const std::size_t selected =
            (static_cast<std::size_t>(challengeCycle_) + offset) % challenges.size();
        statuses_[challenges[selected]].active = true;
    }
}

void ObjectiveSystem::expireRecentGathering(double elapsedSeconds) {
    while (!recentGathering_.empty() &&
           elapsedSeconds - recentGathering_.front().first > 60.0)
        recentGathering_.pop_front();
}

void ObjectiveSystem::refreshProgress(double elapsedSeconds) {
    expireRecentGathering(elapsedSeconds);
    int recentAmount = 0;
    for (const auto& [time, amount] : recentGathering_) {
        static_cast<void>(time);
        recentAmount += amount;
    }
    const int resourceTypesToday =
        (dayWoodGathered_ > 0 ? 1 : 0) +
        (dayStoneGathered_ > 0 ? 1 : 0) +
        (dayCrystalsGathered_ > 0 ? 1 : 0);
    for (auto& status : statuses_) {
        switch (status.definition.metric) {
        case ObjectiveMetric::TreesDestroyed: status.progress = totalTreesDestroyed_; break;
        case ObjectiveMetric::StonesDestroyed: status.progress = totalStonesDestroyed_; break;
        case ObjectiveMetric::CrystalsGathered: status.progress = totalCrystalsGathered_; break;
        case ObjectiveMetric::TotalResourcesGathered: status.progress = totalResourcesGathered_; break;
        case ObjectiveMetric::LargeDepositDepleted: status.progress = largeDepositsDepleted_; break;
        case ObjectiveMetric::AllResourceTypesInDay: status.progress = resourceTypesToday; break;
        case ObjectiveMetric::BareHandsDepletion: status.progress = bareHandsDepletions_; break;
        case ObjectiveMetric::ResourcesInSixtySeconds: status.progress = recentAmount; break;
        case ObjectiveMetric::ConsecutiveDepletions: status.progress = consecutiveDepletions_; break;
        case ObjectiveMetric::NightResourcesGathered: status.progress = nightResourcesGathered_; break;
        case ObjectiveMetric::FarResourceGathered: status.progress = farResourcesGathered_; break;
        case ObjectiveMetric::EnemiesKilled:
        case ObjectiveMetric::BuildingsPlaced:
        case ObjectiveMetric::ModularPiecesPlaced:
        case ObjectiveMetric::BuildingsUpgraded:
        case ObjectiveMetric::StructuresRepaired:
        case ObjectiveMetric::WavesCompleted:
        case ObjectiveMetric::CoinsCollected:
        case ObjectiveMetric::ChestsOpened:
        case ObjectiveMetric::LootCollected:
        case ObjectiveMetric::PlayerDashes:
        case ObjectiveMetric::RifleShots:
        case ObjectiveMetric::ElementalHits:
        case ObjectiveMetric::TrapHits:
        case ObjectiveMetric::CannonShots:
        case ObjectiveMetric::BombsThrown:
        case ObjectiveMetric::EarlyWavesStarted:
        case ObjectiveMetric::StructuresFortified:
        case ObjectiveMetric::GatesToggled:
        case ObjectiveMetric::BuildingsSold:
        case ObjectiveMetric::FallsSaved:
            status.progress = eventMetricProgress_[
                static_cast<std::size_t>(status.definition.metric)];
            break;
        case ObjectiveMetric::Count: break;
        }
        status.progress = std::min(status.progress, status.definition.target);
    }
}

bool ObjectiveSystem::chainPrerequisiteCompleted(const ObjectiveStatus& status) const {
    if (status.definition.chain.empty() || status.definition.chainOrder <= 0) return true;
    return std::ranges::any_of(statuses_, [&status](const ObjectiveStatus& candidate) {
        return candidate.definition.chain == status.definition.chain &&
               candidate.definition.chainOrder == status.definition.chainOrder - 1 &&
               candidate.completed;
    });
}

std::vector<ObjectiveCompletion> ObjectiveSystem::collectCompletions() {
    std::vector<ObjectiveCompletion> result;
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& status : statuses_) {
            if (!status.active || status.completed ||
                status.progress + 1e-9 < status.definition.target ||
                !chainPrerequisiteCompleted(status)) continue;
            status.completed = true;
            changed = true;
            result.push_back({status.definition.id, status.definition.title,
                              status.definition.insightReward,
                              status.definition.kind, status.cycle});
        }
    }
    return result;
}

std::vector<ObjectiveCompletion> ObjectiveSystem::onResourceEvent(
    const ObjectiveResourceEvent& event) {
    if (event.amount > 0) {
        totalResourcesGathered_ += event.amount;
        recentGathering_.push_back({event.elapsedSeconds, event.amount});
        if (event.wood) dayWoodGathered_ += event.amount;
        else dayStoneGathered_ += event.amount;
        if (event.night) nightResourcesGathered_ += event.amount;
        if (event.hasCore && event.distanceFromCore > 180.0)
            ++farResourcesGathered_;
    }
    if (event.depleted) {
        if (event.wood) ++totalTreesDestroyed_;
        else ++totalStonesDestroyed_;
        ++consecutiveDepletions_;
        if (event.largeDeposit) ++largeDepositsDepleted_;
        if (event.bareHands) ++bareHandsDepletions_;
    }
    refreshProgress(event.elapsedSeconds);
    return collectCompletions();
}

std::vector<ObjectiveCompletion> ObjectiveSystem::onCrystalsGathered(
    int amount, double elapsedSeconds, bool night) {
    if (amount > 0) {
        totalCrystalsGathered_ += amount;
        totalResourcesGathered_ += amount;
        dayCrystalsGathered_ += amount;
        if (night) nightResourcesGathered_ += amount;
        recentGathering_.push_back({elapsedSeconds, amount});
    }
    refreshProgress(elapsedSeconds);
    return collectCompletions();
}

std::vector<ObjectiveCompletion> ObjectiveSystem::onGameplayEvent(
    ObjectiveMetric metric, int amount, double elapsedSeconds) {
    const auto index = static_cast<std::size_t>(metric);
    if (metric == ObjectiveMetric::Count ||
        index >= eventMetricProgress_.size() || amount <= 0) {
        return {};
    }
    const int room = std::numeric_limits<int>::max() -
        eventMetricProgress_[index];
    eventMetricProgress_[index] += std::min(room, amount);
    refreshProgress(elapsedSeconds);
    return collectCompletions();
}

void ObjectiveSystem::onGatheringMiss() {
    consecutiveDepletions_ = 0;
    refreshProgress(recentGathering_.empty() ? 0.0 : recentGathering_.back().first);
}

std::vector<ObjectiveCompletion> ObjectiveSystem::beginNewDay() {
    ++challengeCycle_;
    dayWoodGathered_ = 0;
    dayStoneGathered_ = 0;
    dayCrystalsGathered_ = 0;
    bareHandsDepletions_ = 0;
    consecutiveDepletions_ = 0;
    nightResourcesGathered_ = 0;
    recentGathering_.clear();
    activateChallenges();
    refreshProgress(0.0);
    return {};
}

std::span<const ObjectiveStatus> ObjectiveSystem::statuses() const { return statuses_; }

std::vector<std::size_t> ObjectiveSystem::recommended(std::size_t maximum) const {
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < statuses_.size(); ++index) {
        const auto& status = statuses_[index];
        if (!status.active || status.completed || !chainPrerequisiteCompleted(status)) continue;
        result.push_back(index);
    }
    std::ranges::sort(result, [this](std::size_t left, std::size_t right) {
        const auto score = [](const ObjectiveStatus& status) {
            const double fraction = status.definition.target > 0.0
                ? status.progress / status.definition.target : 0.0;
            const double kindBonus = status.definition.kind == ObjectiveKind::Challenge
                ? 2.0 : status.definition.kind == ObjectiveKind::WorldEvent ? 1.0 : 0.0;
            return kindBonus + fraction;
        };
        return score(statuses_[left]) > score(statuses_[right]);
    });
    if (result.size() > maximum) result.resize(maximum);
    return result;
}

ObjectiveRunState ObjectiveSystem::saveState() const {
    ObjectiveRunState state{
        .challengeCycle = challengeCycle_,
        .totalTreesDestroyed = totalTreesDestroyed_,
        .totalStonesDestroyed = totalStonesDestroyed_,
        .totalCrystalsGathered = totalCrystalsGathered_,
        .totalResourcesGathered = totalResourcesGathered_,
        .dayWoodGathered = dayWoodGathered_,
        .dayStoneGathered = dayStoneGathered_,
        .dayCrystalsGathered = dayCrystalsGathered_,
        .consecutiveDepletions = consecutiveDepletions_,
        .largeDepositsDepleted = largeDepositsDepleted_,
        .bareHandsDepletions = bareHandsDepletions_,
        .nightResourcesGathered = nightResourcesGathered_,
        .farResourcesGathered = farResourcesGathered_};
    for (const auto& status : statuses_)
        state.statuses.push_back({status.definition.id, status.progress,
                                  status.completed, status.active, status.cycle});
    state.recentGathering.assign(recentGathering_.begin(), recentGathering_.end());
    state.eventMetricProgress.assign(
        eventMetricProgress_.begin(), eventMetricProgress_.end());
    return state;
}

bool ObjectiveSystem::loadState(const ObjectiveRunState& state) {
    if (state.challengeCycle < 0 || state.totalTreesDestroyed < 0 ||
        state.totalStonesDestroyed < 0 || state.totalCrystalsGathered < 0 ||
        state.totalResourcesGathered < 0 || state.dayWoodGathered < 0 ||
        state.dayStoneGathered < 0 || state.dayCrystalsGathered < 0 ||
        state.consecutiveDepletions < 0 || state.largeDepositsDepleted < 0 ||
        state.bareHandsDepletions < 0 || state.nightResourcesGathered < 0 ||
        state.farResourcesGathered < 0) return false;
    if (state.eventMetricProgress.size() > eventMetricProgress_.size() ||
        std::ranges::any_of(
            state.eventMetricProgress,
            [](int progress) { return progress < 0; })) {
        return false;
    }
    std::unordered_map<std::string, ObjectiveSavedStatus> saved;
    for (const auto& status : state.statuses) {
        if (!std::isfinite(status.progress) || status.progress < 0.0 ||
            !saved.emplace(status.id, status).second) return false;
    }
    ObjectiveSystem loaded{definitions_};
    loaded.challengeCycle_ = state.challengeCycle;
    loaded.totalTreesDestroyed_ = state.totalTreesDestroyed;
    loaded.totalStonesDestroyed_ = state.totalStonesDestroyed;
    loaded.totalCrystalsGathered_ = state.totalCrystalsGathered;
    loaded.totalResourcesGathered_ = state.totalResourcesGathered;
    loaded.dayWoodGathered_ = state.dayWoodGathered;
    loaded.dayStoneGathered_ = state.dayStoneGathered;
    loaded.dayCrystalsGathered_ = state.dayCrystalsGathered;
    loaded.consecutiveDepletions_ = state.consecutiveDepletions;
    loaded.largeDepositsDepleted_ = state.largeDepositsDepleted;
    loaded.bareHandsDepletions_ = state.bareHandsDepletions;
    loaded.nightResourcesGathered_ = state.nightResourcesGathered;
    loaded.farResourcesGathered_ = state.farResourcesGathered;
    loaded.recentGathering_.assign(state.recentGathering.begin(), state.recentGathering.end());
    std::ranges::copy(
        state.eventMetricProgress,
        loaded.eventMetricProgress_.begin());
    for (auto& status : loaded.statuses_) {
        const auto found = saved.find(status.definition.id);
        if (found == saved.end()) continue;
        status.progress = found->second.progress;
        status.completed = found->second.completed;
        status.active = found->second.active;
        status.cycle = found->second.cycle;
    }
    *this = std::move(loaded);
    return true;
}

const char* objectiveKindName(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone: return "MILESTONES";
    case ObjectiveKind::Challenge: return "CHALLENGES";
    case ObjectiveKind::WorldEvent: return "WORLD EVENTS";
    }
    return "OBJECTIVES";
}

std::vector<ObjectiveDefinition> loadObjectiveDefinitions(
    const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) return ObjectiveSystem::defaultDefinitions();
    try {
        const nlohmann::json json = nlohmann::json::parse(stream);
        if (!json.is_array()) return ObjectiveSystem::defaultDefinitions();
        std::vector<ObjectiveDefinition> result;
        for (const auto& item : json) {
            const std::string kindName = item.at("kind").get<std::string>();
            const std::string metricName = item.at("metric").get<std::string>();
            ObjectiveKind kind;
            if (kindName == "milestone") kind = ObjectiveKind::Milestone;
            else if (kindName == "challenge") kind = ObjectiveKind::Challenge;
            else if (kindName == "world_event") kind = ObjectiveKind::WorldEvent;
            else throw nlohmann::json::other_error::create(501, "invalid objective kind", &item);
            const std::unordered_map<std::string, ObjectiveMetric> metrics{
                {"trees_destroyed", ObjectiveMetric::TreesDestroyed},
                {"stones_destroyed", ObjectiveMetric::StonesDestroyed},
                {"crystals_gathered", ObjectiveMetric::CrystalsGathered},
                {"total_resources", ObjectiveMetric::TotalResourcesGathered},
                {"large_deposit", ObjectiveMetric::LargeDepositDepleted},
                {"all_resources_day", ObjectiveMetric::AllResourceTypesInDay},
                {"bare_hands", ObjectiveMetric::BareHandsDepletion},
                {"resources_60s", ObjectiveMetric::ResourcesInSixtySeconds},
                {"consecutive_depletions", ObjectiveMetric::ConsecutiveDepletions},
                {"night_resources", ObjectiveMetric::NightResourcesGathered},
                {"far_resource", ObjectiveMetric::FarResourceGathered},
                {"enemies_killed", ObjectiveMetric::EnemiesKilled},
                {"buildings_placed", ObjectiveMetric::BuildingsPlaced},
                {"modular_pieces", ObjectiveMetric::ModularPiecesPlaced},
                {"buildings_upgraded", ObjectiveMetric::BuildingsUpgraded},
                {"structures_repaired", ObjectiveMetric::StructuresRepaired},
                {"waves_completed", ObjectiveMetric::WavesCompleted},
                {"coins_collected", ObjectiveMetric::CoinsCollected},
                {"chests_opened", ObjectiveMetric::ChestsOpened},
                {"loot_collected", ObjectiveMetric::LootCollected},
                {"player_dashes", ObjectiveMetric::PlayerDashes},
                {"rifle_shots", ObjectiveMetric::RifleShots},
                {"elemental_hits", ObjectiveMetric::ElementalHits},
                {"trap_hits", ObjectiveMetric::TrapHits},
                {"cannon_shots", ObjectiveMetric::CannonShots},
                {"bombs_thrown", ObjectiveMetric::BombsThrown},
                {"early_waves", ObjectiveMetric::EarlyWavesStarted},
                {"structures_fortified", ObjectiveMetric::StructuresFortified},
                {"gates_toggled", ObjectiveMetric::GatesToggled},
                {"buildings_sold", ObjectiveMetric::BuildingsSold},
                {"falls_saved", ObjectiveMetric::FallsSaved},
            };
            const auto metric = metrics.find(metricName);
            if (metric == metrics.end())
                throw nlohmann::json::other_error::create(501, "invalid objective metric", &item);
            ObjectiveDefinition definition{
                .id = item.at("id").get<std::string>(),
                .title = item.at("title").get<std::string>(),
                .description = item.value("description", std::string{}),
                .kind = kind,
                .metric = metric->second,
                .target = item.at("target").get<double>(),
                .insightReward = item.at("insightReward").get<double>(),
                .chain = item.value("chain", std::string{}),
                .chainOrder = item.value("chainOrder", 0),
            };
            if (definition.id.empty() || !std::isfinite(definition.target) ||
                definition.target <= 0.0 ||
                !std::isfinite(definition.insightReward) ||
                definition.insightReward < 0.0)
                throw nlohmann::json::other_error::create(501, "invalid objective", &item);
            result.push_back(std::move(definition));
        }
        return result.empty() ? ObjectiveSystem::defaultDefinitions() : result;
    } catch (const nlohmann::json::exception&) {
        return ObjectiveSystem::defaultDefinitions();
    }
}

} // namespace ian
