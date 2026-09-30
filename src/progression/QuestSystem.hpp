#pragma once

#include "progression/QuestTypes.hpp"

#include <array>
#include <filesystem>
#include <span>
#include <string_view>

namespace ian {

struct MetaProgression;

struct QuestSignal {
    QuestMetric metric;
    double amount{1.0};
    std::string_view subject{};
    std::string_view playerClass{};
};

struct QuestCompletion {
    std::string id;
    QuestReward reward;
};

struct QuestUpdate {
    bool changed{};
    std::vector<QuestCompletion> completed;
};

struct QuestClaimResult {
    bool claimed{};
    int silverGranted{};
};

struct QuestStatus {
    const QuestDefinition* definition{};
    std::vector<double> progress;
    double fraction{};
    bool prerequisitesMet{};
    bool failedThisRun{};
    bool completed{};
    bool rewardClaimed{};
    bool rewardAvailable{};
    bool unlockAvailable{};
    bool unlockOwned{};
};

struct QuestCategoryProgress {
    std::size_t completed{};
    std::size_t total{};
};

enum class QuestPurchaseResult { Purchased, UnknownUnlock, RequirementsNotMet, AlreadyOwned, InsufficientSilver };

class QuestSystem {
  public:
    QuestSystem();
    explicit QuestSystem(std::vector<QuestDefinition> definitions);
    [[nodiscard]] static std::vector<QuestDefinition> defaultDefinitions();

    // Seeds existing lifetime statistics, preserves old unlocks, and never
    // retroactively pays rewards for a migrated profile.
    void initialize(MetaProgression& progression) const;
    void beginRun(MetaProgression& progression) const;
    void resumeRun(MetaProgression& progression) const;
    void endRun(MetaProgression& progression) const;
    [[nodiscard]] QuestUpdate record(MetaProgression& progression,
                                     std::span<const QuestSignal> signals) const;
    [[nodiscard]] QuestUpdate refresh(MetaProgression& progression) const;
    [[nodiscard]] QuestClaimResult claimReward(
        MetaProgression& progression, std::string_view questId) const;
    [[nodiscard]] QuestPurchaseResult purchaseUnlock(
        MetaProgression& progression, std::string_view unlockId) const;
    [[nodiscard]] bool ownsUnlock(const MetaProgression& progression,
                                   std::string_view unlockId) const;
    [[nodiscard]] std::vector<QuestStatus> statuses(
        const MetaProgression& progression, bool completedLast = true) const;
    // Unclaimed rewards first, then available unfinished goals closest to
    // completion. Ties retain catalog order in both menus.
    [[nodiscard]] std::vector<QuestStatus> recommendedStatuses(
        const MetaProgression& progression, std::size_t limit = 4) const;
    [[nodiscard]] QuestCategoryProgress categoryProgress(
        const MetaProgression& progression, QuestCategory category) const;
    [[nodiscard]] std::span<const QuestDefinition> definitions() const;

  private:
    struct Subscription { std::size_t quest; std::size_t condition; };
    [[nodiscard]] QuestUpdate collect(MetaProgression& progression, bool createPendingRewards) const;
    [[nodiscard]] bool prerequisitesMet(const QuestDefinition& definition,
                                        const QuestState& state) const;
    std::vector<QuestDefinition> definitions_;
    std::array<std::vector<Subscription>, static_cast<std::size_t>(QuestMetric::Count)> subscriptions_;
};

// An invalid catalog fails as a whole rather than partially registering rewards.
[[nodiscard]] std::vector<QuestDefinition> loadQuestDefinitions(const std::filesystem::path& path);

} // namespace ian
