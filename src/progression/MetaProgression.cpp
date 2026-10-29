#include "progression/MetaProgression.hpp"
#include "core/SaturatingArithmetic.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace ian {
namespace {

constexpr int BerserkerWaveRequirement = 6;
constexpr int VampireKillRequirement = 250;
constexpr int AlchemistLootRequirement = 20;
constexpr int ChronomancerClearRequirement = 1;

int nonNegative(const nlohmann::json& json, const char* key) {
    const auto found = json.find(key);
    if (found == json.end() || !found->is_number_integer()) return 0;
    return std::max(0, found->get<int>());
}

bool validQuestState(const QuestState& state) {
    if (state.progress.size() > 4096 || state.ownedUnlocks.size() > 4096 ||
        state.purchasedUnlocks.size() > 4096) return false;
    const auto validId = [](const std::string& id) { return !id.empty() && id.size() <= 128; };
    for (const auto& [id, progress] : state.progress) {
        if (!validId(id) || progress.values.size() > 64 || (progress.rewardClaimed && !progress.completed)) return false;
        for (const double value : progress.values) {
            if (!std::isfinite(value) || value < 0.0 ||
                value > static_cast<double>(std::numeric_limits<int>::max()) + 1.0) return false;
        }
    }
    for (const auto& id : state.ownedUnlocks) if (!validId(id)) return false;
    for (const auto& id : state.purchasedUnlocks) {
        if (!validId(id) || !state.ownedUnlocks.contains(id)) return false;
    }
    return true;
}

} // namespace

bool isPlayerClassUnlocked(
    PlayerClass playerClass, const MetaProgression& progression) {
    switch (playerClass) {
    case PlayerClass::Vanguard:
    case PlayerClass::Ranger:
    case PlayerClass::Engineer:
    case PlayerClass::Prospector:
        return true;
    case PlayerClass::Berserker:
        return progression.quests.initialized
            ? progression.quests.ownedUnlocks.contains("class.berserker")
            : progression.bestWave >= BerserkerWaveRequirement;
    case PlayerClass::Vampire:
        return progression.quests.initialized
            ? progression.quests.ownedUnlocks.contains("class.vampire")
            : progression.enemiesDefeated >= VampireKillRequirement;
    case PlayerClass::Alchemist:
        return progression.quests.initialized
            ? progression.quests.ownedUnlocks.contains("class.alchemist")
            : progression.lootCollected >= AlchemistLootRequirement;
    case PlayerClass::Chronomancer:
        return progression.quests.initialized
            ? progression.quests.ownedUnlocks.contains("class.chronomancer")
            : progression.stageClears >= ChronomancerClearRequirement;
    case PlayerClass::None:
        return false;
    }
    return false;
}

std::string_view playerClassUnlockRequirement(PlayerClass playerClass) {
    switch (playerClass) {
    case PlayerClass::Berserker: return "REACH WAVE 6";
    case PlayerClass::Vampire: return "DEFEAT 250 ENEMIES";
    case PlayerClass::Alchemist: return "COLLECT 20 ITEMS";
    case PlayerClass::Chronomancer: return "CLEAR THE STAGE";
    default: return "UNLOCKED";
    }
}

float playerClassUnlockProgress(
    PlayerClass playerClass, const MetaProgression& progression) {
    double progress = 1.0;
    switch (playerClass) {
    case PlayerClass::Berserker:
        progress = static_cast<double>(progression.bestWave) /
            BerserkerWaveRequirement;
        break;
    case PlayerClass::Vampire:
        progress = static_cast<double>(progression.enemiesDefeated) /
            VampireKillRequirement;
        break;
    case PlayerClass::Alchemist:
        progress = static_cast<double>(progression.lootCollected) /
            AlchemistLootRequirement;
        break;
    case PlayerClass::Chronomancer:
        progress = static_cast<double>(progression.stageClears) /
            ChronomancerClearRequirement;
        break;
    default:
        break;
    }
    return static_cast<float>(std::clamp(progress, 0.0, 1.0));
}

static bool readMetaProgressionFile(
    std::string_view path, MetaProgression& progression) {
    try {
        std::ifstream input{std::string(path)};
        if (!input) return false;
        const nlohmann::json root = nlohmann::json::parse(input);
        if (!root.is_object()) return false;
        const int version = root.value("version", 1);
        if (version < 1 || version > 4) return false;
        if (version >= 3 && !root.contains("quests")) return false;
        MetaProgression loaded;
        loaded.silver = nonNegative(root, "silver");
        loaded.runsPlayed = nonNegative(root, "runsPlayed");
        // Existing players already know the placement controls. Fresh profiles
        // keep their first two successful placements across runs and reloads.
        loaded.placementControlHintsUsed = root.contains("placementControlHintsUsed")
            ? std::clamp(nonNegative(root, "placementControlHintsUsed"), 0, 2)
            : (loaded.runsPlayed > 0 ? 2 : 0);
        loaded.stageClears = nonNegative(root, "stageClears");
        loaded.bestWave = nonNegative(root, "bestWave");
        loaded.enemiesDefeated = nonNegative(root, "enemiesDefeated");
        loaded.lootCollected = nonNegative(root, "lootCollected");
        loaded.resourcesGathered = nonNegative(root, "resourcesGathered");
        if (root.contains("quests")) {
            const auto& quests = root.at("quests");
            if (!quests.is_object()) return false;
            const int questVersion = quests.at("version").get<int>();
            if (questVersion < 1 || questVersion > 2) return false;
            loaded.quests.initialized = quests.at("initialized").get<bool>();
            loaded.quests.runActive = quests.at("runActive").get<bool>();
            loaded.quests.ownedUnlocks = quests.at("ownedUnlocks").get<decltype(loaded.quests.ownedUnlocks)>();
            loaded.quests.purchasedUnlocks = quests.at("purchasedUnlocks").get<decltype(loaded.quests.purchasedUnlocks)>();
            const auto& progress = quests.at("progress");
            if (!progress.is_object() || progress.size() > 4096) return false;
            for (const auto& [id, saved] : progress.items()) {
                loaded.quests.progress.emplace(id, QuestSavedProgress{
                    .values = saved.at("values").get<std::vector<double>>(),
                    .completed = saved.at("completed").get<bool>(),
                    // Version 1 paid automatically, or intentionally skipped
                    // rewards during legacy migration. Neither may pay again.
                    .rewardClaimed = questVersion == 1 ? saved.at("completed").get<bool>()
                        : saved.at("rewardClaimed").get<bool>(),
                });
            }
            if (!validQuestState(loaded.quests)) return false;
        }
        if (version >= 4) {
            const auto& shop = root.at("shop");
            const int shopVersion=shop.at("version").get<int>();
            if (shopVersion<1 || shopVersion>3) return false;
            const auto& levels = shop.at("levels");
            if (!levels.is_object() || levels.size() > MetaShopUpgradeCount) return false;
            for (const auto& [id, level] : levels.items()) {
                const auto d = std::ranges::find(MetaShopDefinitions, id, &MetaShopDefinition::id);
                if (d == MetaShopDefinitions.end() || !level.is_number_integer()) return false;
                const int value = level.get<int>();
                const bool legacySlots=shopVersion==1 &&
                    (d->upgrade==MetaShopUpgrade::TomeSlots || d->upgrade==MetaShopUpgrade::BuildingSlots);
                const int maximum=legacySlots ? 4 : d->maximumLevel;
                if (value < 0 || value > maximum) return false;
                const bool retired=!activeMetaShopUpgrade(d->upgrade);
                if(shopVersion>=3 && retired && value!=0) return false;
                const int kept=retired ? 0 : std::min(value,d->maximumLevel);
                // Old ranks three/four are no longer sold. Refund their actual
                // prices once when converting the profile to the new shop schema.
                for(int rank=kept;rank<value;++rank)
                    loaded.silver=saturatingAdd(loaded.silver,static_cast<int>(std::ceil(
                        static_cast<double>(d->baseCost)*std::pow(1.5,rank))));
                loaded.shopLevels[static_cast<std::size_t>(d->upgrade)] = kept;
            }
            if(shopVersion<3 && loaded.quests.purchasedUnlocks.contains("meta.refresh"))
                loaded.silver=saturatingAdd(loaded.silver,5); // Retired discovery fee; ranks now own every use.
            loaded.disabledUnlocks=shop.value("disabledUnlocks",decltype(loaded.disabledUnlocks){});
            if(loaded.disabledUnlocks.size()>4096 || std::ranges::any_of(loaded.disabledUnlocks,
                [](const auto& id){return id.empty() || id.size()>128;})) return false;
            if (!shop.at("silverRemainder").is_number_integer()) return false;
            for (const auto& d : MetaShopDefinitions) {
                if (loaded.shopLevels[static_cast<std::size_t>(d.upgrade)] > 0 &&
                    !metaShopAvailable(loaded, d.upgrade)) return false;
            }
            loaded.silverRewardRemainder = shop.at("silverRemainder").get<int>();
            if (loaded.silverRewardRemainder < 0 || loaded.silverRewardRemainder >= 100) return false;
        }
        progression = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool loadMetaProgression(
    std::string_view path, MetaProgression& progression) {
    return readMetaProgressionFile(path, progression) ||
        readMetaProgressionFile(std::string(path) + ".bak", progression);
}

bool saveMetaProgression(
    std::string_view path, const MetaProgression& progression) {
    try {
        if (!validQuestState(progression.quests) || progression.disabledUnlocks.size()>4096 ||
            std::ranges::any_of(progression.disabledUnlocks,[](const auto& id){return id.empty() || id.size()>128;})) return false;
        const std::filesystem::path outputPath{path};
        if (outputPath.has_parent_path()) {
            std::filesystem::create_directories(outputPath.parent_path());
        }
        nlohmann::json progress = nlohmann::json::object();
        for (const auto& [id, saved] : progression.quests.progress) {
            progress[id] = {{"values", saved.values}, {"completed", saved.completed}, {"rewardClaimed", saved.rewardClaimed}};
        }
        nlohmann::json levels = nlohmann::json::object();
        for (const auto& d : MetaShopDefinitions) {
            const int level = progression.shopLevels[static_cast<std::size_t>(d.upgrade)];
            if (level < 0 || level > d.maximumLevel || (level > 0 && !metaShopAvailable(progression, d.upgrade))) return false;
            levels[std::string(d.id)] = level;
        }
        if (progression.silverRewardRemainder < 0 || progression.silverRewardRemainder >= 100) return false;
        const nlohmann::json root{
            {"version", 5},
            {"shop", {{"version", 3}, {"levels", levels}, {"silverRemainder", progression.silverRewardRemainder},
                {"disabledUnlocks",progression.disabledUnlocks}}},
            {"silver", std::max(0, progression.silver)},
            {"runsPlayed", std::max(0, progression.runsPlayed)},
            {"placementControlHintsUsed", std::clamp(progression.placementControlHintsUsed, 0, 2)},
            {"stageClears", std::max(0, progression.stageClears)},
            {"bestWave", std::max(0, progression.bestWave)},
            {"enemiesDefeated", std::max(0, progression.enemiesDefeated)},
            {"lootCollected", std::max(0, progression.lootCollected)},
            {"resourcesGathered", std::max(0, progression.resourcesGathered)},
            {"quests", {{"version", 2}, {"initialized", progression.quests.initialized},
                {"runActive", progression.quests.runActive}, {"progress", progress},
                {"ownedUnlocks", progression.quests.ownedUnlocks},
                {"purchasedUnlocks", progression.quests.purchasedUnlocks}}},
        };
        const std::filesystem::path temporary{outputPath.string() + ".tmp"};
        std::ofstream output{temporary};
        if (!output) return false;
        output << root.dump(2) << '\n';
        output.flush();
        if (!output.good()) return false;
        output.close();
        if (output.fail()) return false;
        std::error_code error;
        std::filesystem::rename(temporary, outputPath, error);
        if (!error) return true;
        // Windows cannot replace an existing destination with rename. Keep
        // the previous profile recoverable until the new one is installed.
        const std::filesystem::path backup{outputPath.string() + ".bak"};
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(outputPath, backup, error);
        if (error) return false;
        std::filesystem::rename(temporary, outputPath, error);
        if (!error) {
            std::filesystem::remove(backup, error);
            return true;
        }
        std::error_code restoreError;
        std::filesystem::rename(backup, outputPath, restoreError);
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace ian
