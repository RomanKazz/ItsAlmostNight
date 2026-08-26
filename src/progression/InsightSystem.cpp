#include "progression/InsightSystem.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace ian {
namespace {

template <typename T>
void readValue(const nlohmann::json& object, const char* key, T& value) {
    if (const auto found = object.find(key); found != object.end() && found->is_number()) {
        value = found->get<T>();
    }
}

template <std::size_t Size>
void readArray(const nlohmann::json& object, const char* key,
               std::array<double, Size>& values) {
    const auto found = object.find(key);
    if (found == object.end() || !found->is_array() || found->size() != Size) return;
    for (std::size_t index = 0; index < Size; ++index) {
        if ((*found)[index].is_number()) values[index] = (*found)[index].get<double>();
    }
}

bool validNonNegative(double value) { return std::isfinite(value) && value >= 0.0; }

} // namespace

InsightConfig InsightConfig::defaults() { return {}; }

InsightSystem::InsightSystem(InsightConfig config) : config_(std::move(config)) {
    if (!std::isfinite(config_.baseRequirement) || config_.baseRequirement <= 0.0)
        config_.baseRequirement = 100.0;
    if (!std::isfinite(config_.requirementGrowth) || config_.requirementGrowth < 0.0)
        config_.requirementGrowth = 0.0;
    progress_.requiredInsight = requirementFor(0);
}

double InsightSystem::requirementFor(int totalPoints) const {
    return std::max(1.0, config_.baseRequirement +
        static_cast<double>(std::max(0, totalPoints)) * config_.requirementGrowth);
}

double InsightSystem::applyDiminishing(
    double amount, InsightCategory category, double& effectiveMultiplier) {
    const std::size_t index = static_cast<std::size_t>(category);
    double cursor = cycleBaseEarned_[index];
    double remaining = amount;
    double result = 0.0;
    const double fullEnd = std::max(0.0, config_.fullRateBudget);
    const double reducedEnd = fullEnd + std::max(0.0, config_.reducedRateBudget);

    const double fullPart = std::min(remaining, std::max(0.0, fullEnd - cursor));
    result += fullPart;
    remaining -= fullPart;
    cursor += fullPart;

    const double reducedPart = std::min(remaining, std::max(0.0, reducedEnd - cursor));
    result += reducedPart * std::max(0.0, config_.reducedRateMultiplier);
    remaining -= reducedPart;
    result += remaining * std::max(0.0, config_.exhaustedMultiplier);

    cycleBaseEarned_[index] += amount;
    effectiveMultiplier = amount > 0.0 ? result / amount : 1.0;
    return result;
}

InsightGrantResult InsightSystem::grantInsight(
    double amount, InsightSource source, InsightCategory category,
    const InsightGrantContext& context) {
    InsightGrantResult result{
        .source = source, .category = category, .baseAmount = amount,
        .modifier = context.modifier, .eventId = context.eventId,
        .playerId = context.playerId, .oneTime = context.oneTime,
        .bypassDiminishing = context.bypassDiminishing,
        .requirement = progress_.requiredInsight};
    if (!std::isfinite(amount) || amount <= 0.0 ||
        !std::isfinite(context.modifier) || context.modifier <= 0.0 ||
        source == InsightSource::Count || category == InsightCategory::Count) {
        lastGrant_ = result;
        return result;
    }
    if (context.oneTime && context.eventId != 0 && consumedEventIds_.contains(context.eventId)) {
        result.duplicate = true;
        ++blockedDuplicateEvents_;
        lastGrant_ = result;
        return result;
    }
    if (context.oneTime && context.eventId != 0) consumedEventIds_.insert(context.eventId);

    const double modified = amount * context.modifier;
    result.diminishingMultiplier = 1.0;
    result.finalAmount = context.bypassDiminishing
        ? modified : applyDiminishing(modified, category, result.diminishingMultiplier);
    if (!std::isfinite(result.finalAmount) || result.finalAmount <= 0.0) {
        lastGrant_ = result;
        return result;
    }

    result.accepted = true;
    result.insightBefore = progress_.currentInsight;
    progress_.currentInsight += result.finalAmount;
    progress_.totalInsightEarned += result.finalAmount;
    earnedByCategory_[static_cast<std::size_t>(category)] += result.finalAmount;
    earnedBySource_[static_cast<std::size_t>(source)] += result.finalAmount;

    while (progress_.currentInsight + 1e-9 >= progress_.requiredInsight) {
        progress_.currentInsight = std::max(0.0, progress_.currentInsight - progress_.requiredInsight);
        ++progress_.totalLevelsEarned;
        ++result.levelsGranted;
        progress_.requiredInsight = requirementFor(progress_.totalLevelsEarned);
    }
    result.insightAfter = progress_.currentInsight;
    lastGrant_ = result;
    return result;
}

void InsightSystem::beginNewDiminishingCycle() { cycleBaseEarned_.fill(0.0); }
void InsightSystem::markEventConsumed(std::uint64_t eventId) {
    if (eventId != 0) consumedEventIds_.insert(eventId);
}
void InsightSystem::reset() {
    progress_ = {};
    progress_.requiredInsight = requirementFor(0);
    cycleBaseEarned_.fill(0.0);
    earnedByCategory_.fill(0.0);
    earnedBySource_.fill(0.0);
    consumedEventIds_.clear();
    blockedDuplicateEvents_ = 0;
    lastGrant_ = {};
}

const InsightProgress& InsightSystem::progress() const { return progress_; }
const InsightConfig& InsightSystem::config() const { return config_; }
const InsightGrantResult& InsightSystem::lastGrant() const { return lastGrant_; }
const std::array<double, InsightCategoryCount>& InsightSystem::earnedByCategory() const {
    return earnedByCategory_;
}
std::uint64_t InsightSystem::blockedDuplicateEvents() const { return blockedDuplicateEvents_; }

InsightRunState InsightSystem::saveState() const {
    InsightRunState state{
        .progress = progress_, .cycleBaseEarned = cycleBaseEarned_,
        .earnedByCategory = earnedByCategory_, .earnedBySource = earnedBySource_,
        .blockedDuplicateEvents = blockedDuplicateEvents_};
    state.consumedEventIds.assign(consumedEventIds_.begin(), consumedEventIds_.end());
    return state;
}

bool InsightSystem::loadState(const InsightRunState& state) {
    constexpr std::size_t MaximumConsumedEventIds = 100'000U;
    const double expectedRequirement =
        requirementFor(state.progress.totalLevelsEarned);
    if (!validNonNegative(state.progress.currentInsight) ||
        !std::isfinite(state.progress.requiredInsight) || state.progress.requiredInsight <= 0.0 ||
        state.progress.currentInsight >= state.progress.requiredInsight + 1e-9 ||
        state.progress.totalLevelsEarned < 0 ||
        !validNonNegative(state.progress.totalInsightEarned) ||
        state.progress.totalInsightEarned + 1e-9 <
            state.progress.currentInsight ||
        !std::isfinite(expectedRequirement) ||
        std::abs(state.progress.requiredInsight - expectedRequirement) >
            1e-9 * std::max(1.0, expectedRequirement) ||
        state.consumedEventIds.size() > MaximumConsumedEventIds) {
        return false;
    }
    for (double value : state.cycleBaseEarned) if (!validNonNegative(value)) return false;
    for (double value : state.earnedByCategory) if (!validNonNegative(value)) return false;
    for (double value : state.earnedBySource) if (!validNonNegative(value)) return false;

    std::unordered_set<std::uint64_t> consumedEventIds;
    consumedEventIds.reserve(state.consumedEventIds.size());
    for (const std::uint64_t eventId : state.consumedEventIds) {
        if (eventId == 0U || !consumedEventIds.insert(eventId).second) {
            return false;
        }
    }
    progress_ = state.progress;
    cycleBaseEarned_ = state.cycleBaseEarned;
    earnedByCategory_ = state.earnedByCategory;
    earnedBySource_ = state.earnedBySource;
    consumedEventIds_ = std::move(consumedEventIds);
    blockedDuplicateEvents_ = state.blockedDuplicateEvents;
    lastGrant_ = {};
    return true;
}

InsightConfig loadInsightConfig(const std::filesystem::path& path) {
    InsightConfig config = InsightConfig::defaults();
    std::ifstream stream(path);
    if (!stream) return config;
    try {
        const auto json = nlohmann::json::parse(stream);
        readValue(json, "baseRequirement", config.baseRequirement);
        readValue(json, "requirementGrowth", config.requirementGrowth);
        if (const auto it = json.find("diminishingReturns"); it != json.end() && it->is_object()) {
            readValue(*it, "fullRateBudget", config.fullRateBudget);
            readValue(*it, "reducedRateBudget", config.reducedRateBudget);
            readValue(*it, "reducedRateMultiplier", config.reducedRateMultiplier);
            readValue(*it, "exhaustedMultiplier", config.exhaustedMultiplier);
        }
        if (const auto it = json.find("rewards"); it != json.end() && it->is_object()) {
            readValue(*it, "introGatherObjective", config.introGatherObjective);
            readValue(*it, "introCoreObjective", config.introCoreObjective);
            readValue(*it, "normalWave", config.normalWave);
            readValue(*it, "milestoneWave", config.milestoneWave);
            readValue(*it, "bossWave", config.bossWave);
            readValue(*it, "milestoneWaveInterval", config.milestoneWaveInterval);
            readArray(*it, "enemy", config.enemy);
            readArray(*it, "resourcePerUnit", config.resourcePerUnit);
            readArray(*it, "resourceDepleted", config.resourceDepleted);
            readArray(*it, "building", config.building);
            readArray(*it, "modularBuilding", config.modularBuilding);
            readValue(*it, "firstBuildingTypeBonus", config.firstBuildingTypeBonus);
            readValue(*it, "repairPerHealth", config.repairPerHealth);
            readArray(*it, "chest", config.chest);
        }
        if (const auto it = json.find("hud"); it != json.end() && it->is_object()) {
            readValue(*it, "smallPulseSeconds", config.hudSmallPulseSeconds);
            readValue(*it, "largePulseSeconds", config.hudLargePulseSeconds);
            readValue(*it, "largeRewardThreshold", config.hudLargeRewardThreshold);
            readValue(*it, "pointSequenceSeconds", config.hudPointSequenceSeconds);
            readValue(*it, "aggregationWindowSeconds", config.hudAggregationWindowSeconds);
        }
    } catch (const nlohmann::json::exception&) {
        return InsightConfig::defaults();
    }
    return config;
}

std::string_view insightSourceName(InsightSource source) {
    constexpr std::array names{
        "Objective", "Wave Completed", "Enemy Killed", "Elite Killed", "Boss Killed",
        "Resource Gathered", "Resource Depleted", "Structure Built", "Structure Repaired",
        "Chest Opened", "Area Discovered", "Point of Interest", "World Event",
        "Challenge Completed", "Biome Discovered", "Secret Found", "Other"};
    const auto index = static_cast<std::size_t>(source);
    return index < names.size() ? names[index] : "Unknown";
}

std::string_view insightCategoryName(InsightCategory category) {
    constexpr std::array names{"Combat", "Gathering", "Building", "Repair", "Exploration"};
    const auto index = static_cast<std::size_t>(category);
    return index < names.size() ? names[index] : "Unknown";
}

} // namespace ian
