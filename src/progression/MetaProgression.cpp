#include "progression/MetaProgression.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
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
        return progression.bestWave >= BerserkerWaveRequirement;
    case PlayerClass::Vampire:
        return progression.enemiesDefeated >= VampireKillRequirement;
    case PlayerClass::Alchemist:
        return progression.lootCollected >= AlchemistLootRequirement;
    case PlayerClass::Chronomancer:
        return progression.stageClears >= ChronomancerClearRequirement;
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

bool loadMetaProgression(
    std::string_view path, MetaProgression& progression) {
    try {
        std::ifstream input{std::string(path)};
        if (!input) return false;
        const nlohmann::json root = nlohmann::json::parse(input);
        if (!root.is_object()) return false;
        MetaProgression loaded;
        loaded.runsPlayed = nonNegative(root, "runsPlayed");
        loaded.stageClears = nonNegative(root, "stageClears");
        loaded.bestWave = nonNegative(root, "bestWave");
        loaded.enemiesDefeated = nonNegative(root, "enemiesDefeated");
        loaded.lootCollected = nonNegative(root, "lootCollected");
        loaded.resourcesGathered = nonNegative(root, "resourcesGathered");
        progression = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool saveMetaProgression(
    std::string_view path, const MetaProgression& progression) {
    try {
        const std::filesystem::path outputPath{path};
        if (outputPath.has_parent_path()) {
            std::filesystem::create_directories(outputPath.parent_path());
        }
        const nlohmann::json root{
            {"version", 1},
            {"runsPlayed", std::max(0, progression.runsPlayed)},
            {"stageClears", std::max(0, progression.stageClears)},
            {"bestWave", std::max(0, progression.bestWave)},
            {"enemiesDefeated", std::max(0, progression.enemiesDefeated)},
            {"lootCollected", std::max(0, progression.lootCollected)},
            {"resourcesGathered", std::max(0, progression.resourcesGathered)},
        };
        std::ofstream output{outputPath};
        if (!output) return false;
        output << root.dump(2) << '\n';
        return output.good();
    } catch (...) {
        return false;
    }
}

} // namespace ian
