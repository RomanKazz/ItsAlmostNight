#include "progression/SkillTree.hpp"

#include <algorithm>

namespace ian {
namespace {

std::vector<SkillNodeDefinition> defaultNodes() {
    return {
        {"core", "LIVING CORE",
         "The origin of every future specialization.",
         SkillBranch::Root, {0.0F, 0.0F}, 0, {}},

        {"construction", "BLUEPRINTS",
         "Opens the construction research branch.",
         SkillBranch::Construction, {-150.0F, 0.0F}, 1,
         {"core"}},
        {"verticality", "VERTICALITY",
         "Future upgrades for platforms and ramps.",
         SkillBranch::Construction, {-300.0F, -75.0F}, 1,
         {"construction"}},
        {"fortification", "FORTIFICATION",
         "Future upgrades for walls and foundations.",
         SkillBranch::Construction, {-300.0F, 75.0F}, 1,
         {"construction"}},
        {"automation", "AUTOMATION",
         "Future production and logistics structures.",
         SkillBranch::Construction, {-450.0F, 0.0F}, 2,
         {"verticality", "fortification"}},

        {"defenses", "DEFENSES",
         "Opens the defensive structure branch.",
         SkillBranch::Defenses, {0.0F, -150.0F}, 1,
         {"core"}},
        {"ballistics", "BALLISTICS",
         "Future turret range and projectile upgrades.",
         SkillBranch::Defenses, {-75.0F, -300.0F}, 1,
         {"defenses"}},
        {"traps", "FIELD CONTROL",
         "Future traps and enemy control tools.",
         SkillBranch::Defenses, {75.0F, -300.0F}, 1,
         {"defenses"}},
        {"artillery", "ARTILLERY",
         "Future heavy defensive machinery.",
         SkillBranch::Defenses, {0.0F, -450.0F}, 2,
         {"ballistics", "traps"}},

        {"weapons", "ARMORY",
         "Opens the personal weapon branch.",
         SkillBranch::Weapons, {150.0F, 0.0F}, 1,
         {"core"}},
        {"rifle", "RIFLECRAFT",
         "Future handling, damage and magazine upgrades.",
         SkillBranch::Weapons, {300.0F, -75.0F}, 1,
         {"weapons"}},
        {"ordnance", "ORDNANCE",
         "Future explosives and alternate ammunition.",
         SkillBranch::Weapons, {300.0F, 75.0F}, 1,
         {"weapons"}},
        {"arsenal", "MASTER ARSENAL",
         "Future capstone for advanced weapon systems.",
         SkillBranch::Weapons, {450.0F, 0.0F}, 2,
         {"rifle", "ordnance"}},

        {"survival", "SURVIVAL",
         "Opens the endurance and gathering branch.",
         SkillBranch::Survival, {0.0F, 150.0F}, 1,
         {"core"}},
        {"scavenging", "SCAVENGING",
         "Future harvesting and carrying upgrades.",
         SkillBranch::Survival, {-75.0F, 300.0F}, 1,
         {"survival"}},
        {"resilience", "RESILIENCE",
         "Future movement and recovery upgrades.",
         SkillBranch::Survival, {75.0F, 300.0F}, 1,
         {"survival"}},
        {"pioneer", "PIONEER",
         "Future capstone for surviving endless nights.",
         SkillBranch::Survival, {0.0F, 450.0F}, 2,
         {"scavenging", "resilience"}},
    };
}

} // namespace

SkillTree::SkillTree()
    : nodes_(defaultNodes()), unlocked_(nodes_.size(), false) {
    reset();
}

const std::vector<SkillNodeDefinition>& SkillTree::nodes() const {
    return nodes_;
}

SkillNodeState SkillTree::state(std::size_t index) const {
    if (index >= nodes_.size()) {
        return SkillNodeState::Hidden;
    }
    if (unlocked_[index]) {
        return SkillNodeState::Unlocked;
    }
    if (prerequisitesUnlocked(nodes_[index])) {
        return SkillNodeState::Available;
    }
    return shouldBeVisible(nodes_[index])
               ? SkillNodeState::Locked
               : SkillNodeState::Hidden;
}

SkillNodeState SkillTree::state(std::string_view id) const {
    const auto index = indexOf(id);
    return index ? state(*index) : SkillNodeState::Hidden;
}

bool SkillTree::unlock(std::size_t index) {
    if (state(index) != SkillNodeState::Available) {
        return false;
    }
    unlocked_[index] = true;
    return true;
}

bool SkillTree::isUnlocked(std::string_view id) const {
    const auto index = indexOf(id);
    return index && unlocked_[*index];
}

std::optional<std::size_t> SkillTree::indexOf(
    std::string_view id) const {
    const auto found = std::find_if(
        nodes_.begin(), nodes_.end(),
        [id](const SkillNodeDefinition& node) {
            return node.id == id;
        });
    if (found == nodes_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(
        std::distance(nodes_.begin(), found));
}

std::vector<std::size_t> SkillTree::childrenOf(
    std::string_view id) const {
    std::vector<std::size_t> children;
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        const auto& prerequisites = nodes_[index].prerequisites;
        if (std::ranges::find(prerequisites, id) !=
            prerequisites.end()) {
            children.push_back(index);
        }
    }
    return children;
}

int SkillTree::unlockedCount() const {
    return static_cast<int>(std::ranges::count(unlocked_, true));
}

void SkillTree::reset() {
    std::fill(unlocked_.begin(), unlocked_.end(), false);
    if (!nodes_.empty()) {
        unlocked_.front() = true;
    }
}

bool SkillTree::prerequisitesUnlocked(
    const SkillNodeDefinition& node) const {
    return std::ranges::all_of(
        node.prerequisites,
        [this](const std::string& prerequisite) {
            return isUnlocked(prerequisite);
        });
}

bool SkillTree::shouldBeVisible(
    const SkillNodeDefinition& node) const {
    return std::ranges::any_of(
        node.prerequisites,
        [this](const std::string& prerequisite) {
            return isUnlocked(prerequisite);
        });
}

std::string_view skillBranchName(SkillBranch branch) {
    switch (branch) {
    case SkillBranch::Construction:
        return "CONSTRUCTION";
    case SkillBranch::Defenses:
        return "DEFENSES";
    case SkillBranch::Weapons:
        return "WEAPONS";
    case SkillBranch::Survival:
        return "SURVIVAL";
    case SkillBranch::Root:
        return "ORIGIN";
    }
    return "ORIGIN";
}

} // namespace ian
