#include "TestHarness.hpp"
#include "progression/ProgressionCardRules.hpp"
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
        tree.state("efficient_strikes") == ian::SkillNodeState::Available &&
            tree.state("lumber_mill") == ian::SkillNodeState::Available &&
            tree.state("quarry") == ian::SkillNodeState::Available &&
            tree.state("power_swing") == ian::SkillNodeState::Available,
        "all progression cards are visible without tree dependencies");

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
                ian::SkillNodeState::Available &&
            tree.state("efficient_strikes") ==
                ian::SkillNodeState::Available,
        "selecting one flat card does not gate unrelated cards");

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
        "independent gathering cards apply in any order");
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
                ian::SkillNodeState::Available,
        "legacy weapon records no longer depend on Combat Training");

    ian::SkillTree cardTree;
    const auto cardRifle = cardTree.indexOf("rifle");
    require(
        cardRifle && !ian::progressionCardEligible(
            cardTree, *cardRifle,
            {.playerLevel = 20,
             .coreLevel = 8,
             .wavesSurvived = 20,
             .playerClass = ian::PlayerClass::Ranger}),
        "deterministic Core weapon unlocks never enter card offers");
    const auto legacyLumber = cardTree.indexOf("lumber_mill");
    const auto turretCalibration =
        cardTree.indexOf("turret_calibration");
    require(
        legacyLumber && turretCalibration &&
            !ian::progressionCardEligible(
                cardTree, *legacyLumber,
                {.playerLevel = 20, .coreLevel = 8,
                 .wavesSurvived = 20}) &&
            !ian::progressionCardEligible(
                cardTree, *turretCalibration,
                {.playerLevel = 20, .coreLevel = 1,
                 .wavesSurvived = 20}) &&
            ian::progressionCardEligible(
                cardTree, *turretCalibration,
                {.playerLevel = 20, .coreLevel = 2,
                 .wavesSurvived = 20}),
        "Core progression replaces unlock cards while preserving defense modifiers");
    const auto cardMarksman = cardTree.indexOf("marksman");
    require(
        cardMarksman &&
            !ian::progressionCardEligible(
                cardTree, *cardMarksman,
                {.playerLevel = 6, .coreLevel = 1}) &&
            !ian::progressionCardEligible(
                cardTree, *cardMarksman,
                {.playerLevel = 7, .coreLevel = 1}) &&
            ian::progressionCardEligible(
                cardTree, *cardMarksman,
                {.playerLevel = 7, .coreLevel = 2}),
        "weapon specialization cards require level and Core weapon access");
    const auto wideSwing = cardTree.indexOf("wide_swing");
    const auto impactDash = cardTree.indexOf("impact_dash");
    const auto supplyDrop = cardTree.indexOf("nightly_chest");
    require(
        wideSwing && impactDash && supplyDrop &&
            ian::progressionCardEligible(
                cardTree, *wideSwing,
                {.playerLevel = 2}) &&
            ian::progressionCardEligible(
                cardTree, *impactDash,
                {.playerLevel = 4}) &&
            ian::progressionCardEligible(
                cardTree, *supplyDrop,
                {.playerLevel = 4}),
        "self-contained cards never require another selected card");
    const auto obsoleteDelivery = cardTree.indexOf("frequent_bounty");
    const auto obsoleteSafeDelivery = cardTree.indexOf("safe_delivery");
    require(
        obsoleteDelivery && obsoleteSafeDelivery &&
            !ian::progressionCardEligible(
                cardTree, *obsoleteDelivery,
                {.playerLevel = 20}) &&
            !ian::progressionCardEligible(
                cardTree, *obsoleteSafeDelivery,
                {.playerLevel = 20}),
        "conditional legacy chest cards stay out of new offers");
    require(
        cardTree.purchase(*wideSwing, false) ==
                ian::SkillPurchaseError::None &&
            cardTree.hasEffect("gather.power_swing") &&
            cardTree.purchase(*impactDash, false) ==
                ian::SkillPurchaseError::None &&
            cardTree.hasEffect("dash.unlock"),
        "standalone variants include the base mechanic they modify");

    ian::SkillTree levelTree;
    const auto repeatableCard = levelTree.indexOf("efficient_strikes");
    require(repeatableCard.has_value(), "repeatable power card exists");
    for (int level = 1; level <= 5; ++level) {
        require(
            ian::progressionCardEligible(
                levelTree, *repeatableCard,
                {.playerLevel = 20}) &&
                levelTree.purchase(*repeatableCard, false) ==
                    ian::SkillPurchaseError::None &&
                levelTree.level(*repeatableCard) == level,
            "repeatable card advances exactly one level");
    }
    require(
        levelTree.isMaxLevel(*repeatableCard) &&
            !ian::progressionCardEligible(
                levelTree, *repeatableCard,
                {.playerLevel = 20}) &&
            levelTree.purchase(*repeatableCard, false) ==
                ian::SkillPurchaseError::AlreadyUnlocked,
        "max-level cards leave the offer pool");
    requireNear(
        levelTree.effectValue("gather.damage"), 1.25, 1e-9,
        "each card level adds another copy of its effect");
    ian::SkillTree restoredLevelTree;
    require(
        restoredLevelTree.loadState(levelTree.saveState()) &&
            restoredLevelTree.level("efficient_strikes") == 5 &&
            restoredLevelTree.isMaxLevel(*repeatableCard),
        "card levels round-trip through legacy-compatible saves");
    require(
        cardTree.purchase(*cardRifle, false, false) ==
            ian::SkillPurchaseError::None,
        "legacy weapon card state remains loadable for old runs");
    const auto flatSaved = cardTree.saveState();
    ian::SkillTree flatLoaded;
    require(
        flatLoaded.loadState(flatSaved) &&
            flatLoaded.isUnlocked("rifle"),
        "flat card unlocks survive save migration without old prerequisites");
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
        dependent.state("leaf") == ian::SkillNodeState::Available,
        "legacy dependency metadata is flattened by the card collection");
    dependent.grantPoints(3);
    require(
        dependent.purchase(*dependent.indexOf("child")) ==
                ian::SkillPurchaseError::None &&
            dependent.points() == 1 &&
            dependent.state("leaf") == ian::SkillNodeState::Available &&
            dependent.effectValue("test.value") == 0.25,
        "generic flat-card effects work without a C++ enum value");

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
            !gatedMigration.isUnlocked("combat_training") &&
            !gatedMigration.isUnlocked("sprinter") &&
            gatedMigration.isUnlocked("club") &&
            gatedMigration.isUnlocked("dash"),
        "old saves load flat cards without synthesizing tree gates");

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
            dash->prerequisites.empty() &&
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
            definitionById("crystal_mine")->minimumWavesSurvived == 1 &&
            definitionById("night_shift") != definitions.end() &&
            definitionById("night_shift")->effects.front().key ==
                "production.night_speed" &&
            definitionById("night_shift")->effects.front().value == 0.5 &&
            std::ranges::all_of(
                definitions,
                [](const ian::SkillNodeDefinition& node) {
                    return node.prerequisites.empty();
                }) &&
            std::ranges::all_of(
                definitions,
                [](const ian::SkillNodeDefinition& node) {
                    return std::fmod(node.position.x, 190.0F) == 0.0F &&
                           std::fmod(node.position.y, 190.0F) == 0.0F;
                }),
        "flat data-driven card collection loads without dependencies");
#endif

    tree.reset();
    require(
        tree.unlockedCount() == 1,
        "reset preserves only the root unlock");
}
