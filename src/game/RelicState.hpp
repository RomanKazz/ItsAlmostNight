#pragma once
#include "buildings/BuildingSystem.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <cstdint>
#include <vector>

namespace ian {
[[nodiscard]] constexpr bool isRelicFiringTower(BuildingType type) {
    switch(type) {
    case BuildingType::Turret: case BuildingType::GunTurret: case BuildingType::Cannon:
    case BuildingType::Catapult: case BuildingType::Mortar: case BuildingType::MissileLauncher:
    case BuildingType::TeslaTower: case BuildingType::LaserTower: case BuildingType::Flamethrower:
    case BuildingType::WindBlower: return true;
    default:return false;
    }
}
struct RelicGhost { BuildingInstance building; double remaining{}; };
struct RelicTowerHitClock { EntityId id; double lastAttackSeconds{-1e9}; bool springArmed{}; };
// Persisted separately from item stacks: acquiring another copy never resets progress.
struct RelicState {
    std::array<int, 3> depletedResources{};
    std::uint64_t manualRollSerial{};
    int chosenStatCards{};
    double bootRemaining{};
    double coreStasisRemaining{};
    int safeDeposits{};
    bool fuseUsed{};
    bool weaponTokenReady{};
    std::vector<BuildingInstance> destroyedTowers;
    std::vector<RelicGhost> ghosts;
    std::vector<RelicTowerHitClock> towerHitClocks;
};
[[nodiscard]] inline bool validRelicState(const RelicState& state, double coordinateLimit, int maxStoreys) {
    if (state.chosenStatCards<0 || state.chosenStatCards>5 || state.safeDeposits<0 || state.safeDeposits>3 ||
        !std::isfinite(state.bootRemaining) || state.bootRemaining<0 || state.bootRemaining>5 ||
        !std::isfinite(state.coreStasisRemaining) || state.coreStasisRemaining<0 || state.coreStasisRemaining>1000000 ||
        std::ranges::any_of(state.depletedResources,[](int n){return n<0 || n>100;}) ||
        state.ghosts.size()>10000 || state.destroyedTowers.size()>10000 || state.towerHitClocks.size()>10000) return false;
    const auto validTower=[&](const BuildingInstance& b) {
        return isRelicFiringTower(b.type) && b.id.index>=1000 && b.id.index<UINT32_MAX-4096 && b.id.generation!=0 &&
            b.rotation<16 && b.level>0 && std::isfinite(b.health) && std::isfinite(b.maxHealth) &&
            b.health>0 && b.health<=b.maxHealth && b.maxHealth<=1e9 &&
            std::abs(static_cast<double>(b.gridPosition.x))<=coordinateLimit && std::abs(static_cast<double>(b.gridPosition.z))<=coordinateLimit &&
            std::isfinite(b.baseHeight) && std::abs(b.baseHeight)<=1000 &&
            std::isfinite(b.foundationBottomHeight) && std::abs(b.foundationBottomHeight)<=1000 && b.platformStorey>=-1 && b.platformStorey<maxStoreys;
    };
    std::unordered_set<std::uint64_t> ids;
    const auto unique=[&](EntityId id){return ids.insert((static_cast<std::uint64_t>(id.generation)<<32U)|id.index).second;};
    for (const auto& b:state.destroyedTowers) if(!validTower(b) || !unique(b.id)) return false;
    ids.clear();
    for (const auto& g:state.ghosts) if(!validTower(g.building) || !unique(g.building.id) || !std::isfinite(g.remaining) || g.remaining<=0 || g.remaining>1000000) return false;
    ids.clear();
    for (const auto& c:state.towerHitClocks) if(!unique(c.id) || !std::isfinite(c.lastAttackSeconds)) return false;
    return true;
}
}
