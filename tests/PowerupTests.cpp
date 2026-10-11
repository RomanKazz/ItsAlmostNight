#include "TestHarness.hpp"
#include "economy/PowerupSystem.hpp"
#include "game/Simulation.hpp"
#include <limits>
#include <memory>

void runPowerupTests() {
    using namespace ian;
    PowerupSystem system;
    requireNear(system.defenseDamageMultiplier(),1,1e-12,"no fury bonus at baseline");
    require(!system.active(static_cast<PowerupType>(255)), "invalid powerup query is harmless");
    std::array<int,3> drops{};
    for (std::uint64_t i=0;i<100000;++i) {
        const auto a = PowerupSystem::rollDrop(i,false,false);
        require(a == PowerupSystem::rollDrop(i,false,false),"drop rolls deterministic");
        if (a) ++drops[0];
        if (PowerupSystem::rollDrop(i,true,false)) ++drops[1];
        if (PowerupSystem::rollDrop(i,false,true)) ++drops[2];
    }
    require(drops[0]>850 && drops[0]<1150 && drops[1]>4700 && drops[1]<5300 &&
        drops[2]>19000 && drops[2]<21000,"rare, elite and boss drop balance");
    require(system.spawn(PowerupType::Fury,{0,0,0}),"fury spawns");
    static_cast<void>(system.tick(.3,{10,0,0}));
    require(!system.active(PowerupType::Fury),"drop is not activated from afar");
    const auto picked = system.tick(0,{0,0,0});
    require(picked[1] && system.active(PowerupType::Fury),"proximity collects eligible token");
    requireNear(system.defenseDamageMultiplier(),1.5,1e-12,"fury damage multiplier");
    requireNear(system.defenseSpeedMultiplier(),1.5,1e-12,"fury attack speed multiplier");
    static_cast<void>(system.tick(7,{})); system.activate(PowerupType::Fury);
    requireNear(system.remaining(PowerupType::Fury),15,1e-12,"repeat collection refreshes, no infinite stack");
    static_cast<void>(system.tick(15,{}));
    requireNear(system.defenseDamageMultiplier(),1,1e-12,"expiry removes fury bonus");
    system.activate(PowerupType::GoldRush);
    require(system.enemyCoinMultiplier()==3,"gold rush triples enemy reward");
    system.activate(PowerupType::Magnet);
    requireNear(system.remaining(PowerupType::Magnet),6,1e-12,"magnet duration");
    for (int i=0;i<32;++i) require(system.spawn(PowerupType::Magnet,{10,0,0}),"bounded slots fill");
    require(!system.spawn(PowerupType::Magnet,{10,0,0}),"overflow gracefully drops token, never reallocates");
    static_cast<void>(system.tick(45,{},false));
    require(PowerupSystem::validState(system.state()),"expired tokens leave a valid save state");
    require(system.spawn(PowerupType::GoldRush,{10,0,0}),"expired slots reusable");
    system.reset();
    require(!system.active(PowerupType::GoldRush) && !system.state().pickups[0].active,"reset clears all buffs and slots");
    auto bad = system.state(); bad.remaining[0]=std::numeric_limits<double>::quiet_NaN();
    require(!system.loadState(bad),"non-finite powerup state rejected");
    bad=system.state(); bad.pickups[0].type=static_cast<PowerupType>(255);
    require(!system.loadState(bad),"invalid powerup enum rejected");

    auto run=std::make_unique<Simulation>();
    require(!run->grantDebugPowerup(PowerupType::Fury), "main menu cannot activate gameplay powerups");
    run->startRunFromSeed(PlayerClass::Vanguard,12345);
    const double baselineDamage=run->snapshot().upgradeStats.defenseDamage;
    for (std::size_t i=0;i<PowerupTypeCount;++i) {
        const auto type=static_cast<PowerupType>(i);
        require(run->grantDebugPowerup(type), "debug activation accepts every powerup type");
        requireNear(run->snapshot().powerups.remaining[i],PowerupDefinitions[i].duration,1e-12,
            "debug grant activates immediately without requiring world pickup");
    }
    requireNear(run->snapshot().upgradeStats.defenseDamage,baselineDamage*1.5,1e-12,
        "debug fury immediately refreshes attack modifiers and snapshot");
    require(run->grantDebugPowerup(PowerupType::Fury), "debug powerup supports repeated grants");
    requireNear(run->snapshot().upgradeStats.defenseDamage,baselineDamage*1.5,1e-12,
        "repeated debug fury does not compound damage");
    require(!run->grantDebugPowerup(static_cast<PowerupType>(255)), "debug grant rejects invalid enum");
    auto saved=run->saveSuspendedRunState();
    saved.runUpgradeChoicePending=false; saved.runUpgradeSelectionsRemaining=0;
    saved.powerups.remaining={6,15,20};
    saved.powerups.pickups[0]={{20,0,20},PowerupType::Fury,2,true};
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved),"suspended run preserves temporary timers and tokens");
    const auto damage=run->snapshot().upgradeStats.defenseDamage;
    const auto rate=run->snapshot().upgradeStats.defenseFireRate;
    requireNear(run->snapshot().powerups.remaining[1],15,1e-12,"resume keeps fury timer");
    run->togglePause(); run->tick(10,{});
    requireNear(run->snapshot().powerups.remaining[1],15,1e-12,"pause freezes powerups");
    run->togglePause(); run->tick(15,{});
    requireNear(run->snapshot().upgradeStats.defenseDamage,damage/1.5,1e-12,"fury expiry rebuilds actual defense buffs");
    requireNear(run->snapshot().upgradeStats.defenseFireRate,rate/1.5,1e-12,"attack speed expiry restores baseline");
    run->returnToMainMenu(); run->startRunFromSeed(PlayerClass::Vanguard,12345);
    require(run->snapshot().powerups.remaining[1]==0,"new run cannot retain fury");

    TerrainHeightfield terrain; terrain.generate(12345);
    CoinPickupSystem coins; CollisionWorld collision;
    const Vec3 far{30,terrain.getHeight(30,0),0};
    coins.spawnValue(far,5,1,terrain,0);
    coins.spawnHeart(far,2,terrain,0);
    require(coins.spawnExperience(far,3,1,3,terrain,0)>0,"magnet test XP fixture");
    coins.magnetizeExperienceAndCoins();
    for (const auto& p : coins.pickups()) require(p.magnetized == (p.kind!=PickupKind::Heart),
        "global magnet attracts XP and coins, leaves remote hearts");
    int collected=0;
    for (int i=0;i<200;++i) collected += coins.tick(.05,{0,terrain.getHeight(0,0)+1.6,0},terrain,collision).value;
    require(collected==5,"far coins fly to player and reward once");
}
