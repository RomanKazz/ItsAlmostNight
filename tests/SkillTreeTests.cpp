#include "TestHarness.hpp"
#include "progression/SkillTree.hpp"

void runSkillTreeTests() {
    ian::SkillTree tree;

    require(
        tree.state("bare_hands") == ian::SkillNodeState::Unlocked,
        "skill tree root starts unlocked");
    require(
        tree.state("axe") ==
            ian::SkillNodeState::Available,
        "root child starts available");
    const auto axe = tree.indexOf("axe");
    require(axe.has_value(), "axe node exists");
    require(tree.purchase(*axe) == ian::SkillPurchaseError::InsufficientPoints,
            "cost blocks purchase without points");

    ian::SkillTree unlimitedTree;
    const auto unlimitedAxe = unlimitedTree.indexOf("axe");
    require(
        unlimitedAxe &&
            unlimitedTree.purchase(*unlimitedAxe, false) ==
                ian::SkillPurchaseError::None &&
            unlimitedTree.points() == 0 &&
            unlimitedTree.isUnlocked("axe"),
        "free purchase unlocks a skill without consuming points");

    tree.grantPoints(1);
    require(tree.purchase(*axe) == ian::SkillPurchaseError::None,
            "available skill consumes point");
    require(tree.points() == 0 && tree.hasEffect(ian::SkillEffect::UnlockAxe),
            "purchase applies effect and cost");
    require(tree.state("pickaxe") == ian::SkillNodeState::Available,
            "siblings remain independently available");
    require(
        tree.state("auto_switch_tools") ==
            ian::SkillNodeState::Locked,
        "automatic tool switching requires both gathering tools");
    require(tree.state("rifle") == ian::SkillNodeState::Locked,
            "rifle stays locked behind club");

    tree.grantPoints(2);
    const auto pickaxe = tree.indexOf("pickaxe");
    const auto autoSwitch = tree.indexOf("auto_switch_tools");
    require(
        pickaxe && autoSwitch &&
            tree.purchase(*pickaxe) ==
                ian::SkillPurchaseError::None &&
            tree.state(*autoSwitch) ==
                ian::SkillNodeState::Available &&
            tree.purchase(*autoSwitch) ==
                ian::SkillPurchaseError::None &&
            tree.hasEffect(
                ian::SkillEffect::AutoSwitchTools),
        "axe and pickaxe converge into automatic tool switching");

    ian::SkillTree dependent({
        {"root", "ROOT", "", "root", ian::SkillBranch::Root, {0, 0}, 0, {}, ian::SkillEffect::BareHands},
        {"child", "CHILD", "", "child", ian::SkillBranch::Weapons, {1, 0}, 2, {"root"}, ian::SkillEffect::UnlockClub},
        {"leaf", "LEAF", "", "leaf", ian::SkillBranch::Weapons, {2, 0}, 1, {"child"}, ian::SkillEffect::UnlockHammer},
    });
    require(dependent.state("leaf") == ian::SkillNodeState::Locked,
            "dependency keeps child locked");
    dependent.grantPoints(3);
    require(dependent.purchase(*dependent.indexOf("child")) == ian::SkillPurchaseError::None &&
                dependent.points() == 1 && dependent.state("leaf") == ian::SkillNodeState::Available,
            "point grant, cost, and dependency transition work");

    const auto saved = tree.saveState();
    ian::SkillTree loaded;
    require(loaded.loadState(saved) && loaded.isUnlocked("axe"),
            "run state restores unlocked nodes");
    ian::SkillTree legacyLoaded;
    require(
        legacyLoaded.loadState({
            .points = 0,
            .unlockedNodeIds = {"bare_hands", "axe"},
        }) &&
            legacyLoaded.isUnlocked("axe") &&
            legacyLoaded.state("auto_switch_tools") ==
                ian::SkillNodeState::Locked,
        "older saves remain valid after adding automatic tool switching");

#ifdef IAN_SOURCE_DIR
    const auto definitions = ian::loadSkillTreeDefinitions(
        std::string(IAN_SOURCE_DIR) + "/assets/data/skills.json");
    require(definitions.size() == 8 &&
                definitions.front().icon == "placeholder_hands" &&
                definitions[3].prerequisites ==
                    std::vector<std::string>{"axe", "pickaxe"} &&
                definitions[5].id == "ice_wand" &&
                definitions[5].cost == 2,
            "data-driven skill definitions load from JSON");
#endif

    tree.reset();
    require(tree.unlockedCount() == 1,
            "reset preserves only the root unlock");
}
