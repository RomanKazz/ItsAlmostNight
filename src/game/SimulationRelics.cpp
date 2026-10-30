#include "game/Simulation.hpp"
#include "core/DeterministicRandom.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ResourceWorld.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace ian {
namespace {
double horizontalDistance(Vec3 a, Vec3 b) { return std::hypot(a.x-b.x, a.z-b.z); }
}
int Simulation::relicStacks(LootUpgradeEffect effect) const { return lootStacks_[lootUpgradeIndex(effect)]; }
bool Simulation::rollManualRelic(double chance, int stacks) {
    if (stacks <= 0) return false;
    // Independent stack rolls and die rerolls, collapsed to an equivalent single
    // Bernoulli trial. Bounded work even for debug-sized stacks; saved serial.
    const double attempts = static_cast<double>(stacks) * (1.0 + relicStacks(LootUpgradeEffect::LuckyDie));
    const double probability = -std::expm1(attempts * std::log1p(-std::clamp(chance, 0.0, 0.999999)));
    return unitRandom(mixBits64(static_cast<std::uint64_t>(terrain_.seed()) ^ ++relics_.manualRollSerial ^ 0x7050c2715eULL)) < probability;
}
double Simulation::relicProductionMultiplier(ResourceType type) const {
    const auto index = static_cast<std::size_t>(type);
    return index < 3 ? 1.0 + .01 * std::min(100, relics_.depletedResources[index]) * relicStacks(LootUpgradeEffect::Abacus) : 1.0;
}
void Simulation::rebuildRelicCombatBuildings() {
    if (relics_.ghosts.empty()) { relicCombatBuildings_.clear(); return; }
    relicCombatBuildings_.assign(buildings_.buildings().begin(), buildings_.buildings().end());
    for (const auto& ghost : relics_.ghosts) if (ghost.remaining > 0) relicCombatBuildings_.push_back(ghost.building);
}
void Simulation::tickRelics(double seconds) {
    relics_.bootRemaining = std::max(0.0, relics_.bootRemaining-seconds);
    relics_.coreStasisRemaining = std::max(0.0, relics_.coreStasisRemaining-seconds);
    bool expired = false;
    for (auto& ghost : relics_.ghosts) { ghost.remaining -= seconds; expired |= ghost.remaining <= 0; }
    std::erase_if(relics_.ghosts, [](const auto& g){return g.remaining <= 0;});
    rebuildRelicCombatBuildings(); // Health changes must be visible to Siren immediately.
    if (expired) {
        towers_.syncBuildings(relics_.ghosts.empty()?buildings_.buildings():relicCombatBuildings_);
        cannons_.syncBuildings(relics_.ghosts.empty()?buildings_.buildings():relicCombatBuildings_);
    }
}
void Simulation::rememberDestroyedTower(const std::optional<BuildingInstance>& building) {
    if (!building || !isRelicFiringTower(building->type)) return;
    // One record per actual destruction; sales/debug removal never reach here.
    if (std::ranges::find(relics_.destroyedTowers, building->id, &BuildingInstance::id) == relics_.destroyedTowers.end()) {
        if (relics_.destroyedTowers.size() == 10000) relics_.destroyedTowers.erase(relics_.destroyedTowers.begin());
        relics_.destroyedTowers.push_back(*building);
    }
    const int candles = relicStacks(LootUpgradeEffect::GhostCandle);
    if (candles > 0) {
        auto ghost = *building;
        ghost.health = ghost.maxHealth; // Ghosts are invulnerable and do not trigger low-HP Siren.
        relics_.ghosts.push_back({ghost, 8.0 + 5.0 * (candles-1)});
    }
}
std::optional<BuildingDamageResult> Simulation::damageBuildingWithRelics(EntityId id, double damage) {
    const auto core = buildings_.core();
    if (core && core->id == id && relics_.coreStasisRemaining > 0) damage = 0;
    if (core && core->id == id && damage >= core->health &&
        state_ == RunState::Wave && !relics_.fuseUsed && relicStacks(LootUpgradeEffect::CoreFuse) > 0) {
        relics_.fuseUsed = true;
        relics_.coreStasisRemaining = 5.0 + 2.0 * (relicStacks(LootUpgradeEffect::CoreFuse)-1);
        // Leave one HP. Stasis also blocks pending enemy attacks/blast damage.
        damage = std::max(0.0, core->health - 1.0);
        if (damage <= 0) return BuildingDamageResult{core->id,core->type,core->gridPosition,core->health,false,core->baseHeight};
    }
    return buildings_.damage(id, damage);
}
void Simulation::relicNightfall() {
    relics_.fuseUsed = false;
    relics_.coreStasisRemaining = 0;
    relics_.safeDeposits = std::clamp(crystals_ / 25, 0, 3);
    refreshRunUpgradeRuntimeEffects();
}
void Simulation::relicDawn() {
    // Interest is computed before other dawn/wave rewards, from each stock separately.
    const auto interest = [&](int stock) {
        return static_cast<int>(std::min(static_cast<double>(std::numeric_limits<int>::max()),
            std::floor(stock * .05 * relicStacks(LootUpgradeEffect::DawnCoin))));
    };
    const std::array gains{interest(wood_),interest(stone_),interest(crystals_)};
    const std::array previous{wood_,stone_,crystals_};
    addWood(gains[0]); addStone(gains[1]); addCrystals(gains[2]);
    const std::array actual{wood_-previous[0],stone_-previous[1],crystals_-previous[2]};
    for (std::size_t i=0;i<actual.size();++i) if (actual[i]>0)
        events_.push_back({.type=GameEventType::ResourceGranted,.resourceType=static_cast<ResourceType>(i),.position=playerPosition_,.amount=actual[i]});
    relics_.safeDeposits = 0;
    relics_.coreStasisRemaining = 0;
    int remaining = relicStacks(LootUpgradeEffect::RebuildBlueprint);
    for (auto it = relics_.destroyedTowers.begin(); it != relics_.destroyedTowers.end() && remaining > 0;) {
        const auto& dead = *it;
        const bool blocked = std::ranges::any_of(buildings_.buildings(), [&](const auto& live) {
            return live.gridPosition == dead.gridPosition && live.platformStorey == dead.platformStorey;
        });
        // A demolished raised platform cannot support a restored tower.
        const bool wide=buildingFootprintHalfExtent(dead.type)>0.75;
        const auto surface=foundations_.buildingSurface(dead.gridPosition.x-(wide?1:0),dead.gridPosition.z-(wide?1:0),wide?2:1,dead.baseHeight);
        const bool supported=dead.platformStorey<0 || (surface && surface->storey==dead.platformStorey && std::abs(surface->height-dead.baseHeight)<.05);
        const auto center=buildingWorldPosition(dead);
        const double extent=buildingFootprintHalfExtent(dead.type);
        const bool resourceBlocked=dead.platformStorey<0 && resourceOverlapsRectangle(resources_.nodes(),
            center.x-extent,center.x+extent,center.z-extent,center.z+extent);
        if (blocked || !supported || resourceBlocked) { ++it; continue; }
        auto restored = dead;
        restored.health = restored.maxHealth * .40;
        if (!buildings_.restoreDestroyedTower(restored)) { ++it; continue; }
        --remaining;
        std::erase_if(relics_.ghosts,[&](const auto& ghost){return ghost.building.id==restored.id;});
        events_.push_back({.type=GameEventType::BuildingPlaced,.entityId=restored.id,
            .buildingType=restored.type,.building=restored,.position=buildingWorldPosition(restored)});
        it = relics_.destroyedTowers.erase(it);
    }
    syncWorldStructures();
    refreshRunUpgradeRuntimeEffects();
}
bool Simulation::mirrorStatCard(RunUpgradeEffect effect) {
    const int mirrors = relicStacks(LootUpgradeEffect::UpgradeMirror);
    // Acquisitions do not count; stat rolls for an already owned defense do.
    if (mirrors <= 0 || runUpgradeCannonStyle(effect)) return false;
    if (runUpgradeBuilding(effect) && (!runDefenseCard(effect) || runUpgradeStacks_[runUpgradeIndex(effect)] <= 1)) return false;
    if (runUpgradeWeapon(effect) && (!runWeaponLevelCard(effect) || runUpgradeStacks_[runUpgradeIndex(effect)] <= 1)) return false;
    if (effect == RunUpgradeEffect::RerollToken || effect == RunUpgradeEffect::WiderChoice ||
        effect == RunUpgradeEffect::LockChoice || effect == RunUpgradeEffect::DoubleDown) return false;
    relics_.chosenStatCards = saturatingAdd(relics_.chosenStatCards, 1);
    if (relics_.chosenStatCards < std::max(2,6-mirrors)) return false;
    relics_.chosenStatCards=0;
    return true;
}
void Simulation::processRelicEvents(std::size_t firstEvent) {
    bool productionChanged = false;
    const bool springOwned=relicStacks(LootUpgradeEffect::IdleSpring)>0;
    const auto clockKey=[](EntityId id){return (static_cast<std::uint64_t>(id.generation)<<32U)|id.index;};
    const auto indexClocks=[&] {
        relicTowerClockIndices_.clear();
        for (std::size_t i=0;i<relics_.towerHitClocks.size();++i) relicTowerClockIndices_[clockKey(relics_.towerHitClocks[i].id)]=i;
    };
    if (springOwned && relicTowerClockIndices_.size()!=relics_.towerHitClocks.size()) indexClocks();
    const auto clockFor = [&](EntityId source) -> RelicTowerHitClock& {
        const auto key=clockKey(source);
        if (const auto found=relicTowerClockIndices_.find(key);found!=relicTowerClockIndices_.end()) return relics_.towerHitClocks[found->second];
        if (relics_.towerHitClocks.size()==10000) {relics_.towerHitClocks.erase(relics_.towerHitClocks.begin());indexClocks();}
        relicTowerClockIndices_[key]=relics_.towerHitClocks.size();
        relics_.towerHitClocks.push_back({source,-1e9,false});
        return relics_.towerHitClocks.back();
    };
    const auto initialLimit = events_.size();
    for (std::size_t i = firstEvent; i < initialLimit; ++i) {
        const auto event = events_[i]; // Appending effects can reallocate the event vector.
        if (springOwned && event.type==GameEventType::BuildingPlaced && event.entityId && event.buildingType && isRelicFiringTower(*event.buildingType)) {
            clockFor(*event.entityId).lastAttackSeconds=elapsedSeconds_;
        }
        if (springOwned && (event.type==GameEventType::TowerFired || event.type==GameEventType::CannonFired) && event.sourceId) {
            auto& clock=clockFor(*event.sourceId);
            if (elapsedSeconds_-clock.lastAttackSeconds >= 3.0) clock.springArmed=true;
            clock.lastAttackSeconds=elapsedSeconds_;
        }
        if ((event.type == GameEventType::ResourceHit || event.type == GameEventType::ResourceCollected) &&
            event.resourceType == ResourceType::Stone && !event.sourceId &&
            rollManualRelic(.05, relicStacks(LootUpgradeEffect::CrystalPan))) {
            pendingResourceGrants_.push_back({ResourceType::Crystal,event.position,1,ResourcePickupFlightSeconds});
        }
        if (event.type == GameEventType::ResourceCollected && event.resourceType && isHarvestableResource(*event.resourceType)) {
            const auto type = *event.resourceType;
            auto& depleted = relics_.depletedResources[static_cast<std::size_t>(type)];
            if (relicStacks(LootUpgradeEffect::Abacus) > 0) {
                depleted = std::min(100, depleted+1);
                productionChanged = true;
            }
            if (type == ResourceType::Wood && !event.sourceId) {
                if (relicStacks(LootUpgradeEffect::LumberBoot) > 0) relics_.bootRemaining = 5.0;
                if (event.entityId && rollManualRelic(.10, relicStacks(LootUpgradeEffect::Acorn)))
                    static_cast<void>(resources_.scheduleRegrowth(*event.entityId, 1.5));
            }
            if (type == ResourceType::Stone && !event.sourceId && relicStacks(LootUpgradeEffect::MiningHammer) > 0) {
                std::vector<EntityId> neighbours;
                for (const auto& node : resources_.nodes())
                    if (node.active && node.type == ResourceType::Stone && horizontalDistance(node.position,event.position) <= 4.0)
                        neighbours.push_back(node.id);
                const double damage = event.damage * .50 * relicStacks(LootUpgradeEffect::MiningHammer);
                for (const auto id : neighbours) if (const auto hit = resources_.damage(id, damage)) {
                    events_.push_back({.type=hit->collected?GameEventType::ResourceCollected:GameEventType::ResourceHit,
                        .entityId=id,.sourceId=event.entityId,.resourceType=hit->type,.position=hit->position,.amount=hit->amount,.damage=damage});
                    if (hit->amount > 0) pendingResourceGrants_.push_back({hit->type,hit->position,hit->amount,ResourcePickupFlightSeconds});
                    if (hit->collected && relicStacks(LootUpgradeEffect::Abacus) > 0) {auto& count=relics_.depletedResources[1];count=std::min(100,count+1);}
                }
            }
        }
        if (event.type == GameEventType::BuildingRepaired || event.type == GameEventType::ModularBuildingRepaired) {
            const int magnets = relicStacks(LootUpgradeEffect::RepairMagnet);
            if (magnets > 0 && event.amount > 0) {
                std::optional<EntityId> target;
                double nearest = 6.0;
                for (const auto& building : buildings_.buildings()) {
                    const double distance = horizontalDistance(buildingWorldPosition(building),event.position);
                    if (building.id != event.entityId && building.health < building.maxHealth && distance < nearest) {
                        target=building.id;nearest=distance;
                    }
                }
                if (target) static_cast<void>(buildings_.restoreHealth(*target,event.intensity * .30 * magnets));
            }
        }
        const bool direct = ChestItemCombatSystem::directHit(event);
        if (!direct || !event.entityId || event.damage <= 0) continue;
        double bonus = 0;
        if (springOwned && event.buildingType && event.sourceId && isRelicFiringTower(*event.buildingType)) {
            auto& clock=clockFor(*event.sourceId);
            // Tesla and Laser apply direct hits rather than projectile launches.
            if (elapsedSeconds_-clock.lastAttackSeconds >= 3.0) clock.springArmed=true;
            clock.lastAttackSeconds=elapsedSeconds_;
            if (clock.springArmed) {
                bonus=.40*relicStacks(LootUpgradeEffect::IdleSpring);
                clock.springArmed=false;
            }
        } else if (!event.buildingType && relics_.weaponTokenReady) {
            relics_.weaponTokenReady=false;
            bonus=.25*relicStacks(LootUpgradeEffect::WeaponToken);
        }
        if (bonus > 0) if (const auto hit=enemies_.damage(*event.entityId,
                (event.projectileDamage > 0 ? event.projectileDamage : event.damage)*bonus)) {
            events_.push_back({.type=GameEventType::ItemHit,.entityId=hit->id,.sourceId=event.sourceId,
                .buildingType=event.buildingType,.position=hit->position,.damage=hit->damage,.secondaryImpact=true});
            if (hit->killed) events_.push_back(enemyKilledEvent(hit->id,hit->type,hit->eliteAffixes,hit->position,event.sourceId,event.buildingType));
        }
    }
    if (productionChanged && relicStacks(LootUpgradeEffect::Abacus) > 0) refreshRunUpgradeRuntimeEffects();
}
}
