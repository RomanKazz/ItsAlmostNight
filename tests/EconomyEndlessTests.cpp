#include "OvertimeChecks.hpp"
#include "enemies/EnemyDifficulty.hpp"
#include "graphics/EnemyAnimationSampling.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>

void runWaveDirectorTests();

namespace {
void checkWorldProducers() {
    using namespace ian;
    auto map=MapDefinition::defaults();
    map.resources.clear(); map.obstacles.clear();
    auto world=WorldConfig::defaults();
    world.terrainResolution=33;
    world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    const auto balance=GameBalance::defaults();
    auto run=std::make_unique<Simulation>(balance,map,world);
    run->startRunFromSeed(PlayerClass::Vanguard,817);
    auto save=run->saveSuspendedRunState();
    save.coins=1000; save.wood=save.stone=0;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(save),"funded world economy checkpoint loads");
    for(const auto type : {WorldLandmarkType::LumberMill,WorldLandmarkType::Mine}) {
        const auto find=[&]() -> WorldLandmarkInstance {
            const auto& snapshot=run->snapshot();
            const auto it=std::ranges::find(snapshot.worldLandmarks,type,&WorldLandmarkInstance::type);
            require(it!=snapshot.worldLandmarks.end(),"seed contains both world producers");
            return *it;
        };
        const double base=type==WorldLandmarkType::Mine ? 40.0 : 60.0;
        const int restore=type==WorldLandmarkType::Mine ? 80 : 60;
        auto landmark=find();
        requireNear(landmark.incomePerMinute,base,1e-9,"restore preview uses the actual base income");
        PlayerCommand interact;
        interact.overrideAimedWorldLandmark=true;
        interact.aimedWorldLandmarkOverride=landmark.id;
        interact.interact=InteractCommand{};
        auto before=run->snapshot().coins;
        run->tick(0,interact);
        require(find().activated && run->snapshot().coins==before-restore,"restoring charges the configured coin price once");
        run->tick(5);
        const double progress=find().productionProgress;
        for(int level=2;level<=5;++level) {
            before=run->snapshot().coins;
            const auto previous=find();
            run->tick(0,interact);
            landmark=find();
            require(landmark.level==level && run->snapshot().aimedWorldLandmark==landmark.id &&
                run->snapshot().coins==before-previous.upgradeCoinCost,
                "each producer level charges exactly its displayed price");
            requireNear(landmark.incomePerMinute,base*(1+.5*(level-1)),1e-9,"income improves in even steps");
            requireNear(landmark.productionProgress,progress,1e-9,"upgrading preserves unfinished production");
        }
        before=run->snapshot().coins;
        run->tick(0,interact);
        require(find().level==5 && find().upgradeCoinCost==0 && run->snapshot().coins==before,
            "maximum-level producers cannot charge for another upgrade");
        save=run->saveSuspendedRunState();
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(save),"upgraded producer checkpoint round-trips");
        require(find().level==5 && find().activated,"producer level and activation survive load");
        requireNear(find().productionProgress,progress,1e-9,"fractional production survives load");
    }
    // Prices, UI income and simulation output must agree, including failed purchases.
    save=run->saveSuspendedRunState();
    save.coins=0; save.wood=save.stone=0;
    for(auto& landmark:save.worldLandmarks)
        if(landmarkShrineKind(landmark.type)==ShrineKind::None) {
            landmark.level=1; landmark.productionProgress=0;
            landmark.activated=landmark.type==WorldLandmarkType::LumberMill;
        }
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(save),"unfunded producer fixture loads");
    const auto it=std::ranges::find(run->snapshot().worldLandmarks,WorldLandmarkType::Mine,&WorldLandmarkInstance::type);
    const auto mineId=it->id;
    PlayerCommand rejected;
    rejected.overrideAimedWorldLandmark=true; rejected.aimedWorldLandmarkOverride=mineId;
    rejected.interact=InteractCommand{};
    run->tick(0,rejected);
    const auto mine=std::ranges::find(run->snapshot().worldLandmarks,mineId,&WorldLandmarkInstance::id);
    require(!mine->activated && run->snapshot().coins==0,"unaffordable restore leaves producer and wallet unchanged");
    run->tick(10);
    require(run->snapshot().wood==10 && run->snapshot().stone==0,
        "a restored mill grants its displayed sixty wood per minute in ten-second batches");

    // Ordinary producer cards must not level or speed up the unique world landmarks.
    save=run->saveSuspendedRunState();
    for(const auto effect : {RunUpgradeEffect::ProductionSpeed,RunUpgradeEffect::LumberMillProduction,RunUpgradeEffect::QuarryProduction}) {
        save.runUpgradeStacks[runUpgradeIndex(effect)]=1;
        save.runUpgradePower[runUpgradeIndex(effect)]=1;
    }
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(save),"production-card and legacy-tome bonus checkpoint loads");
    run->tick(10);
    require(run->snapshot().wood==20,"production cards do not change actual world-producer output");
    for(const auto& landmark:run->snapshot().worldLandmarks) {
        if(landmarkShrineKind(landmark.type)!=ShrineKind::None) continue;
        require(landmark.level==1,"world producer levels change only through local paid interactions");
        requireNear(landmark.incomePerMinute,landmark.type==WorldLandmarkType::Mine ? 40.0 : 60.0,1e-9,
            "world-producer hover income excludes ordinary building-card bonuses");
    }

    require(!run->startDebugEndlessNight(),"debug night requires a placed core");
    PlayerCommand god; god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
    run->tick(0,god);
    PlayerCommand place; place.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};
    run->tick(0,place);
    while(run->snapshot().runUpgradeChoicePending) require(run->selectRunUpgrade(0),"debug-night fixture resolves choices");
    const auto core=run->snapshot().coreId;
    const int wood=run->snapshot().wood;
    require(run->startDebugEndlessNight(),"debug menu can start night eleven directly");
    require(run->snapshot().wave==11 && run->snapshot().finalNight && run->snapshot().phaseDuration==0 &&
        run->snapshot().overtimeSeconds==0 && run->snapshot().coreId==core && run->snapshot().wood==wood,
        "debug night preserves the player's base and resources while starting fresh pressure");
    run->tick(1);
    requireNear(run->snapshot().overtimeSeconds,1,1e-9,"debug-started endless night advances normally");
    run->togglePause();
    require(run->startDebugEndlessNight() && run->snapshot().state==RunState::Wave && run->snapshot().wave==11 &&
        run->snapshot().overtimeSeconds==0,"debug restart also leaves pause and resets the pressure clock");

    EnemySystem enemies;
    const std::array spawns{EnemySpawn{.type=EnemyType::Basic,.position={0,0,0}},
        EnemySpawn{.type=EnemyType::Flying,.position={10,0,0}}};
    enemies.spawnWave(spawns);
    const auto basicSpeed=enemies.enemies()[0].speed;
    const auto flyingSpeed=enemies.enemies()[1].speed;
    enemies.setWaveSpeedMultiplier(enemyWaveSpeedGrowth(6));
    enemies.setWaveSpeedMultiplier(enemyWaveSpeedGrowth(6));
    requireNear(enemies.enemies()[0].speed,basicSpeed*1.1,1e-9,"night speed applies once to live ground enemies");
    requireNear(enemies.enemies()[1].speed,flyingSpeed*1.1,1e-9,"night speed also applies to flying enemies");
    enemies.spawnGroup(std::span<const EnemySpawn>{spawns}.first(1));
    requireNear(enemies.enemies().back().speed,basicSpeed*1.1,1e-9,"reinforcements inherit the same night speed");
    requireNear(EnemySmoothAnimationDistance,40,1e-9,"full-rate animation radius is doubled to forty metres");
}
}

int main() {
    runWaveDirectorTests();
    checkWorldProducers();
    checkOvertime();
    std::cout << "World producer economy, wave scaling, endless progression/save and animation distance passed\n";
}
