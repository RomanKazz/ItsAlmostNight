#include "game/Simulation.hpp"
#include "game/StumpRoots.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ian {

void Simulation::updatePlayerClassAbilities(double deltaSeconds) {
    classAbilityState_.stumpPulseRemaining = std::max(0.0, classAbilityState_.stumpPulseRemaining-deltaSeconds);
    updateStumpRootWaves(deltaSeconds);
    classAbilityState_.exhaustionRemaining = std::max(0.0,classAbilityState_.exhaustionRemaining-deltaSeconds);
    const double rageBefore = classAbilityState_.rageRemaining;
    classAbilityState_.rageRemaining = std::max(0.0,rageBefore-deltaSeconds);
    if (rageBefore > 0 && classAbilityState_.rageRemaining <= 0) {
        classAbilityState_.exhaustionRemaining = std::max(0.0,2-(deltaSeconds-rageBefore));
        refreshRunUpgradeRuntimeEffects();
    }
}

void Simulation::registerStumpMeleeHit(Vec3 direction, double damage) {
    if (playerClass_ != PlayerClass::Stump || damage <= 0 || !std::isfinite(damage)) return;
    classAbilityState_.stumpPulseRemaining = .26;
    if (++classAbilityState_.stumpCombo < stump_roots::HitsToCharge) return;
    classAbilityState_.stumpCombo = 0;
    classAbilityState_.stumpPulseRemaining = .65;
    const double length = std::hypot(direction.x,direction.z);
    direction = length > 1e-6 ? Vec3{direction.x/length,0,direction.z/length}
        : lookDirection(playerYaw_,0);
    Vec3 origin{playerPosition_.x+direction.x*.45,0,playerPosition_.z+direction.z*.45};
    origin.y = terrain_.getHeight(origin.x,origin.z);
    if (stumpRootWaves_.size() >= 4) stumpRootWaves_.erase(stumpRootWaves_.begin());
    StumpRootWave wave{.origin=origin,.direction=direction,
        .damage=damage*stump_roots::DamageFraction,.scale=classLevelBonuses().rootWaveScale,.hitEnemies={}};
    wave.hitEnemies.reserve(64);
    stumpRootWaves_.push_back(std::move(wave));
    events_.push_back({.type=GameEventType::StumpRootWave,.position=origin,
        .targetPosition=Vec3{origin.x+direction.x,origin.y,origin.z+direction.z},
        .damage=damage*stump_roots::DamageFraction,.intensity=classLevelBonuses().rootWaveScale,
        .duration=stump_roots::VisualSeconds});
}

void Simulation::updateStumpRootWaves(double deltaSeconds) {
    if (deltaSeconds <= 0 || stumpRootWaves_.empty()) return;
    for (auto& wave : stumpRootWaves_) {
        const double previous = stump_roots::front(wave.elapsed,wave.scale);
        wave.elapsed += deltaSeconds;
        const double current = stump_roots::front(wave.elapsed,wave.scale);
        for (const auto& enemy : enemies_.enemies()) {
            if (!enemy.active || enemyIsFlying(enemy.type) || enemy.position.y > 1.7 ||
                !stump_roots::reaches(wave.origin,wave.direction,enemy.position,previous,current,wave.scale) ||
                std::ranges::find(wave.hitEnemies,enemy.id) != wave.hitEnemies.end()) continue;
            wave.hitEnemies.push_back(enemy.id);
            const auto hit = enemies_.damage(enemy.id,wave.damage);
            if (!hit) continue;
            static_cast<void>(enemies_.applyStatus(hit->id,StatusEffectType::Slow,{},
                stump_roots::SlowSeconds,stump_roots::SlowFraction));
            Vec3 contact = hit->position;
            contact.y += terrain_.getHeight(contact.x,contact.z)+.45;
            events_.push_back({.type=GameEventType::StumpRootHit,.entityId=hit->id,
                .position=contact,.damage=hit->damage,.projectileDamage=wave.damage,
                .secondaryImpact=true});
            if (hit->killed) events_.push_back(enemyKilledEvent(hit->id,hit->type,hit->eliteAffixes,hit->position));
        }
    }
    std::erase_if(stumpRootWaves_,[](const auto& wave) {return wave.elapsed >= stump_roots::TravelSeconds;});
}

void Simulation::chainHarvestResources(EntityId depleted, ResourceType type, Vec3 center, double damage) {
    if (playerClass_ != PlayerClass::Prospector || !hasStorageSpace(type)) return;
    struct Target { EntityId id; Vec3 position; double distance; };
    std::vector<Target> targets;
    for (const auto& node : resources_.nodes()) {
        if (!node.active || node.id == depleted || node.type != type) continue;
        const double distance = std::hypot(node.position.x-center.x,node.position.z-center.z);
        if (distance <= classLevelBonuses().chainHarvestRadius) targets.push_back({node.id,node.position,distance});
    }
    std::ranges::sort(targets,[](const Target& a,const Target& b) {
        return a.distance == b.distance ? a.id.index < b.id.index : a.distance < b.distance;
    });
    for (std::size_t i=0;i<std::min(std::size_t{2},targets.size());++i) {
        const auto hit = resources_.damage(targets[i].id,damage*0.5);
        if (!hit) continue;
        events_.push_back({.type=hit->collected?GameEventType::ResourceCollected:GameEventType::ResourceHit,
            .entityId=hit->nodeId,.resourceType=hit->type,.position=hit->position,
            .targetPosition=targets[i].position,.amount=hit->amount,.damage=damage*0.5,
            .matchingTool=true,.night=state_==RunState::Wave || state_==RunState::Sunset});
        if (hit->amount > 0) pendingResourceGrants_.push_back({.type=hit->type,.position=hit->position,
            .amount=hit->amount,.remaining=ResourcePickupFlightSeconds});
        // Chained hits do not recursively trigger another chain.
    }
}

} // namespace ian
