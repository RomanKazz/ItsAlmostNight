#include "enemies/EnemySystem.hpp"
#include "core/Geometry.hpp"
#include "enemies/EnemyCollision.hpp"
#include "world/CollisionWorld.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {
constexpr std::array<double,3> Clearances{1.0,1.3,1.5};
std::size_t clearanceClass(const EnemyInstance& enemy) {
    const double radius=enemyCollisionCylinder(enemy).radius;
    return radius<=Clearances[0] ? 0 : radius<=Clearances[1] ? 1 : 2;
}
int fieldIndex(const auto& field, Vec3 position) {
    constexpr int size = 25, half = 12;
    const int x = static_cast<int>(std::lround(position.x)) - field.center.x + half;
    const int z = static_cast<int>(std::lround(position.z)) - field.center.z + half;
    return x >= 0 && z >= 0 && x < size && z < size ? z * size + x : -1;
}
void releaseDecoy(EnemyInstance& enemy) {
    if (enemy.target == enemy.decoyTarget && enemy.decoyTarget) {
        enemy.target.reset();
        enemy.state = EnemyState::MoveToCore;
    }
    enemy.decoyTarget.reset();
    enemy.decoyRemaining = 0;
    enemy.aiUpdateRemaining = 0;
}
}

void EnemySystem::updateDecoyBeacons(double deltaSeconds,
    const std::vector<BuildingInstance>& buildings, const FlowField& flowField,
    EnemyNavigationView navigation, bool layoutChanged) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) return;
    if (!layoutChanged && decoyBeacons_.empty()) return;
    const auto alive = [&](EntityId id) {
        return std::ranges::any_of(buildings, [id](const auto& b) {
            return b.id == id && b.type == BuildingType::DecoyBeacon && b.health > 0;
        });
    };
    std::erase_if(decoyBeacons_, [&](const auto& beacon) { return !alive(beacon.buildingId); });
    for (const auto& building : buildings) {
        if (building.type != BuildingType::DecoyBeacon || building.health <= 0) continue;
        auto beacon = std::ranges::find(decoyBeacons_, building.id, &DecoyBeaconRuntime::buildingId);
        if (beacon == decoyBeacons_.end()) {
            decoyBeacons_.push_back({building.id, buildingWorldPosition(building), building.level});
            layoutChanged = true;
        } else {
            beacon->position = buildingWorldPosition(building);
            beacon->level = building.level;
        }
    }
    if (decoyNavigation_.size() != decoyBeacons_.size()*Clearances.size()) layoutChanged = true;
    if (layoutChanged) {
        decoyNavigation_.resize(decoyBeacons_.size()*Clearances.size());
        // A bounded local field uses the existing world's static cells, solid
        // structures and collision checks. Rebuild only when the layout changes.
        // Four-connected routing cannot cut diagonally through closed walls.
        constexpr std::array<GridPosition,4> neighbors{{{-1,0},{0,-1},{1,0},{0,1}}};
        for (std::size_t i=0;i<decoyBeacons_.size();++i) {
            const auto& beacon = decoyBeacons_[i];
            for(std::size_t clearance=0;clearance<Clearances.size();++clearance) {
            const double bodyRadius=Clearances[clearance];
            auto& field = decoyNavigation_[i*Clearances.size()+clearance];
            field.center = {static_cast<int>(std::lround(beacon.position.x)),static_cast<int>(std::lround(beacon.position.z))};
            field.distance.fill(-1);
            for (int z=0;z<DecoyNavigationField::Size;++z) for (int x=0;x<DecoyNavigationField::Size;++x) {
                const GridPosition cell{field.center.x+x-DecoyNavigationField::HalfExtent,
                    field.center.z+z-DecoyNavigationField::HalfExtent};
                const Vec3 position{double(cell.x),beacon.position.y+1.7,double(cell.z)};
                const bool ownCell = std::abs(position.x-beacon.position.x)<1.1 && std::abs(position.z-beacon.position.z)<1.1;
                const auto* terrainCell = flowField.cellAt(cell);
                bool blocked = terrainCell ? terrainCell->blocked : navigation.collisionWorld == nullptr;
                if (!ownCell && !blocked) {
                    blocked = navigation.collisionWorld
                        ? !navigation.collisionWorld->canOccupyCircle(position,bodyRadius)
                        : terrainCell && terrainCell->terrainCost >= 8;
                    if (!blocked) for (const auto& structure : structureBuffer_) {
                        if (structure.id == beacon.buildingId || structure.traversable ||
                            beacon.position.y < structure.minimumEnemySurfaceHeight ||
                            beacon.position.y > structure.maximumEnemySurfaceHeight) continue;
                        const double dx=position.x-structure.position.x,dz=position.z-structure.position.z;
                        const double radius=structure.radius+bodyRadius;
                        if (dx*dx+dz*dz<radius*radius) {blocked=true;break;}
                    }
                }
                if (blocked) field.distance[static_cast<std::size_t>(z*DecoyNavigationField::Size+x)]=-2;
            }
            std::array<int,DecoyNavigationField::Size*DecoyNavigationField::Size> queue{};
            constexpr int center = DecoyNavigationField::HalfExtent*DecoyNavigationField::Size+DecoyNavigationField::HalfExtent;
            if (field.distance[center] == -2) continue;
            field.distance[center]=0;queue[0]=center;
            std::size_t head=0,tail=1;
            while(head<tail) {
                const int current=queue[head++];
                const int x=current%DecoyNavigationField::Size,z=current/DecoyNavigationField::Size;
                for(const auto offset:neighbors) {
                    const int nx=x+offset.x,nz=z+offset.z;
                    if(nx<0 || nz<0 || nx>=DecoyNavigationField::Size || nz>=DecoyNavigationField::Size)continue;
                    const auto next=static_cast<std::size_t>(nz*DecoyNavigationField::Size+nx);
                    if(field.distance[next]!=-1)continue;
                    field.distance[next]=static_cast<std::int16_t>(field.distance[static_cast<std::size_t>(current)]+1);
                    queue[tail++]=static_cast<int>(next);
                }
            }
            }
        }
    }
    for(auto& enemy:enemies_) {
        if(!enemy.active)continue;
        enemy.decoyFlashRemaining=std::max(0.0,enemy.decoyFlashRemaining-deltaSeconds);
        if(!enemy.decoyTarget)continue;
        enemy.decoyRemaining=std::max(0.0,enemy.decoyRemaining-deltaSeconds);
        if(enemy.decoyRemaining<=1e-9 || !alive(*enemy.decoyTarget) || !decoyWaypoint(enemy)) releaseDecoy(enemy);
    }
    for(std::size_t i=0;i<decoyBeacons_.size();++i) {
        auto& beacon=decoyBeacons_[i];
        beacon.pulseAge+=deltaSeconds;
        beacon.pulseRemaining-=deltaSeconds;
        if(beacon.pulseRemaining>1e-9)continue;
        // One pulse at the latest boundary; a long update never stacks taunts.
        beacon.pulseAge=std::fmod(std::max(0.0,-beacon.pulseRemaining),DecoyBeaconInterval);
        beacon.pulseRemaining=DecoyBeaconInterval-beacon.pulseAge;
        if(beacon.pulseAge>=DecoyBeaconDuration)continue;
        const double radius=decoyBeaconRadius(beacon.level);
        struct Candidate {EnemyInstance* enemy{};double distance{};};
        std::array<Candidate,15> nearest{};
        std::size_t count=0;
        const auto maximum=static_cast<std::size_t>(decoyBeaconTargets(beacon.level));
        spatialHash_.forEachNearby(beacon.position,radius,[&](const SpatialEntry& entry) {
            auto* enemy=findEnemy(entry.id);
            if(!enemy || !enemy->active || enemy->type==EnemyType::Boss || enemy->decoyTarget ||
                enemy->spawnAnimationRemaining>0 || enemy->portalArrivalRemaining>0 || enemy->splitAnimationRemaining>0) return;
            if(enemy->type!=EnemyType::Flying && std::abs(enemy->worldSurfaceHeight-beacon.position.y)>1.0) return;
            const double dx=enemy->position.x-beacon.position.x,dz=enemy->position.z-beacon.position.z;
            const double distance=dx*dx+dz*dz;
            if(distance>radius*radius) return;
            const auto& path=decoyNavigation_[i*Clearances.size()+clearanceClass(*enemy)];
            const int slot=fieldIndex(path,enemy->position);
            if(enemy->type!=EnemyType::Flying && (slot<0 || path.distance[static_cast<std::size_t>(slot)]<0)) return;
            if((enemy->eliteAffixes ? DecoyBeaconDuration*.5 : DecoyBeaconDuration)<=beacon.pulseAge) return;
            std::size_t insert=0;
            while(insert<count && (nearest[insert].distance<distance ||
                (nearest[insert].distance==distance && nearest[insert].enemy->id.index<enemy->id.index)))++insert;
            if(insert>=maximum)return;
            const auto last=std::min(count,maximum-1);
            for(std::size_t j=last;j>insert;--j)nearest[j]=nearest[j-1];
            nearest[insert]={enemy,distance};count=std::min(maximum,count+1);
        });
        for(std::size_t candidate=0;candidate<count;++candidate) {
            auto& enemy=*nearest[candidate].enemy;
            enemy.decoyTarget=beacon.buildingId;
            enemy.decoyRemaining=(enemy.eliteAffixes ? DecoyBeaconDuration*.5 : DecoyBeaconDuration)-beacon.pulseAge;
            enemy.decoyFlashRemaining=std::max(0.0,.65-beacon.pulseAge);
            enemy.aiUpdateRemaining=0;
            // Keep cooldown and already-launched projectiles: an attack in
            // progress is neither cancelled nor granted an extra free strike.
        }
    }
}

std::optional<Vec3> EnemySystem::decoyWaypoint(const EnemyInstance& enemy) const {
    if(!enemy.decoyTarget || enemy.decoyRemaining<=0)return std::nullopt;
    const auto beacon=std::ranges::find(decoyBeacons_,*enemy.decoyTarget,&DecoyBeaconRuntime::buildingId);
    if(beacon==decoyBeacons_.end())return std::nullopt;
    if(enemy.type==EnemyType::Flying)return beacon->position;
    if(std::abs(enemy.worldSurfaceHeight-beacon->position.y)>1.0)return std::nullopt;
    const auto& field=decoyNavigation_[static_cast<std::size_t>(beacon-decoyBeacons_.begin())*Clearances.size()+clearanceClass(enemy)];
    const int slot=fieldIndex(field,enemy.position);
    if(slot<0 || field.distance[static_cast<std::size_t>(slot)]<0)return std::nullopt;
    if(field.distance[static_cast<std::size_t>(slot)]<=1)return beacon->position;
    const int x=slot%DecoyNavigationField::Size,z=slot/DecoyNavigationField::Size;
    int best=slot;
    constexpr std::array<GridPosition,4> offsets{{{-1,0},{0,-1},{1,0},{0,1}}};
    for(const auto offset:offsets) {
        const int nx=x+offset.x,nz=z+offset.z;
        if(nx<0 || nz<0 || nx>=DecoyNavigationField::Size || nz>=DecoyNavigationField::Size)continue;
        const int next=nz*DecoyNavigationField::Size+nx;
        if(field.distance[static_cast<std::size_t>(next)]>=0 && field.distance[static_cast<std::size_t>(next)]<field.distance[static_cast<std::size_t>(best)])best=next;
    }
    return Vec3{double(field.center.x+best%DecoyNavigationField::Size-DecoyNavigationField::HalfExtent),
        beacon->position.y,double(field.center.z+best/DecoyNavigationField::Size-DecoyNavigationField::HalfExtent)};
}
} // namespace ian
