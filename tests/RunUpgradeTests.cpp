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
    requireNear(upgraded.playerDamageMultiplier / baseDamage, 1.3, 1e-12,
        "legendary upgrade triples the usual damage increment");
    require(upgraded.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::Damage)] == 1,
        "rarity power is independent from upgrade level");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::Luck,
        RunUpgradeRarity::Legendary), "luck upgrade granted");
    requireNear(simulation->snapshot().upgradeStats.luck, 0.3, 1e-12,
        "luck stat exposes the actual rarity modifier");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::TwinBatteries,
        RunUpgradeRarity::Common), "common discrete upgrade granted");
    require(simulation->snapshot().upgradeStats.sideShots == 1,
        "discrete effects use integer rarity bonuses");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::TwinBatteries,
        RunUpgradeRarity::Legendary), "last partial bonus granted");
    require(simulation->snapshot().upgradeStats.sideShots == 3 &&
        !simulation->grantDebugRunUpgrade(RunUpgradeEffect::TwinBatteries),
        "discrete bonuses respect combat caps without empty offers");

    constexpr std::array expectedDamageBonus{0.10, 0.15, 0.20, 0.25, 0.30};
    constexpr std::array expectedHealthBonus{12.0, 18.0, 24.0, 30.0, 36.0};
    constexpr std::array expectedBuildRangeBonus{3, 5, 6, 8, 9};
    auto tierSimulation = std::make_unique<Simulation>();
    for (std::size_t tierIndex = 0; tierIndex < RunUpgradeRarityCount; ++tierIndex) {
        tierSimulation->returnToMainMenu();
        tierSimulation->startRunFromSeed(PlayerClass::Vanguard, 6789);
        const auto tier = static_cast<RunUpgradeRarity>(tierIndex);
        const auto initial = tierSimulation->snapshot();
        require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, tier) &&
            tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::MaximumHealth, tier) &&
            tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::DefenseDamage, tier) &&
            tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::BuildRadius, tier),
            "each rarity grants its numeric upgrades");
        const auto& actual = tierSimulation->snapshot();
        requireNear(actual.playerDamageMultiplier / initial.playerDamageMultiplier,
            1.0 + expectedDamageBonus[tierIndex], 1e-12,
            "rarity changes actual player damage, not only the card text");
        requireNear(actual.playerMaxHealth - initial.playerMaxHealth,
            expectedHealthBonus[tierIndex], 1e-12, "each rarity grants the expected max HP");
        requireNear(actual.upgradeStats.defenseDamage - initial.upgradeStats.defenseDamage,
            expectedDamageBonus[tierIndex] * 1.2, 1e-12,
            "defense damage follows the same rarity scale");
        require(actual.coreBuildRadius - initial.coreBuildRadius == expectedBuildRangeBonus[tierIndex],
            "build range uses the displayed integer bonus");
    }

    tierSimulation->returnToMainMenu();
    tierSimulation->startRunFromSeed(PlayerClass::Vanguard, 6789);
    require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::ClusterPayload,
        RunUpgradeRarity::Uncommon), "uncommon cluster upgrade granted");
    require(tierSimulation->snapshot().upgradeStats.clusterExplosions == 3,
        "uncommon +2 explosion card grants exactly +3, not +4 from rounded power");
    const auto clusterSave = tierSimulation->saveSuspendedRunState();
    auto clusterRestored = std::make_unique<Simulation>();
    require(clusterRestored->loadSuspendedRunState(clusterSave) &&
        clusterRestored->snapshot().upgradeStats.clusterExplosions == 3,
        "reload preserves fractional power for integral explosion counts");
    requireNear(runUpgradeAppliedPowerGain(RunUpgradeEffect::ClusterPayload,
        RunUpgradeRarity::Legendary, 1.5), 0.5, 1e-12,
        "preview reports the remaining single explosion at the cap");
    require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::ClusterPayload,
        RunUpgradeRarity::Legendary) &&
        tierSimulation->snapshot().upgradeStats.clusterExplosions == 4,
        "higher rarity respects the existing four-explosion combat cap");
    require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::GunShrapnel,
        RunUpgradeRarity::Uncommon), "uncommon shrapnel granted");
    require(runUpgradeDiscreteAmount(RunUpgradeEffect::GunShrapnel,
        tierSimulation->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::GunShrapnel)]) == 5,
        "uncommon +3 shards rounds actual shards to +5");
    require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary),
        "all five legendary levels retain their full bonus");
    requireNear(tierSimulation->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::Damage)],
        15.0, 1e-12, "numeric power cap matches the highest rarity");

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
