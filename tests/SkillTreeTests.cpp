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
    require(
        tree.state("hold_to_gather") ==
            ian::SkillNodeState::Available,
        "hold gathering unlocks after Smart Tools");
    tree.grantPoints(1);
    const auto holdToGather = tree.indexOf("hold_to_gather");
    require(
        holdToGather &&
            tree.purchase(*holdToGather) ==
                ian::SkillPurchaseError::None &&
            tree.hasEffect(ian::SkillEffect::HoldToGather),
        "hold gathering node applies continuous gathering effect");
    require(tree.state("power_swing") ==
                ian::SkillNodeState::Available,
            "Power Swing follows hold gathering");
    tree.grantPoints(1);
    const auto powerSwing = tree.indexOf("power_swing");
    require(
        powerSwing &&
            tree.purchase(*powerSwing) ==
                ian::SkillPurchaseError::None &&
            tree.hasEffect(ian::SkillEffect::PowerSwing),
        "Power Swing applies radial gathering effect");
    require(
        tree.state("nightly_chest") ==
            ian::SkillNodeState::Available,
        "nightly chest branches directly from Bare Hands");
    tree.grantPoints(1);
    const auto nightlyChest = tree.indexOf("nightly_chest");
    require(
        nightlyChest &&
            tree.purchase(*nightlyChest) ==
                ian::SkillPurchaseError::None &&
            tree.hasEffect(ian::SkillEffect::NightlyChest),
        "nightly chest node applies survived-night reward effect");
    require(tree.state("safe_delivery") ==
                ian::SkillNodeState::Available,
            "Safe Delivery follows Night's Bounty");
    tree.grantPoints(1);
    const auto safeDelivery = tree.indexOf("safe_delivery");
    require(
        safeDelivery &&
            tree.purchase(*safeDelivery) ==
                ian::SkillPurchaseError::None &&
            tree.hasEffect(ian::SkillEffect::SafeDelivery),
        "Safe Delivery applies nearby chest effect");
    tree.grantPoints(4);
    const auto club = tree.indexOf("club");
    const auto bombs = tree.indexOf("bombs");
    const auto hammer = tree.indexOf("hammer");
    const auto fieldRepairs = tree.indexOf("field_repairs");
    require(
        club && bombs && hammer && fieldRepairs &&
            tree.purchase(*club) ==
                ian::SkillPurchaseError::None &&
            tree.purchase(*bombs) ==
                ian::SkillPurchaseError::None &&
            tree.purchase(*hammer) ==
                ian::SkillPurchaseError::None &&
            tree.purchase(*fieldRepairs) ==
                ian::SkillPurchaseError::None &&
            tree.hasEffect(ian::SkillEffect::UnlockBombs) &&
            tree.hasEffect(ian::SkillEffect::FieldRepairs),
        "combat and construction continuations unlock effects");
    require(
        tree.state("light_footwork") ==
            ian::SkillNodeState::Available,
        "movement branch starts at Bare Hands");
    tree.grantPoints(3);
    const auto lightFootwork = tree.indexOf("light_footwork");
    const auto dash = tree.indexOf("dash");
    require(
        lightFootwork && dash &&
            tree.purchase(*lightFootwork) ==
                ian::SkillPurchaseError::None &&
            tree.state(*dash) ==
                ian::SkillNodeState::Available &&
            tree.purchase(*dash) ==
                ian::SkillPurchaseError::None &&
            tree.hasEffect(ian::SkillEffect::LightFootwork) &&
            tree.hasEffect(ian::SkillEffect::Dash) &&
            tree.nodes()[*dash].size ==
                ian::SkillNodeSize::Large,
        "movement progression unlocks a large Dash node");

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
    const auto definitionById = [&definitions](std::string_view id) {
        return std::ranges::find(
            definitions, id, &ian::SkillNodeDefinition::id);
    };
    const auto powerDefinition = definitionById("power_swing");
    const auto bombDefinition = definitionById("bombs");
    const auto repairDefinition = definitionById("field_repairs");
    const auto safeDefinition = definitionById("safe_delivery");
    const auto dashDefinition = definitionById("dash");
    require(definitions.size() == 16 &&
                definitions.front().icon == "placeholder_hands" &&
                definitions[3].prerequisites ==
                    std::vector<std::string>{"axe", "pickaxe"} &&
                definitions[4].id == "hold_to_gather" &&
                definitions[4].prerequisites ==
                    std::vector<std::string>{"auto_switch_tools"} &&
                powerDefinition != definitions.end() &&
                bombDefinition != definitions.end() &&
                repairDefinition != definitions.end() &&
                safeDefinition != definitions.end() &&
                dashDefinition != definitions.end() &&
                dashDefinition->size ==
                    ian::SkillNodeSize::Large &&
                safeDefinition->prerequisites ==
                    std::vector<std::string>{"nightly_chest"} &&
                std::ranges::all_of(
                    definitions,
                    [](const ian::SkillNodeDefinition& node) {
                        return std::fmod(node.position.x, 190.0F) == 0.0F &&
                               std::fmod(node.position.y, 190.0F) == 0.0F;
                    }),
            "data-driven skill definitions load from JSON");
#endif

    tree.reset();
    require(tree.unlockedCount() == 1,
            "reset preserves only the root unlock");
}
