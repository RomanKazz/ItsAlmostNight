#include "progression/SkillTree.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
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
    if (value == "economy") return SkillBranch::Economy;
    return SkillBranch::Root;
}

} // namespace

std::vector<SkillNodeDefinition> SkillTree::defaultDefinitions() {
    const auto make = [](
        std::string id, std::string title, std::string description,
        std::string icon, SkillBranch branch, SkillTreePoint position,
        int cost, std::vector<std::string> dependencies,
        std::vector<SkillEffectDefinition> effects = {},
        std::string exclusiveGroup = {},
        SkillNodeSize size = SkillNodeSize::Small) {
        return SkillNodeDefinition{
            .id = std::move(id), .title = std::move(title),
            .description = std::move(description), .icon = std::move(icon),
            .branch = branch, .position = position, .cost = cost,
            .prerequisites = std::move(dependencies),
            .effects = std::move(effects),
            .exclusiveGroup = std::move(exclusiveGroup), .size = size};
    };
    using E = SkillEffectDefinition;
    return {
        make("bare_hands", "SURVIVOR", "The origin of every run.", "placeholder_hands", SkillBranch::Root, {0, 0}, 0, {}),

        make("axe", "AXE", "Unlock the axe for efficient wood gathering.", "placeholder_axe", SkillBranch::Gathering, {-190, 0}, 1, {"bare_hands"}, {E{"unlock.axe"}}),
        make("pickaxe", "PICKAXE", "Unlock the pickaxe for efficient stone gathering.", "placeholder_pickaxe", SkillBranch::Gathering, {-190, 190}, 1, {"bare_hands"}, {E{"unlock.pickaxe"}}),
        make("efficient_strikes", "EFFICIENT STRIKES", "Tools deal 25% more damage to resources.", "placeholder_tools", SkillBranch::Gathering, {-380, 0}, 1, {"axe", "pickaxe"}, {E{"gather.damage", 0.25}}),
        make("power_swing", "POWER SWING", "Every third resource hit also strikes nearby deposits.", "placeholder_tools", SkillBranch::Gathering, {-570, 0}, 2, {"efficient_strikes"}, {E{"gather.power_swing"}}, {}, SkillNodeSize::Large),
        make("wide_swing", "WIDE SWING", "Power Swing reaches 50% farther.", "placeholder_tools", SkillBranch::Gathering, {-760, 0}, 1, {"power_swing"}, {E{"gather.power_swing_radius", 0.5}}),
        make("hands_on", "HANDS-ON", "Manual gathering is 45% stronger, but producers work 20% slower.", "placeholder_hands", SkillBranch::Gathering, {-570, 190}, 2, {"efficient_strikes"}, {E{"gather.damage", 0.45}, E{"production.speed", -0.20}}, "gathering_doctrine", SkillNodeSize::Large),
        make("industrialist", "INDUSTRIALIST", "Producers work 40% faster, but manual gathering is 20% weaker.", "placeholder_hammer", SkillBranch::Gathering, {-570, 380}, 2, {"efficient_strikes"}, {E{"production.speed", 0.40}, E{"gather.damage", -0.20}}, "gathering_doctrine", SkillNodeSize::Large),

        make("combat_training", "COMBAT TRAINING", "All player attacks deal 10% more damage.", "placeholder_club", SkillBranch::Weapons, {0, -190}, 1, {"bare_hands"}, {E{"player.damage", 0.10}}),
        make("club", "CLUB", "Unlock the club and its sweeping melee attacks.", "placeholder_club", SkillBranch::Weapons, {-380, -380}, 1, {"bare_hands"}, {E{"unlock.club"}}),
        make("rifle", "RIFLE", "Unlock accurate ranged combat.", "placeholder_rifle", SkillBranch::Weapons, {-190, -380}, 1, {"bare_hands"}, {E{"unlock.rifle"}}),
        make("ice_wand", "ICE WAND", "Unlock freezing projectiles and area control.", "ice_wand", SkillBranch::Weapons, {0, -380}, 2, {"bare_hands"}, {E{"unlock.ice_wand"}}),
        make("fire_wand", "FIRE WAND", "Unlock burning projectiles and damage over time.", "fire_wand", SkillBranch::Weapons, {190, -380}, 2, {"bare_hands"}, {E{"unlock.fire_wand"}}),
        make("bombs", "BOMBS", "Unlock throwable explosive crowd control.", "placeholder_bomb", SkillBranch::Weapons, {380, -380}, 1, {"bare_hands"}, {E{"unlock.bombs"}}),
        make("bruiser", "BRUISER", "Club damage and knockback increase greatly, but its area shrinks.", "placeholder_club", SkillBranch::Weapons, {-570, -570}, 2, {"club"}, {E{"club.damage", 0.55}, E{"club.knockback", 0.50}, E{"club.area", -0.20}}, "club_style", SkillNodeSize::Large),
        make("crowd_breaker", "CROWD BREAKER", "Club swings cover a much wider area, but deal less damage.", "placeholder_club", SkillBranch::Weapons, {-380, -570}, 2, {"club"}, {E{"club.area", 0.55}, E{"club.damage", -0.20}}, "club_style", SkillNodeSize::Large),
        make("marksman", "MARKSMAN", "Rifle shots gain 70% damage and 35% range, but fire 30% slower.", "placeholder_rifle", SkillBranch::Weapons, {-190, -570}, 3, {"rifle"}, {E{"rifle.damage", 0.70}, E{"rifle.range", 0.35}, E{"rifle.fire_rate", -0.30}}, "rifle_style", SkillNodeSize::Large),
        make("assault_rifle", "ASSAULT DRILL", "Rifle fires 50% faster with a larger magazine, but loses damage.", "placeholder_rifle", SkillBranch::Weapons, {-190, -760}, 3, {"rifle"}, {E{"rifle.damage", -0.20}, E{"rifle.fire_rate", 0.50}, E{"rifle.magazine", 4.0}}, "rifle_style", SkillNodeSize::Large),
        make("deep_freeze", "DEEP FREEZE", "Ice covers a wider area and freezes longer, but deals less damage.", "ice_wand", SkillBranch::Weapons, {0, -570}, 3, {"ice_wand"}, {E{"ice.radius", 0.35}, E{"ice.freeze_duration", 0.55}, E{"ice.damage", -0.20}}, "ice_style", SkillNodeSize::Large),
        make("ice_lance", "ICE LANCE", "Ice projectiles deal heavy direct damage but have a smaller blast.", "ice_wand", SkillBranch::Weapons, {0, -760}, 3, {"ice_wand"}, {E{"ice.damage", 0.65}, E{"ice.radius", -0.25}}, "ice_style", SkillNodeSize::Large),
        make("wildfire", "WILDFIRE", "Fire spreads through a larger area and burns longer, with weaker impact.", "fire_wand", SkillBranch::Weapons, {190, -570}, 3, {"fire_wand"}, {E{"fire.radius", 0.40}, E{"fire.burn_duration", 0.50}, E{"fire.damage", -0.20}}, "fire_style", SkillNodeSize::Large),
        make("inferno", "INFERNO", "Fire impact and burn damage increase, but the blast becomes smaller.", "fire_wand", SkillBranch::Weapons, {190, -760}, 3, {"fire_wand"}, {E{"fire.damage", 0.45}, E{"fire.burn_damage", 0.45}, E{"fire.radius", -0.20}}, "fire_style", SkillNodeSize::Large),
        make("thermal_shock", "THERMAL SHOCK", "Fire striking a frozen enemy consumes Freeze for a violent burst.", "fire_wand", SkillBranch::Weapons, {380, -760}, 3, {"ice_wand", "fire_wand"}, {E{"element.thermal_shock", 18.0}}, {}, SkillNodeSize::Large),

        make("hammer", "HAMMER", "Unlock repair and active fortification.", "placeholder_hammer", SkillBranch::Construction, {190, 0}, 1, {"bare_hands"}, {E{"unlock.hammer"}}),
        make("reinforced_frames", "REINFORCED FRAMES", "All structures gain 20% maximum health.", "placeholder_hammer", SkillBranch::Construction, {380, 0}, 1, {"hammer"}, {E{"building.health", 0.20}}),
        make("field_repairs", "FIELD REPAIRS", "Surviving structures recover 18% health after every night.", "placeholder_hammer", SkillBranch::Construction, {570, 0}, 2, {"reinforced_frames"}, {E{"wave.repair_fraction", 0.18}}, {}, SkillNodeSize::Large),
        make("defense_engineering", "DEFENSE ENGINEERING", "Turrets, cannons and spike traps deal 12% more damage.", "placeholder_hammer", SkillBranch::Construction, {380, 190}, 1, {"hammer"}, {E{"defense.damage", 0.12}}),
        make("turret_calibration", "TURRET CALIBRATION", "Turrets gain 20% range and damage.", "placeholder_rifle", SkillBranch::Construction, {570, 190}, 1, {"defense_engineering"}, {E{"tower.range", 0.20}, E{"tower.damage", 0.20}}),
        make("rapid_battery", "RAPID BATTERY", "Turrets fire 45% faster but lose 20% range.", "placeholder_rifle", SkillBranch::Construction, {760, 190}, 3, {"turret_calibration"}, {E{"tower.fire_rate", 0.45}, E{"tower.range", -0.20}}, "turret_doctrine", SkillNodeSize::Large),
        make("long_watch", "LONG WATCH", "Turrets gain 45% range and 30% damage, but fire slower.", "placeholder_rifle", SkillBranch::Construction, {760, 380}, 3, {"turret_calibration"}, {E{"tower.range", 0.45}, E{"tower.damage", 0.30}, E{"tower.fire_rate", -0.25}}, "turret_doctrine", SkillNodeSize::Large),
        make("artillery_corps", "ARTILLERY CORPS", "Cannons gain 30% blast radius and damage.", "placeholder_bomb", SkillBranch::Construction, {570, 380}, 2, {"defense_engineering"}, {E{"cannon.radius", 0.30}, E{"cannon.damage", 0.30}}),
        make("trap_engineer", "TRAP ENGINEER", "Traps gain radius, damage and faster recovery.", "placeholder_tools", SkillBranch::Construction, {380, 380}, 2, {"defense_engineering"}, {E{"trap.radius", 0.25}, E{"trap.damage", 0.30}, E{"trap.fire_rate", 0.25}}),
        make("high_ground", "HIGH GROUND", "Defenses built above terrain deal 35% more damage.", "placeholder_hammer", SkillBranch::Construction, {570, 570}, 3, {"artillery_corps", "trap_engineer"}, {E{"defense.high_ground_damage", 0.35}}, {}, SkillNodeSize::Large),

        make("light_footwork", "LIGHT FOOTWORK", "Accelerate, stop and turn 55% faster.", "placeholder_boot", SkillBranch::Movement, {570, -190}, 1, {"bare_hands"}, {E{"player.acceleration", 0.55}}),
        make("sprinter", "SPRINTER", "Move 10% faster at all times.", "placeholder_boot", SkillBranch::Movement, {760, -190}, 1, {"light_footwork"}, {E{"player.move_speed", 0.10}}),
        make("dash", "DASH", "Burst in the movement direction.", "placeholder_dash", SkillBranch::Movement, {950, -190}, 2, {"light_footwork"}, {E{"dash.unlock"}}, {}, SkillNodeSize::Large),
        make("impact_dash", "IMPACT DASH", "Starting a dash damages and knocks back nearby enemies.", "placeholder_dash", SkillBranch::Movement, {1140, -190}, 2, {"dash"}, {E{"dash.impact_damage", 10.0}}, {}, SkillNodeSize::Large),
        make("long_dash", "LONG DASH", "Dash travels farther and faster, but recovers 35% slower.", "placeholder_dash", SkillBranch::Movement, {1140, 0}, 2, {"dash"}, {E{"dash.speed", 0.30}, E{"dash.cooldown", -0.35}}, "dash_style", SkillNodeSize::Large),
        make("rapid_dash", "RAPID DASH", "Dash recovers 45% faster, but travels a shorter distance.", "placeholder_dash", SkillBranch::Movement, {1140, -380}, 2, {"dash"}, {E{"dash.speed", -0.20}, E{"dash.cooldown", 0.45}}, "dash_style", SkillNodeSize::Large),

        make("nightly_chest", "NIGHT'S BOUNTY", "Spawn an additional chest after every survived night.", "placeholder_chest", SkillBranch::Economy, {0, 190}, 2, {"bare_hands"}, {E{"loot.nightly_chests", 1.0}}, {}, SkillNodeSize::Large),
        make("safe_delivery", "SAFE DELIVERY", "Reward chests arrive closer to the base.", "placeholder_chest", SkillBranch::Economy, {0, 380}, 1, {"nightly_chest"}, {E{"loot.safe_delivery", 1.0}}),
        make("keymaster", "KEYMASTER", "All chests cost 20% less gold to open.", "placeholder_key", SkillBranch::Economy, {190, 380}, 1, {"nightly_chest"}, {E{"loot.chest_cost", -0.20}}),
        make("early_planning", "EARLY PLANNING", "Early wave starts grant Gold and Insight; all early rewards increase 25%.", "placeholder_hourglass", SkillBranch::Economy, {0, 570}, 1, {"safe_delivery"}, {E{"early.base_bonus", 0.25}}),
        make("mercenary_contract", "MERCENARY CONTRACT", "Early starts grant much more Gold, but no bonus Insight.", "placeholder_coin", SkillBranch::Economy, {-190, 760}, 3, {"early_planning"}, {E{"early.gold", 1.0}, E{"early.insight", -1.0}}, "early_contract", SkillNodeSize::Large),
        make("scholar_contract", "SCHOLAR CONTRACT", "Early starts grant much more Insight, but no bonus Gold.", "placeholder_blueprint", SkillBranch::Economy, {190, 760}, 3, {"early_planning"}, {E{"early.gold", -1.0}, E{"early.insight", 1.0}}, "early_contract", SkillNodeSize::Large),
        make("expanded_storage", "EXPANDED STORAGE", "Every storage building holds 40% more resources.", "placeholder_crate", SkillBranch::Economy, {380, 570}, 2, {"keymaster"}, {E{"storage.capacity", 0.40}}),
        make("scavenger", "SCAVENGER", "Destructible world props drop 50% more coins.", "placeholder_crate", SkillBranch::Economy, {-380, 570}, 2, {"safe_delivery"}, {E{"prop.coins", 0.50}}),
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
    if (conflictsWithUnlocked(nodes_[index])) return SkillNodeState::Locked;
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
    if (conflictsWithUnlocked(nodes_[index]))
        return SkillPurchaseError::MutuallyExclusive;
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

bool SkillTree::isExcluded(std::size_t index) const {
    return index < nodes_.size() && !unlocked_[index] &&
        conflictsWithUnlocked(nodes_[index]);
}

bool SkillTree::hasEffect(std::string_view key) const {
    return effectValue(key) != 0.0;
}

double SkillTree::effectValue(std::string_view key) const {
    double result = 0.0;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (!unlocked_[i]) continue;
        for (const SkillEffectDefinition& effect : nodes_[i].effects) {
            if (effect.key == key) result += effect.value;
        }
    }
    return result;
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
        if (!index) {
            if (id == "auto_switch_tools") {
                continue;
            }
            if (id == "hold_to_gather") {
                const auto replacement = indexOf("efficient_strikes");
                if (!replacement) return false;
                loaded[*replacement] = true;
                continue;
            }
            return false;
        }
        loaded[*index] = true;
    }
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].cost == 0) loaded[i] = true;
        if (!loaded[i]) continue;
        for (const auto& prerequisite : nodes_[i].prerequisites) {
            const auto parent = indexOf(prerequisite);
            if (!parent || !loaded[*parent]) return false;
        }
        if (!nodes_[i].exclusiveGroup.empty()) {
            for (std::size_t other = i + 1; other < nodes_.size(); ++other) {
                if (loaded[other] &&
                    nodes_[other].exclusiveGroup == nodes_[i].exclusiveGroup) {
                    return false;
                }
            }
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

bool SkillTree::conflictsWithUnlocked(
    const SkillNodeDefinition& node) const {
    if (node.exclusiveGroup.empty()) return false;
    for (std::size_t index = 0; index < nodes_.size(); ++index) {
        if (unlocked_[index] &&
            nodes_[index].exclusiveGroup == node.exclusiveGroup &&
            nodes_[index].id != node.id) {
            return true;
        }
    }
    return false;
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
            if (value.contains("effects")) {
                for (const auto& effect : value.at("effects")) {
                    SkillEffectDefinition definition;
                    if (effect.is_string()) {
                        definition.key = effect.get<std::string>();
                    } else {
                        definition.key = effect.at("key").get<std::string>();
                        definition.value = effect.value("value", 1.0);
                    }
                    if (definition.key.empty() ||
                        !std::isfinite(definition.value)) {
                        return SkillTree::defaultDefinitions();
                    }
                    node.effects.push_back(std::move(definition));
                }
            }
            node.exclusiveGroup = value.value("exclusiveGroup", "");
            node.size = value.value("size", "small") == "large"
                ? SkillNodeSize::Large
                : SkillNodeSize::Small;
            if (node.id.empty() || node.cost < 0 ||
                !std::isfinite(node.position.x) ||
                !std::isfinite(node.position.y) ||
                !ids.insert(node.id).second) {
                return SkillTree::defaultDefinitions();
            }
            nodes.push_back(std::move(node));
        }
        for (const SkillNodeDefinition& node : nodes) {
            if (std::ranges::any_of(
                    node.prerequisites,
                    [&ids](const std::string& dependency) {
                        return !ids.contains(dependency);
                    })) {
                return SkillTree::defaultDefinitions();
            }
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
    case SkillBranch::Economy: return "ECONOMY";
    case SkillBranch::Root: return "ORIGIN";
    }
    return "ORIGIN";
}

} // namespace ian
