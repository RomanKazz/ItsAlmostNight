#include "TestHarness.hpp"
#include "game/ChestItemCombatSystem.hpp"
#include "game/Simulation.hpp"
#include "combat/AttackHit.hpp"
#include "combat/DefenseTargeting.hpp"
#include <algorithm>
#include <memory>

void runChestItemCombatTests() {
    using namespace ian;
    static_assert(lootUpgradeIndex(LootUpgradeEffect::FuelJerrycan) == 6);
    static_assert(lootUpgradeIndex(LootUpgradeEffect::Nail) == 8);
    static_assert(lootUpgradeIndex(LootUpgradeEffect::CoalBell) == 19);
    for (const auto effect : {RunUpgradeEffect::ProjectileCount, RunUpgradeEffect::Bounces,
        RunUpgradeEffect::Pierce, RunUpgradeEffect::ClusterPayload, RunUpgradeEffect::GunShrapnel,
        RunUpgradeEffect::NightlyBomb, RunUpgradeEffect::Overkill}) {
        const auto epic = runUpgradePowerGain(effect, RunUpgradeRarity::Epic);
        const auto rare = runUpgradePowerGain(effect, RunUpgradeRarity::Rare);
        requireNear(epic,1.6,1e-12,"count upgrades retain epic fractional power");
        requireNear(rare,1.4,1e-12,"count upgrades retain rare fractional power");
        require(runUpgradeDiscreteAmount(effect,epic+rare)==3*runUpgradeCountPerPower(effect),
            "all count effects floor the accumulated total, not individual awards");
    }
    auto definitions = GameBalance::defaults().enemies;
    for (auto& definition : definitions) definition.health = 200;
    const auto fixture = [&] {
        auto enemies = std::make_unique<EnemySystem>(definitions);
        const std::array<Vec3, 3> positions{{{0,0,-2}, {1.5,0,-2}, {3,0,-2}}};
        enemies->spawnWave(positions);
        return enemies;
    };
    std::array<int, LootUpgradeEffectCount> stacks{};
    const auto put = [&](LootUpgradeEffect e, int count = 1) { stacks[lootUpgradeIndex(e)] = count; };
    const auto direct = [](EntityId id) {
        return GameEvent{.type = GameEventType::ProjectileHit, .entityId = id,
            .sourceId = EntityId{1000,1}, .buildingType = BuildingType::GunTurret,
            .position = {0,0,-2}, .damage = 10, .projectileDamage = 10};
    };
    {
        auto physical = direct(EntityId{2000,1}); physical.secondaryImpact=true;
        require(ChestItemCombatSystem::directHit(physical),"physical bounce and pierce preserve item synergies");
        physical.amount=-1;
        require(!ChestItemCombatSystem::directHit(physical),"generated kill shards cannot reroll item effects");
    }
    {
        auto enemies = fixture();
        const auto id = enemies->enemies()[0].id;
        enemies->applyPoison(id, {}, 4, 10);
        enemies->applyPoison(id, {}, 4, 5);
        requireNear(enemyStatusEffect(*enemies->enemy(id), StatusEffectType::Poison).intensity, 10, 1e-10,
            "ordinary poison keeps strongest dose without stacking");
        for (int n=0;n<8;++n) enemies->applyPoison(id, {}, 4, 10, {}, 4);
        const auto status = enemyStatusEffect(*enemies->enemy(id), StatusEffectType::Poison);
        require(status.poisonDoses == 4, "mushroom doses stop at their per-target bound");
        requireNear(status.intensity, 20.5, 1e-10, "mushroom strengthens poison multiplicatively with damage");
        (void)enemies->tickPoison(4);
        enemies->applyPoison(id, {}, 4, 10, {}, 4);
        require(enemyStatusEffect(*enemies->enemy(id), StatusEffectType::Poison).poisonDoses == 1,
            "expired mushroom doses do not leak into new poison");
    }
    {
        auto enemies = fixture(); ChestItemCombatSystem system;
        stacks.fill(0); put(LootUpgradeEffect::TargetingLens);
        const auto id = enemies->enemies()[1].id;
        auto hit = direct(id); hit.sourceId.reset(); hit.buildingType.reset();
        std::vector<GameEvent> events{hit}; system.tick(0, *enemies, stacks, events, 0);
        requireNear(enemies->markedDefenseDamage(id),1.25,1e-10,"lens marks surviving player-hit targets");
        require(markedDefenseTarget(*enemies,BuildingType::GunTurret,{},10,0,8) == id,
            "defenses prefer marked enemies in their valid sector");
        require(!markedDefenseTarget(*enemies,BuildingType::GunTurret,{},1,0,8),
            "lens cannot bypass defense range");
        require(!markedDefenseTarget(*enemies,BuildingType::GunTurret,{},10,3.141592653589793,1),
            "lens cannot bypass defense sector");
        AttackStats stats; stats.damage=10; stats.critChance=0; stats.knockback=0;
        const auto result=applyAttackHit(*enemies,id,stats,{},123);
        require(result.has_value(),"marked defense fixture hits");
        requireNear(result->result.damage,12.5,1e-10,"lens multiplies defense damage only");
        requireNear(enemies->damage(id,10)->damage,10,1e-10,"lens does not multiply player or periodic damage");
        auto simulation=std::make_unique<Simulation>(); simulation->startRun(PlayerClass::Prospector);
        for(std::size_t n=19;n<LootUpgradeEffectCount;++n) simulation->grantLootUpgrade(static_cast<LootUpgradeEffect>(n));
        require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::ProjectileCount,RunUpgradeRarity::Epic),
            "epic fractional tome can be granted");
        requireNear(simulation->snapshot().upgradeStats.projectileCountBonus,1.6,1e-12,
            "UI snapshot retains the fractional projectile bonus");
        require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::ProjectileCount,RunUpgradeRarity::Rare),
            "rare fractional tome can follow an epic tome");
        requireNear(simulation->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)],3,1e-12,
            "awards accumulate 1.6 plus 1.4 without rounding");
        require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::Bounces,RunUpgradeRarity::Epic),
            "fractional bounce remainder can be saved");
        const auto saved=simulation->saveSuspendedRunState();
        auto restored=std::make_unique<Simulation>();
        require(restored->loadSuspendedRunState(saved),"new items restore through run checkpoints");
        for(std::size_t n=19;n<LootUpgradeEffectCount;++n)
            require(restored->snapshot().lootStacks[n]==1,"new item stacks survive restore");
        requireNear(restored->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)],3,1e-12,
            "fractional tome accumulation survives save/load");
        requireNear(restored->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::Bounces)],1.6,1e-12,
            "save/load retains an unused 0.6 bounce remainder");
        require(restored->grantDebugRunUpgrade(RunUpgradeEffect::Bounces,RunUpgradeRarity::Rare),
            "a loaded remainder accepts another fractional upgrade");
        requireNear(restored->snapshot().upgradeStats.bounces,3,1e-12,
            "loaded fractional bounces accumulate to three");
        restored->restartRun();
        for(std::size_t n=19;n<LootUpgradeEffectCount;++n)
            require(restored->snapshot().lootStacks[n]==0,"new item stacks clear on reset");
    }
    {
        auto enemies=fixture(); ChestItemCombatSystem system;
        stacks.fill(0);put(LootUpgradeEffect::Nail);
        std::vector<GameEvent> events;
        for(int n=0;n<5;++n) events.push_back(direct(enemies->enemies()[0].id));
        system.tick(0,*enemies,stacks,events,0);
        require(std::count_if(events.begin(),events.end(),[](const GameEvent& e){return e.type==GameEventType::ChainLightningHit;})==2,
            "five turret hits trigger one two-jump nail chain");
        const auto after=enemies->enemy(enemies->enemies()[1].id)->health;
        const auto start=events.size(); system.tick(0,*enemies,stacks,events,start);
        requireNear(enemies->enemy(enemies->enemies()[1].id)->health,after,1e-10,"nail chain cannot recursively retrigger items");
        require(events.size()==start,"secondary nail events are bounded");
        system.reset();events.clear();
        auto first=direct(enemies->enemies()[0].id), second=first;second.sourceId=EntityId{1001,1};
        events={first,first,first,second,second,second};system.tick(0,*enemies,stacks,events,0);
        require(events.size()==6,"nail counts independently per building");
    }
    {
        auto enemies=fixture(); ChestItemCombatSystem system;
        stacks.fill(0);put(LootUpgradeEffect::PowderPouch);
        auto hit=direct(enemies->enemies()[0].id);hit.critical=true;
        std::vector<GameEvent> events{hit};system.tick(0,*enemies,stacks,events,0);
        requireNear(enemies->enemy(enemies->enemies()[1].id)->health,196.5,1e-10,"critical turret hit explodes into neighbors");
        requireNear(enemies->enemy(enemies->enemies()[0].id)->health,200,1e-10,"powder does not double-hit the primary target");
        events={hit};events.front().amount=-1;events.front().secondaryImpact=true;system.tick(0,*enemies,stacks,events,0);
        require(events.size()==1,"kill-shard crit cannot start a powder explosion");
    }
    {
        auto enemies=fixture();ChestItemCombatSystem system;
        stacks.fill(0);put(LootUpgradeEffect::CoalBell);
        auto hit=direct(enemies->enemies()[0].id);hit.buildingType=BuildingType::Flamethrower;
        std::vector<GameEvent> events{hit};system.tick(0,*enemies,stacks,events,0);
        const auto killed=enemies->damage(*hit.entityId,1000);
        events={enemyKilledEvent(killed->id,killed->type,killed->eliteAffixes,killed->position,hit.sourceId,hit.buildingType)};
        system.tick(0,*enemies,stacks,events,0);
        require(std::any_of(events.begin(),events.end(),[](const GameEvent& e){return e.type==GameEventType::ItemExplosion;}),
            "natural flamethrower kills activate coal without a canister");
        events.clear();system.tick(.5,*enemies,stacks,events,0);
        require(std::any_of(events.begin(),events.end(),[](const GameEvent& e){return e.type==GameEventType::ItemBurnHit && e.buildingType==BuildingType::Flamethrower;}),
            "coal spreads attributed afterburn to neighboring enemies");
    }
    {
        auto enemies=fixture();ChestItemCombatSystem system;
        stacks.fill(0);put(LootUpgradeEffect::InfectedPouch);
        const auto id=enemies->enemies()[0].id;
        enemies->applyPoison(id,EntityId{1000,1},4,10,BuildingType::GunTurret,4);
        const auto killed=enemies->damage(id,1000);
        std::vector<GameEvent> events{enemyKilledEvent(killed->id,killed->type,killed->eliteAffixes,killed->position)};
        system.tick(0,*enemies,stacks,events,0);
        require(system.clouds().size()==1,"poisoned death creates one cloud");
        events.clear();system.tick(.5,*enemies,stacks,events,0);
        require(enemyHasStatus(*enemies->enemy(enemies->enemies()[1].id),StatusEffectType::Poison),"cloud poisons a neighboring enemy");
        requireNear(ChestItemCombatSystem::gasMaskProtection({},*enemies,1),.03,1e-10,"mask protection comes from nearby poisoned enemies");
        requireNear(ChestItemCombatSystem::gasMaskProtection({},*enemies,100),.35,1e-10,"mask cannot grant immunity");
        requireNear(ChestItemCombatSystem::gasMaskProtection({100,0,100},*enemies,1),0,1e-10,"distant poison gives no mask protection");
        system.reset();require(system.clouds().empty(),"cloud storage clears on reset");
    }
    {
        auto enemies=fixture();ChestItemCombatSystem system;
        stacks.fill(0);put(LootUpgradeEffect::IcePrism);
        const auto id=enemies->enemies()[0].id, neighbor=enemies->enemies()[1].id;
        (void)enemies->applyStatus(id,StatusEffectType::Freeze,{},4);
        (void)enemies->applyStatus(neighbor,StatusEffectType::Slow,{},4,.5);
        const auto killed=enemies->damage(id,1000);
        std::vector<GameEvent> events{enemyKilledEvent(killed->id,killed->type,killed->eliteAffixes,killed->position)};
        system.tick(0,*enemies,stacks,events,0);
        requireNear(enemies->enemy(neighbor)->health,196.5,1e-10,"prism shards amplify damage against slowed enemies");
        const auto count=events.size();system.tick(0,*enemies,stacks,events,0);
        require(events.size()==count,"a frozen corpse shatters once");
    }
}
