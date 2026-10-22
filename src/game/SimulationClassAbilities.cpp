#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ian {

void Simulation::updatePlayerClassAbilities(double deltaSeconds) {
    classAbilityState_.exhaustionRemaining = std::max(0.0,classAbilityState_.exhaustionRemaining-deltaSeconds);
    const double rageBefore = classAbilityState_.rageRemaining;
    classAbilityState_.rageRemaining = std::max(0.0,rageBefore-deltaSeconds);
    if (rageBefore > 0 && classAbilityState_.rageRemaining <= 0) {
        classAbilityState_.exhaustionRemaining = std::max(0.0,2-(deltaSeconds-rageBefore));
        refreshRunUpgradeRuntimeEffects();
    }
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
