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
    return SkillBranch::Root;
}

SkillEffect parseEffect(std::string_view value) {
    if (value == "unlock_axe") return SkillEffect::UnlockAxe;
    if (value == "unlock_pickaxe") return SkillEffect::UnlockPickaxe;
    if (value == "unlock_club") return SkillEffect::UnlockClub;
    if (value == "unlock_hammer") return SkillEffect::UnlockHammer;
    return SkillEffect::BareHands;
}

} // namespace

std::vector<SkillNodeDefinition> SkillTree::defaultDefinitions() {
    return {
        {"bare_hands", "BARE HANDS", "Gather wood and stone at 25% tool speed.",
         "placeholder_hands", SkillBranch::Root, {0, 0}, 0, {}, SkillEffect::BareHands},
        {"axe", "AXE", "Unlocks the axe and fast wood gathering.",
         "placeholder_axe", SkillBranch::Gathering, {-190, 0}, 1, {"bare_hands"}, SkillEffect::UnlockAxe},
        {"pickaxe", "PICKAXE", "Unlocks stone and crystal mining.",
         "placeholder_pickaxe", SkillBranch::Gathering, {190, 0}, 1, {"bare_hands"}, SkillEffect::UnlockPickaxe},
        {"club", "CLUB", "Unlocks a stronger melee weapon.",
         "placeholder_club", SkillBranch::Weapons, {0, -190}, 1, {"bare_hands"}, SkillEffect::UnlockClub},
        {"hammer", "HAMMER", "Unlocks repair and active fortification.",
         "placeholder_hammer", SkillBranch::Construction, {0, 190}, 1, {"bare_hands"}, SkillEffect::UnlockHammer},
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

SkillPurchaseError SkillTree::purchase(std::size_t index) {
    if (index >= nodes_.size()) return SkillPurchaseError::InvalidNode;
    if (unlocked_[index]) return SkillPurchaseError::AlreadyUnlocked;
    if (!prerequisitesUnlocked(nodes_[index])) return SkillPurchaseError::DependenciesLocked;
    if (points_ < nodes_[index].cost) return SkillPurchaseError::InsufficientPoints;
    points_ -= nodes_[index].cost;
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
    case SkillEffect::UnlockHammer: return "unlock_hammer";
    }
    return "bare_hands";
}

} // namespace ian
