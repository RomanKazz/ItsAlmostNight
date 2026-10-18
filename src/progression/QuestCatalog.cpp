#include "progression/QuestSystem.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace ian {

std::vector<QuestDefinition> QuestSystem::defaultDefinitions() {
    return {
        {"class_berserker", "Reach wave 6", "Reach wave 6",
         QuestCategory::Characters, {{QuestMetric::WaveReached, QuestScope::Lifetime, 6.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "class.berserker", 0}},
        {"class_vampire", "Defeat 250 enemies", "Defeat 250 enemies",
         QuestCategory::Characters, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 250.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "class.vampire", 0}},
        {"class_alchemist", "Collect 20 items", "Collect 20 items",
         QuestCategory::Characters, {{QuestMetric::ItemsCollected, QuestScope::Lifetime, 20.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "class.alchemist", 0}},
        {"class_chronomancer", "Clear the stage", "Clear the stage",
         QuestCategory::Characters, {{QuestMetric::StageClears, QuestScope::Lifetime, 1.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "class.chronomancer", 0}},
        {"kills_100", "Defeat 100 enemies", "Defeat 100 enemies",
         QuestCategory::General, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 100.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "", 0}},
        {"kills_1000", "Defeat 1000 enemies", "Defeat 1000 enemies",
         QuestCategory::General, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 1000.0, QuestComparison::AtLeast, "", ""}},
         {"kills_100"}, {2, "", 0}},
        {"kills_5000", "Defeat 5000 enemies", "Defeat 5000 enemies",
         QuestCategory::General, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 5000.0, QuestComparison::AtLeast, "", ""}},
         {"kills_1000"}, {2, "", 0}},
        {"gun_turret_kills", "Defeat 250 enemies with gun turret", "Defeat 250 enemies with gun turret",
         QuestCategory::Buildings, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 250.0, QuestComparison::AtLeast, "gun_turret", ""}},
         {}, {2, "", 0}},
        {"cannon_kills", "Defeat 150 enemies with cannon", "Defeat 150 enemies with cannon",
         QuestCategory::Buildings, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 150.0, QuestComparison::AtLeast, "cannon", ""}},
         {}, {2, "", 0}},
        {"turret_kills", "Defeat 250 enemies with turret", "Defeat 250 enemies with turret",
         QuestCategory::Buildings, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 250.0, QuestComparison::AtLeast, "turret", ""}},
         {}, {2, "", 0}},
        {"trap_kills", "Defeat 100 enemies with trap", "Defeat 100 enemies with trap",
         QuestCategory::Buildings, {{QuestMetric::EnemiesDefeated, QuestScope::Lifetime, 100.0, QuestComparison::AtLeast, "spike_trap", ""}},
         {}, {2, "", 0}},
        {"build_25", "Place 25 buildings", "Place 25 buildings",
         QuestCategory::Buildings, {{QuestMetric::BuildingsPlaced, QuestScope::Lifetime, 25.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "", 0}},
        {"build_100", "Place 100 buildings", "Place 100 buildings",
         QuestCategory::Buildings, {{QuestMetric::BuildingsPlaced, QuestScope::Lifetime, 100.0, QuestComparison::AtLeast, "", ""}},
         {"build_25"}, {1, "", 0}},
        {"gun_run", "Defeat 100 enemies with gun turrets in one run", "Defeat 100 enemies with gun turrets in one run",
         QuestCategory::Buildings, {{QuestMetric::EnemiesDefeated, QuestScope::Run, 100.0, QuestComparison::AtLeast, "gun_turret", ""}},
         {}, {2, "", 0}},
        {"engineer_run", "Repair 15 structures as Engineer in one run", "Repair 15 structures as Engineer in one run",
         QuestCategory::Buildings, {{QuestMetric::StructuresRepaired, QuestScope::Run, 15.0, QuestComparison::AtLeast, "", "engineer"}},
         {}, {2, "", 0}},
        {"upgrade_buildings", "Upgrade 20 buildings", "Upgrade 20 buildings",
         QuestCategory::Buildings, {{QuestMetric::BuildingsUpgraded, QuestScope::Lifetime, 20.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "", 0}},
        {"upgrade_tower_power", "Get Tower Power to level 5", "Get Tower Power to level 5",
         QuestCategory::Upgrades, {{QuestMetric::UpgradeLevel, QuestScope::Run, 5.0, QuestComparison::AtLeast, "defense_damage", ""}},
         {}, {2, "", 0}},
        {"upgrade_rapid_fire", "Get Rapid Fire to level 5", "Get Rapid Fire to level 5",
         QuestCategory::Upgrades, {{QuestMetric::UpgradeLevel, QuestScope::Run, 5.0, QuestComparison::AtLeast, "defense_fire_rate", ""}},
         {}, {2, "", 0}},
        {"upgrade_ricochet", "Get Ricochet to level 3", "Get Ricochet to level 3",
         QuestCategory::Upgrades, {{QuestMetric::UpgradeLevel, QuestScope::Run, 3.0, QuestComparison::AtLeast, "ricochet", ""}},
         {}, {2, "", 0}},
        {"defense_double", "Reach 2x defense damage in one run", "Reach 2x defense damage in one run",
         QuestCategory::Upgrades, {{QuestMetric::DefenseDamage, QuestScope::Run, 2.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "", 0}},
        {"hp_200", "Reach 200 maximum HP in one run", "Reach 200 maximum HP in one run",
         QuestCategory::General, {{QuestMetric::MaximumHealth, QuestScope::Run, 200.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "", 0}},
        {"level_20", "Reach player level 20", "Reach player level 20",
         QuestCategory::General, {{QuestMetric::PlayerLevel, QuestScope::Run, 20.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "", 0}},
        {"chests_10", "Open 10 chests", "Open 10 chests",
         QuestCategory::Items, {{QuestMetric::ChestsOpened, QuestScope::Lifetime, 10.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "", 0}},
        {"items_50", "Collect 50 items", "Collect 50 items",
         QuestCategory::Items, {{QuestMetric::ItemsCollected, QuestScope::Lifetime, 50.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "", 0}},
        {"items_run", "Collect 10 items in one run", "Collect 10 items in one run",
         QuestCategory::Items, {{QuestMetric::ItemsCollected, QuestScope::Run, 10.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "", 0}},
        {"resources_1000", "Gather 1000 resources", "Gather 1000 resources",
         QuestCategory::General, {{QuestMetric::ResourcesGathered, QuestScope::Lifetime, 1000.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "", 0}},
        {"coins_500", "Pick up 500 gold", "Pick up 500 gold",
         QuestCategory::General, {{QuestMetric::CoinsCollected, QuestScope::Lifetime, 500.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "", 0}},
        {"clear_without_chests", "Clear a stage without opening chests", "Clear a stage without opening chests",
         QuestCategory::Challenges, {{QuestMetric::StageClears, QuestScope::Run, 1.0, QuestComparison::AtLeast, "", ""}, {QuestMetric::ChestsOpened, QuestScope::Run, 0.0, QuestComparison::AtMost, "", ""}},
         {}, {4, "", 0}},
        {"challenges_1", "Complete a portal challenge", "Complete a portal challenge",
         QuestCategory::Challenges, {{QuestMetric::ChallengesCompleted, QuestScope::Lifetime, 1.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "", 0}},
        {"challenges_5", "Complete 5 portal challenges", "Complete 5 portal challenges",
         QuestCategory::Challenges, {{QuestMetric::ChallengesCompleted, QuestScope::Lifetime, 5.0, QuestComparison::AtLeast, "", ""}},
         {"challenges_1"}, {2, "", 0}},
        {"challenges_20", "Complete 20 portal challenges", "Complete 20 portal challenges",
         QuestCategory::Challenges, {{QuestMetric::ChallengesCompleted, QuestScope::Lifetime, 20.0, QuestComparison::AtLeast, "", ""}},
         {"challenges_5"}, {2, "", 0}},
        {"quests_10", "Complete 10 quests", "Complete 10 quests",
         QuestCategory::General, {{QuestMetric::QuestsCompleted, QuestScope::Lifetime, 10.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "meta.refresh", 10}},
        {"quests_20", "Complete 20 quests", "Complete 20 quests",
         QuestCategory::General, {{QuestMetric::QuestsCompleted, QuestScope::Lifetime, 20.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "meta.skip", 15}},
        {"quests_30", "Complete 30 quests", "Complete 30 quests",
         QuestCategory::General, {{QuestMetric::QuestsCompleted, QuestScope::Lifetime, 30.0, QuestComparison::AtLeast, "", ""}},
         {}, {1, "meta.banish", 20}},
        {"purchases_3", "Purchase 3 permanent unlocks", "Purchase 3 permanent unlocks",
         QuestCategory::General, {{QuestMetric::UnlocksPurchased, QuestScope::Lifetime, 3.0, QuestComparison::AtLeast, "", ""}},
         {}, {2, "", 0}},
    };
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
