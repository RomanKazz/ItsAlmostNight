#include "TestHarness.hpp"
#include "game/Simulation.hpp"

#include <limits>
#include <memory>
#include <numeric>

void runRunUpgradeTests() {
    using namespace ian;
    const auto base = runUpgradeRarityProbabilities(0.0);
    requireNear(base[0], 0.60, 1e-12, "common base chance");
    requireNear(base[4], 0.01, 1e-12, "legendary base chance");
    for (double luck : {0.0, 0.5, 1.0, 1000.0}) {
        const auto weights = runUpgradeRarityProbabilities(luck);
        requireNear(std::accumulate(weights.begin(), weights.end(), 0.0),
            1.0, 1e-12, "rarity chances normalize at every luck level");
        for (double weight : weights) require(weight > 0.0, "all rarities stay possible");
        require(weights[4] >= base[4], "luck improves legendary odds");
    }
    const auto lucky = runUpgradeRarityProbabilities(0.5);
    require(lucky[0] < base[0] && lucky[2] + lucky[3] + lucky[4] >
        base[2] + base[3] + base[4], "luck shifts offers toward higher tiers");
    require(rollRunUpgradeRarity(0, 0.59) == RunUpgradeRarity::Common &&
        rollRunUpgradeRarity(0, 0.61) == RunUpgradeRarity::Uncommon &&
        rollRunUpgradeRarity(0, 0.995) == RunUpgradeRarity::Legendary,
        "rarity sampling honors chance intervals");

    auto simulation = std::make_unique<Simulation>();
    simulation->returnToMainMenu();
    simulation->startRunFromSeed(PlayerClass::Vanguard, 12345);
    const double baseDamage = simulation->snapshot().playerDamageMultiplier;
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage,
        RunUpgradeRarity::Legendary), "legendary numeric upgrade granted");
    const auto& upgraded = simulation->snapshot();
    requireNear(upgraded.playerDamageMultiplier / baseDamage, 1.2, 1e-12,
        "legendary upgrade doubles the usual damage increment");
    require(upgraded.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::Damage)] == 1,
        "rarity power is independent from upgrade level");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::Luck,
        RunUpgradeRarity::Legendary), "luck upgrade granted");
    requireNear(simulation->snapshot().upgradeStats.luck, 0.2, 1e-12,
        "luck stat exposes the actual rarity modifier");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::TwinBatteries,
        RunUpgradeRarity::Legendary), "legendary discrete upgrade granted");
    require(simulation->snapshot().upgradeStats.sideShots == 2,
        "discrete effects use integer rarity bonuses");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::TwinBatteries,
        RunUpgradeRarity::Legendary), "last partial bonus granted");
    require(simulation->snapshot().upgradeStats.sideShots == 3 &&
        !simulation->grantDebugRunUpgrade(RunUpgradeEffect::TwinBatteries),
        "discrete bonuses respect combat caps without empty offers");

    PlayerCommand xp;
    xp.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
    xp.grantInsight = GrantInsightCommand{1000.0};
    simulation->tick(1.0 / 60.0, xp);
    require(simulation->snapshot().runUpgradeChoicePending, "XP produces upgrade offers");
    const auto saved = simulation->saveSuspendedRunState();
    auto restored = std::make_unique<Simulation>();
    require(restored->loadSuspendedRunState(saved), "rarity checkpoint restores");
    require(restored->snapshot().runUpgradeRarities == simulation->snapshot().runUpgradeRarities &&
        restored->snapshot().runUpgradeChoices == simulation->snapshot().runUpgradeChoices &&
        restored->snapshot().runUpgradePower == simulation->snapshot().runUpgradePower,
        "reload preserves offered rarities and all accumulated power");
    requireNear(restored->snapshot().playerDamageMultiplier,
        simulation->snapshot().playerDamageMultiplier, 1e-12, "reload preserves numeric bonuses");
    const auto before = restored->snapshot();
    const auto effect = before.runUpgradeChoices[0];
    const auto tier = before.runUpgradeRarities[0];
    require(restored->selectRunUpgrade(0), "offered upgrade selected");
    requireNear(restored->snapshot().runUpgradePower[runUpgradeIndex(effect)],
        before.runUpgradePower[runUpgradeIndex(effect)] + std::min(
            runUpgradePowerGain(effect, tier), runUpgradePowerLimit(effect) -
                before.runUpgradePower[runUpgradeIndex(effect)]),
        1e-12, "selection applies the offered rarity exactly");

    auto legacy = saved;
    legacy.runUpgradePower.fill(0.0);
    restored->returnToMainMenu();
    require(restored->loadSuspendedRunState(legacy), "legacy count-only checkpoint restores");
    requireNear(restored->snapshot().upgradeStats.luck, 0.1, 1e-12,
        "legacy checkpoints recover common-tier power from counts");
    auto invalid = saved;
    restored->returnToMainMenu();
    invalid.runUpgradePower[0] = std::numeric_limits<double>::quiet_NaN();
    require(!restored->loadSuspendedRunState(invalid), "non-finite upgrade power rejected");
    invalid = saved;
    invalid.runUpgradeRarities[0] = static_cast<RunUpgradeRarity>(5);
    require(!restored->loadSuspendedRunState(invalid), "invalid offer rarity rejected");
    simulation->returnToMainMenu();
    simulation->startRunFromSeed(PlayerClass::Vanguard, 12345);
    requireNear(simulation->snapshot().upgradeStats.luck, 0, 1e-12,
        "new runs reset luck and rarity power");
}
