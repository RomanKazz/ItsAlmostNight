#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>
#include <set>

using namespace ian;

std::unique_ptr<Simulation> shrineRun(std::uint32_t seed=817) {
    auto map=MapDefinition::defaults();map.resources.clear();map.obstacles.clear();
    auto world=WorldConfig::defaults();world.terrainAmplitude=0;world.pondMinimumCount=world.pondMaximumCount=0;
    auto run=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    run->startRunFromSeed(PlayerClass::Vanguard,seed);
    return run;
}

void restore(Simulation& run,SuspendedRunState state) {
    run.returnToMainMenu();
    require(run.loadSuspendedRunState(state),"shrine checkpoint restores");
}
void approach(Simulation& run,EntityId id,bool funded=true) {
    auto saved=run.saveSuspendedRunState();
    const auto landmark=std::ranges::find(saved.worldLandmarks,id,&WorldLandmarkInstance::id);
    require(landmark!=saved.worldLandmarks.end(),"shrine exists in checkpoint");
    const auto landmarkType=landmark->type;
    const auto landmarkPosition=landmark->position;
    if (landmarkType==WorldLandmarkType::BuildingShrine &&
        std::ranges::none_of(saved.buildings,[](const auto& building){return building.type==BuildingType::Core;})) {
        saved.playerPosition={0.0,1.8,3.0};
        restore(run,saved);
        PlayerCommand place{};
        place.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
        place.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};
        run.tick(0,place);
        require(std::ranges::any_of(run.snapshot().buildings,[](const auto& building){return building.type==BuildingType::Core;}),
            "defense shrine fixture has a Core");
        while(run.snapshot().runUpgradeChoicePending)
            require(run.selectRunUpgrade(0),"core fixture resolves starting reward");
        saved=run.saveSuspendedRunState();
    }
    saved.playerPosition={landmarkPosition.x,landmarkPosition.y+1.8,landmarkPosition.z+3};
    if(funded) {saved.wood=10000;saved.stone=10000;saved.crystals=10000;}
    restore(run,saved);
}
void interact(Simulation& run,EntityId id) {
    PlayerCommand command{};command.interact=InteractCommand{};command.overrideAimedWorldLandmark=true;command.aimedWorldLandmarkOverride=id;
    run.tick(1.0/60,command);
}
int main() {
    require(!availableShrineBlessing(RunUpgradeEffect::AttackSpeed), "standalone weapon speed is excluded from shrine offers");
    require(availableShrineBlessing(RunUpgradeEffect::DefenseFireRate), "defense speed remains in building shrines");
    for(const auto& tome:RunTomeDefinitions) {
        const bool present=std::ranges::any_of(ShrineBlessings,[&](const auto& blessing){return blessing.icon==tome.effect;});
        require(present || retiredRunUpgrade(tome.effect),"shrine pool covers active tomes");
    }
    for(const auto& blessing:ShrineBlessings)
        require(blessing.name==runTomeDefinition(blessing.icon)->name,"shrine offers reuse tome names/icons");
    for (const auto seed:{817U,271U,998U}) {
        auto run=shrineRun(seed);const auto first=run->saveSuspendedRunState();
        int players=0,buildings=0;
        for(const auto& landmark:first.worldLandmarks) {
            players+=landmark.type==WorldLandmarkType::PlayerShrine;
            buildings+=landmark.type==WorldLandmarkType::BuildingShrine;
            require(!landmark.activated,"shrines start unused");
        }
        require(players==12 && buildings==12,"each map contains twelve shrines of each type");
        auto again=shrineRun(seed)->saveSuspendedRunState();
        require(again.worldLandmarks.size()==first.worldLandmarks.size(),"seed preserves landmark count");
        for(std::size_t i=0;i<first.worldLandmarks.size();++i)
            requireNear(first.worldLandmarks[i].position.x,again.worldLandmarks[i].position.x,1e-12,"seed preserves shrine placement");
    }
    // Real generated terrain (ponds, slopes and resource clusters), not only flat fixtures.
    for(const auto seed:{104U,835U,2209U}) {
        auto natural=std::make_unique<Simulation>();natural->startRunFromSeed(PlayerClass::Vanguard,seed);
        int count=0;
        for(const auto& landmark:natural->snapshot().worldLandmarks)
            count+=landmarkShrineKind(landmark.type)!=ShrineKind::None;
        require(count==24,"default natural terrain has all twenty-four safe shrine sites");
    }
    auto run=shrineRun();
    for (const auto effect:{RunUpgradeEffect::Luck,RunUpgradeEffect::Damage,RunUpgradeEffect::AttackSpeed,RunUpgradeEffect::BuildingHealth})
        require(run->grantDebugRunUpgrade(effect),"fixture fills all tome slots");
    require(run->snapshot().tomeCount==4,"fixture has no free tome slots");
    const auto original=run->saveSuspendedRunState();
    const auto player=std::ranges::find(original.worldLandmarks,WorldLandmarkType::PlayerShrine,&WorldLandmarkInstance::type)->id;
    const auto building=std::ranges::find(original.worldLandmarks,WorldLandmarkType::BuildingShrine,&WorldLandmarkInstance::type)->id;
    // No resources: atomically reject, preserve the shrine and price.
    approach(*run,player,false);auto poor=run->saveSuspendedRunState();poor.wood=poor.stone=poor.crystals=0;restore(*run,poor);
    interact(*run,player);
    require(!run->snapshot().runUpgradeChoicePending && run->snapshot().shrines.activations==0,"unaffordable activation has no side effects");
    approach(*run,player);const auto before=run->snapshot();
    const auto cost=shrineActivationCost(ShrineKind::Player,0);interact(*run,player);
    const auto paid=run->snapshot();
    require(paid.runUpgradeChoicePending && paid.runUpgradeChoiceCount==3 && paid.shrines.offer==ShrineKind::Player,"paid shrine offers three blessings");
    require(paid.wood==before.wood-cost.wood && paid.stone==before.stone-cost.stone && paid.crystals==before.crystals-cost.crystals,"all three resources charged exactly once");
    std::set<RunUpgradeEffect> offered;
    for(std::size_t i=0;i<3;++i) {
        require(shrineBlessing(ShrineKind::Player,paid.runUpgradeChoices[i]),"player pool contains only personal stats");
        offered.insert(paid.runUpgradeChoices[i]);
    }
    require(offered.size()==3 && !run->rerollRunUpgrades(),"blessings are distinct and cannot use normal rerolls");
    const auto pending=run->saveSuspendedRunState();restore(*run,pending);
    require(run->snapshot().shrines.offer==ShrineKind::Player && run->snapshot().runUpgradeChoices==pending.runUpgradeChoices,"paid unselected offer survives resume");
    auto legacyPoisonOffer = pending;
    legacyPoisonOffer.runUpgradeChoices[0] = RunUpgradeEffect::PoisonChance;
    restore(*run, legacyPoisonOffer);
    for (std::size_t i = 0; i < run->snapshot().runUpgradeChoiceCount; ++i)
        require(availableShrineBlessing(run->snapshot().runUpgradeChoices[i]),
            "retired poison shrine choice is replaced on resume");
    restore(*run, pending);
    const auto stacks=run->snapshot().runUpgradeStacks;
    require(run->selectRunUpgrade(0),"one blessing can be selected");
    require(run->snapshot().runUpgradeStacks==stacks && run->snapshot().tomeCount==before.tomeCount,"blessings never create tomes or consume slots");
    interact(*run,player);
    require(!run->snapshot().runUpgradeChoicePending && run->snapshot().shrines.activations==1,"used shrine cannot be activated twice");
    const auto nextCost=shrineActivationCost(ShrineKind::Buildings,1),baseCost=shrineActivationCost(ShrineKind::Buildings,0);
    require(nextCost.wood>baseCost.wood && nextCost.stone>baseCost.stone && nextCost.crystals>baseCost.crystals,"both types share increasing resource prices");
    approach(*run,building);interact(*run,building);
    int coreOffers=0;
    for(std::size_t i=0;i<3;++i) {
        require(shrineBlessing(ShrineKind::Buildings,run->snapshot().runUpgradeChoices[i]),"building pool contains only structure stats and Core");
        coreOffers+=run->snapshot().runUpgradeChoices[i]==RunUpgradeEffect::UpgradeCore;
    }
    require(coreOffers==1,"every defense shrine reserves exactly one Core card");
    for (const auto kind:{ShrineKind::Player,ShrineKind::Buildings}) {
        auto rareRun=shrineRun();
        approach(*rareRun,kind==ShrineKind::Player ? player : building);
        interact(*rareRun,kind==ShrineKind::Player ? player : building);
        auto rareOffer=rareRun->saveSuspendedRunState();
        const auto effect=kind==ShrineKind::Player ? RunUpgradeEffect::Damage : RunUpgradeEffect::DefenseDamage;
        rareOffer.runUpgradeChoices[0]=effect;
        rareOffer.runUpgradeRarities[0]=RunUpgradeRarity::Rare;
        restore(*rareRun,rareOffer);
        const auto initial=rareRun->snapshot();
        require(rareRun->selectRunUpgrade(0),"Rare shrine blessing can be selected");
        const auto upgraded=rareRun->snapshot();
        requireNear(shrineBonusPower(upgraded.shrines,kind,effect),1.4,1e-10,
            "Rare shrine blessing saves 1.4x power");
        if(kind==ShrineKind::Player)
            requireNear(upgraded.playerDamageMultiplier,initial.playerDamageMultiplier*1.14,1e-10,
                "Rare personal blessing scales gameplay damage");
        else
            requireNear(upgraded.upgradeStats.defenseDamage,initial.upgradeStats.defenseDamage*1.168,1e-10,
                "Rare defense blessing scales gameplay damage");
        restore(*rareRun,rareRun->saveSuspendedRunState());
        requireNear(shrineBonusPower(rareRun->snapshot().shrines,kind,effect),1.4,1e-10,
            "Rare blessing power survives resume");
    }
    {
        auto coreRun=shrineRun();
        approach(*coreRun,building);interact(*coreRun,building);
        const auto coreOffer=coreRun->snapshot();
        const auto coreIndex=static_cast<std::size_t>(std::ranges::find(coreOffer.runUpgradeChoices,RunUpgradeEffect::UpgradeCore)-coreOffer.runUpgradeChoices.begin());
        require(coreIndex<coreOffer.runUpgradeChoiceCount,"Core card appears in defense shrine");
        const auto initialLevel=coreOffer.coreLevel;
        const auto initialStacks=coreOffer.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UpgradeCore)];
        require(coreRun->selectRunUpgrade(coreIndex),"Core shrine card applies");
        const auto selected=coreRun->snapshot();
        require(selected.coreLevel==initialLevel+1 &&
            selected.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UpgradeCore)]==initialStacks+1 &&
            shrineBonusCount(selected.shrines,ShrineKind::Buildings,RunUpgradeEffect::UpgradeCore)==1,
            "Core shrine card uses real Core progression and persists shrine count");
        restore(*coreRun,coreRun->saveSuspendedRunState());
    }
    {
        require(shrineBlessing(ShrineKind::Buildings, RunUpgradeEffect::ProductionSpeed) &&
            availableShrineBlessing(RunUpgradeEffect::ProductionSpeed) &&
            !shrineBlessing(ShrineKind::Player, RunUpgradeEffect::ProductionSpeed),
            "production blessing is available only in building shrines");
        auto productionRun = shrineRun();
        approach(*productionRun, building);
        auto producers = productionRun->saveSuspendedRunState();
        for (std::size_t i = 0; i < RunEconomyBuildingTypes.size(); ++i) {
            const auto type = RunEconomyBuildingTypes[i];
            producers.buildings.push_back({.id={static_cast<std::uint32_t>(2000+i),1},
                .type=type, .gridPosition={3+static_cast<int>(i)*3,0},
                .health=100, .maxHealth=100});
            const auto card = runUpgradeIndex(*buildingRunUpgrade(type));
            producers.runUpgradeStacks[card] = 1;
            producers.runUpgradePower[card] = 1;
        }
        producers.producerRuntimes.clear();
        restore(*productionRun, producers);
        const auto baseRates = productionRun->snapshot().producerRatesPerSecond;
        double expectedPower = 0;
        for (const auto rarity : {RunUpgradeRarity::Common, RunUpgradeRarity::Legendary}) {
            const auto state = productionRun->saveSuspendedRunState();
            const auto unused = std::ranges::find_if(state.worldLandmarks, [](const auto& landmark) {
                return landmark.type == WorldLandmarkType::BuildingShrine && !landmark.activated;
            });
            require(unused != state.worldLandmarks.end(), "production fixture has an unused building shrine");
            approach(*productionRun, unused->id);
            interact(*productionRun, unused->id);
            auto offer = productionRun->saveSuspendedRunState();
            offer.runUpgradeChoices = {RunUpgradeEffect::ProductionSpeed,
                RunUpgradeEffect::DefenseDamage, RunUpgradeEffect::UpgradeCore};
            offer.runUpgradeRarities[0] = rarity;
            restore(*productionRun, offer);
            const auto tomeStacks = productionRun->snapshot().runUpgradeStacks;
            require(productionRun->selectRunUpgrade(0), "production shrine blessing applies");
            expectedPower += runUpgradeRarityMultiplier(rarity);
            const auto after = productionRun->snapshot();
            requireNear(after.upgradeStats.productionSpeed, 1 + .12 * expectedPower, 1e-12,
                "production blessings stack their rarity power without occupying tome slots");
            require(after.runUpgradeStacks == tomeStacks, "production blessing leaves tome and blueprint levels unchanged");
            for (const auto type : RunEconomyBuildingTypes) {
                const auto i = static_cast<std::size_t>(type);
                require(after.producerRatesPerSecond[i] > baseRates[i],
                    "production blessing accelerates lumber mills, quarries and crystal mines");
                const auto card = std::ranges::find_if(after.producerCards, [&](const auto& view) {
                    return view.stats.current.productionInterval &&
                        std::ranges::any_of(after.buildings, [&](const auto& b) {
                            return b.id == view.buildingId && b.type == type;
                        });
                });
                require(card != after.producerCards.end(), "each producer has a real runtime card");
                requireNear(after.producerRatesPerSecond[i],
                    *card->stats.current.productionPerCycle / *card->stats.current.productionInterval,
                    1e-12, "production blessing preview agrees with the actual producer cycle");
            }
            restore(*productionRun, productionRun->saveSuspendedRunState());
            requireNear(productionRun->snapshot().upgradeStats.productionSpeed,
                1 + .12 * expectedPower, 1e-12, "stacked production blessings survive resume");
        }
    }
    // Exercise every stat and verify side isolation, stacking and resume.
    for(const auto& blessing:ShrineBlessings) {
        if (!availableShrineBlessing(blessing.effect)) continue;
        auto test=shrineRun();
        approach(*test,blessing.kind==ShrineKind::Player ? player : building);
        interact(*test,blessing.kind==ShrineKind::Player ? player : building);
        auto state=test->saveSuspendedRunState();state.runUpgradeChoices[0]=blessing.effect;
        state.runUpgradeRarities[0]=RunUpgradeRarity::Common;
        std::size_t index=1;
        if(blessing.kind==ShrineKind::Buildings) state.runUpgradeChoices[index++]=RunUpgradeEffect::UpgradeCore;
        for(const auto& other:ShrineBlessings)
            if(other.kind==blessing.kind && other.effect!=blessing.effect && availableShrineBlessing(other.effect) && index<3) state.runUpgradeChoices[index++]=other.effect;
        restore(*test,state);const auto stats=test->snapshot();
        require(test->selectRunUpgrade(0),"every shrine blessing applies");
        const auto after=test->snapshot();
        require(shrineBonusCount(after.shrines,blessing.kind,blessing.effect)==1,"dedicated blessing counter increments");
        if(blessing.kind==ShrineKind::Player)
            requireNear(after.upgradeStats.defenseDamage,stats.upgradeStats.defenseDamage,1e-12,"personal blessings do not buff defenses");
        else requireNear(after.playerDamageMultiplier,stats.playerDamageMultiplier,1e-12,"building blessings do not buff player damage");
        switch(blessing.effect) {
        case RunUpgradeEffect::Damage: requireNear(after.playerDamageMultiplier,stats.playerDamageMultiplier*1.10,1e-10,"player damage multiplier");break;
        case RunUpgradeEffect::AttackSpeed: requireNear(after.upgradeStats.playerAttackSpeed,stats.upgradeStats.playerAttackSpeed*1.15,1e-10,"personal attack speed multiplier");break;
        case RunUpgradeEffect::MoveSpeed: requireNear(after.playerMoveSpeedMultiplier,stats.playerMoveSpeedMultiplier*1.08,1e-10,"personal movement multiplier");break;
        case RunUpgradeEffect::MaximumHealth:
            requireNear(after.playerMaxHealth,stats.playerMaxHealth+12,1e-10,"flat HP blessing");
            requireNear(after.upgradeStats.buildingHealth,stats.upgradeStats.buildingHealth*1.15,1e-10,"personal HP blessing also fortifies structures");break;
        case RunUpgradeEffect::RecoverableArmor: requireNear(after.playerMaxRecoverableArmor,stats.playerMaxRecoverableArmor+8,1e-10,"flat shield blessing");break;
        case RunUpgradeEffect::DefenseDamage: requireNear(after.upgradeStats.defenseDamage,stats.upgradeStats.defenseDamage*1.12,1e-10,"global defense damage multiplier");break;
        case RunUpgradeEffect::DefenseFireRate: requireNear(after.upgradeStats.defenseFireRate,stats.upgradeStats.defenseFireRate*1.10,1e-10,"global defense speed multiplier");break;
        case RunUpgradeEffect::BuildingCapacity:
            require(after.totalBuildingLimit == stats.totalBuildingLimit + 1 && after.tomeCount == stats.tomeCount &&
                after.buildingLimitBonus == stats.buildingLimitBonus + 1,
                "building shrine adds exactly one shared place without a tome slot"); break;
        case RunUpgradeEffect::BuildingHealth:
            requireNear(after.upgradeStats.buildingHealth,stats.upgradeStats.buildingHealth*1.15,1e-10,"global structure durability multiplier");
            requireNear(after.playerMaxHealth,stats.playerMaxHealth*1.15,1e-10,"building HP blessing also fortifies the player");break;
        case RunUpgradeEffect::ProductionSpeed: requireNear(after.upgradeStats.productionSpeed,stats.upgradeStats.productionSpeed*1.12,1e-10,"global producer speed multiplier");break;
        case RunUpgradeEffect::ExperienceGain: requireNear(after.upgradeStats.experienceGain,stats.upgradeStats.experienceGain+.10,1e-10,"personal experience blessing applies without a tome slot");break;
        case RunUpgradeEffect::Luck: requireNear(after.upgradeStats.luck,stats.upgradeStats.luck+.10,1e-10,"same luck as tome");break;
        case RunUpgradeEffect::Regeneration: requireNear(after.upgradeStats.healthRegeneration,stats.upgradeStats.healthRegeneration+.5,1e-10,"same regeneration as tome");break;
        case RunUpgradeEffect::GatheringSpeed: requireNear(after.upgradeStats.toolAttackSpeed,stats.upgradeStats.toolAttackSpeed*1.15,1e-10,"same gathering as tome");break;
        case RunUpgradeEffect::ResourceCapacity: requireNear(after.upgradeStats.resourceCapacity,stats.upgradeStats.resourceCapacity+.20,1e-10,"capacity expands world resource stock");break;
        case RunUpgradeEffect::ProjectileCount: require(after.upgradeStats.projectileCountBonus==stats.upgradeStats.projectileCountBonus+1,"same quantity as tome");break;
        case RunUpgradeEffect::Bounces: require(after.upgradeStats.bounces==stats.upgradeStats.bounces+1,"same bounce as tome");break;
        case RunUpgradeEffect::Pierce: require(after.upgradeStats.pierce==stats.upgradeStats.pierce+1,"same pierce as tome");break;
        case RunUpgradeEffect::ProjectileSpeed: requireNear(after.upgradeStats.projectileSpeed,stats.upgradeStats.projectileSpeed*1.20,1e-10,"same velocity as tome");break;
        case RunUpgradeEffect::AttackDuration: requireNear(after.upgradeStats.attackDuration,stats.upgradeStats.attackDuration*1.20,1e-10,"same duration as tome");break;
        case RunUpgradeEffect::AttackSize: requireNear(after.upgradeStats.attackSize,stats.upgradeStats.attackSize*1.15,1e-10,"same size as tome");break;
        case RunUpgradeEffect::CriticalChance: requireNear(after.upgradeStats.critChance,stats.upgradeStats.critChance+.075,1e-10,"same crit chance as tome");break;
        case RunUpgradeEffect::CriticalDamage: requireNear(after.upgradeStats.critDamage,stats.upgradeStats.critDamage*1.25,1e-10,"same crit multiplier as tome");break;
        case RunUpgradeEffect::AttackKnockback: requireNear(after.upgradeStats.knockback,stats.upgradeStats.knockback*1.20,1e-10,"same knockback as tome");break;
        case RunUpgradeEffect::PoisonChance:
        case RunUpgradeEffect::BloodHarvest: break; // Shared on-hit/kill flow consumes the scoped power.
        default: require(false,"each supported blessing has a regression check");break;
        }
        const auto completed=test->saveSuspendedRunState();restore(*test,completed);
        require(test->snapshot().shrines.playerBonuses==completed.shrines.playerBonuses && test->snapshot().shrines.buildingBonuses==completed.shrines.buildingBonuses,"selected blessings persist through save/load");
        auto corrupt=completed;++corrupt.shrines.activations;test->returnToMainMenu();
        require(!test->loadSuspendedRunState(corrupt),"inconsistent shrine counters are rejected");
    }
    // Exercise real hits, not only metadata: player venom must not leak from a
    // building shrine, while ten personal doses guarantee the shared poison flow.
    for (const auto kind:{ShrineKind::Player,ShrineKind::Buildings}) {
        auto poison=shrineRun();auto doses=poison->saveSuspendedRunState();
        doses.shrines.activations=10;
        shrineBonusCounts(doses.shrines,kind)[runUpgradeIndex(RunUpgradeEffect::PoisonChance)]=10;
        shrineBonusPowers(doses.shrines,kind)[runUpgradeIndex(RunUpgradeEffect::PoisonChance)]=10.0;
        int marked=0;
        for(auto& landmark:doses.worldLandmarks)
            if(landmarkShrineKind(landmark.type)==kind && marked++<10) landmark.activated=true;
        restore(*poison,doses);
        PlayerCommand build{};build.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
        build.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};poison->tick(0,build);
        while(poison->snapshot().runUpgradeChoicePending) {
            std::size_t choice=0;
            while(poison->snapshot().runUpgradeChoices[choice]==RunUpgradeEffect::PoisonChance) ++choice;
            require(poison->selectRunUpgrade(choice),"venom fixture resolves unrelated core rewards");
        }
        PlayerCommand spawn{};
        spawn.spawnEnemy=SpawnEnemyCommand{.type=EnemyType::Heavy,.count=1};poison->tick(0,spawn);
        require(!poison->snapshot().enemies.empty(),"shrine venom fixture spawns its target");
        PlayerCommand attack{};
        attack.castChainLightning=CastChainLightningCommand{.firstTarget=poison->snapshot().enemies.front().id,.damage=10,.maximumTargets=1};
        poison->tick(0,attack);
        require(enemyHasStatus(poison->snapshot().enemies.front(),StatusEffectType::Poison)==(kind==ShrineKind::Player),
            "shrine venom uses real hits and respects player/building scope");
    }
    {
        auto simultaneous=shrineRun();PlayerCommand setup{};
        setup.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
        setup.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};simultaneous->tick(0,setup);
        while(simultaneous->snapshot().runUpgradeChoicePending) require(simultaneous->selectRunUpgrade(0),"same-frame fixture resolves core reward");
        approach(*simultaneous,player);
        PlayerCommand spawn{};spawn.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
        spawn.spawnEnemy=SpawnEnemyCommand{.type=EnemyType::Heavy,.count=1};simultaneous->tick(0,spawn);
        require(!simultaneous->snapshot().enemies.empty(),"same-frame fixture has an enemy");
        PlayerCommand killAndPay{};killAndPay.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
        killAndPay.castChainLightning=CastChainLightningCommand{.firstTarget=simultaneous->snapshot().enemies.front().id,.damage=100000,.maximumTargets=1};
        killAndPay.interact=InteractCommand{};killAndPay.overrideAimedWorldLandmark=true;killAndPay.aimedWorldLandmarkOverride=player;
        simultaneous->tick(1.0/60,killAndPay);
        require(simultaneous->snapshot().shrines.offer==ShrineKind::Player,"same-frame kill preserves paid shrine offer");
        require(std::ranges::any_of(simultaneous->snapshot().coinPickups,[](const auto& pickup){return pickup.kind==PickupKind::Experience;}),
            "opening a shrine never discards the same-frame kill's XP drops");
        auto queued=simultaneous->saveSuspendedRunState();queued.runUpgradeSelectionsRemaining=2;
        restore(*simultaneous,queued);require(simultaneous->selectRunUpgrade(0),"paid blessing resolves before queued level reward");
        require(simultaneous->snapshot().shrines.offer==ShrineKind::None && simultaneous->snapshot().runUpgradeChoicePending,
            "normal level offer continues after the paid shrine choice");
    }
    auto exhausted=shrineRun();const auto everyShrine=exhausted->saveSuspendedRunState().worldLandmarks;
    PlayerCommand god{};god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};exhausted->tick(1.0/60,god);
    PlayerCommand place{};place.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};exhausted->tick(1.0/60,place);
    const auto core=std::ranges::find(exhausted->snapshot().buildings,BuildingType::Core,&BuildingInstance::type);
    require(core!=exhausted->snapshot().buildings.end(),"stacking fixture has a core");
    PlayerCommand upgrade{};upgrade.upgradeBuilding=UpgradeBuildingCommand{core->id};exhausted->tick(1.0/60,upgrade);
    restore(*exhausted,exhausted->saveSuspendedRunState());
    require(!exhausted->snapshot().unlimitedResources && exhausted->snapshot().woodCapacity>=240,"core level two supports late shrine costs in normal play");
    const double basePlayer=exhausted->snapshot().playerDamageMultiplier;
    const double baseDefense=exhausted->snapshot().upgradeStats.defenseDamage;
    for (const auto& landmark:everyShrine) {
        const auto kind=landmarkShrineKind(landmark.type);if(kind==ShrineKind::None) continue;
        approach(*exhausted,landmark.id);interact(*exhausted,landmark.id);
        auto purchase=exhausted->saveSuspendedRunState();
        const auto effect=kind==ShrineKind::Player ? RunUpgradeEffect::Damage : RunUpgradeEffect::DefenseDamage;
        purchase.runUpgradeChoices[0]=effect;std::size_t i=1;
        purchase.runUpgradeRarities[0]=RunUpgradeRarity::Common;
        if(kind==ShrineKind::Buildings) purchase.runUpgradeChoices[i++]=RunUpgradeEffect::UpgradeCore;
        for(const auto& other:ShrineBlessings)
            if(other.kind==kind && other.effect!=effect && availableShrineBlessing(other.effect) && i<3)
                purchase.runUpgradeChoices[i++]=other.effect;
        restore(*exhausted,purchase);require(exhausted->selectRunUpgrade(0),"all twenty-four shrines remain usable once each");
    }
    require(exhausted->snapshot().shrines.activations==24,"all paid activations are counted");
    requireNear(exhausted->snapshot().playerDamageMultiplier,basePlayer*2.20,1e-10,"personal blessings stack additively within their source");
    requireNear(exhausted->snapshot().upgradeStats.defenseDamage,baseDefense*2.44,1e-10,"building blessings stack and remain multiplicative with other sources");
    // A legacy two-landmark map uses the pre-shrine seeded challenge sites.
    // Find a seed whose new sites coincide with those historical positions.
    bool legacyRestored=false;
    for (std::uint32_t seed=1;seed<=8 && !legacyRestored;++seed) {
        auto old=shrineRun(seed);auto legacy=old->saveSuspendedRunState();
        std::erase_if(legacy.worldLandmarks,[](const auto& value){return landmarkShrineKind(value.type)!=ShrineKind::None;});
        old->returnToMainMenu();
        legacyRestored=old->loadSuspendedRunState(legacy);
        if(legacyRestored) require(old->snapshot().worldLandmarks.size()==2,"older maps resume without spawning shrines through bases");
    }
    require(legacyRestored,"two-landmark saves remain loadable with historical seeded layouts");
    run->returnToMainMenu();
    run->startRunFromSeed(PlayerClass::Vanguard,817);
    require(run->snapshot().shrines.activations==0 && run->snapshot().worldLandmarks.size()==26,"new run resets spent shrines and blessings");
}
