#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "progression/MetaProgression.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
#include <set>

namespace ian {
struct BasicClassesTestAccess {
    static bool owns(const Simulation& s,BuildingType type){return s.ownsRunBuilding(type);}
    static bool weapon(const Simulation& s,PlayerWeapon type){return s.playerWeaponUnlocked(type);}
    static bool hasRoots(const Simulation& s){return !s.stumpRootWaves_.empty();}
    static const auto& events(const Simulation& s){return s.events_;}
    static void level(Simulation& s,int level) {
        auto progress=s.saveProgressionState();
        progress.insight.progress.totalLevelsEarned=level-1;
        require(s.loadProgressionState(progress),"level-up fixture accepts class progression");
    }
    static void refresh(Simulation& s) {s.refreshRunUpgradeRuntimeEffects();s.invalidateSnapshotCache();}
    static void position(Simulation& s) {
        s.playerPosition_={0,1.7,0};s.playerYaw_=3.141592653589793;s.playerPitch_=0;
        s.aimedEnemy_.reset();s.aimedResource_.reset();s.invalidateSnapshotCache();
    }
    static EnemySystem& enemies(Simulation& s){return s.enemies_;}
    static double strike(Simulation& s) {
        s.pickaxeCooldownRemaining_=0;s.events_.clear();s.updatePlayerActions(0,{.usePickaxe=true});s.invalidateSnapshotCache();
        double damage=0;for(const auto& event:s.events_)if(event.type==GameEventType::PickaxeHit)damage+=event.damage;
        return damage;
    }
    static double rifleShot(Simulation& s) {
        s.events_.clear();s.playerWeapons_.selectWeapon(PlayerWeapon::Rifle);s.playerPosition_.y=.6;s.playerWeapons_.reloadImmediately();
        s.updatePlayerActions(0,{.fireRifle=true});s.invalidateSnapshotCache();
        double damage=0;for(const auto& event:s.events_)if(event.type==GameEventType::ProjectileHit)damage+=event.damage;
        return damage;
    }
    static double rifleInterval(const Simulation& s){return s.playerWeapons_.fireInterval();}
    static double bombInterval(const Simulation& s){return s.bombs_.cooldownDuration();}
    static void armBombCooldown(Simulation& s){s.bombs_.restoreCooldown(s.bombs_.cooldownDuration());s.invalidateSnapshotCache();}
    static BuildingSystem& buildings(Simulation& s){return s.buildings_;}
    static FoundationSystem& foundations(Simulation& s){return s.foundations_;}
    static void stock(Simulation& s,int wood,int stone,int crystal){s.wood_=wood;s.stone_=stone;s.crystals_=crystal;s.invalidateSnapshotCache();}
    static void repair(Simulation& s,EntityId id){s.events_.clear();s.processBuildingActions({.repairBuilding=RepairBuildingCommand{id}});s.invalidateSnapshotCache();}
    static double productionInterval(const Simulation& s){return s.crystalMines_.productionInterval(BuildingType::LumberMill);}
};
}
int main() {
    using namespace ian;
    using A=BasicClassesTestAccess;
    auto balance=GameBalance::defaults();
    balance.gameplay.pickaxeDamageVariation=0;balance.gameplay.pickaxeCriticalChance=0;
    auto map=MapDefinition::defaults();map.obstacles.clear();map.resources.clear();
    map.resources.push_back({.type=ResourceType::Wood,.position={12,0,12},.radius=.5,.health=100,.yield=100,.respawnSeconds=30});
    map.resources.push_back({.type=ResourceType::Stone,.position={-12,0,12},.radius=.5,.health=100,.yield=100,.respawnSeconds=30});
    auto world=WorldConfig::defaults();world.terrainAmplitude=0;world.pondMinimumCount=world.pondMaximumCount=0;
    auto insight=InsightConfig::defaults();insight.baseRequirement=100000;insight.earlyRequirementDiscounts.fill(0);
    const auto create=[&](PlayerClass type) {
        auto s=std::make_unique<Simulation>(balance,map,world,insight);s->startRunFromSeed(type,812399U);A::position(*s);return s;
    };
    require(PlayerClassDefinitions.size()==4,"the active roster contains exactly four test characters");
    std::set<PlayerClass> ids;
    for(const auto& definition:PlayerClassDefinitions) {
        require(ids.insert(definition.type).second && isPlayerClassUnlocked(definition.type,{}),"every new character has a unique ID and starts unlocked");
        auto s=create(definition.type);
        const auto start=s->snapshot();
        require(start.playerClass==definition.type && start.selectedWeapon==definition.startingWeapon,"new class selects its real starting weapon");
        require(A::weapon(*s,PlayerWeapon::Axe) && A::weapon(*s,PlayerWeapon::Pickaxe),"every class keeps axe and pickaxe");
        require(A::owns(*s,definition.startingBuilding),"each class owns its advertised defense blueprint");
        require(start.classAbilityState.stumpCombo==0 && start.classAbilityState.rageRemaining==0,"new characters start without legacy special abilities");
        A::level(*s,11);
        const auto level=s->snapshot();
        const double hp=definition.type==PlayerClass::Lumberjack?30:20;
        requireNear(level.playerMaxHealth-start.playerMaxHealth,hp,1e-8,"ten earned levels grant the advertised flat HP");
        requireNear(level.playerHealth-start.playerHealth,hp,1e-8,"level growth grants only the newly added HP");
        A::refresh(*s);A::refresh(*s);A::level(*s,11);
        requireNear(s->snapshot().playerMaxHealth,level.playerMaxHealth,1e-8,"refresh and repeated progression loads do not stack growth");
        A::armBombCooldown(*s);
        const auto saved=s->saveSuspendedRunState();
        auto loaded=create(PlayerClass::Sharpshooter);loaded->returnToMainMenu();
        require(loaded->loadSuspendedRunState(saved),"new class ID round-trips through suspended-run validation");
        require(loaded->snapshot().playerClass==definition.type,"resume preserves the new character identity");
        requireNear(loaded->snapshot().bombCooldownRemaining,saved.bombCooldownRemaining,1e-8,
            "bomb cooldown validates and restores against the saved class, not the previous run's class");
        requireNear(A::bombInterval(*loaded),A::bombInterval(*s),1e-8,"resuming restores the class bomb speed after resetting transient projectiles");
        requireNear(loaded->snapshot().playerMaxHealth,level.playerMaxHealth,1e-8,"resume preserves derived level HP exactly once");
        requireNear(loaded->snapshot().playerHealth,level.playerHealth,1e-8,"resume does not heal the player again");
        for(std::size_t i=0;i<level.resourceNodes.size();++i)require(loaded->snapshot().resourceNodes[i].yield==level.resourceNodes[i].yield,"resuming cannot multiply resource yields twice");
        loaded->restartRun();
        requireNear(loaded->snapshot().playerMaxHealth,start.playerMaxHealth,1e-8,"restart resets level growth while retaining class base stats");
    }
    auto baseline=create(PlayerClass::None),lumber=create(PlayerClass::Lumberjack),shooter=create(PlayerClass::Sharpshooter),mechanic=create(PlayerClass::Mechanic),miner=create(PlayerClass::Miner);
    requireNear(shooter->snapshot().playerMaxHealth,baseline->snapshot().playerMaxHealth*.85,1e-8,"shooter starts with 15 percent less HP");
    requireNear(lumber->snapshot().playerMoveSpeedMultiplier,baseline->snapshot().playerMoveSpeedMultiplier*.9,1e-8,"lumberjack moves ten percent slower");
    require(miner->snapshot().lootStacks[lootUpgradeIndex(LootUpgradeEffect::CrystalPan)]==1,"miner starts with the actual crystal pan relic");
    const auto resourceYield=[](const Simulation& s,ResourceType type){
        for(const auto& node:s.snapshot().resourceNodes)if(node.type==type)return node.yield;
        return 0;
    };
    requireNear(resourceYield(*lumber,ResourceType::Wood),resourceYield(*baseline,ResourceType::Wood)*1.2,1,"lumberjack receives twenty percent more wood");
    require(resourceYield(*lumber,ResourceType::Stone)==resourceYield(*baseline,ResourceType::Stone),"lumberjack does not also gain stone yield");
    for(const auto type:{ResourceType::Wood,ResourceType::Stone})requireNear(resourceYield(*miner,type),resourceYield(*baseline,type)*1.2,1,"miner gets twenty percent extra manual resource yield");
    const auto spawn=[](Simulation& s){
        const std::array enemies{EnemySpawn{.type=EnemyType::Basic,.position={0,0,2},.healthMultiplier=1000}};
        A::enemies(s).spawnGroup(enemies);
    };
    // Compare the same club attack with variation and critical hits disabled.
    auto oldClub=create(PlayerClass::Stump);spawn(*oldClub);spawn(*lumber);spawn(*mechanic);
    const double normal=A::strike(*oldClub),boosted=A::strike(*lumber),reduced=A::strike(*mechanic);
    require(normal>0,"melee fixture actually damages an enemy");
    requireNear(boosted/normal,1.2,1e-8,"lumberjack's bonus reaches the real melee hit");
    requireNear(reduced/normal,.85,1e-8,"mechanic's drawback reduces actual club damage");
    A::level(*lumber,11);A::enemies(*lumber).reset();spawn(*lumber);
    requireNear(A::strike(*lumber)/normal,1.4,1e-8,"melee growth adds base percentages instead of compounding");
    require(lumber->snapshot().classAbilityState.stumpCombo==0 && !A::hasRoots(*lumber),"lumberjack cannot trigger retired Stump roots");
    auto oldRifle=create(PlayerClass::None);spawn(*shooter);spawn(*oldRifle);
    requireNear(A::rifleInterval(*shooter),A::rifleInterval(*oldRifle)/1.15,1e-8,"ranged speed reaches the real rifle interval");
    requireNear(A::bombInterval(*shooter),A::bombInterval(*oldRifle)/1.15,1e-8,"ranged speed also reaches bombs");
    const double baseShot=A::rifleShot(*oldRifle);require(baseShot>0,"rifle fixture actually damages an enemy");
    A::level(*shooter,11);
    requireNear(A::rifleShot(*shooter)/baseShot,1.2,1e-8,"ranged growth reaches the actual rifle shot");
    A::level(*miner,11);
    requireNear(miner->snapshot().upgradeStats.productionSpeed,1.2,1e-8,"miner gains twenty percent production after ten levels");
    requireNear(A::productionInterval(*miner),A::productionInterval(*baseline)/1.2,1e-8,"production growth changes actual producer cadence");
    A::level(*mechanic,11);
    requireNear(mechanic->snapshot().upgradeStats.defenseDamage,1.2,1e-8,"mechanic's defense damage grows with levels");
    requireNear(miner->snapshot().upgradeStats.defenseDamage,.9,1e-8,"miner pays its defense damage drawback");
    // Full-price stock is deliberately unavailable: validation must use the discount.
    auto& buildings=A::buildings(*mechanic);
    const auto core=buildings.place(BuildingType::Core,{0,0},0,999,999,999);
    const auto wall=buildings.place(BuildingType::Wall,{2,0},0,999,999,999);
    require(core && wall,"repair fixture builds core and wall");
    requireNear(wall->building.maxHealth,buildings.baseMaxHealth(BuildingType::Wall)*1.2,1e-8,"mechanic's health passive reaches real structures");
    static_cast<void>(buildings.damage(wall->building.id,wall->building.maxHealth*.8));
    const auto price=buildings.validateRepair(wall->building.id,999,999,999,.7).cost;
    const auto full=buildings.validateRepair(wall->building.id,999,999,999).cost;
    require(price.wood+price.stone<full.wood+full.stone,"the discount lowers the real repair bill");
    A::stock(*mechanic,price.wood,price.stone,price.crystals);A::repair(*mechanic,wall->building.id);
    require(std::ranges::any_of(A::events(*mechanic),[](const GameEvent& e){return e.type==GameEventType::BuildingRepaired;}),"repair succeeds with only the discounted resources");
    require(mechanic->snapshot().wood==0 && mechanic->snapshot().stone==0,"repair charges the discounted bill exactly once");
    require(buildings.validateRepair(core->building.id,999,999,999,.7).error==BuildingActionError::Unsupported,"mechanic still cannot manually repair the core");
    auto& frames=A::foundations(*mechanic);
    const auto frame=frames.placePlatformFrame(frames.previewFoundation({4.2,0,4.2},{0,1.7,0}));
    require(frame.has_value(),"modular repair fixture creates a supported ground platform");
    requireNear(frame->maxHealth,PlatformFrameMaxHealth*1.2,1e-8,"mechanic's building HP bonus also reaches platforms");
    static_cast<void>(frames.damage(frame->id,frame->maxHealth*.8));
    const auto modular=frames.repair(frame->id,999,999,.7);
    require(modular.valid() && modular.cost.wood==5 && modular.cost.stone==7,
        "platform repair discounts the same 80 percent missing-health bill before rounding");
    for(const auto& legacy:LegacyPlayerClassDefinitions) {
        require(!isSelectablePlayerClass(legacy.type) && playerClassDefinition(legacy.type),"retired classes remain readable but are not selectable");
        auto old=create(legacy.type);const auto saved=old->saveSuspendedRunState();old->returnToMainMenu();
        require(old->loadSuspendedRunState(saved),"every old class can resume an existing run");
    }
    return 0;
}
