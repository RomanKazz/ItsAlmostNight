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

    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCannon),
        "checkpoint fixture owns its cannon specialization options");
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

    // The run arsenal counts types, independently of constructed instances.
    auto arsenal = std::make_unique<Simulation>();
    for (const auto& character : PlayerClassDefinitions) {
        arsenal->returnToMainMenu();
        arsenal->startRunFromSeed(character.type, 4321);
        require(arsenal->snapshot().buildingArsenalCount == 1 &&
            !arsenal->grantDebugRunUpgrade(*buildingRunUpgrade(character.startingBuilding)),
            "every character owns its starter once, without spending another slot");
    }
    arsenal->returnToMainMenu();
    arsenal->startRunFromSeed(PlayerClass::Vanguard, 4321);
    require(arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCannon, RunUpgradeRarity::Legendary) &&
        arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockTurret) &&
        arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockSpikeTrap) &&
        arsenal->snapshot().buildingArsenalCount == RunBuildingSlotCount &&
        !arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCatapult) &&
        !arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCannon),
        "acquisition is unique and stops at four types, regardless of rarity");
    const auto arsenalSave = arsenal->saveSuspendedRunState();
    restored->returnToMainMenu();
    require(arsenalSave.buildingArsenalVersion == 1 && restored->loadSuspendedRunState(arsenalSave) &&
        restored->snapshot().buildingArsenalCount == 4 &&
        restored->snapshot().runUpgradeStacks == arsenal->snapshot().runUpgradeStacks,
        "unbuilt blueprints survive save and resume");
    auto invalidArsenal = arsenalSave;
    invalidArsenal.buildingArsenalVersion = 2;
    restored->returnToMainMenu();
    require(!restored->loadSuspendedRunState(invalidArsenal), "future arsenal formats rejected");
    invalidArsenal = arsenalSave;
    invalidArsenal.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] = 2;
    require(!restored->loadSuspendedRunState(invalidArsenal), "duplicate blueprint counts rejected");

    auto balance = GameBalance::defaults();
    balance.buildings[static_cast<std::size_t>(BuildingType::Core)].wood = 0;
    auto world = WorldConfig::defaults();
    world.terrainAmplitude = 0;
    auto map = MapDefinition::defaults();
    map.resources.clear();
    map.obstacles.clear();
    auto fastInsight = InsightConfig::defaults();
    fastInsight.baseRequirement = 1.0;
    fastInsight.requirementGrowth = 0.0;
    auto normal = std::make_unique<Simulation>(balance, map, world, fastInsight);
    normal->returnToMainMenu();
    normal->startRunFromSeed(PlayerClass::Engineer, 4321);
    PlayerCommand placeCore;
    placeCore.placeBuilding = PlaceBuildingCommand{BuildingType::Core, {0, 0}, 0};
    normal->tick(1.0 / 60.0, placeCore);
    const auto unlocked = normal->snapshot().unlockedBuildings;
    for (auto type : RunBuildingTypes) {
        require(unlocked[static_cast<std::size_t>(type)] == (type == BuildingType::Cannon),
            "construction catalog uses the character starter");
    }
    require(normal->previewPlacement(BuildingType::Turret, {4, 0}).error == PlacementError::BlueprintRequired &&
        normal->snapshot().unlockedBuildings[static_cast<std::size_t>(BuildingType::LumberMill)],
        "unowned combat placement rejected while economy remains available");
    require(normal->snapshot().runUpgradeChoicePending,
        "normal construction XP produces level-up cards");
    const auto offer = normal->snapshot();
    require(runUpgradeBuilding(offer.runUpgradeChoices[0]).has_value(),
        "free arsenal slot guarantees a new-building option");
    for (std::size_t index = 0; index < offer.runUpgradeChoiceCount; ++index) {
        const auto offeredEffect = offer.runUpgradeChoices[index];
        require(offeredEffect != RunUpgradeEffect::GunPiercing && offeredEffect != RunUpgradeEffect::GunShrapnel &&
            offeredEffect != RunUpgradeEffect::TwinBatteries && offeredEffect != RunUpgradeEffect::ReactiveTraps,
            "family-specific cards require a compatible owned building");
        if (const auto building = runUpgradeBuilding(offeredEffect)) {
            require(*building != BuildingType::Cannon && offer.runUpgradeRarities[index] == RunUpgradeRarity::Common,
                "starter is absent from acquisition offers and acquisition has fixed power");
        }
    }
    const auto pendingArsenal = normal->saveSuspendedRunState();
    auto normalRestored = std::make_unique<Simulation>(balance, map, world, fastInsight);
    normalRestored->returnToMainMenu();
    require(normalRestored->loadSuspendedRunState(pendingArsenal) &&
        normalRestored->snapshot().runUpgradeChoices == offer.runUpgradeChoices &&
        normalRestored->snapshot().runUpgradeRarities == offer.runUpgradeRarities &&
        normalRestored->selectRunUpgrade(0) && normalRestored->snapshot().buildingArsenalCount == 2,
        "resume preserves exact normal offers and selection acquires a second type");
    require(normal->selectRunUpgrade(0), "normal acquisition selected");
    for (auto blueprintEffect : {RunUpgradeEffect::UnlockGunTurret, RunUpgradeEffect::UnlockTurret,
                        RunUpgradeEffect::UnlockSpikeTrap, RunUpgradeEffect::UnlockSlowTrap,
                        RunUpgradeEffect::UnlockCatapult}) {
        if (normal->snapshot().buildingArsenalCount < 4) (void)normal->grantDebugRunUpgrade(blueprintEffect);
    }
    require(normal->grantDebugRunUpgrade(RunUpgradeEffect::RerollToken) && normal->rerollRunUpgrades(),
        "full arsenal offer regenerated");
    for (std::size_t index = 0; index < normal->snapshot().runUpgradeChoiceCount; ++index) {
        require(!runUpgradeBuilding(normal->snapshot().runUpgradeChoices[index]),
            "full arsenal replaces all acquisition cards with improvements");
    }

    auto legacyArsenal = arsenalSave;
    legacyArsenal.buildingArsenalVersion = 0;
    legacyArsenal.runUpgradeStacks.fill(0);
    legacyArsenal.runUpgradePower.fill(0);
    legacyArsenal.buildings = {
        {.id = {1000, 1}, .type = BuildingType::Core, .gridPosition = {0, 0}, .health = 500, .maxHealth = 500},
        {.id = {1001, 1}, .type = BuildingType::Catapult, .gridPosition = {4, 0}, .health = 210, .maxHealth = 210},
    };
    legacyArsenal.resumeState = RunState::BuildPhase;
    restored->returnToMainMenu();
    require(restored->loadSuspendedRunState(legacyArsenal) && restored->snapshot().buildingArsenalCount == 2 &&
        restored->snapshot().unlockedBuildings[static_cast<std::size_t>(BuildingType::Catapult)],
        "legacy checkpoints retain combat types already built");
    arsenal->returnToMainMenu();
    arsenal->startRunFromSeed(PlayerClass::Vanguard, 4321);
    require(arsenal->snapshot().buildingArsenalCount == 1 &&
        arsenal->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] == 0,
        "next run clears acquired blueprints");
    arsenal->returnToMainMenu();
    arsenal->startSandboxRun();
    for (auto type : RunBuildingTypes) require(arsenal->snapshot().unlockedBuildings[static_cast<std::size_t>(type)],
        "sandbox keeps all construction types available");

    auto cannonRun = std::make_unique<Simulation>();
    auto resumedCannonRun = std::make_unique<Simulation>();
    for (auto cannonEffect : {RunUpgradeEffect::CannonHeavyShell, RunUpgradeEffect::CannonScattershot,
                        RunUpgradeEffect::CannonClusterShell}) {
        cannonRun->returnToMainMenu();
        cannonRun->startRunFromSeed(PlayerClass::Vanguard, 4321);
        require(!cannonRun->grantDebugRunUpgrade(cannonEffect), "cannon specialization requires an owned cannon");
        require(cannonRun->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCannon) &&
                cannonRun->grantDebugRunUpgrade(cannonEffect, RunUpgradeRarity::Legendary),
            "acquired cannon accepts one specialization");
        require(cannonRun->snapshot().upgradeStats.cannonAttackStyle == *runUpgradeCannonStyle(cannonEffect) &&
                cannonRun->snapshot().runUpgradePower[runUpgradeIndex(cannonEffect)] == 1.0,
            "specialization has fixed power across rarities and appears in stats");
        for (auto other : {RunUpgradeEffect::CannonHeavyShell, RunUpgradeEffect::CannonScattershot,
                           RunUpgradeEffect::CannonClusterShell}) {
            require(!cannonRun->grantDebugRunUpgrade(other),
                "specialization cannot stack, repeat or combine with another attack style");
        }
        auto checkpoint = cannonRun->saveSuspendedRunState();
        resumedCannonRun->returnToMainMenu();
        require(resumedCannonRun->loadSuspendedRunState(checkpoint) &&
                resumedCannonRun->snapshot().upgradeStats.cannonAttackStyle == *runUpgradeCannonStyle(cannonEffect),
            "resume restores chosen cannon specialization");
        const auto other = cannonEffect == RunUpgradeEffect::CannonHeavyShell
            ? RunUpgradeEffect::CannonScattershot : RunUpgradeEffect::CannonHeavyShell;
        checkpoint.runUpgradeStacks[runUpgradeIndex(other)] = 1;
        checkpoint.runUpgradePower[runUpgradeIndex(other)] = 1.0;
        resumedCannonRun->returnToMainMenu();
        require(!resumedCannonRun->loadSuspendedRunState(checkpoint),
            "incompatible saved cannon styles rejected");
        checkpoint.runUpgradeStacks[runUpgradeIndex(cannonEffect)] = std::numeric_limits<int>::max();
        checkpoint.runUpgradeStacks[runUpgradeIndex(other)] = std::numeric_limits<int>::max();
        require(!resumedCannonRun->loadSuspendedRunState(checkpoint),
            "malformed huge saved style counts rejected without integer overflow");
    }
    cannonRun->returnToMainMenu();
    cannonRun->startRunFromSeed(PlayerClass::Engineer, 4321);
    require(cannonRun->snapshot().upgradeStats.cannonAttackStyle == CannonAttackStyle::Standard,
        "next run resets cannon specialization");

    require(normal->grantDebugRunUpgrade(RunUpgradeEffect::CannonScattershot),
        "normal run chooses a cannon attack style");
    for (int reroll = 0; reroll < 3; ++reroll) {
        require(normal->grantDebugRunUpgrade(RunUpgradeEffect::RerollToken) && normal->rerollRunUpgrades(),
            "specialized arsenal rerolls its remaining upgrades");
        const auto specializedOffers = normal->snapshot();
        for (std::size_t choice = 0; choice < specializedOffers.runUpgradeChoiceCount; ++choice) {
            require(!runUpgradeCannonStyle(specializedOffers.runUpgradeChoices[choice]),
                "chosen cannon specialization removes all other styles from the card pool");
        }
    }

}
