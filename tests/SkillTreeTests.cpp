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
    tree.grantPoints(1);
    require(tree.purchase(*axe) == ian::SkillPurchaseError::None,
            "available skill consumes point");
    require(tree.points() == 0 && tree.hasEffect(ian::SkillEffect::UnlockAxe),
            "purchase applies effect and cost");
    require(tree.state("pickaxe") == ian::SkillNodeState::Available,
            "siblings remain independently available");

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

#ifdef IAN_SOURCE_DIR
    const auto definitions = ian::loadSkillTreeDefinitions(
        std::string(IAN_SOURCE_DIR) + "/assets/data/skills.json");
    require(definitions.size() == 5 && definitions.front().icon == "placeholder_hands",
            "data-driven skill definitions load from JSON");
#endif

    tree.reset();
    require(tree.unlockedCount() == 1,
            "reset preserves only the root unlock");
}
