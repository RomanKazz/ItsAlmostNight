#pragma once

#include "game/PlayerClass.hpp"
#include "progression/SkillTree.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string_view>

namespace ian {

struct ProgressionCardContext {
    int playerLevel{1};
    int coreLevel{};
    int wavesSurvived{};
    PlayerClass playerClass{PlayerClass::None};
};

[[nodiscard]] constexpr bool progressionCardIsDeterministicUnlock(
    std::string_view id) {
    return id == "lumber_mill" || id == "quarry" ||
        id == "crystal_mine" || id == "crossbow_unlock" ||
        id == "cannon_unlock" || id == "catapult_unlock" ||
        id == "club" || id == "rifle" || id == "bombs" ||
        id == "ice_wand" || id == "fire_wand";
}

[[nodiscard]] constexpr bool progressionCardIsRetired(
    std::string_view id) {
    return id == "combat_training" || id == "power_swing" ||
        id == "reinforced_frames" || id == "defense_engineering" ||
        id == "sprinter" || id == "dash" ||
        id == "frequent_bounty" || id == "safe_delivery";
}

[[nodiscard]] constexpr bool progressionCardIsHidden(
    std::string_view id) {
    return progressionCardIsDeterministicUnlock(id) ||
        progressionCardIsRetired(id);
}

[[nodiscard]] constexpr int progressionCardRequiredCoreLevel(
    std::string_view id) {
    if (id == "concussive_swings" || id == "bruiser" ||
        id == "crowd_breaker") {
        return 1;
    }
    if (id == "night_shift" || id == "turret_calibration" ||
        id == "rapid_battery" || id == "long_watch" ||
        id == "trap_engineer" || id == "marksman" ||
        id == "assault_rifle") {
        return 2;
    }
    if (id == "artillery_corps" || id == "high_ground" ||
        id == "bomb_pouch") {
        return 3;
    }
    if (id == "deep_freeze" || id == "ice_lance") return 4;
    if (id == "wildfire" || id == "inferno" ||
        id == "thermal_shock") {
        return 5;
    }
    return 0;
}

[[nodiscard]] inline int progressionCardMinimumLevel(
    const SkillNodeDefinition& node) {
    if (node.cost >= 3) return 7;
    if (node.cost == 2) return 4;
    return 2;
}

[[nodiscard]] inline bool progressionCardEligible(
    const SkillTree& tree, std::size_t index,
    const ProgressionCardContext& context) {
    if (index >= tree.nodes().size() || tree.isMaxLevel(index) ||
        tree.isExcluded(index)) {
        return false;
    }
    const SkillNodeDefinition& node = tree.nodes()[index];
    if (node.cost <= 0 ||
        progressionCardIsHidden(node.id) ||
        context.playerLevel < progressionCardMinimumLevel(node) ||
        context.coreLevel < node.minimumCoreLevel ||
        context.wavesSurvived < node.minimumWavesSurvived) {
        return false;
    }
    return context.coreLevel >=
        progressionCardRequiredCoreLevel(node.id);
}

[[nodiscard]] inline int progressionCardClassWeight(
    SkillBranch branch, PlayerClass playerClass) {
    switch (playerClass) {
    case PlayerClass::Engineer:
        return branch == SkillBranch::Construction ? 7 : 1;
    case PlayerClass::Prospector:
        return branch == SkillBranch::Gathering ? 5
            : branch == SkillBranch::Construction ? 3
            : branch == SkillBranch::Economy ? 2 : 1;
    case PlayerClass::Ranger:
        return branch == SkillBranch::Weapons ? 4
            : branch == SkillBranch::Movement ? 3
            : branch == SkillBranch::Construction ? 3 : 1;
    case PlayerClass::Vanguard:
        return branch == SkillBranch::Construction ? 5
            : branch == SkillBranch::Weapons ? 3 : 1;
    case PlayerClass::Berserker:
    case PlayerClass::Vampire:
        return branch == SkillBranch::Weapons ? 5
            : branch == SkillBranch::Construction ? 2
            : branch == SkillBranch::Movement ? 2 : 1;
    case PlayerClass::Alchemist:
        return branch == SkillBranch::Economy ? 4
            : branch == SkillBranch::Weapons ? 3
            : branch == SkillBranch::Construction ? 2 : 1;
    case PlayerClass::Chronomancer:
        return branch == SkillBranch::Movement ? 4
            : branch == SkillBranch::Economy ? 3
            : branch == SkillBranch::Construction ? 2 : 1;
    case PlayerClass::None:
        return branch == SkillBranch::Construction ? 3
            : branch == SkillBranch::Economy ? 2 : 1;
    }
    return 1;
}

} // namespace ian
