#include "TestHarness.hpp"
#include "progression/SkillTree.hpp"

#include <cmath>
#include <ranges>

void runSkillTreeTests() {
    ian::SkillTree tree;
    require(
        tree.state("bare_hands") == ian::SkillNodeState::Unlocked,
        "skill tree root starts unlocked");
    require(
        tree.state("axe") == ian::SkillNodeState::Available,
        "root child starts available");
    require(
        tree.state("efficient_strikes") == ian::SkillNodeState::Hidden &&
            tree.state("lumber_mill") == ian::SkillNodeState::Hidden &&
            tree.state("quarry") == ian::SkillNodeState::Hidden &&
            tree.state("power_swing") == ian::SkillNodeState::Hidden,
        "deeper skill nodes begin hidden");

    const auto axe = tree.indexOf("axe");
    require(axe.has_value(), "axe node exists");
    require(
        tree.purchase(*axe) ==
            ian::SkillPurchaseError::InsufficientPoints,
        "cost blocks purchase without points");
    tree.grantPoints(1);
    require(
        tree.purchase(*axe) == ian::SkillPurchaseError::None &&
            tree.hasEffect("unlock.axe") &&
            tree.effectValue("unlock.axe") == 1.0 &&
            tree.state("lumber_mill") ==
                ian::SkillNodeState::Available &&
            tree.state("quarry") ==
                ian::SkillNodeState::Hidden &&
            tree.state("efficient_strikes") ==
                ian::SkillNodeState::Locked,
        "Axe reveals Lumber Mill without revealing the Pickaxe branch");

    tree.grantPoints(16);
    const auto pickaxe = tree.indexOf("pickaxe");
    const auto efficient = tree.indexOf("efficient_strikes");
    const auto handsOn = tree.indexOf("hands_on");
    const auto industrialist = tree.indexOf("industrialist");
    require(
        pickaxe && efficient && handsOn && industrialist &&
            tree.purchase(*pickaxe) == ian::SkillPurchaseError::None &&
            tree.state("quarry") ==
                ian::SkillNodeState::Available &&
            tree.purchase(*efficient) == ian::SkillPurchaseError::None &&
            tree.purchase(*handsOn) == ian::SkillPurchaseError::None,
        "gathering doctrine prerequisites unlock in order");
    requireNear(
        tree.effectValue("gather.damage"), 0.70, 1e-9,
        "multiple data-driven modifiers accumulate");
    require(
        tree.state(*industrialist) == ian::SkillNodeState::Locked &&
            tree.isExcluded(*industrialist) &&
            tree.purchase(*industrialist) ==
                ian::SkillPurchaseError::MutuallyExclusive,
        "choosing one doctrine permanently locks its alternative");

    ian::SkillTree rifleTree;
    rifleTree.grantPoints(10);
    const auto rifle = rifleTree.indexOf("rifle");
    const auto combatTraining =
        rifleTree.indexOf("combat_training");
    const auto marksman = rifleTree.indexOf("marksman");
    const auto assault = rifleTree.indexOf("assault_rifle");
    require(
        combatTraining && rifle &&
            rifleTree.state(*rifle) ==
                ian::SkillNodeState::Hidden &&
            rifleTree.purchase(*rifle) ==
                ian::SkillPurchaseError::DependenciesLocked,
        "hidden weapon unlock cannot bypass Combat Training");
    require(
        combatTraining && rifle && marksman && assault &&
            rifleTree.purchase(*combatTraining) ==
                ian::SkillPurchaseError::None &&
            rifleTree.purchase(*rifle) ==
                ian::SkillPurchaseError::None &&
            rifleTree.purchase(*marksman) ==
                ian::SkillPurchaseError::None &&
            rifleTree.effectValue("rifle.damage") > 0.69 &&
            rifleTree.purchase(*assault) ==
                ian::SkillPurchaseError::MutuallyExclusive,
        "weapon specializations are functional and exclusive");

    ian::SkillTree dependent({
        {.id = "root", .title = "ROOT", .icon = "root",
         .branch = ian::SkillBranch::Root, .position = {0, 0},
         .cost = 0},
        {.id = "child", .title = "CHILD", .icon = "child",
         .branch = ian::SkillBranch::Weapons, .position = {190, 0},
         .cost = 2, .prerequisites = {"root"},
         .effects = {{"test.value", 0.25}}},
        {.id = "leaf", .title = "LEAF", .icon = "leaf",
         .branch = ian::SkillBranch::Weapons, .position = {380, 0},
         .cost = 1, .prerequisites = {"child"}},
    });
    require(
        dependent.state("leaf") == ian::SkillNodeState::Hidden,
        "dependency keeps a distant leaf hidden");
    dependent.grantPoints(3);
    require(
        dependent.purchase(*dependent.indexOf("child")) ==
                ian::SkillPurchaseError::None &&
            dependent.points() == 1 &&
            dependent.state("leaf") ==
                ian::SkillNodeState::Available &&
            dependent.effectValue("test.value") == 0.25,
        "generic effect works without adding a C++ enum value");

    const auto saved = rifleTree.saveState();
    ian::SkillTree loaded;
    require(
        loaded.loadState(saved) && loaded.isUnlocked("marksman") &&
            loaded.state("assault_rifle") ==
                ian::SkillNodeState::Locked,
        "run state restores effects and exclusion choices");

    ian::SkillTree migrated;
    require(
        migrated.loadState({
            .points = 0,
            .unlockedNodeIds = {
                "bare_hands", "axe", "pickaxe",
                "auto_switch_tools", "hold_to_gather",
                "power_swing",
            },
        }) &&
            migrated.isUnlocked("efficient_strikes") &&
            migrated.isUnlocked("power_swing"),
        "retired convenience nodes migrate without breaking old runs");

    ian::SkillTree gatedMigration;
    require(
        gatedMigration.loadState({
            .points = 0,
            .unlockedNodeIds = {
                "bare_hands", "club", "light_footwork",
                "dash",
            },
        }) &&
            gatedMigration.isUnlocked("combat_training") &&
            gatedMigration.isUnlocked("sprinter"),
        "old saves gain newly inserted prerequisite gates");

#ifdef IAN_SOURCE_DIR
    const auto definitions = ian::loadSkillTreeDefinitions(
        std::string(IAN_SOURCE_DIR) + "/assets/data/skills.json");
    const auto definitionById = [&definitions](std::string_view id) {
        return std::ranges::find(
            definitions, id, &ian::SkillNodeDefinition::id);
    };
    const auto thermal = definitionById("thermal_shock");
    const auto dash = definitionById("dash");
    const auto longerDays = definitionById("longer_days");
    const auto contract = definitionById("mercenary_contract");
    const auto crossbow = definitionById("crossbow_unlock");
    const auto cannon = definitionById("cannon_unlock");
    const auto catapult = definitionById("catapult_unlock");
    require(
        definitions.size() == 58 &&
            definitionById("auto_switch_tools") == definitions.end() &&
            definitionById("hold_to_gather") == definitions.end() &&
            thermal != definitions.end() &&
            thermal->effects.size() == 1 &&
            thermal->effects.front().key ==
                "element.thermal_shock" &&
            dash != definitions.end() &&
            dash->size == ian::SkillNodeSize::Large &&
            dash->prerequisites ==
                std::vector<std::string>{"sprinter"} &&
            longerDays != definitions.end() &&
            longerDays->effects.size() == 1 &&
            longerDays->effects.front().key ==
                "day.duration_seconds" &&
            contract != definitions.end() &&
            contract->exclusiveGroup == "early_contract" &&
            crossbow != definitions.end() &&
            crossbow->minimumCoreLevel == 2 &&
            crossbow->effects.front().key == "unlock.crossbow" &&
            cannon != definitions.end() &&
            cannon->minimumCoreLevel == 3 &&
            cannon->effects.front().key == "unlock.cannon" &&
            catapult != definitions.end() &&
            catapult->minimumCoreLevel == 4 &&
            catapult->effects.front().key == "unlock.catapult" &&
            definitionById("lumber_mill") != definitions.end() &&
            definitionById("quarry") != definitions.end() &&
            definitionById("crystal_mine") != definitions.end() &&
            definitionById("night_shift") != definitions.end() &&
            definitionById("night_shift")->effects.front().key ==
                "production.night_speed" &&
            definitionById("night_shift")->effects.front().value == 0.5 &&
            std::ranges::all_of(
                definitions,
                [](const ian::SkillNodeDefinition& node) {
                    return std::fmod(node.position.x, 190.0F) == 0.0F &&
                           std::fmod(node.position.y, 190.0F) == 0.0F;
                }),
        "expanded data-driven tree loads on a clean grid");
#endif

    tree.reset();
    require(
        tree.unlockedCount() == 1,
        "reset preserves only the root unlock");
}
