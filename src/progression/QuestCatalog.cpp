#include "progression/QuestSystem.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace ian {

std::vector<QuestDefinition> QuestSystem::defaultDefinitions() {
#include "progression/QuestCatalogData.inc"
}

namespace {

template <typename Enum>
Enum parseEnum(const std::string& value, const std::map<std::string, Enum>& names) {
    const auto found = names.find(value);
    if (found == names.end()) throw std::invalid_argument("Unknown quest catalog value: " + value);
    return found->second;
}

} // namespace

std::vector<QuestDefinition> loadQuestDefinitions(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot read quest catalog");
    const auto root = nlohmann::json::parse(input);
    if (root.at("version").get<int>() != 1) throw std::invalid_argument("Unsupported quest catalog version");
    static const std::map<std::string, QuestMetric> Metrics{
        {"runs_played", QuestMetric::RunsPlayed},
        {"enemies_defeated", QuestMetric::EnemiesDefeated},
        {"enemy_type_defeated", QuestMetric::EnemyTypeDefeated},
        {"resources_gathered", QuestMetric::ResourcesGathered},
        {"buildings_placed", QuestMetric::BuildingsPlaced},
        {"buildings_upgraded", QuestMetric::BuildingsUpgraded},
        {"structures_repaired", QuestMetric::StructuresRepaired},
        {"chests_opened", QuestMetric::ChestsOpened},
        {"items_collected", QuestMetric::ItemsCollected},
        {"coins_collected", QuestMetric::CoinsCollected},
        {"waves_completed", QuestMetric::WavesCompleted},
        {"wave_reached", QuestMetric::WaveReached},
        {"stage_clears", QuestMetric::StageClears},
        {"challenges_completed", QuestMetric::ChallengesCompleted},
        {"player_level", QuestMetric::PlayerLevel},
        {"upgrade_level", QuestMetric::UpgradeLevel},
        {"maximum_health", QuestMetric::MaximumHealth},
        {"defense_damage", QuestMetric::DefenseDamage},
        {"quests_completed", QuestMetric::QuestsCompleted},
        {"unlocks_purchased", QuestMetric::UnlocksPurchased},
        {"secret_found", QuestMetric::SecretFound},
    };
    static const std::map<std::string, QuestCategory> Categories{
        {"characters", QuestCategory::Characters},
        {"buildings", QuestCategory::Buildings},
        {"weapons", QuestCategory::Weapons},
        {"upgrades", QuestCategory::Upgrades},
        {"items", QuestCategory::Items},
        {"general", QuestCategory::General},
        {"challenges", QuestCategory::Challenges},
        {"cosmetics", QuestCategory::Cosmetics},
    };
    static const std::map<std::string, QuestScope> Scopes{{"lifetime", QuestScope::Lifetime}, {"run", QuestScope::Run}};
    static const std::map<std::string, QuestComparison> Comparisons{{"at_least", QuestComparison::AtLeast}, {"at_most", QuestComparison::AtMost}};
    std::vector<QuestDefinition> definitions;
    for (const auto& entry : root.at("quests")) {
        QuestDefinition definition{
            .id = entry.at("id").get<std::string>(),
            .title = entry.at("title").get<std::string>(),
            .description = entry.at("description").get<std::string>(),
            .category = parseEnum(entry.at("category").get<std::string>(), Categories),
        };
        for (const auto& condition : entry.at("conditions")) {
            definition.conditions.push_back({
                .metric = parseEnum(condition.at("metric").get<std::string>(), Metrics),
                .scope = parseEnum(condition.value("scope", std::string("lifetime")), Scopes),
                .target = condition.at("target").get<double>(),
                .comparison = parseEnum(condition.value("comparison", std::string("at_least")), Comparisons),
                .subject = condition.value("subject", std::string()),
                .playerClass = condition.value("playerClass", std::string()),
            });
        }
        definition.notification = entry.value("notification", std::string());
        definition.prerequisites = entry.value("prerequisites", std::vector<std::string>());
        if (entry.contains("reward")) {
            const auto& reward = entry.at("reward");
            definition.reward = {reward.value("silver", 0), reward.value("unlockId", std::string()), reward.value("purchaseCost", 0)};
        }
        definitions.push_back(std::move(definition));
    }
    if (definitions.empty()) throw std::invalid_argument("Empty quest catalog");
    const QuestSystem validation(definitions);
    return definitions;
}

} // namespace ian
