#pragma once

#include "game/PlayerClass.hpp"
#include "progression/SkillTree.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>

namespace ian {

struct ProgressionCardContext {
    int playerLevel{1};
    int coreLevel{};
    int wavesSurvived{};
    PlayerClass playerClass{PlayerClass::None};
};

[[nodiscard]] constexpr bool progressionCardIsLegacyBuildingUnlock(
    std::string_view id) {
    return id == "lumber_mill" || id == "quarry" ||
        id == "crystal_mine" || id == "crossbow_unlock" ||
        id == "cannon_unlock" || id == "catapult_unlock";
}

[[nodiscard]] constexpr int progressionCardRequiredCoreLevel(
    std::string_view id) {
    if (id == "night_shift" || id == "turret_calibration" ||
        id == "rapid_battery" || id == "long_watch" ||
        id == "trap_engineer") {
        return 2;
    }
    if (id == "artillery_corps" || id == "high_ground") {
        return 3;
    }
    return 0;
}

[[nodiscard]] inline int progressionCardMinimumLevel(
    const SkillNodeDefinition& node) {
    if (node.cost >= 3) return 7;
    if (node.cost == 2) return 4;
    return 2;
}

[[nodiscard]] inline std::span<const std::string_view>
progressionCardRequiredCards(std::string_view id) {
    using Requirements = std::array<std::string_view, 2>;
    static constexpr Requirements None{};
    static constexpr Requirements PowerSwing{"power_swing"};
    static constexpr Requirements Bombs{"bombs"};
    static constexpr Requirements Club{"club"};
    static constexpr Requirements Rifle{"rifle"};
    static constexpr Requirements Ice{"ice_wand"};
    static constexpr Requirements Fire{"fire_wand"};
    static constexpr Requirements IceAndFire{"ice_wand", "fire_wand"};
    static constexpr Requirements Dash{"dash"};
    static constexpr Requirements NightlyChest{"nightly_chest"};
    if (id == "wide_swing") return {PowerSwing.data(), 1U};
    if (id == "bomb_pouch") return {Bombs.data(), 1U};
    if (id == "concussive_swings" || id == "bruiser" ||
        id == "crowd_breaker") return {Club.data(), 1U};
    if (id == "marksman" || id == "assault_rifle") {
        return {Rifle.data(), 1U};
    }
    if (id == "deep_freeze" || id == "ice_lance") {
        return {Ice.data(), 1U};
    }
    if (id == "wildfire" || id == "inferno") {
        return {Fire.data(), 1U};
    }
    if (id == "thermal_shock") return IceAndFire;
    if (id == "frequent_bounty") {
        return {NightlyChest.data(), 1U};
    }
    if (id == "impact_dash" || id == "long_dash" ||
        id == "rapid_dash") return {Dash.data(), 1U};
    return {None.data(), 0U};
}

[[nodiscard]] inline bool progressionCardRequirementsMet(
    const SkillTree& tree, const SkillNodeDefinition& node) {
    return std::ranges::all_of(
        progressionCardRequiredCards(node.id),
        [&tree](std::string_view required) {
            return tree.isUnlocked(required);
        });
}

[[nodiscard]] inline bool progressionCardEligible(
    const SkillTree& tree, std::size_t index,
    const ProgressionCardContext& context) {
    if (index >= tree.nodes().size() || tree.isUnlocked(
            tree.nodes()[index].id) || tree.isExcluded(index)) {
        return false;
    }
    const SkillNodeDefinition& node = tree.nodes()[index];
    if (node.cost <= 0 ||
        progressionCardIsLegacyBuildingUnlock(node.id) ||
        context.playerLevel < progressionCardMinimumLevel(node) ||
        context.coreLevel < node.minimumCoreLevel ||
        context.wavesSurvived < node.minimumWavesSurvived) {
        return false;
    }
    if (context.coreLevel < progressionCardRequiredCoreLevel(node.id)) {
        return false;
    }
    return progressionCardRequirementsMet(tree, node);
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
