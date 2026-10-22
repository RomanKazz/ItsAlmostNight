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
    double requirementGrowth{15.0};
    // XP discounts for the first four rewards; later levels use the normal curve.
    std::array<double, 4> earlyRequirementDiscounts{{70.0, 50.0, 35.0, 20.0}};
    double fullRateBudget{28.0};
    double reducedRateBudget{28.0};
    double reducedRateMultiplier{0.5};
    double exhaustedMultiplier{0.2};

    double introGatherObjective{};
    double introCoreObjective{};
    double normalWave{42.0};
    double milestoneWave{77.0};
    double bossWave{140.0};
    int milestoneWaveInterval{5};
    // EnemyType order: Basic, Fast, Heavy, Boss, Ranged, Sapper, Flying,
    // Splitter, Splitling, Bee, AirTank.
    std::array<double, 11> enemy{{3.0, 3.0, 8.0, 70.0, 4.0, 6.0, 4.0,
                                 7.0, 3.0, 5.0, 12.0}};
    std::array<double, 3> resourcePerUnit{};
    std::array<double, 3> resourceDepleted{{11.0, 14.0, 17.0}};
    std::array<double, 3> prop{{24.0, 32.0, 48.0}};
    std::array<double, 10> building{{
        5.6, 1.05, 2.1, 2.1, 4.2,
        1.4, 1.4, 2.1, 2.1, 1.75}};
    std::array<double, 4> modularBuilding{{1.4, 1.4, 0.7, 1.4}};
    double firstBuildingTypeBonus{2.8};
    double repairPerHealth{0.021};
    std::array<double, 2> chest{{28.0, 49.0}};

    double hudSmallPulseSeconds{0.25};
    double hudLargePulseSeconds{0.55};
    double hudLargeRewardThreshold{14.0};
    double hudPointSequenceSeconds{0.48};
    double hudAggregationWindowSeconds{0.8};

    [[nodiscard]] static InsightConfig defaults();
};

struct InsightProgress {
    double currentInsight{};
    double requiredInsight{100.0};
    int totalLevelsEarned{};
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
    int levelsGranted{};
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
    void reset();

    [[nodiscard]] const InsightProgress& progress() const;
    [[nodiscard]] const InsightConfig& config() const;
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
};

[[nodiscard]] InsightConfig loadInsightConfig(const std::filesystem::path& path);
[[nodiscard]] std::string_view insightSourceName(InsightSource source);
[[nodiscard]] std::string_view insightCategoryName(InsightCategory category);

} // namespace ian
