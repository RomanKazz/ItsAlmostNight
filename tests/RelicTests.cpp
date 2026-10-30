#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "core/DeterministicRandom.hpp"
#include <memory>
#include <algorithm>
#include <set>

namespace ian {
struct RelicTestAccess {
    static RelicState& state(Simulation& s) {return s.relics_;}
    static ResourceSystem& resources(Simulation& s) {return s.resources_;}
    static BuildingSystem& buildings(Simulation& s) {return s.buildings_;}
    static EnemySystem& enemies(Simulation& s) {return s.enemies_;}
    static std::vector<GameEvent>& events(Simulation& s) {return s.events_;}
    static void process(Simulation& s) {s.processRelicEvents(0);s.events_.clear();}
    static void night(Simulation& s) {s.relicNightfall();}
    static void dawn(Simulation& s) {s.relicDawn();}
    static void debug(Simulation& s,const PlayerCommand& c) {s.processDebugCommands(c);}
    static void wave(Simulation& s) {s.state_=RunState::Wave;}
    static bool roll(Simulation& s,double chance,int stacks) {return s.rollManualRelic(chance,stacks);}
    static double production(Simulation& s,ResourceType t) {return s.relicProductionMultiplier(t);}
    static void stocks(Simulation& s,int w,int stone,int c) {s.wood_=w;s.stone_=stone;s.crystals_=c;}
    static void elapsed(Simulation& s,double value) {s.elapsedSeconds_=value;}
    static auto damage(Simulation& s,EntityId id,double amount) {return s.damageBuildingWithRelics(id,amount);}
    static void destroy(Simulation& s,BuildingInstance b) {(void)s.buildings_.damage(b.id,b.maxHealth*2);s.rememberDestroyedTower(b);s.syncWorldStructures();}
    static void tick(Simulation& s,double seconds) {s.tickRelics(seconds);}
    static void attacks(Simulation& s,double seconds) {s.updateTowerCombat(seconds);}
    static bool mirror(Simulation& s,RunUpgradeEffect e) {return s.mirrorStatCard(e);}
    static void repair(Simulation& s,EntityId id) {s.selectedBuilding_.reset();s.processBuildingCommands({.repairBuilding=RepairBuildingCommand{id}});}
    static void barrel(Simulation& s,ResourceType type) {s.dropPropLoot({{7,1},type,{},true,1},{},137);}
    static void grants(Simulation& s) {s.updatePendingResourceGrants(1);}
    static void upgradeOffer(Simulation& s,RunUpgradeEffect effect,std::uint64_t seed=444) {
        s.runUpgradeChoicePending_=true;s.runUpgradeChoiceCount_=1;s.runUpgradeSelectionsRemaining_=1;
        s.runUpgradeChoices_[0]=effect;s.runUpgradeRarities_[0]=RunUpgradeRarity::Common;s.runUpgradeRollSeeds_[0]=seed;s.runUpgradePaidChoicePrice_=0;
    }
};
}
void runRelicTests() {
    using namespace ian;using A=RelicTestAccess;
    static_assert(lootUpgradeIndex(LootUpgradeEffect::Acorn)==26);
    static_assert(lootUpgradeIndex(LootUpgradeEffect::UpgradeMirror)==43);
    std::array<int,5> counts{};
    std::set<std::size_t> unique;
    for (const auto item:ActiveLootEffects) {++counts[static_cast<std::size_t>(lootUpgradeRarity(item))];unique.insert(lootUpgradeIndex(item));}
    require(unique.size()==18 && counts[0]==5 && counts[2]==5 && counts[3]==5 && counts[4]==3,"all eighteen relics have their fixed four tiers");
    {
        LootChestSystem loot;
        for (auto tier:{LootRarity::Common,LootRarity::Rare,LootRarity::Epic,LootRarity::Legendary}) {
            std::set<LootUpgradeEffect> drawn;
            for (unsigned seed=0;seed<300;++seed) {
                loot.spawnLooseLoot({},tier,seed);
                const auto item=loot.chests().back().loot;
                require(item.rarity==lootUpgradeRarity(item.effect),"crate and loose drops keep canonical item rarity");
                require(std::ranges::find(ActiveLootEffects,item.effect)!=ActiveLootEffects.end(),"old items never enter new loot pools");
                drawn.insert(item.effect);
            }
            require(drawn.size()==static_cast<std::size_t>(tier==LootRarity::Epic?3:5),"every item in each tier can drop");
        }
    }
    const auto fixture=[] {
        auto s=std::make_unique<Simulation>();s->startRunFromSeed(PlayerClass::Vanguard,4811);
        return s;
    };
    const auto building=[](unsigned id,BuildingType type,int x=0,int z=0) {
        return BuildingInstance{.id={id,1},.type=type,.gridPosition={x,z},.health=100,.maxHealth=100};
    };
    const auto setBuildings=[](Simulation& s,std::vector<BuildingInstance> values) {
        auto levels=A::buildings(s).blueprintLevels();require(A::buildings(s).restoreBuildings(values,levels,20),"relic structure fixture valid");
        A::tick(s,0);
    };
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::LuckyDie, LootRarity::Legendary);
        A::events(*s).clear();int successes=0;
        for(int i=0;i<10000;++i) successes+=A::roll(*s,.1,2);
        require(successes>3200 && successes<3700,"two stacks and one die give independent 1-.9^4 chance");
        const auto serial=A::state(*s).manualRollSerial;
        require(!A::roll(*s,.1,0) && A::state(*s).manualRollSerial==serial,"absent items do not consume random rolls");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::Acorn);s->grantLootUpgrade(LootUpgradeEffect::LumberBoot);s->grantLootUpgrade(LootUpgradeEffect::Abacus);
        ResourceSystem nodes({{ResourceType::Wood,{},.3,10,10,90},{ResourceType::Stone,{2,0,0},.3,10,10,90}});
        A::resources(*s)=std::move(nodes);const auto tree=A::resources(*s).nodes()[0].id;(void)A::resources(*s).damage(tree,10);
        // Deterministically choose a successful acorn serial, then process the actual depletion event.
        while(!A::roll(*s,.1,1)) {} --A::state(*s).manualRollSerial;
        A::events(*s)={{.type=GameEventType::ResourceCollected,.entityId=tree,.resourceType=ResourceType::Wood,.damage=10}};A::process(*s);
        requireNear(A::state(*s).bootRemaining,5,1e-9,"tree depletion starts five second boot buff");
        requireNear(A::resources(*s).nodes()[0].respawnRemaining,1.5,1e-9,"acorn schedules accelerated real resource respawn");
        const auto treePosition=A::resources(*s).nodes()[0].position;
        A::resources(*s).tick(2,{},48,Vec3{0,0,0});
        require(!A::resources(*s).nodes()[0].active,"acorn waits while player occupies its regrowth spot");
        A::resources(*s).tick(.1,{},48,Vec3{10,0,10});
        require(A::resources(*s).nodes()[0].active && A::resources(*s).nodes()[0].position.x==treePosition.x &&
            A::resources(*s).nodes()[0].position.z==treePosition.z,"acorn grows the same tree locally instead of relocating across the map");
        const auto& growingTree=A::resources(*s).nodes()[0];
        requireNear(resourceRegrowthScale(growingTree),.18,1e-6,"acorn tree begins at destruction's final scale");
        requireNear(resourceRegrowthOpacity(growingTree),0,1e-6,"acorn tree begins transparent");
        A::resources(*s).tick(ResourceRegrowthDuration*.5,{},48);
        requireNear(resourceRegrowthScale(growingTree),.59,1e-6,"acorn tree grows smoothly through the reverse destruction midpoint");
        require(resourceRegrowthOpacity(growingTree)>0 && resourceRegrowthOpacity(growingTree)<1,
            "acorn tree fades in while growing");
        A::resources(*s).tick(0,{},48);
        requireNear(resourceRegrowthScale(growingTree),.59,1e-6,"paused resource updates preserve regrowth pose");
        ResourceSystem restoredNodes({{ResourceType::Wood,{},.3,10,10,90},{ResourceType::Stone,{2,0,0},.3,10,10,90}});
        require(restoredNodes.restoreNodes(A::resources(*s).nodes(),1.0),"in-progress regrowth can be restored");
        requireNear(restoredNodes.nodes()[0].regrowthRemaining,growingTree.regrowthRemaining,1e-9,
            "resource restore retains growth progress rather than popping to full size");
        A::resources(*s).tick(ResourceRegrowthDuration,{},48);
        requireNear(resourceRegrowthScale(growingTree),1,1e-6,"regrowth finishes at the normal tree size");
        requireNear(resourceRegrowthOpacity(growingTree),1,1e-6,"regrowth finishes fully opaque");
        requireNear(A::production(*s,ResourceType::Wood),1.01,1e-9,"abacus grows matching producer only");
        requireNear(A::production(*s,ResourceType::Stone),1,1e-9,"abacus does not buff another resource");
        for(int i=0;i<150;++i) {A::events(*s)={{.type=GameEventType::ResourceCollected,.sourceId=EntityId{5,1},.resourceType=ResourceType::Wood}};A::process(*s);}
        requireNear(A::production(*s,ResourceType::Wood),2,1e-9,"abacus caps at one hundred depletions per type");
        s->grantLootUpgrade(LootUpgradeEffect::Abacus);requireNear(A::production(*s,ResourceType::Wood),3,1e-9,"additional abacus multiplies earned bonus without reset");
        A::tick(*s,5);requireNear(A::state(*s).bootRemaining,0,1e-9,"boot buff expires");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::CrystalPan);s->grantLootUpgrade(LootUpgradeEffect::MiningHammer);s->grantLootUpgrade(LootUpgradeEffect::Abacus);
        A::resources(*s)=ResourceSystem({{ResourceType::Stone,{},.3,100,10,90},{ResourceType::Stone,{1,0,0},.3,10,10,90},{ResourceType::Stone,{6,0,0},.3,100,10,90}});
        const auto farHealth=A::resources(*s).nodes()[2].health;
        const auto id=A::resources(*s).nodes()[0].id;(void)A::resources(*s).damage(id,100);
        while(!A::roll(*s,.05,1)) {} --A::state(*s).manualRollSerial;
        A::events(*s)={{.type=GameEventType::ResourceCollected,.entityId=id,.resourceType=ResourceType::Stone,.position={},.damage=100}};A::process(*s);A::grants(*s);
        require(s->snapshot().crystals==1,"crystal pan grants crystal through ordinary resource grant path");
        require(!A::resources(*s).nodes()[1].active && A::resources(*s).nodes()[2].health==farHealth,"hammer damages nearby stones, range bounded and nonrecursive");
        requireNear(A::production(*s,ResourceType::Stone),1.02,1e-9,"hammer depletion counts once for abacus");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::RepairNail);s->grantLootUpgrade(LootUpgradeEffect::RepairMagnet);
        auto a=building(1000,BuildingType::GunTurret);a.health=50;auto b=building(1001,BuildingType::GunTurret,2);b.health=10;
        setBuildings(*s,{a,b});A::stocks(*s,0,0,0);
        while(!A::roll(*s,.1,1)) {} --A::state(*s).manualRollSerial;
        A::events(*s).clear();A::repair(*s,a.id);A::process(*s);
        const auto& all=A::buildings(*s).buildings();require(all[0].health==100,"free nail repairs despite empty inventory");
        requireNear(all[1].health,25,1e-9,"magnet heals closest other structure by thirty percent of actual manual repair");
        require(s->snapshot().wood==0 && s->snapshot().stone==0,"free repair never subtracts costs");
    }
    {
        TowerSystem towers;CannonSystem cannons;
        auto t=building(1000,BuildingType::GunTurret);auto c=building(1001,BuildingType::Mortar);
        const auto normal=towers.finalStats(t).attackInterval;const auto mortar=cannons.finalStats(c).attackInterval;
        towers.setSirenStacks(2);cannons.setSirenStacks(2);t.health=39;c.health=39;
        requireNear(towers.finalStats(t).attackInterval,normal/1.3,1e-9,"siren uses actual tower attack interval");
        requireNear(cannons.finalStats(c).attackInterval,mortar/1.3,1e-9,"siren applies to artillery too");
        t.health=40;requireNear(towers.finalStats(t).attackInterval,normal,1e-9,"forty percent threshold is strictly below");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::Clover);A::stocks(*s,0,0,0);A::barrel(*s,ResourceType::Barrel);
        auto snap=s->snapshot();require(snap.wood==6 && snap.stone==3 && snap.crystals==1,"clover grants each resource from capacity");
        A::barrel(*s,ResourceType::Crate);require(s->snapshot().wood==6,"clover does not trigger on crates");
        s->grantLootUpgrade(LootUpgradeEffect::DawnCoin);A::stocks(*s,20,20,0);A::dawn(*s);
        require(s->snapshot().wood==21 && s->snapshot().stone==21,"dawn coin adds five percent of own inventory");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::WeaponToken);s->grantLootUpgrade(LootUpgradeEffect::IdleSpring);
        auto& enemies=A::enemies(*s);enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Heavy,.position={0,0,-2}}});
        const auto target=enemies.enemies()[0].id;const double hp=enemies.enemy(target)->health;
        A::debug(*s,{.selectWeapon=SelectWeaponCommand{PlayerWeapon::Pickaxe}});
        require(A::state(*s).weaponTokenReady,"a real weapon switch arms token");
        A::state(*s).weaponTokenReady=false;
        A::debug(*s,{.selectWeapon=SelectWeaponCommand{PlayerWeapon::Pickaxe}});
        require(!A::state(*s).weaponTokenReady,"reselecting current weapon does not rearm token");
        A::state(*s).weaponTokenReady=true;
        A::events(*s)={{.type=GameEventType::PickaxeHit,.entityId=target,.damage=10}};A::process(*s);
        requireNear(enemies.enemy(target)->health,hp-2.5,1e-9,"token applies only the next player hit bonus");
        A::events(*s)={{.type=GameEventType::PickaxeHit,.entityId=target,.damage=10}};A::process(*s);
        requireNear(enemies.enemy(target)->health,hp-2.5,1e-9,"token is consumed once");
        const GameEvent hit{.type=GameEventType::ProjectileHit,.entityId=target,.sourceId=EntityId{1000,1},.buildingType=BuildingType::GunTurret,.damage=10,.projectileDamage=10};
        A::events(*s)={hit,hit};A::process(*s);requireNear(enemies.enemy(target)->health,hp-6.5,1e-9,"spring boosts one hit even with simultaneous multi-shot");
        A::elapsed(*s,2.9);A::events(*s)={hit};A::process(*s);requireNear(enemies.enemy(target)->health,hp-6.5,1e-9,"spring waits for three full seconds without hits");
        A::elapsed(*s,3.5);A::events(*s)={{.type=GameEventType::TowerFired,.sourceId=EntityId{1000,1},.buildingType=BuildingType::GunTurret}};A::process(*s);
        A::elapsed(*s,5.91);A::events(*s)={hit};A::process(*s);
        requireNear(enemies.enemy(target)->health,hp-6.5,1e-9,"missed shots prevent spring from charging while firing");
        A::elapsed(*s,9.0);A::events(*s)={hit};A::process(*s);requireNear(enemies.enemy(target)->health,hp-10.5,1e-9,"spring rearms after new idle interval");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::CoreFuse);s->grantLootUpgrade(LootUpgradeEffect::CrystalSafe);
        auto core=building(1000,BuildingType::Core);setBuildings(*s,{core});A::stocks(*s,0,0,100);A::night(*s);A::wave(*s);
        require(A::state(*s).safeDeposits==3,"safe locks maximum three deposits at nightfall without spending crystals");
        const auto savedCrystals=s->snapshot().crystals;A::stocks(*s,0,0,0);require(A::state(*s).safeDeposits==3,"night safe buff does not fluctuate after spending");
        require(savedCrystals==100,"safe never spends stock");
        const auto hit=A::damage(*s,core.id,1000);require(hit && !hit->destroyed && hit->remainingHealth==1,"fuse prevents lethal core hit");
        requireNear(A::state(*s).coreStasisRemaining,5,1e-9,"fuse stops all enemy simulation for five seconds");
        A::enemies(*s).spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={3,0,3}},EnemySpawn{.type=EnemyType::Boss,.position={5,0,5}}});
        const auto frozen=A::enemies(*s).enemies();
        for(int i=0;i<12;++i) s->tick(.1,{});
        const auto& stopped=A::enemies(*s).enemies();
        for(std::size_t i=0;i<frozen.size();++i) requireNear(stopped[i].position.x,frozen[i].position.x,1e-9,"fuse stops boss and ordinary enemy movement");
        A::tick(*s,5);const auto second=A::damage(*s,core.id,1000);require(second && second->destroyed,"fuse cannot save core twice in one night");
        A::night(*s);require(!A::state(*s).fuseUsed,"new night refreshes fuse");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::GhostCandle);s->grantLootUpgrade(LootUpgradeEffect::GhostCandle);s->grantLootUpgrade(LootUpgradeEffect::RebuildBlueprint);
        A::resources(*s)=ResourceSystem(std::vector<ResourceNodeDefinition>{});
        auto t=building(1000,BuildingType::TeslaTower);auto core=building(1001,BuildingType::Core,8,8);setBuildings(*s,{t,core});A::destroy(*s,t);
        require(A::buildings(*s).buildings().size()==1 && A::state(*s).ghosts.size()==1,"ghost is no longer a live collision/repair/production building");
        requireNear(A::state(*s).ghosts[0].remaining,13,1e-9,"extra candle adds five seconds");
        A::enemies(*s).spawnGroup(std::array{EnemySpawn{.type=EnemyType::Heavy,.position={0,0,-2}}});
        const double hp=A::enemies(*s).enemies()[0].health;A::attacks(*s,1);
        require(A::enemies(*s).enemies()[0].health<hp,"ghost fires through real tower combat");
        A::tick(*s,13);require(A::state(*s).ghosts.empty(),"ghost expires with no perpetual tower");
        A::dawn(*s);const auto restored=std::ranges::find(A::buildings(*s).buildings(),t.id,&BuildingInstance::id);
        require(restored!=A::buildings(*s).buildings().end(),"blueprint restores real destroyed tower");
        requireNear(restored->health/restored->maxHealth,.4,1e-9,"blueprint restores forty percent HP");
        require(A::state(*s).destroyedTowers.empty(),"restored tower cannot be restored twice");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::GhostCandle);
        A::resources(*s)=ResourceSystem(std::vector<ResourceNodeDefinition>{});
        auto t=building(1000,BuildingType::GunTurret);setBuildings(*s,{t});A::destroy(*s,t);
        A::dawn(*s);require(A::state(*s).ghosts.size()==1 && A::state(*s).ghosts[0].remaining==8,"dawn does not shorten candle duration");
        s->grantLootUpgrade(LootUpgradeEffect::RebuildBlueprint);A::dawn(*s);
        require(A::state(*s).ghosts.empty() && A::buildings(*s).buildings().size()==1,"blueprint replaces own ghost with exactly one live tower");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::UpgradeMirror);
        require(!A::mirror(*s,RunUpgradeEffect::UnlockCannon),"mirror ignores new buildings");
        for(int n=1;n<=5;++n) require(A::mirror(*s,RunUpgradeEffect::MoveSpeed)==(n==5),"mirror duplicates every fifth stat choice");
        s->grantLootUpgrade(LootUpgradeEffect::UpgradeMirror);s->grantLootUpgrade(LootUpgradeEffect::UpgradeMirror);s->grantLootUpgrade(LootUpgradeEffect::UpgradeMirror);s->grantLootUpgrade(LootUpgradeEffect::UpgradeMirror);
        require(!A::mirror(*s,RunUpgradeEffect::MoveSpeed) && A::mirror(*s,RunUpgradeEffect::MoveSpeed),"mirror interval bottoms out at two");
        A::state(*s).chosenStatCards=1;
        const auto before=s->saveSuspendedRunState().runPlayerMoveSpeedMultiplier;
        A::upgradeOffer(*s,RunUpgradeEffect::MoveSpeed);require(s->selectRunUpgrade(0),"mirror card can be chosen");
        requireNear(s->saveSuspendedRunState().runPlayerMoveSpeedMultiplier-before,.16,1e-9,"actual card effect applies twice, not just preview");
    }
    {
        auto s=fixture();s->grantLootUpgrade(LootUpgradeEffect::Abacus);A::state(*s).depletedResources={17,23,9};A::state(*s).chosenStatCards=4;A::state(*s).manualRollSerial=71;
        auto saved=s->saveSuspendedRunState();auto loaded=std::make_unique<Simulation>();require(loaded->loadSuspendedRunState(saved),"relic save loads");
        require(A::state(*loaded).depletedResources==A::state(*s).depletedResources && A::state(*loaded).manualRollSerial==71 && A::state(*loaded).chosenStatCards==4,"relic counters and random sequence survive save/load");
        saved.relics.depletedResources[0]=101;loaded->returnToMainMenu();require(!loaded->loadSuspendedRunState(saved),"invalid relic progression rejected");
        loaded->restartRun();require(A::state(*loaded).chosenStatCards==0 && A::state(*loaded).depletedResources[0]==0,"new run clears relic progress");
    }
}
