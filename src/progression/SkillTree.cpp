#include "progression/SkillTree.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <limits>
#include <unordered_set>

namespace ian {
namespace {

SkillBranch parseBranch(std::string_view value) {
    if (value == "gathering") return SkillBranch::Gathering;
    if (value == "weapons") return SkillBranch::Weapons;
    if (value == "construction") return SkillBranch::Construction;
    if (value == "movement") return SkillBranch::Movement;
    return SkillBranch::Root;
}

SkillEffect parseEffect(std::string_view value) {
    if (value == "unlock_axe") return SkillEffect::UnlockAxe;
    if (value == "unlock_pickaxe") return SkillEffect::UnlockPickaxe;
    if (value == "unlock_club") return SkillEffect::UnlockClub;
    if (value == "unlock_ice_wand") return SkillEffect::UnlockIceWand;
    if (value == "unlock_fire_wand") return SkillEffect::UnlockFireWand;
    if (value == "unlock_hammer") return SkillEffect::UnlockHammer;
    if (value == "unlock_rifle") return SkillEffect::UnlockRifle;
    if (value == "auto_switch_tools") return SkillEffect::AutoSwitchTools;
    if (value == "hold_to_gather") return SkillEffect::HoldToGather;
    if (value == "nightly_chest") return SkillEffect::NightlyChest;
    if (value == "power_swing") return SkillEffect::PowerSwing;
    if (value == "safe_delivery") return SkillEffect::SafeDelivery;
    if (value == "field_repairs") return SkillEffect::FieldRepairs;
    if (value == "unlock_bombs") return SkillEffect::UnlockBombs;
    if (value == "light_footwork") return SkillEffect::LightFootwork;
    if (value == "dash") return SkillEffect::Dash;
    return SkillEffect::BareHands;
}

} // namespace

std::vector<SkillNodeDefinition> SkillTree::defaultDefinitions() {
    return {
        {"bare_hands", "BARE HANDS", "Gather wood and stone at 25% tool speed.",
         "placeholder_hands", SkillBranch::Root, {0, 0}, 0, {}, SkillEffect::BareHands},
        {"axe", "AXE", "Unlocks the axe and fast wood gathering.",
         "placeholder_axe", SkillBranch::Gathering, {0, 190}, 1, {"bare_hands"}, SkillEffect::UnlockAxe},
        {"pickaxe", "PICKAXE", "Unlocks stone and crystal mining.",
         "placeholder_pickaxe", SkillBranch::Gathering, {-190, 0}, 1, {"bare_hands"}, SkillEffect::UnlockPickaxe},
        {"auto_switch_tools", "SMART TOOLS",
         "Automatically switches between the axe and pickaxe for the aimed resource.",
         "placeholder_tools", SkillBranch::Gathering, {-190, 190}, 1,
         {"axe", "pickaxe"}, SkillEffect::AutoSwitchTools},
        {"hold_to_gather", "HOLD TO HARVEST",
         "Hold the attack mouse button to gather resources continuously.",
         "placeholder_tools", SkillBranch::Gathering, {-380, 190}, 1,
         {"auto_switch_tools"}, SkillEffect::HoldToGather},
        {"power_swing", "POWER SWING",
         "Every third resource hit also strikes nearby resources.",
         "placeholder_tools", SkillBranch::Gathering, {-570, 190}, 1,
         {"hold_to_gather"}, SkillEffect::PowerSwing},
        {"club", "CLUB", "Unlocks a stronger melee weapon.",
         "placeholder_club", SkillBranch::Weapons, {0, -190}, 1, {"bare_hands"}, SkillEffect::UnlockClub},
        {"ice_wand", "ICE WAND",
         "Launch a freezing orb that freezes enemies near the impact.",
         "ice_wand", SkillBranch::Weapons, {190, -190}, 2,
         {"bare_hands"}, SkillEffect::UnlockIceWand},
        {"fire_wand", "FIRE WAND",
         "Launch a fire orb that ignites enemies near the impact.",
         "fire_wand", SkillBranch::Weapons, {190, -380}, 2,
         {"ice_wand"}, SkillEffect::UnlockFireWand},
        {"rifle", "RIFLE", "Unlocks the rifle and ranged combat.",
         "placeholder_rifle", SkillBranch::Weapons, {0, -380}, 1, {"club"}, SkillEffect::UnlockRifle},
        {"bombs", "BOMBS",
         "Unlocks throwable bombs for explosive crowd control.",
         "placeholder_bomb", SkillBranch::Weapons, {-190, -380}, 1,
         {"club"}, SkillEffect::UnlockBombs},
        {"hammer", "HAMMER", "Unlocks repair and active fortification.",
         "placeholder_hammer", SkillBranch::Construction, {190, 0}, 1, {"bare_hands"}, SkillEffect::UnlockHammer},
        {"field_repairs", "FIELD REPAIRS",
         "Restores 15% health to all surviving structures after each night.",
         "placeholder_hammer", SkillBranch::Construction, {380, 0}, 1,
         {"hammer"}, SkillEffect::FieldRepairs},
        {"nightly_chest", "NIGHT'S BOUNTY",
         "Spawns one additional chest after every successfully survived night.",
         "placeholder_chest", SkillBranch::Construction, {190, 190}, 1,
         {"bare_hands"}, SkillEffect::NightlyChest},
        {"safe_delivery", "SAFE DELIVERY",
         "Night's Bounty chests spawn closer to the base.",
         "placeholder_chest", SkillBranch::Construction, {380, 190}, 1,
         {"nightly_chest"}, SkillEffect::SafeDelivery},
        {"light_footwork", "LIGHT FOOTWORK",
         "Accelerate, stop, and change direction faster without increasing top speed.",
         "placeholder_boot", SkillBranch::Movement, {380, -190}, 1,
         {"bare_hands"}, SkillEffect::LightFootwork},
        {"dash", "DASH",
         "Press Right Mouse Button to burst in the movement direction. One charge, recovered over time.",
         "placeholder_dash", SkillBranch::Movement, {570, -190}, 2,
         {"light_footwork"}, SkillEffect::Dash, SkillNodeSize::Large},
    };
}

SkillTree::SkillTree() : SkillTree(defaultDefinitions()) {}

SkillTree::SkillTree(std::vector<SkillNodeDefinition> nodes)
    : nodes_(nodes.empty() ? defaultDefinitions() : std::move(nodes)),
      unlocked_(nodes_.size(), false) {
    reset();
}

const std::vector<SkillNodeDefinition>& SkillTree::nodes() const { return nodes_; }

SkillNodeState SkillTree::state(std::size_t index) const {
    if (index >= nodes_.size()) return SkillNodeState::Hidden;
    if (unlocked_[index]) return SkillNodeState::Unlocked;
    return prerequisitesUnlocked(nodes_[index]) ? SkillNodeState::Available
                                                 : SkillNodeState::Locked;
}

SkillNodeState SkillTree::state(std::string_view id) const {
    const auto index = indexOf(id);
    return index ? state(*index) : SkillNodeState::Hidden;
}

SkillPurchaseError SkillTree::purchase(
    std::size_t index, bool spendPoints) {
    if (index >= nodes_.size()) return SkillPurchaseError::InvalidNode;
    if (unlocked_[index]) return SkillPurchaseError::AlreadyUnlocked;
    if (!prerequisitesUnlocked(nodes_[index])) return SkillPurchaseError::DependenciesLocked;
    if (spendPoints && points_ < nodes_[index].cost)
        return SkillPurchaseError::InsufficientPoints;
    if (spendPoints) points_ -= nodes_[index].cost;
    unlocked_[index] = true;
    return SkillPurchaseError::None;
}

bool SkillTree::unlock(std::size_t index) {
    return purchase(index) == SkillPurchaseError::None;
}

void SkillTree::grantPoints(int amount) {
    if (amount <= 0) return;
    const int room = std::numeric_limits<int>::max() - points_;
    points_ += std::min(room, amount);
}

int SkillTree::points() const { return points_; }

bool SkillTree::isUnlocked(std::string_view id) const {
    const auto index = indexOf(id);
    return index && unlocked_[*index];
}

bool SkillTree::hasEffect(SkillEffect effect) const {
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (unlocked_[i] && nodes_[i].effect == effect) return true;
    }
    return false;
}

std::optional<std::size_t> SkillTree::indexOf(std::string_view id) const {
    const auto found = std::ranges::find(nodes_, id, &SkillNodeDefinition::id);
    if (found == nodes_.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(nodes_.begin(), found));
}

std::vector<std::size_t> SkillTree::childrenOf(std::string_view id) const {
    std::vector<std::size_t> children;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (std::ranges::find(nodes_[i].prerequisites, id) != nodes_[i].prerequisites.end())
            children.push_back(i);
    }
    return children;
}

int SkillTree::unlockedCount() const {
    return static_cast<int>(std::ranges::count(unlocked_, true));
}

SkillTreeRunState SkillTree::saveState() const {
    SkillTreeRunState result{.points = points_};
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        if (unlocked_[i]) result.unlockedNodeIds.push_back(nodes_[i].id);
    return result;
}

bool SkillTree::loadState(const SkillTreeRunState& stateToLoad) {
    if (stateToLoad.points < 0) return false;
    std::vector<bool> loaded(nodes_.size(), false);
    for (const std::string& id : stateToLoad.unlockedNodeIds) {
        const auto index = indexOf(id);
        if (!index) return false;
        loaded[*index] = true;
    }
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].cost == 0) loaded[i] = true;
        if (!loaded[i]) continue;
        for (const auto& prerequisite : nodes_[i].prerequisites) {
            const auto parent = indexOf(prerequisite);
            if (!parent || !loaded[*parent]) return false;
        }
    }
    unlocked_ = std::move(loaded);
    points_ = stateToLoad.points;
    return true;
}

void SkillTree::reset() {
    std::fill(unlocked_.begin(), unlocked_.end(), false);
    points_ = 0;
    for (std::size_t i = 0; i < nodes_.size(); ++i)
        if (nodes_[i].cost == 0 && nodes_[i].prerequisites.empty()) unlocked_[i] = true;
}

bool SkillTree::prerequisitesUnlocked(const SkillNodeDefinition& node) const {
    return std::ranges::all_of(node.prerequisites, [this](const std::string& id) {
        return isUnlocked(id);
    });
}

std::vector<SkillNodeDefinition> loadSkillTreeDefinitions(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return SkillTree::defaultDefinitions();
    try {
        const nlohmann::json data = nlohmann::json::parse(input);
        std::vector<SkillNodeDefinition> nodes;
        std::unordered_set<std::string> ids;
        for (const auto& value : data.at("nodes")) {
            SkillNodeDefinition node;
            node.id = value.at("id").get<std::string>();
            node.title = value.at("title").get<std::string>();
            node.description = value.at("description").get<std::string>();
            node.icon = value.at("icon").get<std::string>();
            node.cost = value.at("cost").get<int>();
            node.prerequisites = value.at("dependencies").get<std::vector<std::string>>();
            node.position = {value.at("position").at("x").get<float>(),
                             value.at("position").at("y").get<float>()};
            node.branch = parseBranch(value.value("branch", "root"));
            node.effect = parseEffect(value.at("effect").get<std::string>());
            node.size = value.value("size", "small") == "large"
                ? SkillNodeSize::Large
                : SkillNodeSize::Small;
            if (node.id.empty() || node.cost < 0 || !ids.insert(node.id).second) return SkillTree::defaultDefinitions();
            nodes.push_back(std::move(node));
        }
        return nodes.empty() ? SkillTree::defaultDefinitions() : nodes;
    } catch (...) {
        return SkillTree::defaultDefinitions();
    }
}

std::string_view skillBranchName(SkillBranch branch) {
    switch (branch) {
    case SkillBranch::Gathering: return "GATHERING";
    case SkillBranch::Weapons: return "WEAPONS";
    case SkillBranch::Construction: return "CONSTRUCTION";
    case SkillBranch::Movement: return "MOVEMENT";
    case SkillBranch::Root: return "ORIGIN";
    }
    return "ORIGIN";
}

std::string_view skillEffectName(SkillEffect effect) {
    switch (effect) {
    case SkillEffect::BareHands: return "bare_hands";
    case SkillEffect::UnlockAxe: return "unlock_axe";
    case SkillEffect::UnlockPickaxe: return "unlock_pickaxe";
    case SkillEffect::UnlockClub: return "unlock_club";
    case SkillEffect::UnlockIceWand: return "unlock_ice_wand";
    case SkillEffect::UnlockFireWand: return "unlock_fire_wand";
    case SkillEffect::UnlockHammer: return "unlock_hammer";
    case SkillEffect::UnlockRifle: return "unlock_rifle";
    case SkillEffect::AutoSwitchTools: return "auto_switch_tools";
    case SkillEffect::HoldToGather: return "hold_to_gather";
    case SkillEffect::NightlyChest: return "nightly_chest";
    case SkillEffect::PowerSwing: return "power_swing";
    case SkillEffect::SafeDelivery: return "safe_delivery";
    case SkillEffect::FieldRepairs: return "field_repairs";
    case SkillEffect::UnlockBombs: return "unlock_bombs";
    case SkillEffect::LightFootwork: return "light_footwork";
    case SkillEffect::Dash: return "dash";
    }
    return "bare_hands";
}

} // namespace ian
