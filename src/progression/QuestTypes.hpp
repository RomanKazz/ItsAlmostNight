#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ian {

enum class QuestCategory { Characters, Buildings, Weapons, Upgrades, Items, General, Challenges, Cosmetics };
enum class QuestScope { Lifetime, Run };
enum class QuestComparison { AtLeast, AtMost };
enum class QuestMetric {
    RunsPlayed, EnemiesDefeated, EnemyTypeDefeated, ResourcesGathered,
    BuildingsPlaced, BuildingsUpgraded, StructuresRepaired, ChestsOpened,
    ItemsCollected, CoinsCollected, WavesCompleted, WaveReached, StageClears,
    ChallengesCompleted, PlayerLevel, UpgradeLevel, MaximumHealth,
    DefenseDamage, QuestsCompleted, UnlocksPurchased, SecretFound,
    NodesDepleted, ProducersBuilt, ShrinesActivated, DefenseShots, DamageTaken,
    CoreDamageTaken, StructuresLost, PlayerDeaths, DawnReserve, Count
};

struct QuestCondition {
    QuestMetric metric{QuestMetric::EnemiesDefeated};
    QuestScope scope{QuestScope::Lifetime};
    double target{1.0};
    QuestComparison comparison{QuestComparison::AtLeast};
    std::string subject;
    std::string playerClass;
    bool operator==(const QuestCondition&) const = default;
};

struct QuestReward {
    int silver{};
    std::string unlockId;
    // Zero means an immediate unlock; positive means eligibility to purchase.
    int purchaseCost{};
    bool operator==(const QuestReward&) const = default;
};

struct QuestDefinition {
    std::string id;
    std::string title;
    std::string description;
    QuestCategory category{QuestCategory::General};
    std::vector<QuestCondition> conditions;
    std::vector<std::string> prerequisites;
    QuestReward reward;
    std::string notification{}; // Short completion result; full objective remains in the quest menu.
    bool operator==(const QuestDefinition&) const = default;
};

struct QuestSavedProgress {
    std::vector<double> values;
    bool completed{};
    bool rewardClaimed{};
    bool operator==(const QuestSavedProgress&) const = default;
};

struct QuestState {
    bool initialized{};
    bool runActive{};
    std::map<std::string, QuestSavedProgress, std::less<>> progress;
    std::set<std::string, std::less<>> ownedUnlocks;
    std::set<std::string, std::less<>> purchasedUnlocks;
    bool operator==(const QuestState&) const = default;
};

} // namespace ian
