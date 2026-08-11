#pragma once

#include "core/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ian {

enum class ObjectiveKind : std::uint8_t {
    Milestone,
    Challenge,
    WorldEvent,
};

enum class ObjectiveMetric : std::uint8_t {
    TreesDestroyed,
    StonesDestroyed,
    CrystalsGathered,
    TotalResourcesGathered,
    LargeDepositDepleted,
    AllResourceTypesInDay,
    BareHandsDepletion,
    ResourcesInSixtySeconds,
    ConsecutiveDepletions,
    NightResourcesGathered,
    FarResourceGathered,
    EnemiesKilled,
    BuildingsPlaced,
    ModularPiecesPlaced,
    BuildingsUpgraded,
    StructuresRepaired,
    WavesCompleted,
    CoinsCollected,
    ChestsOpened,
    LootCollected,
    PlayerDashes,
    RifleShots,
    ElementalHits,
    TrapHits,
    CannonShots,
    BombsThrown,
    EarlyWavesStarted,
    StructuresFortified,
    GatesToggled,
    BuildingsSold,
    FallsSaved,
    Count,
};

struct ObjectiveDefinition {
    std::string id;
    std::string title;
    std::string description;
    ObjectiveKind kind{ObjectiveKind::Milestone};
    ObjectiveMetric metric{ObjectiveMetric::TreesDestroyed};
    double target{1.0};
    double insightReward{5.0};
    std::string chain;
    int chainOrder{};
};

struct ObjectiveStatus {
    ObjectiveDefinition definition;
    double progress{};
    bool completed{};
    bool active{true};
    int cycle{};
};

struct ObjectiveCompletion {
    std::string id;
    std::string title;
    double insightReward{};
    ObjectiveKind kind{ObjectiveKind::Milestone};
    int cycle{};
};

struct ObjectiveResourceEvent {
    bool wood{};
    int amount{};
    bool depleted{};
    bool largeDeposit{};
    bool bareHands{};
    bool night{};
    bool hasCore{};
    double distanceFromCore{};
    double elapsedSeconds{};
};

struct ObjectiveSavedStatus {
    std::string id;
    double progress{};
    bool completed{};
    bool active{};
    int cycle{};
};

struct ObjectiveRunState {
    std::vector<ObjectiveSavedStatus> statuses;
    int challengeCycle{};
    int totalTreesDestroyed{};
    int totalStonesDestroyed{};
    int totalCrystalsGathered{};
    int totalResourcesGathered{};
    int dayWoodGathered{};
    int dayStoneGathered{};
    int dayCrystalsGathered{};
    int consecutiveDepletions{};
    int largeDepositsDepleted{};
    int bareHandsDepletions{};
    int nightResourcesGathered{};
    int farResourcesGathered{};
    std::vector<std::pair<double, int>> recentGathering;
    std::vector<int> eventMetricProgress;
};

class ObjectiveSystem {
  public:
    ObjectiveSystem();
    explicit ObjectiveSystem(std::vector<ObjectiveDefinition> definitions);

    [[nodiscard]] static std::vector<ObjectiveDefinition> defaultDefinitions();
    [[nodiscard]] std::vector<ObjectiveCompletion> onResourceEvent(
        const ObjectiveResourceEvent& event);
    [[nodiscard]] std::vector<ObjectiveCompletion> onCrystalsGathered(
        int amount, double elapsedSeconds, bool night);
    [[nodiscard]] std::vector<ObjectiveCompletion> onGameplayEvent(
        ObjectiveMetric metric, int amount, double elapsedSeconds);
    void onGatheringMiss();
    [[nodiscard]] std::vector<ObjectiveCompletion> beginNewDay();
    void reset();

    [[nodiscard]] std::span<const ObjectiveStatus> statuses() const;
    [[nodiscard]] std::vector<std::size_t> recommended(std::size_t maximum) const;
    [[nodiscard]] ObjectiveRunState saveState() const;
    [[nodiscard]] bool loadState(const ObjectiveRunState& state);

  private:
    void activateChallenges();
    void expireRecentGathering(double elapsedSeconds);
    void refreshProgress(double elapsedSeconds);
    [[nodiscard]] std::vector<ObjectiveCompletion> collectCompletions();
    [[nodiscard]] bool chainPrerequisiteCompleted(const ObjectiveStatus& status) const;

    std::vector<ObjectiveDefinition> definitions_;
    std::vector<ObjectiveStatus> statuses_;
    int challengeCycle_{};
    int totalTreesDestroyed_{};
    int totalStonesDestroyed_{};
    int totalCrystalsGathered_{};
    int totalResourcesGathered_{};
    int dayWoodGathered_{};
    int dayStoneGathered_{};
    int dayCrystalsGathered_{};
    int largeDepositsDepleted_{};
    int bareHandsDepletions_{};
    int consecutiveDepletions_{};
    int nightResourcesGathered_{};
    int farResourcesGathered_{};
    std::deque<std::pair<double, int>> recentGathering_;
    std::array<int, static_cast<std::size_t>(ObjectiveMetric::Count)>
        eventMetricProgress_{};
};

[[nodiscard]] const char* objectiveKindName(ObjectiveKind kind);
[[nodiscard]] std::vector<ObjectiveDefinition> loadObjectiveDefinitions(
    const std::filesystem::path& path);

} // namespace ian
