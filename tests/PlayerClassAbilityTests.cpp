#include "TestHarness.hpp"
#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>

void runPlayerClassAbilityTests() {
    using namespace ian;
    auto balance = GameBalance::defaults();
    auto& core = balance.buildings[static_cast<std::size_t>(BuildingType::Core)];
    core.wood = core.stone = core.crystals = 0;
    auto map = MapDefinition::defaults();
    map.resources.clear(); map.obstacles.clear();
    auto world = WorldConfig::defaults(); world.terrainAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto insight = InsightConfig::defaults(); insight.baseRequirement = 100000;
    insight.earlyRequirementDiscounts.fill(0);
    const auto create = [&](PlayerClass type) {
        auto run = std::make_unique<Simulation>(balance,map,world,insight);
        run->startRunFromSeed(type,0x434C4153U); return run;
    };
    const auto placeCore = [](Simulation& run) {
        PlayerCommand build; build.placeBuilding = PlaceBuildingCommand{BuildingType::Core,{0,0},0};
        run.tick(0.0,build);
        require(run.snapshot().coreId.has_value(),"class ability fixture places a core");
    };
    {
        auto run = create(PlayerClass::Vanguard);
        requireNear(run->snapshot().playerRecoverableArmor,25,1e-12,
            "vanguard starts with its full recharging armor");
        PlayerCommand hit; hit.damagePlayer = DamagePlayerCommand{32};
        run->tick(0.0,hit);
        requireNear(run->snapshot().playerRecoverableArmor,0,1e-12,
            "damage breaks the vanguard armor");
        run->tick(6.0);
        require(run->snapshot().playerRecoverableArmor > 0,
            "vanguard armor still recharges without a movement ability");
    }
    {
        auto run = create(PlayerClass::Ranger);
        PlayerCommand shoot; shoot.fireRifle = true;
        const int magazine = run->snapshot().rifleAmmunition;
        for (int i=0;i<magazine;++i) run->tick(1.0,shoot);
        require(run->snapshot().rifleReloading,"ranger empties its magazine");
        const double reload = run->snapshot().rifleReloadRemaining;
        run->tick(1.0);
        requireNear(run->snapshot().rifleReloadRemaining,reload,1e-12,
            "standing still pauses the ranger's reload");
        PlayerCommand move; move.moveForward = 1;
        run->tick(1.0,move);
        require(run->snapshot().rifleReloadRemaining < reload,
            "moving resumes the ranger's reload");
    }
    {
        auto run = create(PlayerClass::Engineer); placeCore(*run);
        auto baseline = create(PlayerClass::Ranger); placeCore(*baseline);
        require(run->snapshot().coreMaxHealth > baseline->snapshot().coreMaxHealth,
            "engineer retains its building health bonus");
    }
    {
        auto deposits = map;
        deposits.resources = {
            {ResourceType::Wood,{2,0,6},0.2,20,3,100},
            {ResourceType::Wood,{4,0,6},0.2,20,3,100},
            {ResourceType::Wood,{6,0,6},0.2,20,3,100},
        };
        auto harvestBalance = balance; harvestBalance.gameplay.pickaxeDamage = 60;
        harvestBalance.gameplay.resourceToolDamageMultiplier = 1;
        harvestBalance.gameplay.pickaxeDamageVariation = 0;
        harvestBalance.gameplay.pickaxeCriticalChance = 0;
        auto harvest = std::make_unique<Simulation>(harvestBalance,deposits,world,insight);
        harvest->startRunFromSeed(PlayerClass::Prospector,0x434C4153U);
        require(harvest->snapshot().resourceNodes.size() >= 3,
            "chain fixture creates clustered deposits");
        const auto id = harvest->snapshot().resourceNodes.front().id;
        PlayerCommand strike; strike.usePickaxe = true; strike.overrideAimedResource = true;
        strike.aimedResourceOverride = id;
        harvest->tick(0.0,strike);
        const auto events = harvest->takeEvents();
        require(std::ranges::count_if(events,[](const auto& event) {
            return event.type == GameEventType::ResourceHit ||
                event.type == GameEventType::ResourceCollected;
        }) == 3,"depleting a deposit chains to exactly two neighbours");
    }
    {
        auto run = create(PlayerClass::Berserker);
        PlayerCommand hit; hit.damagePlayer = DamagePlayerCommand{30}; run->tick(0.0,hit);
        requireNear(run->snapshot().classAbilityState.rageRemaining,4,1e-12,
            "taking HP damage starts rage");
        run->tick(4.0);
        require(run->snapshot().classAbilityState.exhaustionRemaining > 0,
            "rage expiration still enforces its own cooldown");
        const auto saved = run->saveSuspendedRunState();
        auto loaded = create(PlayerClass::Berserker); loaded->returnToMainMenu();
        require(loaded->loadSuspendedRunState(saved) &&
                loaded->snapshot().classAbilityState.exhaustionRemaining > 0,
            "rage exhaustion survives a suspended run");
        loaded->tick(2.0);
        requireNear(loaded->snapshot().classAbilityState.exhaustionRemaining,0,1e-12,
            "rage exhaustion ends normally after loading");
    }
    {
        auto run = create(PlayerClass::Vampire); placeCore(*run);
        PlayerCommand wound; wound.damagePlayer = DamagePlayerCommand{25}; run->tick(0.0,wound);
        const double health = run->snapshot().playerHealth;
        PlayerCommand spawn; spawn.spawnEnemy = SpawnEnemyCommand{.type=EnemyType::Basic,.count=1};
        run->tick(0.0,spawn);
        require(!run->snapshot().enemies.empty(),"vampire fixture spawns an enemy");
        PlayerCommand kill; kill.castChainLightning = CastChainLightningCommand{
            .firstTarget=run->snapshot().enemies.front().id,.damage=10000,.maximumTargets=1};
        run->tick(0.0,kill);
        requireNear(run->snapshot().playerHealth,health+2,1e-12,
            "vampire kills retain their normal healing");
    }
    {
        const auto alchemist = playerClassLevelBonuses(PlayerClass::Alchemist,1);
        requireNear(alchemist.appleHealing,1.5,1e-12,
            "alchemist retains improved apple healing");
        requireNear(alchemist.potionDurationBonus,3,1e-12,
            "alchemist retains longer potions");
        const auto chronomancer = playerClassLevelBonuses(PlayerClass::Chronomancer,2);
        requireNear(chronomancer.moveSpeed,1.005,1e-12,
            "chronomancer level growth now affects movement speed");
    }
}
