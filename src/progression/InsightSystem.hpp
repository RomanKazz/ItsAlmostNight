#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ian {

enum class InsightSource : std::uint8_t {
    Objective,
    WaveCompleted,
    EnemyKilled,
    EliteKilled,
    BossKilled,
    ResourceGathered,
    ResourceDepleted,
    StructureBuilt,
    StructureRepaired,
    ChestOpened,
    AreaDiscovered,
    PointOfInterestDiscovered,
    WorldEventCompleted,
    ChallengeCompleted,
    BiomeDiscovered,
    SecretFound,
    Other,
    Count,
};

enum class InsightCategory : std::uint8_t {
    Combat,
    Gathering,
    Building,
    Repair,
    Exploration,
    Count,
};

inline constexpr std::size_t InsightSourceCount =
    static_cast<std::size_t>(InsightSource::Count);
inline constexpr std::size_t InsightCategoryCount =
    static_cast<std::size_t>(InsightCategory::Count);

struct InsightConfig {
    double baseRequirement{100.0};
    double requirementGrowth{};
    double fullRateBudget{20.0};
    double reducedRateBudget{20.0};
    double reducedRateMultiplier{0.5};
    double exhaustedMultiplier{0.2};

    double introGatherObjective{60.0};
    double introCoreObjective{40.0};
    double normalWave{30.0};
    double milestoneWave{55.0};
    double bossWave{100.0};
    int milestoneWaveInterval{5};
    // EnemyType order: Basic, Fast, Heavy, Boss, Ranged, Sapper, Flying,
    // Splitter, Splitling.
    std::array<double, 9> enemy{{0.75, 1.0, 3.0, 75.0, 2.0, 3.0, 2.5,
                                 4.0, 0.5}};
    std::array<double, 2> resourcePerUnit{{0.14, 0.22}};
    std::array<double, 2> resourceDepleted{{1.5, 2.0}};
    std::array<double, 10> building{{
        4.0, 0.75, 1.5, 1.5, 3.0,
        1.0, 1.0, 1.5, 1.5, 1.25}};
    std::array<double, 4> modularBuilding{{1.0, 1.0, 0.5, 1.0}};
    double firstBuildingTypeBonus{2.0};
    double repairPerHealth{0.015};
    std::array<double, 2> chest{{4.0, 8.0}};

    double hudSmallPulseSeconds{0.25};
    double hudLargePulseSeconds{0.55};
    double hudLargeRewardThreshold{10.0};
    double hudPointSequenceSeconds{0.48};
    double hudAggregationWindowSeconds{0.8};

    [[nodiscard]] static InsightConfig defaults();
};

struct InsightProgress {
    double currentInsight{};
    double requiredInsight{100.0};
    int totalTreePointsEarned{};
    double totalInsightEarned{};
};

struct InsightGrantContext {
    std::uint64_t eventId{};
    std::optional<std::uint32_t> playerId;
    bool oneTime{};
    bool bypassDiminishing{};
    double modifier{1.0};
};

struct InsightGrantResult {
    bool accepted{};
    bool duplicate{};
    InsightSource source{InsightSource::Other};
    InsightCategory category{InsightCategory::Exploration};
    double baseAmount{};
    double modifier{1.0};
    std::uint64_t eventId{};
    std::optional<std::uint32_t> playerId;
    bool oneTime{};
    bool bypassDiminishing{};
    double diminishingMultiplier{1.0};
    double finalAmount{};
    double insightBefore{};
    double insightAfter{};
    double requirement{};
    int treePointsGranted{};
};

struct InsightRunState {
    InsightProgress progress;
    std::array<double, InsightCategoryCount> cycleBaseEarned{};
    std::array<double, InsightCategoryCount> earnedByCategory{};
    std::array<double, InsightSourceCount> earnedBySource{};
    std::vector<std::uint64_t> consumedEventIds;
    std::uint64_t blockedDuplicateEvents{};
};

class InsightSystem {
  public:
    explicit InsightSystem(InsightConfig config = InsightConfig::defaults());

    [[nodiscard]] InsightGrantResult grantInsight(
        double amount, InsightSource source, InsightCategory category,
        const InsightGrantContext& context = {});
    void beginNewDiminishingCycle();
    void markEventConsumed(std::uint64_t eventId);
    void reset();

    [[nodiscard]] const InsightProgress& progress() const;
    [[nodiscard]] const InsightConfig& config() const;
    [[nodiscard]] const InsightGrantResult& lastGrant() const;
    [[nodiscard]] const std::array<double, InsightCategoryCount>&
    earnedByCategory() const;
    [[nodiscard]] std::uint64_t blockedDuplicateEvents() const;
    [[nodiscard]] InsightRunState saveState() const;
    [[nodiscard]] bool loadState(const InsightRunState& state);

  private:
    [[nodiscard]] double applyDiminishing(
        double amount, InsightCategory category, double& effectiveMultiplier);
    [[nodiscard]] double requirementFor(int totalPoints) const;

    InsightConfig config_;
    InsightProgress progress_;
    std::array<double, InsightCategoryCount> cycleBaseEarned_{};
    std::array<double, InsightCategoryCount> earnedByCategory_{};
    std::array<double, InsightSourceCount> earnedBySource_{};
    std::unordered_set<std::uint64_t> consumedEventIds_;
    std::uint64_t blockedDuplicateEvents_{};
    InsightGrantResult lastGrant_{};
};

[[nodiscard]] InsightConfig loadInsightConfig(const std::filesystem::path& path);
[[nodiscard]] std::string_view insightSourceName(InsightSource source);
[[nodiscard]] std::string_view insightCategoryName(InsightCategory category);

} // namespace ian
