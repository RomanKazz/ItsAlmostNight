#include "TestHarness.hpp"
#include "BuildingCapacityChecks.hpp"
#include "DefenseCardProgressionChecks.hpp"
#include "ProducerCardProgressionChecks.hpp"
#include "WeaponCardProgressionChecks.hpp"
#include "RunUpgradeRetirementChecks.hpp"
#include "CoreDefenseGateChecks.hpp"
#include "ExperienceTomeChecks.hpp"
#include "game/Simulation.hpp"
#include "progression/RunUpgradeOfferSelection.hpp"

#include <limits>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

void runRunUpgradeTests() {
    checkExperienceTome();
    checkBuildingCapacityProgression();
    checkDefenseCardProgression();
    checkProducerCardProgression();
    checkWeaponCardProgression();
    checkRetiredHighStakes();
    checkRetiredBuildingBranches();
    checkCoreDefenseGateAndRepairs();
    using namespace ian;
    {
        auto world=WorldConfig::defaults();world.terrainResolution=33;
        world.terrainAmplitude=world.terrainSurfaceNoiseAmplitude=0;
        world.pondMinimumCount=world.pondMaximumCount=0;
        auto map=MapDefinition::defaults();map.resources.clear();map.obstacles.clear();
        auto run=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
        run->startRunFromSeed(PlayerClass::Miner,348);
        auto saved=run->saveSuspendedRunState();saved.playerHealth=80;
        saved.buildings={
            {.id={1000,1},.type=BuildingType::Core,.gridPosition={0,0},.health=500,.maxHealth=500},
            {.id={1001,1},.type=BuildingType::Wall,.gridPosition={2,0},.health=80,.maxHealth=100}};
        run->returnToMainMenu();require(run->loadSuspendedRunState(saved),"shared health fixture restores");
        const double basePlayer=run->snapshot().playerMaxHealth;
        const double baseBuilding=run->snapshot().upgradeStats.buildingHealth;
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::BuildingHealth,RunUpgradeRarity::Rare),"Fortitude applies");
        requireNear(run->snapshot().playerMaxHealth,basePlayer*1.21,1e-9,"rare Fortitude gives player twenty-one percent HP");
        requireNear(run->snapshot().upgradeStats.buildingHealth,baseBuilding*1.21,1e-9,"Fortitude gives the same HP bonus to structures");
        requireNear(run->snapshot().playerMaxHealth-run->snapshot().playerHealth,20,1e-9,"Fortitude heals only the gained player capacity");
        saved=run->saveSuspendedRunState();
        saved.playerTemporaryHealth=30;saved.playerHealth+=30;
        run->returnToMainMenu();require(run->loadSuspendedRunState(saved),"shared health survives suspension");
        const double before=run->snapshot().playerMaxHealth;
        requireNear(before,basePlayer*1.21+30,1e-9,"temporary health is outside the Fortitude multiplier");
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::BuildingHealth),"Fortitude stacks");
        requireNear(run->snapshot().playerMaxHealth,basePlayer*1.36+30,1e-9,"Fortitude stacks additively without multiplying temporary HP");
        const double upgraded=run->snapshot().playerMaxHealth;
        for(int load=0;load<2;++load) {
            saved=run->saveSuspendedRunState();run->returnToMainMenu();
            require(run->loadSuspendedRunState(saved),"Fortitude saves repeatedly");
            requireNear(run->snapshot().playerMaxHealth,upgraded,1e-9,"resume cannot double-apply shared health");
        }
    }
    {
        auto run = std::make_unique<Simulation>();
        run->startRunFromSeed(PlayerClass::Vanguard, 71234);
        auto offer = run->saveSuspendedRunState();
        offer.runUpgradeChoicePending = true;
        offer.runUpgradeChoiceCount = LevelUpRunUpgradeChoices;
        offer.runUpgradePaidChoicePrice = PaidRunUpgradeChoicePrice;
        offer.runUpgradeSelectionsRemaining = 1;
        offer.runUpgradeChoices = {RunUpgradeEffect::Damage, RunUpgradeEffect::MoveSpeed,
            RunUpgradeEffect::AttackSpeed, RunUpgradeEffect::Luck};
        offer.runUpgradeRarities.fill(RunUpgradeRarity::Rare);
        const auto restore = [&](int coins) {
            offer.coins = coins;
            run->returnToMainMenu();
            require(run->loadSuspendedRunState(offer), "paid level-up offer loads with its price");
        };
        restore(PaidRunUpgradeChoicePrice - 1);
        const auto beforeRejected = run->saveSuspendedRunState();
        require(!run->selectRunUpgrade(3), "insufficient coins cannot select the paid alternative");
        const auto rejected = run->saveSuspendedRunState();
        require(rejected.coins == offer.coins &&
            rejected.runUpgradeStacks == beforeRejected.runUpgradeStacks &&
            rejected.runUpgradePower == beforeRejected.runUpgradePower &&
            rejected.runUpgradeSelectionsRemaining == 1 && rejected.runUpgradeChoicePending &&
            rejected.runUpgradeChoices == offer.runUpgradeChoices &&
            rejected.runUpgradeRarities == offer.runUpgradeRarities &&
            rejected.runUpgradePaidChoicePrice == PaidRunUpgradeChoicePrice,
            "rejected purchase preserves coins, upgrade power and the entire offer");
        restore(PaidRunUpgradeChoicePrice);
        require(run->selectRunUpgrade(3), "exact coin balance buys the fourth alternative");
        const auto bought = run->saveSuspendedRunState();
        require(bought.coins == 0 && !bought.runUpgradeChoicePending &&
            bought.runUpgradeSelectionsRemaining == 0 && !run->selectRunUpgrade(0),
            "paid choice spends once and consumes the same single level-up selection");
        requireNear(bought.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::Luck)],
            runUpgradePowerGain(RunUpgradeEffect::Luck, RunUpgradeRarity::Rare), 1e-12,
            "paid card applies ordinary rarity power without a premium bonus");
        for (std::size_t choice = 0; choice < DefaultRunUpgradeChoices; ++choice) {
            restore(0);
            require(run->selectRunUpgrade(choice) && run->snapshot().coins == 0,
                "all three free cards work without coins");
        }
        restore(50);
        require(run->selectRunUpgrade(0) && run->snapshot().coins == 50,
            "free choice preserves the entire coin balance");
        offer.runUpgradeChoiceCount = DefaultRunUpgradeChoices;
        offer.runUpgradePaidChoicePrice = 0;
        restore(0);
        require(run->snapshot().runUpgradeChoiceCount == DefaultRunUpgradeChoices &&
            run->selectRunUpgrade(2), "legacy pending three-card offers stay free");
    }
    const auto rerollWithSavedToken = [](Simulation& run) {
        auto checkpoint = run.saveSuspendedRunState();
        ++checkpoint.runUpgradeRerollTokens;
        run.returnToMainMenu();
        return run.loadSuspendedRunState(checkpoint) && run.rerollRunUpgrades();
    };
    {
        require(runUpgradeIndex(RunUpgradeEffect::AttackKnockback) == 55 &&
            runUpgradeIndex(RunUpgradeEffect::PoisonChance) == 56,
            "poison appends a stable save ID without reordering previous upgrades");
        requireNear(runPoisonChance(1), .10, 1e-12, "common venom gives ten percent proc chance");
        requireNear(runPoisonChance(2), .20, 1e-12, "legendary venom scales chance");
        requireNear(runPoisonChance(10000), 1, 1e-12, "debug venom cannot exceed guaranteed chance");
        auto run = std::make_unique<Simulation>();
        run->startRunFromSeed(PlayerClass::Vanguard, 12345);
        auto saved = run->saveSuspendedRunState();
        saved.runUpgradeChoicePending = true;
        saved.runUpgradeChoiceCount = 4;
        saved.runUpgradeSelectionsRemaining = 1;
        saved.runUpgradeChoices = {RunUpgradeEffect::PoisonChance, RunUpgradeEffect::Damage,
            RunUpgradeEffect::BuildingHealth, RunUpgradeEffect::Luck};
        saved.runUpgradeRarities.fill(RunUpgradeRarity::Legendary);
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(saved), "retired venom offer is repaired on load");
        for (std::size_t choice = 0; choice < run->snapshot().runUpgradeChoiceCount; ++choice)
            require(run->snapshot().runUpgradeChoices[choice] != RunUpgradeEffect::PoisonChance,
                "retired venom cannot appear in normal rewards");
        const auto index = runUpgradeIndex(RunUpgradeEffect::PoisonChance);
        requireNear(run->snapshot().runUpgradePower[index], 0, 1e-12, "retired venom offer grants no power");
        saved = run->saveSuspendedRunState();
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(saved), "venom upgrade survives save/load");
        requireNear(run->snapshot().runUpgradePower[index], 0, 1e-12, "load keeps retired venom inactive");
        run->returnToMainMenu();
        run->startRunFromSeed(PlayerClass::Vanguard, 12345);
        require(run->snapshot().runUpgradePower[index] == 0, "new runs clear venom");
        PlayerCommand build;
        build.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
        build.placeBuilding = PlaceBuildingCommand{BuildingType::Core, {0, 0}, 0};
        run->tick(0, build);
        require(run->snapshot().coreId.has_value(), "poison fixture places its core");
        while (run->snapshot().runUpgradeChoicePending) {
            const auto& offer = run->snapshot();
            std::size_t choice = 0;
            while (choice < offer.runUpgradeChoiceCount && offer.runUpgradeChoices[choice] == RunUpgradeEffect::PoisonChance) ++choice;
            require(choice < offer.runUpgradeChoiceCount && run->selectRunUpgrade(choice),
                "core fixture resolves unrelated progression rewards");
        }
        PlayerCommand spawn;
        spawn.spawnEnemy = SpawnEnemyCommand{.type = EnemyType::Heavy, .count = 1};
        run->tick(0, spawn);
        require(!run->snapshot().enemies.empty(), "poison integration gets an enemy fixture");
        const auto id = run->snapshot().enemies.front().id;
        PlayerCommand attack;
        attack.castChainLightning = CastChainLightningCommand{.firstTarget = id, .damage = 10, .maximumTargets = 1};
        run->tick(0, attack);
        require(!enemyHasStatus(run->snapshot().enemies.front(), StatusEffectType::Poison),
            "without venom direct hits never poison");
        require(run->grantDebugRunUpgradeBatch(RunUpgradeEffect::PoisonChance, 10),
            "debug menu supports stacked venom grants");
        attack.castChainLightning->damage = run->snapshot().enemies.front().health * .6;
        run->tick(0, attack);
        require(enemyHasStatus(run->snapshot().enemies.front(), StatusEffectType::Poison),
            "guaranteed venom procs from a real player attack");
        const double dps = enemyStatusEffect(run->snapshot().enemies.front(), StatusEffectType::Poison).intensity;
        run->tick(.5, {});
        requireNear(enemyStatusEffect(run->snapshot().enemies.front(), StatusEffectType::Poison).intensity,
            dps, 1e-9, "poison tick cannot proc or multiply its own dose");
        for (int i = 0; i < 8; ++i) run->tick(.5, {});
        require(!run->snapshot().enemies.front().active, "poison kills through gameplay updates");
    }
    for (const auto& tome : RunTomeDefinitions) {
        if (retiredRunUpgrade(tome.effect)) continue;
        require(runUpgradeOfferWeight(tome.effect) == 2,
            "eligible tomes have equal weights, independent of ownership and level");
    }
    {
        const std::array pool{RunUpgradeEffect::Damage, RunUpgradeEffect::AttackSpeed,
            RunUpgradeEffect::MoveSpeed, RunUpgradeEffect::BuildingHealth, RunUpgradeEffect::Luck};
        std::array<int,RunUpgradeEffectCount> stacks{};
        stacks[runUpgradeIndex(RunUpgradeEffect::Damage)]=7;
        stacks[runUpgradeIndex(RunUpgradeEffect::AttackSpeed)]=1;
        std::array<int,5> hits{};
        std::uint64_t random=12345;
        for (int i=0;i<100000;++i) ++hits[chooseRunUpgradeOffer(pool,stacks,random)];
        const int owned=hits[0]+hits[1];
        require(owned>44000 && owned<46000,"two owned tomes share the fixed existing-category chance");
        require(std::abs(hits[0]-hits[1])<1000,"owned tome level does not bias draws");
        for (std::size_t i=2;i<5;++i) require(hits[i]>17300 && hits[i]<19400,
            "new tomes remain available at equal weight to each other");
        const std::array ownedOnly{pool[0],pool[1]};
        for (int i=0;i<1000;++i) require(chooseRunUpgradeOffer(ownedOnly,stacks,random)<2,
            "full tome slots gracefully use only owned group");
        stacks.fill(0);
        for (int i=0;i<1000;++i) require(chooseRunUpgradeOffer(pool,stacks,random)<pool.size(),
            "zero owned tomes gracefully use new group");
        stacks[runUpgradeIndex(RunUpgradeEffect::Damage)]=7;
        stacks[runUpgradeIndex(RunUpgradeEffect::AttackSpeed)]=1;
        std::array<bool,4> seenOwnedCounts{};
        for (int screen=0;screen<20000;++screen) {
            std::vector<RunUpgradeEffect> remaining(pool.begin(),pool.end());
            int ownedChoices=0;
            for (int card=0;card<3;++card) {
                const auto selected=chooseRunUpgradeOffer(remaining,stacks,random);
                ownedChoices+=remaining[selected]==pool[0] || remaining[selected]==pool[1];
                remaining.erase(remaining.begin()+static_cast<std::ptrdiff_t>(selected));
            }
            seenOwnedCounts[static_cast<std::size_t>(ownedChoices)]=true;
        }
        require(seenOwnedCounts[0] && seenOwnedCounts[1] && seenOwnedCounts[2],
            "one shared pool can offer three new, one owned or two owned cards");
    }
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

    {
        auto legacyRun = std::make_unique<Simulation>();
        legacyRun->startRunFromSeed(PlayerClass::Vanguard, 12345);
        PlayerCommand unlimited;
        unlimited.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
        legacyRun->tick(1.0 / 60.0, unlimited);
        PlayerCommand placeCore;
        placeCore.placeBuilding = PlaceBuildingCommand{.type = BuildingType::Core, .gridPosition = {0, 0}};
        legacyRun->tick(1.0 / 60.0, placeCore);
        require(legacyRun->snapshot().coreId.has_value(), "saved radius fixture has a core");
        auto legacy = legacyRun->saveSuspendedRunState();
        auto core = std::ranges::find(legacy.buildings, BuildingType::Core, &BuildingInstance::type);
        core->level = 3;
        core->maxHealth = buildingMaxHealthAtLevel(500.0, 3);
        core->health = core->maxHealth;
        legacy.coreBuildRadius = 14; // Legacy base radius plus an acquired range bonus.
        legacy.coreBuildRadiusLevel = 1;
        legacy.playerWeaponArsenalVersion = 0;
        legacy.selectedWeapon = PlayerWeapon::Rifle;
        legacy.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::BuildRadius)] = 1;
        legacy.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::BuildRadius)] = 1.5;
        legacyRun->returnToMainMenu();
        require(legacyRun->loadSuspendedRunState(legacy), "legacy core radius migrates");
        auto resumed = legacyRun->saveSuspendedRunState();
        require(resumed.coreBuildRadius == 16 && resumed.coreBuildRadiusLevel == 3 &&
            resumed.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::BuildRadius)] == 1.5,
            "migration adds core progression on top of old hammer bonuses");
        require(legacyRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Club)] &&
            legacyRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Rifle)] &&
            !legacyRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Bomb)] &&
            legacyRun->snapshot().selectedWeapon == PlayerWeapon::Rifle,
            "old core-level unlocks migrate without removing the equipped weapon");
        legacyRun->returnToMainMenu();
        require(legacyRun->loadSuspendedRunState(resumed) &&
            legacyRun->saveSuspendedRunState().coreBuildRadius == 16,
            "repeated load never applies radius progression twice");
        resumed.coreBuildRadiusLevel = 4;
        legacyRun->returnToMainMenu();
        require(!legacyRun->loadSuspendedRunState(resumed), "save rejects an accounted level above its core");
    }

    {
        auto weaponRun = std::make_unique<Simulation>();
        weaponRun->startRunFromSeed(PlayerClass::Vanguard, 13579);
        require(weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Axe)] &&
            weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Pickaxe)],
            "gathering tools remain available without cards or core");
        for (const auto weapon : {PlayerWeapon::Club, PlayerWeapon::Rifle, PlayerWeapon::Bomb,
                PlayerWeapon::IceWand, PlayerWeapon::FireWand}) {
            require(!weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(weapon)],
                "combat weapons start locked until their acquisition card");
            const auto effect = *weaponRunUpgrade(weapon);
            require(runUpgradeMaximumLevel(effect) == (weapon==PlayerWeapon::Bomb?1:MaxBuildingLevel) && fixedRunUpgradePower(effect) &&
                fixedRunUpgradeRarity(effect) == RunUpgradeRarity::Common,
                "weapon cards have fixed power and leveled weapons extend to eight");
            const auto inventory = weaponRun->snapshot().unlockedWeapons;
            const auto count = std::ranges::count_if(PlayerWeaponHotbarOrder,
                [&](auto entry) { return inventory[static_cast<std::size_t>(entry)]; });
            if (count >= weaponRun->snapshot().weaponSlotLimit) {
                // The remaining catalog becomes an explicit over-cap legacy/debug
                // fixture; normal acquisition is covered by WeaponArsenalTests.
                require(weaponRun->grantDebugRunUpgradeBatch(effect, 1),
                    "complete weapon catalog uses explicit debug access beyond its slot cap");
            } else {
                // Exercise the normal selection command through a saved pending offer.
                auto offer = weaponRun->saveSuspendedRunState();
                offer.runUpgradeChoicePending = true;
                offer.runUpgradeChoiceCount = 4;
                offer.runUpgradeSelectionsRemaining = 1;
                offer.runUpgradeChoices = {effect, RunUpgradeEffect::Damage,
                    RunUpgradeEffect::BuildingHealth, RunUpgradeEffect::Luck};
                offer.runUpgradeRarities.fill(RunUpgradeRarity::Common);
                weaponRun->returnToMainMenu();
                require(weaponRun->loadSuspendedRunState(offer) && weaponRun->selectRunUpgrade(0),
                    "normal card selection acquires a player weapon");
            }
            require(weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(weapon)] &&
                weaponRun->snapshot().buildingArsenalCount == 1 &&
                !weaponRun->grantDebugRunUpgrade(effect),
                "weapon acquisition does not consume a building slot and cannot repeat");
            PlayerCommand equip;
            equip.selectWeapon = SelectWeaponCommand{weapon};
            weaponRun->tick(1.0 / 60.0, equip);
            require(weaponRun->snapshot().selectedWeapon == weapon,
                "newly acquired weapon can be equipped without upgrading the core");
        }
        require(weaponRun->snapshot().bombsRemaining == std::numeric_limits<int>::max(),
            "bomb blueprint unlocks unlimited throws");
        auto acquired = weaponRun->saveSuspendedRunState();
        require(acquired.playerWeaponArsenalVersion == 2, "weapon ownership uses the new save version");
        weaponRun->returnToMainMenu();
        require(weaponRun->loadSuspendedRunState(acquired), "acquired player weapons resume");
        for (const auto weapon : {PlayerWeapon::Club, PlayerWeapon::Rifle, PlayerWeapon::Bomb,
                PlayerWeapon::IceWand, PlayerWeapon::FireWand})
            require(weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(weapon)],
                "all acquired weapons survive saving");
        auto invalid = acquired;
        invalid.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockRifle)] = 9;
        weaponRun->returnToMainMenu();
        require(!weaponRun->loadSuspendedRunState(invalid), "duplicate saved weapon acquisition is rejected");
        weaponRun->startRunFromSeed(PlayerClass::Alchemist, 13579);
        require(weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::FireWand)] &&
            !weaponRun->grantDebugRunUpgrade(RunUpgradeEffect::UnlockFireWand) &&
            !weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Rifle)],
            "Alchemist retains its starter while a new run clears other weapon acquisitions");
        PlayerCommand unlimited;
        unlimited.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
        weaponRun->tick(1.0 / 60.0, unlimited);
        require(weaponRun->snapshot().unlockedWeapons[static_cast<std::size_t>(PlayerWeapon::Rifle)],
            "unlimited mode continues to bypass weapon acquisitions");
    }

    auto simulation = std::make_unique<Simulation>();
    MetaRunBonuses rarityBonuses; rarityBonuses.additionalTomeSlots=2;
    require(simulation->configureNextRunBonuses(rarityBonuses), "rarity fixture has four purchased tome slots");
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
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::ProjectileCount,
        RunUpgradeRarity::Common), "common discrete upgrade granted");
    require(simulation->snapshot().upgradeStats.projectileCountBonus == 1,
        "a common count upgrade grants one floating-point power unit");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::ProjectileCount,
        RunUpgradeRarity::Legendary), "last partial bonus granted");
    require(simulation->snapshot().upgradeStats.projectileCountBonus == 3 &&
        simulation->grantDebugRunUpgrade(RunUpgradeEffect::ProjectileCount),
        "quantity keeps accumulating beyond three additional projectiles");

    constexpr std::array expectedDamageBonus{0.10, 0.12, 0.14, 0.16, 0.20};
    constexpr std::array expectedHealthBonus{12.0, 14.4, 16.8, 19.2, 24.0};
    auto tierSimulation = std::make_unique<Simulation>();
    for (std::size_t tierIndex = 0; tierIndex < RunUpgradeRarityCount; ++tierIndex) {
        tierSimulation->returnToMainMenu();
        tierSimulation->startRunFromSeed(PlayerClass::Vanguard, 6789);
        const auto tier = static_cast<RunUpgradeRarity>(tierIndex);
        const auto initial = tierSimulation->snapshot();
        require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, tier) &&
            tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::MaximumHealth, tier) &&
            tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::DefenseDamage, tier),
            "each rarity grants its numeric upgrades");
        const auto& actual = tierSimulation->snapshot();
        requireNear(actual.playerDamageMultiplier / initial.playerDamageMultiplier,
            1.0 + expectedDamageBonus[tierIndex], 1e-12,
            "rarity changes actual player damage, not only the card text");
        requireNear(actual.playerMaxHealth - initial.playerMaxHealth,
            expectedHealthBonus[tierIndex], 1e-12, "each rarity grants the expected max HP");
        requireNear(actual.upgradeStats.defenseDamage - initial.upgradeStats.defenseDamage,
            expectedDamageBonus[tierIndex] * 2.4, 1e-12,
            "defense damage follows the same rarity scale");
        require(actual.coreBuildRadius == initial.coreBuildRadius &&
            !tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::BuildRadius, tier),
            "retired range hammer cannot compete with core progression");
    }

    tierSimulation->returnToMainMenu();
    tierSimulation->startRunFromSeed(PlayerClass::Vanguard, 6789);
    require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::ClusterPayload,
        RunUpgradeRarity::Uncommon), "uncommon cluster upgrade granted");
    require(tierSimulation->snapshot().upgradeStats.clusterExplosions == 2.4,
        "uncommon cluster card retains 2.4 stored explosions");
    const auto clusterSave = tierSimulation->saveSuspendedRunState();
    auto clusterRestored = std::make_unique<Simulation>();
    require(clusterRestored->loadSuspendedRunState(clusterSave) &&
        clusterRestored->snapshot().upgradeStats.clusterExplosions == 2.4,
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
        tierSimulation->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::GunShrapnel)]) == 3,
        "uncommon +3 shards floors 3.6 stored shards to three");
    require(tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary) &&
        tierSimulation->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Legendary),
        "all five legendary levels retain their full bonus");
    requireNear(tierSimulation->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::Damage)],
        10.0, 1e-12, "numeric power cap matches the highest rarity");

    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCannon),
        "checkpoint fixture owns its cannon specialization options");
    PlayerCommand xp;
    xp.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
    xp.grantInsight = GrantInsightCommand{1000.0};
    simulation->tick(1.0 / 60.0, xp);
    require(simulation->snapshot().runUpgradeChoicePending, "XP produces upgrade offers");
    auto saved = simulation->saveSuspendedRunState();
    saved.runUpgradeChoiceCount = 4;
    saved.runUpgradeChoices = {RunUpgradeEffect::Damage, RunUpgradeEffect::Luck,
        RunUpgradeEffect::ProjectileCount, RunUpgradeEffect::MoveSpeed};
    auto restored = std::make_unique<Simulation>();
    require(restored->loadSuspendedRunState(saved), "rarity checkpoint restores");
    require(restored->snapshot().runUpgradeRarities == saved.runUpgradeRarities &&
        restored->snapshot().runUpgradeChoices == saved.runUpgradeChoices &&
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
            arsenal->snapshot().runUpgradeStacks[runUpgradeIndex(*buildingRunUpgrade(character.startingBuilding))] == 1,
            "every character starts with one building at level one");
        require(arsenal->grantDebugRunUpgrade(*buildingRunUpgrade(character.startingBuilding)) &&
            arsenal->snapshot().runUpgradeStacks[runUpgradeIndex(*buildingRunUpgrade(character.startingBuilding))] == 2,
            "starting building can advance without a core level gate");
    }
    arsenal->returnToMainMenu();
    arsenal->startRunFromSeed(PlayerClass::Vanguard, 4321);
    auto levelTwoCore = arsenal->saveSuspendedRunState();
    levelTwoCore.buildings.push_back({.id = {1000, 1}, .type = BuildingType::Core,
        .gridPosition = {0, 0}, .level = 2, .health = 500, .maxHealth = 500});
    arsenal->returnToMainMenu();
    require(arsenal->loadSuspendedRunState(levelTwoCore), "arsenal fixture opens shared level two through the core");
    require(arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCannon, RunUpgradeRarity::Legendary) &&
        arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockTurret) &&
        !arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockSpikeTrap) &&
        arsenal->snapshot().buildingArsenalCount == RunBuildingSlotCount &&
        !arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCatapult) &&
        arsenal->grantDebugRunUpgrade(RunUpgradeEffect::UnlockCannon) &&
        arsenal->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] == 2,
        "three types block new acquisitions but allow shared upgrades regardless of rarity");
    const auto arsenalSave = arsenal->saveSuspendedRunState();
    restored->returnToMainMenu();
    require(arsenalSave.buildingArsenalVersion == 5 && restored->loadSuspendedRunState(arsenalSave) &&
        restored->snapshot().buildingArsenalCount == BaseRunBuildingSlots &&
        restored->snapshot().runUpgradeStacks == arsenal->snapshot().runUpgradeStacks,
        "unbuilt blueprints survive save and resume");
    auto invalidArsenal = arsenalSave;
    invalidArsenal.buildingArsenalVersion = 6;
    restored->returnToMainMenu();
    require(!restored->loadSuspendedRunState(invalidArsenal), "future arsenal formats rejected");
    invalidArsenal = arsenalSave;
    invalidArsenal.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] = MaxStructureLevel + 1;
    require(!restored->loadSuspendedRunState(invalidArsenal), "building levels above stored range rejected");

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
        require(unlocked[static_cast<std::size_t>(type)] ==
            (type == BuildingType::Cannon || type == BuildingType::Wall ||
             std::ranges::find(RunEconomyBuildingTypes, type) != RunEconomyBuildingTypes.end()),
            "construction catalog includes the starter, walls and all producers");
    }
    require(normal->previewPlacement(BuildingType::Turret, {4, 0}).error == PlacementError::BlueprintRequired &&
        normal->previewPlacement(BuildingType::LumberMill, {4, 0}).error != PlacementError::BlueprintRequired,
        "combat needs a card, while producers are placeable immediately");
    require(normal->snapshot().runUpgradeChoicePending,
        "normal construction XP produces level-up cards");
    const auto offer = normal->snapshot();
    require(offer.runUpgradeChoiceCount == LevelUpRunUpgradeChoices,
        "level-up presents three free choices and a paid alternative");
    for (std::size_t index = 0; index < offer.runUpgradeChoiceCount; ++index) {
        const auto offeredEffect = offer.runUpgradeChoices[index];
        require(offeredEffect != RunUpgradeEffect::GunShrapnel && offeredEffect != RunUpgradeEffect::ReactiveTraps,
            "family-specific cards require a compatible owned building");
        if (const auto building = runUpgradeBuilding(offeredEffect);
            building && (!runCombatBuilding(*building) ||
                offer.runUpgradeStacks[runUpgradeIndex(offeredEffect)] == 0)) {
            require(offer.runUpgradeRarities[index] == RunUpgradeRarity::Common,
                "new building acquisitions keep fixed one-level power");
        }
    }

    for (std::size_t index = 0; index < offer.runUpgradeChoiceCount; ++index) {
        require(!runUpgradeProductionBuilding(offer.runUpgradeChoices[index]) &&
            offer.runUpgradeChoices[index] != RunUpgradeEffect::ProductionSpeed &&
            (!runUpgradeBuilding(offer.runUpgradeChoices[index]) ||
             std::ranges::find(RunEconomyBuildingTypes,
                 *runUpgradeBuilding(offer.runUpgradeChoices[index])) == RunEconomyBuildingTypes.end()),
            "production improvements are absent until a producer is built");
    }
    auto pendingArsenal = normal->saveSuspendedRunState();
    pendingArsenal.runUpgradeChoiceCount = DefaultRunUpgradeChoices;
    pendingArsenal.runUpgradePaidChoicePrice = 0;
    pendingArsenal.runUpgradeChoices = {RunUpgradeEffect::UnlockTurret,
        RunUpgradeEffect::Damage, RunUpgradeEffect::MoveSpeed};
    pendingArsenal.runUpgradeRarities[0] = RunUpgradeRarity::Common;
    auto normalRestored = std::make_unique<Simulation>(balance, map, world, fastInsight);
    normalRestored->returnToMainMenu();
    require(normalRestored->loadSuspendedRunState(pendingArsenal) &&
        normalRestored->snapshot().runUpgradeChoices == pendingArsenal.runUpgradeChoices &&
        normalRestored->snapshot().runUpgradeRarities == pendingArsenal.runUpgradeRarities &&
        normalRestored->selectRunUpgrade(0) && normalRestored->snapshot().buildingArsenalCount == 2,
        "resume preserves exact offers and selection acquires or upgrades a type");
    normal->returnToMainMenu();
    require(normal->loadSuspendedRunState(pendingArsenal) && normal->selectRunUpgrade(0),
        "normal acquisition selected");
    auto mixedRun = std::make_unique<Simulation>(balance, map, world, fastInsight);
    mixedRun->returnToMainMenu();
    require(mixedRun->loadSuspendedRunState(normal->saveSuspendedRunState()) &&
        mixedRun->grantDebugRunUpgrade(RunUpgradeEffect::Damage) &&
        mixedRun->grantDebugRunUpgrade(RunUpgradeEffect::AttackSpeed),
        "mixed offer fixture owns repeatable tomes");
    auto mixedOffer = mixedRun->saveSuspendedRunState();
    mixedOffer.runUpgradeChoicePending = true;
    mixedOffer.runUpgradeChoiceCount = 3;
    mixedOffer.runUpgradeSelectionsRemaining = 1;
    mixedOffer.runUpgradeRerollTokens = 48;
    mixedOffer.runUpgradeChoices = {RunUpgradeEffect::UnlockTurret,
        RunUpgradeEffect::Damage, RunUpgradeEffect::MoveSpeed};
    mixedRun->returnToMainMenu();
    require(mixedRun->loadSuspendedRunState(mixedOffer), "mixed offer fixture resumes");
    bool sawAllNew = false, sawMixed = false, sawNonBuildingFirst = false;
    for (int roll = 0; roll < 48; ++roll) {
        require(mixedRun->rerollRunUpgrades(), "mixed level-up rerolls");
        const auto choices = mixedRun->snapshot();
        std::size_t newCount = 0;
        for (std::size_t index = 0; index < choices.runUpgradeChoiceCount; ++index) {
            const auto choiceEffect = choices.runUpgradeChoices[index];
            const auto building = runUpgradeBuilding(choiceEffect);
            const auto weapon = runUpgradeWeapon(choiceEffect);
            const bool owned = building
                ? choices.unlockedBuildings[static_cast<std::size_t>(*building)]
                : weapon
                    ? choices.unlockedWeapons[static_cast<std::size_t>(*weapon)]
                    : (runTomeDefinition(choiceEffect)
                        ? runTomeLevel(choiceEffect, choices.runUpgradeStacks) > 0
                        : choices.runUpgradeStacks[runUpgradeIndex(choiceEffect)] > 0);
            newCount += !owned;
            for (std::size_t other = 0; other < index; ++other)
                require(choices.runUpgradeChoices[other] != choiceEffect,
                    "level-up choices are unique");
        }
        sawAllNew |= newCount == 3;
        sawMixed |= newCount < 3;
        sawNonBuildingFirst |= !runUpgradeBuilding(choices.runUpgradeChoices[0]);
    }
    require(sawAllNew && sawMixed && sawNonBuildingFirst,
        "shared pool permits all-new or mixed screens without a forced first building");
    for (auto blueprintEffect : {RunUpgradeEffect::UnlockGunTurret, RunUpgradeEffect::UnlockTurret,
                        RunUpgradeEffect::UnlockSpikeTrap, RunUpgradeEffect::UnlockSlowTrap,
                        RunUpgradeEffect::UnlockCatapult}) {
        if (normal->snapshot().buildingArsenalCount < 4) (void)normal->grantDebugRunUpgrade(blueprintEffect);
    }
    require(rerollWithSavedToken(*normal),
        "full arsenal offer regenerated");
    for (std::size_t index = 0; index < normal->snapshot().runUpgradeChoiceCount; ++index) {
        const auto building = runUpgradeBuilding(normal->snapshot().runUpgradeChoices[index]);
        require(!building || !runSlottedBuilding(*building) ||
            normal->snapshot().runUpgradeStacks[runUpgradeIndex(normal->snapshot().runUpgradeChoices[index])] > 0,
            "full structure arsenal offers only owned structure upgrades");
    }

    for (auto type : RunEconomyBuildingTypes) {
        const auto producerCard = *buildingRunUpgrade(type);
        const auto slotsBefore = normal->snapshot().buildingArsenalCount;
        require(normal->grantDebugRunUpgradeBatch(producerCard, 2),
            "debug can raise producers even beyond normal slot limits");
        require(normal->snapshot().buildingArsenalCount == slotsBefore &&
            normal->snapshot().unlockedBuildings[static_cast<std::size_t>(type)],
            "producer cards never consume a defense slot");
    }
    // Already-acquired cards remain valid in old runs after being retired.
    auto legacyNumericCards = normal->saveSuspendedRunState();
    for (auto retiredEffect : {RunUpgradeEffect::LumberMillProduction, RunUpgradeEffect::QuarryProduction,
            RunUpgradeEffect::CrystalMineProduction, RunUpgradeEffect::DoubleDown, RunUpgradeEffect::RerollToken,
            RunUpgradeEffect::BuildRadius, RunUpgradeEffect::Salvager}) {
        require(retiredRunUpgrade(retiredEffect) && !normal->grantDebugRunUpgrade(retiredEffect),
            "retired numeric and selection cards cannot be newly granted");
        legacyNumericCards.runUpgradeStacks[runUpgradeIndex(retiredEffect)] = 1;
        legacyNumericCards.runUpgradePower[runUpgradeIndex(retiredEffect)] = fixedRunUpgradePower(retiredEffect) ? 1.0 : 2.0;
    }
    legacyNumericCards.runUpgradeRerollTokens = 3;
    legacyNumericCards.bonusSelectionsNextReward = 2;
    normal->returnToMainMenu();
    require(normal->loadSuspendedRunState(legacyNumericCards), "old acquired cards remain compatible");
    const auto economySave = normal->saveSuspendedRunState();
    require(economySave.runUpgradePower == legacyNumericCards.runUpgradePower &&
            economySave.runUpgradeRerollTokens == 3 && economySave.bonusSelectionsNextReward == 2,
        "retiring cards preserves existing power, rerolls and deferred selections");
    normalRestored->returnToMainMenu();
    require(normalRestored->loadSuspendedRunState(economySave) &&
        normalRestored->snapshot().runUpgradeStacks == normal->snapshot().runUpgradeStacks &&
        normalRestored->snapshot().buildingArsenalCount == normal->snapshot().buildingArsenalCount,
        "unbuilt economy blueprints and type improvements survive resume");
    auto invalidEconomy = economySave;
    invalidEconomy.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockLumberMill)] = 9;
    normalRestored->returnToMainMenu();
    require(!normalRestored->loadSuspendedRunState(invalidEconomy), "economic levels beyond eight are rejected");
    normalRestored->startRunFromSeed(PlayerClass::Engineer, 4321);
    for (auto type : RunEconomyBuildingTypes) require(!normalRestored->snapshot().unlockedBuildings[static_cast<std::size_t>(type)] &&
        normalRestored->snapshot().runUpgradeStacks[runUpgradeIndex(*buildingRunUpgrade(type))] == 0,
        "new runs hide producer placement until Core construction, without granting producer cards");

    normalRestored->returnToMainMenu();
    auto producerPending = pendingArsenal;
    producerPending.buildings.push_back({.id = {1001, 1},
        .type = BuildingType::LumberMill, .gridPosition = {4, 0},
        .level = 1, .health = 150, .maxHealth = 150});
    producerPending.runUpgradeChoices = {RunUpgradeEffect::UnlockLumberMill,
        RunUpgradeEffect::Damage, RunUpgradeEffect::MoveSpeed};
    require(normalRestored->loadSuspendedRunState(producerPending) && normalRestored->selectRunUpgrade(0),
        "normal card selection upgrades a constructed producer");
    auto oneProducer = normalRestored->saveSuspendedRunState();
    oneProducer.runUpgradeRerollTokens = 32;
    normalRestored->returnToMainMenu();
    require(normalRestored->loadSuspendedRunState(oneProducer), "single-producer offer fixture resumes");

    for (int roll = 0; roll < 32; ++roll) {
        require(normalRestored->rerollRunUpgrades(), "single-producer choices rerolled");
        const auto candidates = normalRestored->snapshot();
        for (std::size_t index = 0; index < candidates.runUpgradeChoiceCount; ++index) {
            const auto producerEffect = candidates.runUpgradeChoices[index];
            require(!retiredRunUpgrade(producerEffect) && !runUpgradeProductionBuilding(producerEffect),
                "retired cards stay absent across repeated normal offers");
        }
    }
    // Verify eligibility directly: an expanded weighted deck need not draw a
    // particular card within a fixed number of random offers.
    auto productionOffer = normalRestored->saveSuspendedRunState();
    productionOffer.runUpgradeChoices[0] = RunUpgradeEffect::ProductionSpeed;
    productionOffer.runUpgradeRarities[0] = RunUpgradeRarity::Common;
    normalRestored->returnToMainMenu();
    require(normalRestored->loadSuspendedRunState(productionOffer) &&
        normalRestored->snapshot().runUpgradeChoices[0] == RunUpgradeEffect::ProductionSpeed &&
        normalRestored->selectRunUpgrade(0),
        "acquiring a producer retains eligibility for the shared production hammer");
    auto retiredPending = pendingArsenal;
    retiredPending.runUpgradeChoices = {RunUpgradeEffect::RerollToken, RunUpgradeEffect::DoubleDown,
        RunUpgradeEffect::LumberMillProduction, RunUpgradeEffect::QuarryProduction};
    retiredPending.runUpgradeRarities.fill(RunUpgradeRarity::Common);
    normalRestored->returnToMainMenu();
    require(normalRestored->loadSuspendedRunState(retiredPending), "old pending retired cards resume");
    const auto refreshedPending = normalRestored->snapshot();
    require(refreshedPending.runUpgradeChoicePending && refreshedPending.runUpgradeSelectionsRemaining ==
        retiredPending.runUpgradeSelectionsRemaining, "replacement offers preserve pending selections");
    for (std::size_t index = 0; index < refreshedPending.runUpgradeChoiceCount; ++index)
        require(!retiredRunUpgrade(refreshedPending.runUpgradeChoices[index]),
            "old pending retired cards are replaced with current choices");

    auto legacyEconomy = arsenalSave;
    legacyEconomy.buildingArsenalVersion = 1;
    legacyEconomy.coreBuildRadiusLevel = 1;
    legacyEconomy.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] = 1;
    legacyEconomy.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] = 1;
    legacyEconomy.buildings = {
        {.id = {1000, 1}, .type = BuildingType::Core, .gridPosition = {0, 0}, .health = 500, .maxHealth = 500},
        {.id = {1001, 1}, .type = BuildingType::Quarry, .gridPosition = {4, 0}, .health = 210, .maxHealth = 210},
    };
    legacyEconomy.resumeState = RunState::BuildPhase;
    restored->returnToMainMenu();
    require(restored->loadSuspendedRunState(legacyEconomy), "legacy economic fixture resumes");
    require(restored->snapshot().buildingArsenalCount == BaseRunBuildingSlots &&
        restored->snapshot().unlockedBuildings[static_cast<std::size_t>(BuildingType::Quarry)],
        "old checkpoints retain built producers without using defense slots");

    auto legacyArsenal = arsenalSave;
    legacyArsenal.buildingArsenalVersion = 0;
    legacyArsenal.coreBuildRadiusLevel = 1;
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
    for (auto type : RunEconomyBuildingTypes) require(arsenal->snapshot().unlockedBuildings[static_cast<std::size_t>(type)],
        "sandbox retains all economic types");
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
        require(rerollWithSavedToken(*normal),
            "specialized arsenal rerolls its remaining upgrades");
        const auto specializedOffers = normal->snapshot();
        for (std::size_t choice = 0; choice < specializedOffers.runUpgradeChoiceCount; ++choice) {
            require(!runUpgradeCannonStyle(specializedOffers.runUpgradeChoices[choice]),
                "chosen cannon specialization removes all other styles from the card pool");
        }
    }

    {
        auto run = std::make_unique<Simulation>();
        run->startRunFromSeed(PlayerClass::Engineer, 12345);
        require(run->snapshot().tomeCount == 0 && run->snapshot().tomeSlotLimit == 3,
            "fresh run has three empty tome slots");
        const std::array choices{RunUpgradeEffect::ProjectileCount, RunUpgradeEffect::CriticalChance, RunUpgradeEffect::GatheringSpeed};
        for (std::size_t index = 0; index < choices.size(); ++index) {
            auto checkpoint = run->saveSuspendedRunState();
            checkpoint.runUpgradeChoicePending = true;
            checkpoint.runUpgradeChoiceCount = choices.size();
            checkpoint.runUpgradeSelectionsRemaining = 1;
            std::copy(choices.begin(), choices.end(), checkpoint.runUpgradeChoices.begin());
            checkpoint.runUpgradeRarities.fill(RunUpgradeRarity::Common);
            run->returnToMainMenu();
            require(run->loadSuspendedRunState(checkpoint) && run->selectRunUpgrade(index),
                "normal card selection acquires universal stat tomes");
            require(run->snapshot().tomeCount == static_cast<int>(index + 1), "different tomes occupy slots");
        }
        auto checkpoint = run->saveSuspendedRunState();
        checkpoint.runUpgradeChoicePending = true;
        checkpoint.runUpgradeChoiceCount = choices.size();
        checkpoint.runUpgradeSelectionsRemaining = 1;
        std::copy(choices.begin(), choices.end(), checkpoint.runUpgradeChoices.begin());
        checkpoint.runUpgradeRarities.fill(RunUpgradeRarity::Common);
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(checkpoint) && run->selectRunUpgrade(0),
            "full inventory still offers and levels an owned tome");
        require(run->snapshot().tomeCount == 3 &&
            run->snapshot().runUpgradeStacks[runUpgradeIndex(choices[0])] == 2,
            "repeat upgrade uses no additional slot");
        // Migrating a suspended offer must drop new tomes that no longer fit.
        checkpoint = run->saveSuspendedRunState();
        checkpoint.runUpgradeChoicePending = true;
        checkpoint.runUpgradeChoiceCount = 4;
        checkpoint.runUpgradeSelectionsRemaining = 1;
        checkpoint.runUpgradeChoices = {RunUpgradeEffect::Luck, RunUpgradeEffect::Damage,
            RunUpgradeEffect::BuildingHealth, RunUpgradeEffect::MoveSpeed};
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(checkpoint), "full tome inventory resumes");
        for (int reroll = 0; reroll < 4; ++reroll) {
            const auto offered = run->snapshot();
            require(offered.runUpgradeChoicePending, "remaining repeat tomes and blueprints offer choices");
            for (std::size_t index = 0; index < offered.runUpgradeChoiceCount; ++index) {
                const auto tomeEffect = offered.runUpgradeChoices[index];
                require(!runTomeDefinition(tomeEffect) || offered.runUpgradeStacks[runUpgradeIndex(tomeEffect)] > 0,
                    "full slots never offer a new tome, including after resume and rerolls");
            }
            require(rerollWithSavedToken(*run), "tome-filtered offers reroll");
        }
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::Luck), "debug can exceed distinct tome slots");
        checkpoint = run->saveSuspendedRunState();
        const auto power = checkpoint.runUpgradePower;
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(checkpoint) && run->snapshot().tomeCount == 4 &&
            run->snapshot().runUpgradePower == power,
            "legacy or debug inventory over the cap preserves every bonus exactly once");
        run->restartRun();
        require(run->snapshot().tomeCount == 0, "restart clears tome ownership");
        run->returnToMainMenu();
        run->startRunFromSeed(PlayerClass::Engineer, 12345);

        checkpoint = run->saveSuspendedRunState();
        checkpoint.runUpgradeChoicePending = true;
        checkpoint.runUpgradeChoiceCount = 4;
        checkpoint.runUpgradeSelectionsRemaining = 1;
        checkpoint.runUpgradeChoices = {RunUpgradeEffect::Luck, RunUpgradeEffect::Damage,
            RunUpgradeEffect::BuildingHealth, RunUpgradeEffect::MoveSpeed};
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(checkpoint), "stale offer fixture loads");
        for (const auto tomeEffect : choices) require(run->grantDebugRunUpgrade(tomeEffect), "debug fills slots behind open offers");
        require(!run->selectRunUpgrade(0) && run->snapshot().tomeCount == 3,
            "selection revalidates a stale fourth tome before applying its bonus");
    }

    {
        auto run = std::make_unique<Simulation>();
        for (int extraSlots : {1, 2}) {
            MetaRunBonuses bonuses;
            bonuses.additionalTomeSlots = extraSlots;
            require(run->configureNextRunBonuses(bonuses), "slot bonus configures the next run");
            run->startRunFromSeed(PlayerClass::Engineer, 12345);
            for (const auto owned : {RunUpgradeEffect::Damage, RunUpgradeEffect::Luck,
                    RunUpgradeEffect::MoveSpeed})
                require(run->grantDebugRunUpgrade(owned), "expanded inventory fixture owns three tomes");
            auto checkpoint = run->saveSuspendedRunState();
            checkpoint.runUpgradeChoicePending = true;
            checkpoint.runUpgradeChoiceCount = 1;
            checkpoint.runUpgradeSelectionsRemaining = 1;
            checkpoint.runUpgradeChoices[0] = RunUpgradeEffect::CriticalChance;
            checkpoint.runUpgradeRarities.fill(RunUpgradeRarity::Common);
            run->returnToMainMenu();
            require(run->loadSuspendedRunState(checkpoint) && run->snapshot().runUpgradeChoiceCount == 1 &&
                run->selectRunUpgrade(0) && run->snapshot().tomeCount == 4,
                "fourth tome fits a purchased slot and single-card offers survive resume");
            if (extraSlots == 2) {
                checkpoint = run->saveSuspendedRunState();
                checkpoint.runUpgradeChoicePending = true;
                checkpoint.runUpgradeChoiceCount = 1;
                checkpoint.runUpgradeSelectionsRemaining = 1;
                checkpoint.runUpgradeChoices[0] = RunUpgradeEffect::AttackSize;
                run->returnToMainMenu();
                require(run->loadSuspendedRunState(checkpoint) && run->selectRunUpgrade(0) &&
                    run->snapshot().tomeCount == 5, "second meta rank allows a fifth distinct tome");
            }
            run->returnToMainMenu();
        }
    }
    {
        auto exhausted = std::make_unique<Simulation>();
        exhausted->startRunFromSeed(PlayerClass::Engineer, 12345);
        for (const auto owned : {RunUpgradeEffect::Damage, RunUpgradeEffect::Luck,
                RunUpgradeEffect::MoveSpeed, RunUpgradeEffect::BuildingHealth})
            require(exhausted->grantDebugRunUpgradeBatch(owned, 5, RunUpgradeRarity::Legendary),
                "exhaustion fixture maximizes four owned tomes");
        for (const auto acquiredEffect : {RunUpgradeEffect::UnlockTurret, RunUpgradeEffect::UnlockGunTurret,
                RunUpgradeEffect::UnlockSpikeTrap, RunUpgradeEffect::UnlockLumberMill,
                RunUpgradeEffect::UnlockQuarry, RunUpgradeEffect::UnlockCrystalMine,
                RunUpgradeEffect::UnlockClub, RunUpgradeEffect::UnlockRifle,
                RunUpgradeEffect::UnlockIceWand, RunUpgradeEffect::UnlockFireWand,
                RunUpgradeEffect::CannonHeavyShell})
            require((runUpgradeWeapon(acquiredEffect) || runDefenseCard(acquiredEffect))
                    ? exhausted->grantDebugRunUpgradeBatch(acquiredEffect, 1)
                    : exhausted->grantDebugRunUpgrade(acquiredEffect),
                "exhaustion fixture explicitly fills weapon slots beyond the normal limit");
        for (const auto card : {RunUpgradeEffect::UnlockCannon, RunUpgradeEffect::UnlockTurret,
                RunUpgradeEffect::UnlockGunTurret, RunUpgradeEffect::UnlockSpikeTrap,
                RunUpgradeEffect::UnlockLumberMill, RunUpgradeEffect::UnlockQuarry,
                RunUpgradeEffect::UnlockCrystalMine})
            require(exhausted->grantDebugRunUpgradeBatch(card, MaxStructureLevel),
                "exhaustion fixture maximizes all owned building types");
        for (const auto weapon : {PlayerWeapon::Axe,PlayerWeapon::Pickaxe,PlayerWeapon::Club,
                PlayerWeapon::Rifle,PlayerWeapon::IceWand,PlayerWeapon::FireWand})
            require(exhausted->grantDebugRunUpgradeBatch(*weaponRunUpgrade(weapon),MaxBuildingLevel),
                "exhaustion fixture maximizes owned weapons and tools");
        for (const auto perk : {RunUpgradeEffect::NightlyBomb, RunUpgradeEffect::ClusterPayload,
                RunUpgradeEffect::ReactiveTraps, RunUpgradeEffect::GunShrapnel})
            require(exhausted->grantDebugRunUpgradeBatch(perk, runUpgradeMaximumLevel(perk)),
                "exhaustion fixture also maximizes weapon perks outside tome slots");
        require(!exhausted->grantDebugRunUpgradeBatch(RunUpgradeEffect::RiskyInvestment, 2),
            "high stakes cannot be granted even by debug");
        auto checkpoint = exhausted->saveSuspendedRunState();
        checkpoint.runUpgradeChoicePending = true;
        checkpoint.runUpgradeChoiceCount = 1;
        checkpoint.runUpgradeSelectionsRemaining = 2;
        checkpoint.runUpgradeChoices[0] = RunUpgradeEffect::RiskyInvestment;
        checkpoint.runUpgradeRarities.fill(RunUpgradeRarity::Common);
        exhausted->returnToMainMenu();
        require(exhausted->loadSuspendedRunState(checkpoint),
            "a save with a retired card resumes");
        while (exhausted->snapshot().runUpgradeChoicePending) {
            require(exhausted->snapshot().runUpgradeChoiceCount > 0 &&
                !retiredRunUpgrade(exhausted->snapshot().runUpgradeChoices[0]) &&
                exhausted->selectRunUpgrade(0),
                "retired card is replaced by a valid offer while the current pool has options");
        }
        require(exhausted->snapshot().runUpgradeSelectionsRemaining == 0,
            "restored reward choices resolve without trapping the run");
        checkpoint = exhausted->saveSuspendedRunState();
        exhausted->returnToMainMenu();
        require(exhausted->loadSuspendedRunState(checkpoint), "exhausted reward state remains resumable");
    }

    {
        auto shared = std::make_unique<Simulation>();
        shared->startRunFromSeed(PlayerClass::Engineer, 12345);
        const auto baseStats = shared->snapshot();
        require(shared->grantDebugRunUpgrade(RunUpgradeEffect::Damage, RunUpgradeRarity::Rare),
            "shared might tome grants both halves");
        requireNear(shared->snapshot().playerDamageMultiplier, baseStats.playerDamageMultiplier * 1.14, 1e-12,
            "might retains original player damage scaling");
        requireNear(shared->snapshot().upgradeStats.defenseDamage, baseStats.upgradeStats.defenseDamage * 1.168, 1e-12,
            "might adds defense damage at the same rarity");
        require(shared->grantDebugRunUpgrade(RunUpgradeEffect::AttackSpeed, RunUpgradeRarity::Rare),
            "rapid tome upgrades defense speed");
        requireNear(shared->snapshot().upgradeStats.playerAttackSpeed, baseStats.upgradeStats.playerAttackSpeed, 1e-12,
            "rapid leaves player weapon speed unchanged");
        requireNear(shared->snapshot().upgradeStats.defenseFireRate, baseStats.upgradeStats.defenseFireRate * 1.14, 1e-12,
            "rapid speeds up defenses");
        requireNear(shared->snapshot().upgradeStats.toolAttackSpeed, baseStats.upgradeStats.toolAttackSpeed, 1e-12,
            "combat speed no longer accelerates gathering tools");
        require(shared->grantDebugRunUpgrade(RunUpgradeEffect::GatheringSpeed, RunUpgradeRarity::Rare),
            "separate gathering tome is grantable");
        requireNear(shared->snapshot().upgradeStats.toolAttackSpeed, baseStats.upgradeStats.toolAttackSpeed * 1.21, 1e-12,
            "gathering speed uses rarity power rather than copy count");
        requireNear(shared->snapshot().upgradeStats.playerAttackSpeed, baseStats.upgradeStats.playerAttackSpeed, 1e-12,
            "gathering speed leaves combat unchanged");
        const auto beforePerks = shared->snapshot();
        for (auto perk : {RunUpgradeEffect::NightlyBomb, RunUpgradeEffect::ClusterPayload,
                RunUpgradeEffect::ReactiveTraps, RunUpgradeEffect::GunShrapnel})
            require(shared->grantDebugRunUpgrade(perk), "weapon perk grants without a tome slot");
        require(shared->snapshot().tomeCount == beforePerks.tomeCount,
            "weapon perks never consume tome capacity");
        auto checkpoint = shared->saveSuspendedRunState();
        checkpoint.playerAttackSpeedMultiplier += 0.21;
        checkpoint.toolAttackSpeedMultiplier += 0.21;
        shared->returnToMainMenu();
        require(shared->loadSuspendedRunState(checkpoint), "shared power and separate tool baseline resume");
        requireNear(shared->snapshot().upgradeStats.playerAttackSpeed, baseStats.upgradeStats.playerAttackSpeed, 1e-12,
            "old rapid tome player-speed bonus is removed on load");
        requireNear(shared->snapshot().upgradeStats.defenseDamage, beforePerks.upgradeStats.defenseDamage, 1e-12,
            "defense half survives save/load without duplicate application");
        requireNear(shared->snapshot().upgradeStats.toolAttackSpeed, beforePerks.upgradeStats.toolAttackSpeed, 1e-12,
            "tool speed survives save/load exactly");
        // Emulate a pre-merge save: old player and defense stats remain independent.
        checkpoint.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::DefenseDamage)] = 1;
        checkpoint.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::DefenseDamage)] = 1;
        checkpoint.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::DefenseFireRate)] = 1;
        checkpoint.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::DefenseFireRate)] = 1;
        shared->returnToMainMenu();
        require(shared->loadSuspendedRunState(checkpoint), "legacy separate defense tomes remain resumable");
        requireNear(shared->snapshot().upgradeStats.defenseDamage, baseStats.upgradeStats.defenseDamage * 1.12, 1e-12,
            "legacy player damage is not retroactively counted as defense damage");
        require(shared->snapshot().tomeCount == 3 &&
            runTomeLevel(RunUpgradeEffect::Damage, shared->snapshot().runUpgradeStacks) == 2,
            "legacy player and defense variants share one displayed slot and level");
        checkpoint.runUpgradeChoicePending = true;
        checkpoint.runUpgradeChoiceCount = 4;
        checkpoint.runUpgradeSelectionsRemaining = 1;
        checkpoint.runUpgradeChoices = {RunUpgradeEffect::DefenseDamage, RunUpgradeEffect::DefenseFireRate,
            RunUpgradeEffect::LongerDay, RunUpgradeEffect::Overkill};
        shared->returnToMainMenu();
        require(shared->loadSuspendedRunState(checkpoint), "legacy pending offers regenerate on resume");
        for (std::size_t i = 0; i < shared->snapshot().runUpgradeChoiceCount; ++i)
            require(!legacyRunTomeUpgrade(shared->snapshot().runUpgradeChoices[i]),
                "retired duplicate, daylight and overkill cards do not return to normal offers");
        shared->returnToMainMenu();
        shared->startRunFromSeed(PlayerClass::Engineer, 12345);
        requireNear(shared->snapshot().upgradeStats.toolAttackSpeed, baseStats.upgradeStats.toolAttackSpeed, 1e-12,
            "new runs clear previous gathering power and tool baseline");
    }

    auto hammers = std::make_unique<Simulation>();
    hammers->startRunFromSeed(PlayerClass::Engineer, 12345);
    for (std::size_t index = 0; index < RunTomeDefinitions.size(); ++index) {
        const auto& hammer = RunTomeDefinitions[index];
        require(runTomeDefinition(hammer.effect) == &hammer &&
            runUpgradeDefinition(hammer.effect).name == hammer.name,
            "hammer cards share the persistent stat item definition");
        for (std::size_t other = index + 1; other < RunTomeDefinitions.size(); ++other)
            require(hammer.effect != RunTomeDefinitions[other].effect,
                "each hammer owns one stable effect rather than a duplicate bonus");
    }
    for (auto numericEffect : {RunUpgradeEffect::Salvager, RunUpgradeEffect::GatheringSpeed,
            RunUpgradeEffect::LumberMillProduction, RunUpgradeEffect::QuarryProduction,
            RunUpgradeEffect::CrystalMineProduction})
        require(runTomeDefinition(numericEffect), "remaining numeric bonuses use hammer cards");
    for (auto mechanic : {RunUpgradeEffect::UnlockCannon, RunUpgradeEffect::CannonHeavyShell,
            RunUpgradeEffect::RiskyInvestment, RunUpgradeEffect::UnlockRifle, RunUpgradeEffect::CannonScattershot,
            RunUpgradeEffect::NightlyBomb, RunUpgradeEffect::ClusterPayload, RunUpgradeEffect::ReactiveTraps,
            RunUpgradeEffect::GunShrapnel, RunUpgradeEffect::Overkill, RunUpgradeEffect::LongerDay,
            RunUpgradeEffect::DefenseDamage, RunUpgradeEffect::DefenseFireRate})
        require(!runTomeDefinition(mechanic), "blueprints and mechanical perks stay separate from hammers");
    require(!retiredRunUpgrade(RunUpgradeEffect::GatheringSpeed), "gathering is a separate tool-speed tome");
    std::size_t activeCount = 0;
    std::size_t activeHammers = 0;
    for (const auto& card : RunUpgradeDefinitions) {
        if (!retiredRunUpgrade(card.effect)) {
            ++activeCount;
            if (runTomeDefinition(card.effect)) ++activeHammers;
        }
    }
    require(activeCount == 41 && activeHammers == 15,
        "active progression contains forty-one cards and fifteen tomes");
    require(retiredRunUpgrade(RunUpgradeEffect::LongerDay) && retiredRunUpgrade(RunUpgradeEffect::Overkill) &&
            retiredRunUpgrade(RunUpgradeEffect::RiskyInvestment), "daylight, overkill and high stakes are retired");
    const double phaseBeforeHammer = hammers->snapshot().phaseTimeRemaining;
    require(hammers->grantDebugRunUpgrade(RunUpgradeEffect::LongerDay, RunUpgradeRarity::Rare),
        "daylight hammer grants the existing day bonus");
    requireNear(hammers->snapshot().phaseTimeRemaining, phaseBeforeHammer + 7.0, 1e-9,
        "daylight rarity still scales the day extension");
    require(!hammers->grantDebugRunUpgrade(RunUpgradeEffect::Salvager),
        "retired salvage hammer cannot be newly granted");
    const double originalDefense = hammers->snapshot().upgradeStats.defenseDamage;
    require(hammers->grantDebugRunUpgrade(RunUpgradeEffect::DefenseDamage, RunUpgradeRarity::Rare),
        "first might hammer acquired");
    requireNear(hammers->snapshot().upgradeStats.defenseDamage, originalDefense * 1.168, 1e-12,
        "hammer acquisition applies its rarity bonus exactly once");
    require(hammers->grantDebugRunUpgrade(RunUpgradeEffect::DefenseDamage, RunUpgradeRarity::Common),
        "repeat might hammer advances its level");
    requireNear(hammers->snapshot().upgradeStats.defenseDamage, originalDefense * 1.288, 1e-12,
        "hammer upgrades add only the new card bonus");
    require(hammers->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::DefenseDamage)] == 2 &&
        hammers->snapshot().buildingArsenalCount == 1,
        "passive hammers have levels independently of the combat arsenal");
    require(hammers->grantDebugRunUpgrade(RunUpgradeEffect::Regeneration, RunUpgradeRarity::Uncommon),
        "regen hammer accepts rarity scaling");
    requireNear(hammers->snapshot().upgradeStats.healthRegeneration, 0.6, 1e-12,
        "uncommon regen hammer grants fractional HP per second");
    auto woundedHammerSave = hammers->saveSuspendedRunState();
    woundedHammerSave.playerHealth = 20.0;
    woundedHammerSave.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::Salvager)] = 2;
    woundedHammerSave.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::Salvager)] = 3.0;
    hammers->returnToMainMenu();
    require(hammers->loadSuspendedRunState(woundedHammerSave), "hammer levels and power resume");
    require(hammers->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::Salvager)] == 2 &&
            hammers->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::Salvager)] == 3.0 &&
            hammers->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::LongerDay)] == 1.4,
        "converted numeric cards resume on their original IDs without extra bonuses");
    for (int tick = 0; tick < 120; ++tick) hammers->tick(1.0 / 60.0);
    requireNear(hammers->snapshot().playerHealth, 21.2, 1e-9,
        "regen hammer heals during fixed-step gameplay");
    const double pausedHealth = hammers->snapshot().playerHealth;
    hammers->togglePause();
    hammers->tick(2.0);
    requireNear(hammers->snapshot().playerHealth, pausedHealth, 1e-12, "regen pauses with simulation");
    woundedHammerSave.playerHealth = hammers->snapshot().playerMaxHealth - 0.25;
    hammers->returnToMainMenu();
    require(hammers->loadSuspendedRunState(woundedHammerSave), "nearly full hammer fixture resumes");
    hammers->tick(1.0);
    requireNear(hammers->snapshot().playerHealth, hammers->snapshot().playerMaxHealth, 1e-9,
        "regen clamps to maximum health");
    hammers->returnToMainMenu();
    hammers->startRunFromSeed(PlayerClass::Engineer, 12345);
    for (const auto& hammer : RunTomeDefinitions)
        require(hammers->snapshot().runUpgradeStacks[runUpgradeIndex(hammer.effect)] == 0,
            "new run clears owned hammers");
    requireNear(hammers->snapshot().upgradeStats.healthRegeneration, 0.0, 1e-12,
        "new run resets passive regen");

    {
        auto debug = std::make_unique<Simulation>();
        debug->startRunFromSeed(PlayerClass::Vanguard, 12345);
        require(debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::DefenseDamage, 10000,
            RunUpgradeRarity::Legendary), "bulk debug grants exceed ordinary hammer levels");
        requireNear(debug->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::DefenseDamage)],
            20000, 1e-9, "bulk rarity gain is applied once per copy");
        require(debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::ProjectileCount, 2000),
            "debug quantity exceeds ordinary cap");
        require(debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::ProjectileCount, 7),
            "debug grants remain repeatable after reaching normal limits");
        require(debug->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)] == 2007,
            "repeated bulk grants retain exact count");
        for (const auto& definition : RunUpgradeDefinitions) {
            if (const auto building = runUpgradeBuilding(definition.effect);
                building && *building != playerClassDefinition(PlayerClass::Vanguard)->startingBuilding)
                require(debug->grantDebugRunUpgradeBatch(definition.effect, 100), "debug unlocks all blueprints");
        }
        require(debug->snapshot().buildingArsenalCount == RunBuildingTypes.size() - 1U - RunEconomyBuildingTypes.size(),
            "debug bypasses four combat blueprint slots");
        require(debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockCannon, 100) &&
            debug->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] == 200,
            "combat cards continue beyond the eighth model tier");
        require(debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::CannonHeavyShell, 100),
            "debug cannon specialization stays unique regardless of copy count");
        require(debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::CannonScattershot, 100),
            "debug switches cannon specialization");
        require(debug->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::CannonHeavyShell)] == 0 &&
            debug->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::CannonScattershot)] == 1,
            "debug specialization switching cannot stack incompatible styles");
        require(!debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::ProjectileCount, 0) &&
            !debug->grantDebugRunUpgradeBatch(static_cast<RunUpgradeEffect>(-1), 1) &&
            !debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::ProjectileCount, 1, static_cast<RunUpgradeRarity>(99)) &&
            !debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::ProjectileCount, std::numeric_limits<int>::max()),
            "bulk debug rejects invalid input and numeric overflow");
        require(!debug->grantDebugRunUpgradeBatch(RunUpgradeEffect::BuildRadius, 1000),
            "retired upgrades cannot be granted through the debug catalog");
        const auto savedDebug = debug->saveSuspendedRunState();
        debug->returnToMainMenu();
        require(debug->loadSuspendedRunState(savedDebug), "unlimited debug stats and arsenal survive save/load");
        require(debug->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)] == 2007 &&
            debug->snapshot().buildingArsenalCount == RunBuildingTypes.size() - 1U - RunEconomyBuildingTypes.size(), "debug save retains counts and all blueprints");
        debug->returnToMainMenu();
        debug->startRunFromSeed(PlayerClass::Vanguard, 12345);
        require(debug->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)] == 0,
            "new run resets unlimited debug grants");
        for (int i=0;i<5;++i) require(debug->grantDebugRunUpgrade(RunUpgradeEffect::DefenseDamage),
            "ordinary grant keeps its existing progression");
        require(!debug->grantDebugRunUpgrade(RunUpgradeEffect::DefenseDamage),
            "unlimited debug API does not remove ordinary gameplay caps");
    }

}
