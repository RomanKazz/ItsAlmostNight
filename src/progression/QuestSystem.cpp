#include "progression/QuestSystem.hpp"
#include "progression/MetaProgression.hpp"
#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace ian {
namespace {

bool maximumMetric(QuestMetric metric) {
    return metric == QuestMetric::WaveReached || metric == QuestMetric::PlayerLevel ||
        metric == QuestMetric::UpgradeLevel || metric == QuestMetric::MaximumHealth ||
        metric == QuestMetric::DefenseDamage || metric == QuestMetric::QuestsCompleted ||
        metric == QuestMetric::UnlocksPurchased;
}

double legacyValue(QuestMetric metric, const MetaProgression& progression) {
    switch (metric) {
    case QuestMetric::RunsPlayed: return progression.runsPlayed;
    case QuestMetric::EnemiesDefeated: return progression.enemiesDefeated;
    case QuestMetric::ResourcesGathered: return progression.resourcesGathered;
    case QuestMetric::ItemsCollected: return progression.lootCollected;
    case QuestMetric::WaveReached: return progression.bestWave;
    case QuestMetric::StageClears: return progression.stageClears;
    default: return 0.0;
    }
}

bool satisfied(double value, const QuestCondition& condition) {
    return condition.comparison == QuestComparison::AtMost
        ? value <= condition.target : value + 1e-9 >= condition.target;
}

} // namespace

QuestSystem::QuestSystem() : QuestSystem(defaultDefinitions()) {}

QuestSystem::QuestSystem(std::vector<QuestDefinition> definitions)
    : definitions_(std::move(definitions)) {
    std::map<std::string, std::size_t, std::less<>> ids;
    std::set<std::string> unlocks;
    for (std::size_t index = 0; index < definitions_.size(); ++index) {
        const auto& definition = definitions_[index];
        if (definition.id.empty() || definition.id.size() > 128 || !ids.emplace(definition.id, index).second ||
            definition.conditions.empty() || definition.conditions.size() > 64 ||
            definition.reward.unlockId.size() > 128 || definition.reward.silver < 0 ||
            definition.reward.purchaseCost < 0 ||
            (definition.reward.unlockId.empty() && definition.reward.purchaseCost > 0) ||
            (!definition.reward.unlockId.empty() && !unlocks.insert(definition.reward.unlockId).second) ||
            static_cast<unsigned>(definition.category) > static_cast<unsigned>(QuestCategory::Cosmetics)) {
            throw std::invalid_argument("Invalid quest definition");
        }
        bool hasPositiveGoal = false;
        for (std::size_t conditionIndex = 0; conditionIndex < definition.conditions.size(); ++conditionIndex) {
            const auto& condition = definition.conditions[conditionIndex];
            const auto metric = static_cast<std::size_t>(condition.metric);
            if (metric >= subscriptions_.size() || !std::isfinite(condition.target) ||
                condition.target < 0.0 || condition.target > std::numeric_limits<int>::max() ||
                (condition.comparison == QuestComparison::AtLeast && condition.target <= 0.0) ||
                static_cast<unsigned>(condition.scope) > static_cast<unsigned>(QuestScope::Run) ||
                static_cast<unsigned>(condition.comparison) > static_cast<unsigned>(QuestComparison::AtMost) ||
                (condition.comparison == QuestComparison::AtMost && condition.scope != QuestScope::Run)) {
                throw std::invalid_argument("Invalid quest condition");
            }
            hasPositiveGoal = hasPositiveGoal || condition.comparison == QuestComparison::AtLeast;
            subscriptions_[metric].push_back({index, conditionIndex});
        }
        if (!hasPositiveGoal) throw std::invalid_argument("Quest needs a positive goal");
    }
    std::vector<int> visited(definitions_.size());
    const std::function<void(std::size_t)> visit = [&](std::size_t index) {
        if (visited[index] == 1) throw std::invalid_argument("Cyclic quest prerequisites");
        if (visited[index] == 2) return;
        visited[index] = 1;
        for (const auto& prerequisite : definitions_[index].prerequisites) {
            const auto found = ids.find(prerequisite);
            if (found == ids.end()) throw std::invalid_argument("Unknown quest prerequisite");
            visit(found->second);
        }
        visited[index] = 2;
    };
    for (std::size_t index = 0; index < definitions_.size(); ++index) visit(index);
}

void QuestSystem::initialize(MetaProgression& progression) const {
    const bool migrate = !progression.quests.initialized;
    for (const auto& definition : definitions_) {
        auto& progress = progression.quests.progress[definition.id];
        progress.values.resize(definition.conditions.size());
        for (std::size_t index = 0; index < progress.values.size(); ++index) {
            const auto& condition = definition.conditions[index];
            auto& value = progress.values[index];
            if (!std::isfinite(value) || value < 0.0) value = 0.0;
            if (migrate && condition.scope == QuestScope::Lifetime &&
                condition.subject.empty() && condition.playerClass.empty()) {
                value = std::max(value, legacyValue(condition.metric, progression));
            }
            value = std::min(value, condition.target +
                (condition.comparison == QuestComparison::AtMost ? 1.0 : 0.0));
        }
        if (progress.completed && !definition.reward.unlockId.empty() &&
            definition.reward.purchaseCost == 0) {
            progression.quests.ownedUnlocks.insert(definition.reward.unlockId);
        }
    }
    if (migrate) {
        progression.quests.runActive = false;
        static_cast<void>(collect(progression, false));
        progression.quests.initialized = true;
    }
}

void QuestSystem::beginRun(MetaProgression& progression) const {
    initialize(progression);
    for (const auto& definition : definitions_) {
        auto& progress = progression.quests.progress.at(definition.id);
        if (progress.completed) continue;
        for (std::size_t index = 0; index < definition.conditions.size(); ++index) {
            if (definition.conditions[index].scope == QuestScope::Run) progress.values[index] = 0.0;
        }
    }
    progression.quests.runActive = true;
}

void QuestSystem::endRun(MetaProgression& progression) const {
    progression.quests.runActive = false;
}

void QuestSystem::resumeRun(MetaProgression& progression) const {
    initialize(progression);
    if (progression.quests.runActive) return;
    beginRun(progression);
    // Old suspended runs have no quest history. Do not treat unknown past
    // actions as zero for goals such as clearing without opening a chest.
    for (const auto& definition : definitions_) {
        auto& progress = progression.quests.progress.at(definition.id);
        if (progress.completed) continue;
        for (std::size_t index = 0; index < definition.conditions.size(); ++index) {
            const auto& condition = definition.conditions[index];
            if (condition.comparison == QuestComparison::AtMost) {
                progress.values[index] = condition.target + 1.0;
            }
        }
    }
}

bool QuestSystem::prerequisitesMet(const QuestDefinition& definition, const QuestState& state) const {
    return std::ranges::all_of(definition.prerequisites, [&state](const std::string& id) {
        const auto found = state.progress.find(id);
        return found != state.progress.end() && found->second.completed;
    });
}

QuestUpdate QuestSystem::record(MetaProgression& progression, std::span<const QuestSignal> signals) const {
    if (!progression.quests.initialized) initialize(progression);
    bool changed = false;
    for (const auto& signal : signals) {
        const auto metric = static_cast<std::size_t>(signal.metric);
        if (metric >= subscriptions_.size() || !std::isfinite(signal.amount) || signal.amount <= 0.0) continue;
        for (const auto subscription : subscriptions_[metric]) {
            const auto& definition = definitions_[subscription.quest];
            const auto& condition = definition.conditions[subscription.condition];
            if ((condition.scope == QuestScope::Run && !progression.quests.runActive) ||
                (!condition.subject.empty() && condition.subject != signal.subject) ||
                (!condition.playerClass.empty() && condition.playerClass != signal.playerClass)) continue;
            auto& progress = progression.quests.progress.at(definition.id);
            if (progress.completed) continue;
            auto& value = progress.values[subscription.condition];
            const double limit = condition.target + (condition.comparison == QuestComparison::AtMost ? 1.0 : 0.0);
            const double next = std::min(limit, maximumMetric(signal.metric)
                ? std::max(value, signal.amount) : value + signal.amount);
            changed = changed || value != next;
            value = next;
        }
    }
    if (!changed) return {};
    auto update = collect(progression, true);
    update.changed = true;
    return update;
}

QuestUpdate QuestSystem::collect(MetaProgression& progression, bool payRewards) const {
    QuestUpdate update;
    bool changed;
    do {
        changed = false;
        std::size_t completed = 0;
        for (const auto& definition : definitions_) {
            if (progression.quests.progress.at(definition.id).completed) ++completed;
        }
        for (const auto& definition : definitions_) {
            auto& progress = progression.quests.progress.at(definition.id);
            if (progress.completed) continue;
            bool qualifies = prerequisitesMet(definition, progression.quests);
            for (std::size_t index = 0; index < definition.conditions.size(); ++index) {
                const auto& condition = definition.conditions[index];
                if (condition.metric == QuestMetric::QuestsCompleted || condition.metric == QuestMetric::UnlocksPurchased) {
                    progress.values[index] = std::min(condition.target, static_cast<double>(
                        condition.metric == QuestMetric::QuestsCompleted ? completed : progression.quests.purchasedUnlocks.size()));
                }
                if (condition.scope == QuestScope::Run && !progression.quests.runActive) qualifies = false;
                qualifies = qualifies && satisfied(progress.values[index], condition);
            }
            if (!qualifies) continue;
            progress.completed = true;
            changed = true;
            update.changed = true;
            if (!definition.reward.unlockId.empty() && definition.reward.purchaseCost == 0) {
                progression.quests.ownedUnlocks.insert(definition.reward.unlockId);
            }
            if (payRewards) {
                progression.silver = saturatingAdd(std::max(0, progression.silver), definition.reward.silver);
                update.completed.push_back({definition.id, definition.reward});
            }
        }
    } while (changed);
    return update;
}

QuestUpdate QuestSystem::refresh(MetaProgression& progression) const {
    initialize(progression);
    return collect(progression, true);
}

QuestPurchaseResult QuestSystem::purchaseUnlock(MetaProgression& progression, std::string_view unlockId) const {
    if (unlockId.empty()) return QuestPurchaseResult::UnknownUnlock;
    initialize(progression);
    const auto found = std::ranges::find_if(definitions_, [unlockId](const auto& definition) {
        return definition.reward.unlockId == unlockId;
    });
    if (found == definitions_.end()) return QuestPurchaseResult::UnknownUnlock;
    if (ownsUnlock(progression, unlockId)) return QuestPurchaseResult::AlreadyOwned;
    if (!progression.quests.progress.at(found->id).completed) return QuestPurchaseResult::RequirementsNotMet;
    if (progression.silver < found->reward.purchaseCost) return QuestPurchaseResult::InsufficientSilver;
    progression.silver -= found->reward.purchaseCost;
    progression.quests.ownedUnlocks.insert(found->reward.unlockId);
    progression.quests.purchasedUnlocks.insert(found->reward.unlockId);
    static_cast<void>(collect(progression, true));
    return QuestPurchaseResult::Purchased;
}

bool QuestSystem::ownsUnlock(const MetaProgression& progression, std::string_view unlockId) const {
    return progression.quests.ownedUnlocks.contains(unlockId);
}

std::span<const QuestDefinition> QuestSystem::definitions() const { return definitions_; }

std::vector<QuestStatus> QuestSystem::statuses(const MetaProgression& progression, bool completedLast) const {
    std::vector<QuestStatus> result;
    result.reserve(definitions_.size());
    for (const auto& definition : definitions_) {
        const auto found = progression.quests.progress.find(definition.id);
        QuestStatus status{.definition = &definition,
            .progress = std::vector<double>(definition.conditions.size()),
            .prerequisitesMet = prerequisitesMet(definition, progression.quests)};
        if (found != progression.quests.progress.end()) {
            status.completed = found->second.completed;
            std::copy_n(found->second.values.begin(), std::min(status.progress.size(), found->second.values.size()), status.progress.begin());
        }
        for (std::size_t index = 0; index < definition.conditions.size(); ++index) {
            const auto& condition = definition.conditions[index];
            status.fraction += condition.comparison == QuestComparison::AtMost
                ? (satisfied(status.progress[index], condition) ? 1.0 : 0.0)
                : std::clamp(status.progress[index] / condition.target, 0.0, 1.0);
        }
        status.fraction /= static_cast<double>(definition.conditions.size());
        if (status.completed) status.fraction = 1.0;
        status.unlockAvailable = status.completed && !definition.reward.unlockId.empty();
        status.unlockOwned = ownsUnlock(progression, definition.reward.unlockId);
        result.push_back(std::move(status));
    }
    if (completedLast) std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.completed < b.completed; });
    return result;
}

QuestCategoryProgress QuestSystem::categoryProgress(const MetaProgression& progression, QuestCategory category) const {
    QuestCategoryProgress result;
    for (const auto& definition : definitions_) {
        if (definition.category != category) continue;
        ++result.total;
        const auto found = progression.quests.progress.find(definition.id);
        if (found != progression.quests.progress.end() && found->second.completed) ++result.completed;
    }
    return result;
}

} // namespace ian
