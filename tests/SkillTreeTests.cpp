#include "TestHarness.hpp"
#include "progression/SkillTree.hpp"

void runSkillTreeTests() {
    ian::SkillTree tree;

    require(
        tree.state("core") == ian::SkillNodeState::Unlocked,
        "skill tree root starts unlocked");
    require(
        tree.state("construction") ==
            ian::SkillNodeState::Available,
        "root child starts available");
    require(
        tree.state("verticality") ==
            ian::SkillNodeState::Hidden,
        "deeper leaves stay hidden before their branch grows");

    const auto construction = tree.indexOf("construction");
    require(construction.has_value(), "construction node exists");
    require(
        tree.unlock(*construction),
        "available branch can be unlocked");
    require(
        tree.state("verticality") ==
            ian::SkillNodeState::Available,
        "unlocking a branch reveals its first leaf");
    require(
        tree.state("automation") ==
            ian::SkillNodeState::Hidden,
        "multi-parent capstone remains hidden");

    const auto verticality = tree.indexOf("verticality");
    const auto fortification = tree.indexOf("fortification");
    require(verticality && fortification,
            "construction child nodes exist");
    require(tree.unlock(*verticality),
            "first prerequisite unlocks");
    require(
        tree.state("automation") == ian::SkillNodeState::Locked,
        "capstone appears locked after one prerequisite");
    require(tree.unlock(*fortification),
            "second prerequisite unlocks");
    require(
        tree.state("automation") ==
            ian::SkillNodeState::Available,
        "capstone becomes available after every prerequisite");
    require(
        !tree.unlock(*construction),
        "already unlocked nodes cannot unlock twice");

    tree.reset();
    require(tree.unlockedCount() == 1,
            "reset preserves only the root unlock");
}
