#include "combat/TowerSystem.hpp"
#include "combat/DefenseTargeting.hpp"
#include "combat/DefenseTomeScaling.hpp"
#include "combat/DefenseRuntimeSupport.hpp"
#include "combat/DefenseAiming.hpp"
#include "combat/AttackPattern.hpp"
#include "combat/FlamethrowerAttack.hpp"
#include "combat/TeslaAttack.hpp"
#include "combat/LaserAttack.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/TowerRig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double BaseRange = 9.0;
constexpr double SearchInterval = 0.25;
constexpr double TurnSpeed = 12.0;
constexpr double PitchSpeed = 8.0;
constexpr double AimTolerance = 0.08726646259971647;
bool isTower(BuildingType type) {
    return type == BuildingType::WindBlower || type == BuildingType::Turret || type == BuildingType::GunTurret ||
        type == BuildingType::Flamethrower || type == BuildingType::MissileLauncher || type == BuildingType::TeslaTower || type == BuildingType::LaserTower;
}

double attackRange(BuildingType type, std::uint8_t level) {
    level = std::clamp<std::uint8_t>(level, 1, MaxBuildingLevel);
    if (type == BuildingType::WindBlower) return 5.5 + .2 * (level - 1);
    if (type == BuildingType::LaserTower) return 10.0 + .4 * (level - 1);
    if (type == BuildingType::TeslaTower) return 8.0 + 0.35 * (level - 1);
    if (type == BuildingType::MissileLauncher) return 15.0 + 0.5 * (level - 1);
    if (type == BuildingType::Flamethrower) return 4.0 + 0.2 * (level - 1);
    const double base = type == BuildingType::GunTurret ? 10.0 : BaseRange;
    return base + 0.5 * static_cast<double>(level - 1);
}

double attackDamage(BuildingType type, std::uint8_t level) {
    if (type == BuildingType::WindBlower) return 0;
    if (type == BuildingType::LaserTower) return .18 + .045 * (std::clamp<int>(level, 1, MaxBuildingLevel) - 1);
    if (type == BuildingType::TeslaTower) return 1.2 + 0.3 * (std::clamp<int>(level, 1, MaxBuildingLevel) - 1);
    if (type == BuildingType::MissileLauncher) return 2.0 + 0.45 * (std::clamp<int>(level, 1, MaxBuildingLevel) - 1);
    constexpr std::array<double, MaxBuildingLevel> FlameDamage{
        0.5, 0.75, 1.0, 1.3, 1.6, 1.9, 2.25, 2.6};
    if (type == BuildingType::Flamethrower) return FlameDamage[static_cast<std::size_t>(
        std::clamp<int>(level, 1, MaxBuildingLevel) - 1)];
    constexpr std::array<double, MaxBuildingLevel> CrossbowDamage{
        1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0};
    constexpr std::array<double, MaxBuildingLevel> GunDamage{
        1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5};
    const std::size_t index = static_cast<std::size_t>(
        std::clamp<int>(level, 1, MaxBuildingLevel) - 1);
    return type == BuildingType::GunTurret
        ? GunDamage[index] : CrossbowDamage[index];
}

double fireInterval(BuildingType type, std::uint8_t level) {
    const double levelBonus = static_cast<double>(std::clamp<int>(level, 1, MaxBuildingLevel) - 1);
    if (type == BuildingType::WindBlower) return 2.8 / (1.0 + .05 * levelBonus);
    if (type == BuildingType::LaserTower) return .15 / (1.0 + .04 * levelBonus);
    if (type == BuildingType::TeslaTower) return 1.2 / (1.0 + 0.06 * levelBonus);
    if (type == BuildingType::MissileLauncher) return 1.8 / (1.0 + 0.06 * levelBonus);
    if (type == BuildingType::Flamethrower) return 0.2 / (1.0 + 0.04 * levelBonus);
    const double attacksPerSecond = type == BuildingType::GunTurret
        ? 2.5 + 0.1 * levelBonus
        : 1.0 + 0.1 * levelBonus;
    // Three full-damage bullets per gun event preserve the old per-level raw DPS.
    return (type == BuildingType::GunTurret ? 3.0 : 1.0) / attacksPerSecond;
}

int piercingCount(BuildingType type, std::uint8_t level) {
    if (type != BuildingType::Turret) {
        return 0;
    }
    return 2 + (std::max<int>(1, level) - 1) / 2;
}

Vec3 towerPosition(const BuildingInstance& building) {
    Vec3 position = buildingWorldPosition(building);
    if (building.type == BuildingType::WindBlower) { position.y += WindBlowerMuzzleY; return position; }
    if (building.type == BuildingType::LaserTower) { position.y += LaserPitchPivotY; return position; }
    if (building.type == BuildingType::MissileLauncher) { position.y += MissileLauncherPitchPivotY; return position; }
    if (building.type == BuildingType::Flamethrower) {
        position.y += FlamethrowerPitchPivotY;
        return position;
    }
    position.y += building.type == BuildingType::Turret ? 0.746495 : 0.6854454874992371;
    return position;
}

bool withinRange(Vec3 origin, Vec3 target, double range) {
    const double deltaX = target.x - origin.x;
    const double deltaZ = target.z - origin.z;
    return (deltaX * deltaX) + (deltaZ * deltaZ) <= range * range;
}

} // namespace

TowerSystem::TowerSystem() {
    towers_.reserve(64);
    laserBeams_.reserve(MaximumLaserVisualSegments);
    teslaVisited_.reserve(EnemySystem::MaxEnemies);
    shotBuffer_.reserve(256);
    flameBurns_.reserve(EnemySystem::MaxEnemies);
    flameBurnSlots_.fill(-1);
}

double TowerSystem::attackRange(std::uint8_t level) {
    return ian::attackRange(BuildingType::Turret, level);
}

double TowerSystem::attackRange(BuildingType type, std::uint8_t level) {
    return ian::attackRange(type, level);
}

double TowerSystem::attackDamage(std::uint8_t level) {
    return ian::attackDamage(BuildingType::Turret, level);
}

double TowerSystem::attackDamage(BuildingType type, std::uint8_t level) {
    return ian::attackDamage(type, level);
}

double TowerSystem::fireInterval(std::uint8_t level) {
    return ian::fireInterval(BuildingType::Turret, level);
}

double TowerSystem::fireInterval(BuildingType type, std::uint8_t level) {
    return ian::fireInterval(type, level);
}

int TowerSystem::piercingCount(BuildingType type, std::uint8_t level) {
    return ian::piercingCount(type, level);
}

AttackStats TowerSystem::baseStats(BuildingType type, std::uint8_t level) {
    level = std::clamp<std::uint8_t>(level, 1, MaxBuildingLevel);
    AttackStats stats;
    stats.damage = attackDamage(type, level);
    stats.quantity = type == BuildingType::GunTurret ? 3 : type == BuildingType::MissileLauncher ? 2 : 1;
    stats.attackInterval = fireInterval(type, level);
    stats.range = attackRange(type, level);
    stats.projectileSpeed = type == BuildingType::GunTurret ? 22.0 : 14.0;
    stats.pierce = piercingCount(type, level);
    stats.spreadDegrees = type == BuildingType::GunTurret ? 2.0 : 1.0;
    if (type == BuildingType::Turret) stats.fanStepDegrees = 3.0;
    stats.knockback = type == BuildingType::GunTurret ? 0.25 : 0.8;
    if (type == BuildingType::WindBlower) {
        stats.projectileSpeed = 0; stats.spreadDegrees = 0;
        stats.knockback = 8.0 + .6 * (level - 1);
        stats.duration = WindBlowerPulseSeconds;
    }
    if (type == BuildingType::Flamethrower) {
        stats.projectileSpeed = 0.0;
        stats.duration = 2.0; // Duration means afterburn, not projectile lifetime.
        stats.knockback = 0.08;
        stats.spreadDegrees = 0.0;
    }
    if (type == BuildingType::MissileLauncher) {
        stats.projectileSpeed = 12.0 + 0.4 * (level - 1);
        stats.duration = 4.5; stats.homing = 4.0; stats.spreadDegrees = 1.0;
        stats.burstInterval = 0.18; stats.knockback = 1.2;
    }
    if (type == BuildingType::LaserTower) {
        stats.projectileSpeed = 0; stats.spreadDegrees = 0; stats.knockback = 0; stats.bounceRange=4.5;
    }
    if (type == BuildingType::TeslaTower) {
        stats.projectileSpeed = 0; stats.spreadDegrees = 0; stats.knockback = 0.18;
        stats.bounces = 2 + (level - 1) / 3; stats.bounceRange = 4.5; stats.burstInterval = 0.12;
    }
    stats.knockback = defenseBaseKnockback(type, stats.knockback);
    return stats;
}

FinalAttackStats TowerSystem::finalStats(const BuildingInstance& building) const {
    auto itemModifiers = modifiers_;
    if (building.maxHealth > 0 && building.health / building.maxHealth < 0.40)
        itemModifiers[AttackStat::AttackSpeed].multiplier *= 1.0 + 0.15 * sirenStacks_;
    if (building.type == BuildingType::Flamethrower || building.type == BuildingType::MissileLauncher || building.type == BuildingType::TeslaTower || building.type == BuildingType::LaserTower || building.type == BuildingType::WindBlower) {
        return resolveBuildingAttackStats(baseStats(building.type, building.level), building,
            compatibleAttackModifiers(building.type, itemModifiers), bonusType_,
            compatibleAttackModifiers(building.type, typeModifiers_), highGroundDamageMultiplier_,
            buildingUpgradeModifiers_[static_cast<std::size_t>(building.type)]);
    }
    return resolveBuildingAttackStats(baseStats(building.type, building.level), building,
        itemModifiers, bonusType_, typeModifiers_, highGroundDamageMultiplier_,
        buildingUpgradeModifiers_[static_cast<std::size_t>(building.type)]);
}

void TowerSystem::clearProjectiles() {
    projectiles_.reset();
    laserBeams_.clear();
    flameBurns_.clear();
    teslaVisited_.clear();
    flameBurnSlots_.fill(-1);
    for (auto& tower : towers_) {
        tower.flameRemaining = tower.flameAge = tower.flameTailRemaining = 0.0;
        tower.windAge = 1000.0;
        tower.laserFocusTarget.reset(); tower.laserFocusSeconds = tower.laserRemaining = 0;
    }
    for (auto& tower : towers_) { tower.sequenceRemaining = 0; tower.sequenceDelay = 0.0; tower.sequenceInterval = 0.0; }
}
void TowerSystem::reset() {
    clearProjectiles(); modifiers_ = {}; typeModifiers_ = {}; buildingUpgradeModifiers_ = {};
    bonusType_ = BuildingType::Core; highGroundDamageMultiplier_ = 1.0;
    towers_.clear(); shotBuffer_.clear(); attackBuffer_.clear(); launches_.clear(); shotSerial_ = 0; shrapnelCount_ = 0;
}

void TowerSystem::setTypeBonuses(BuildingType type, double damage, double fireRate) {
    bonusType_ = type; typeModifiers_ = {};
    typeModifiers_[AttackStat::Damage].multiplier = std::max(0.05, damage);
    typeModifiers_[AttackStat::AttackSpeed].multiplier = std::max(0.05, fireRate);
}

// Compatibility adapter for older callers, not an independent damage path.
void TowerSystem::setModifiers(double damage, double range, double fireRate,
    double highGroundDamage, int quantityBonus, int pierceBonus) {
    modifiers_ = {};
    modifiers_[AttackStat::Damage].multiplier = std::max(0.05, damage);
    modifiers_[AttackStat::Range].multiplier = std::max(0.05, range);
    modifiers_[AttackStat::AttackSpeed].multiplier = std::max(0.05, fireRate);
    modifiers_[AttackStat::Quantity].flat = std::max(0, quantityBonus);
    modifiers_[AttackStat::Pierce].flat = std::max(0, pierceBonus);
    highGroundDamageMultiplier_ = std::max(1.0, highGroundDamage);
}

void TowerSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    defense_runtime::syncBuildings(towers_, buildings, SearchInterval,
        isTower,
        [](const BuildingInstance&, const TowerRuntime&) { return true; });
    std::erase_if(laserBeams_,[&](const auto& beam) {
        return std::ranges::none_of(buildings,[&](const auto& building){return building.id==beam.sourceId;});
    });
}

std::span<const TowerShot> TowerSystem::tick(double deltaSeconds,
                                             const std::vector<BuildingInstance>& buildings,
                                             EnemySystem& enemies) {
    shotBuffer_.clear();
    attackBuffer_.clear();
    launches_.clear();
    for(auto& beam:laserBeams_)beam.remaining-=deltaSeconds;
    std::erase_if(laserBeams_,[](const auto& beam){return beam.remaining<=0;});
    updateFlameBurns(deltaSeconds, enemies);
    bool flameQueryReady = false;
    bool teslaQueryReady = false;
    teslaTargetCache_.beginTick();
    int teslaDischarges = 0;
    constexpr int TeslaDischargesPerTick = 256;
    for (auto& tower : towers_) {
        const auto building =
            std::find_if(buildings.begin(), buildings.end(), [&tower](const BuildingInstance& item) {
                return item.id == tower.buildingId;
            });
        if (building == buildings.end()) {
            continue;
        }

        if (building->type == BuildingType::WindBlower) {
            tower.previousWindRotor = tower.windRotor;
            tower.windAge += deltaSeconds;
            const double rotorTarget = tower.targetId && tower.fireCooldownRemaining < .5 ? 30.0 :
                tower.windAge < WindBlowerPulseSeconds ? 38.0 : 2.0;
            tower.windRotorSpeed += (rotorTarget - tower.windRotorSpeed) * (1.0 - std::exp(-deltaSeconds * 7.0));
            tower.windRotor = wrapBuildingAngle(tower.windRotor + tower.windRotorSpeed * deltaSeconds);
        }
        tower.previousYaw = tower.yaw;
        tower.previousPitch = tower.pitch;
        const double cooldownOvershoot = std::min(0.0, tower.fireCooldownRemaining - deltaSeconds);
        tower.fireCooldownRemaining = std::max(0.0, tower.fireCooldownRemaining - deltaSeconds);
        tower.targetSearchCooldownRemaining =
            std::max(0.0, tower.targetSearchCooldownRemaining - deltaSeconds);
        const bool wind = building->type == BuildingType::WindBlower;
        const bool crossbow = building->type == BuildingType::Turret;
        const bool missile = building->type == BuildingType::MissileLauncher;
        const bool flame = building->type == BuildingType::Flamethrower;
        const bool laser = building->type == BuildingType::LaserTower;
        tower.laserRemaining = std::max(0.0, tower.laserRemaining - deltaSeconds);
        if (tower.flameTailRemaining > 0) tower.flameAge += deltaSeconds;
        tower.flameRemaining = std::max(0.0, tower.flameRemaining - deltaSeconds);
        tower.flameTailRemaining = std::max(0.0, tower.flameTailRemaining - deltaSeconds);
        if (tower.flameRemaining > 0) tower.flameTailRemaining = flamethrowerTailSeconds(tower.attack.stats);
        const double aimResponse = crossbow ? 10.0 : 12.0;
        const double aimTurnSpeed = crossbow ? 4.0 : TurnSpeed;
        const double aimPitchSpeed = crossbow ? 2.8 : PitchSpeed;
        const Vec3 origin = towerPosition(*building);
        const auto stats = finalStats(*building);
        const double range = wind ? windBlowerRange(stats) : flame ? flamethrowerRange(stats) : stats.range;
        if (wind && tower.windAge < stats.duration) {
            // The authored impulse remains instant. While the visible gust is
            // present, a small sustained push keeps Duration meaningful.
            if (!flameQueryReady) { flameQuery_.rebuild(enemies); flameQueryReady = true; }
            const Vec3 base = buildingWorldPosition(*building);
            const Vec3 direction{-std::sin(tower.windYaw), 0, -std::cos(tower.windYaw)};
            flameQuery_.nearby(enemies, base, range + 2.0, [&](const EnemyInstance& enemy) {
                if (!windBlowerTouches(enemy, base, tower.windYaw, range)) return;
                const double along = enemy.knockbackVelocity.x * direction.x +
                    enemy.knockbackVelocity.z * direction.z;
                const double strength = std::clamp(stats.knockback * 0.18 * deltaSeconds,
                    0.0, std::max(0.0, stats.knockback * 0.45 - along));
                enemies.applyKnockback(enemy.id, direction, strength);
            });
        }
        if (building->type == BuildingType::TeslaTower) {
            // Static, omnidirectional building. Quantity means one controlled
            // chain-discharge sequence, with independent visited IDs per bolt.
            const Vec3 base = buildingWorldPosition(*building);
            const Vec3 energy{base.x,base.y+TeslaEnergyHeight,base.z};
            const auto discharge = [&](const FinalAttackStats& eventStats, int instance) {
                const auto shotStats = defenseInstanceStats(eventStats, building->type, instance);
                if (!teslaQueryReady) { teslaQuery_.rebuild(enemies); teslaQueryReady=true; }
                teslaVisited_.clear();
                auto target = teslaQuery_.bounceTarget(enemies,energy,shotStats.range,
                    BuildingType::TeslaTower,building->baseHeight,teslaVisited_);
                if (preferMarkedTargets_) {
                    Vec3 search=base;
                    std::optional<EnemyInstance> marked;
                    double nearest=shotStats.range*shotStats.range;
                    teslaQuery_.nearby(enemies,search,shotStats.range,[&](const EnemyInstance& enemy) {
                        const double distance=geometry::distanceSquared(attackTargetPosition(enemy),energy);
                        if(enemy.markRemaining>0 && distance<nearest) {marked=enemy;nearest=distance;}
                    });
                    if(marked) target=marked;
                }
                if(!target) {tower.targetId.reset();tower.targetSearchCooldownRemaining=SearchInterval;return false;}
                tower.targetId=target->id;
                const Vec3 direction=geometry::normalizedOr(geometry::subtract(attackTargetPosition(*target),energy));
                const Vec3 muzzle=geometry::add(energy,geometry::scale(direction,0.20));
                ++teslaDischarges;
                applyTeslaChain(enemies,teslaQuery_,target->id,muzzle,shotStats,
                    ++shotSerial_ ^ (static_cast<std::uint64_t>(tower.buildingId.index)<<32U),teslaVisited_,
                    [&](const AttackHit& hit,Vec3 source,Vec3 end,int jump) {
                        auto hitStats=shotStats;hitStats.damage=hit.payloadDamage;
                        shotBuffer_.push_back({.towerId=tower.buildingId,.targetId=hit.result.id,
                            .targetType=hit.result.type,.targetEliteAffixes=hit.result.eliteAffixes,
                            .origin=source,.hitPosition=end,.type=BuildingType::TeslaTower,
                            .damage=hit.result.damage,.payloadDamage=hit.payloadDamage,
                            .muzzleIndex=static_cast<std::uint8_t>(std::min(jump,255)),
                            .secondaryImpact=jump>0,.critical=hit.critical,.killed=hit.result.killed,.stats=hitStats});
                    }, &teslaTargetCache_);
                return true;
            };
            if (tower.sequenceRemaining>0) {
                tower.sequenceDelay-=deltaSeconds;
                while(tower.sequenceRemaining>0 && tower.sequenceDelay<=1e-12 && teslaDischarges<TeslaDischargesPerTick) {
                    if(!discharge(tower.attack.stats, effectiveAttackCount(tower.attack.stats.quantity,1)-tower.sequenceRemaining)) {tower.sequenceRemaining=0;break;}
                    --tower.sequenceRemaining;tower.sequenceDelay+=tower.sequenceInterval;
                }
            }
            if(tower.fireCooldownRemaining<=0 && tower.sequenceRemaining==0 && tower.targetSearchCooldownRemaining<=0 && teslaDischarges<TeslaDischargesPerTick) {
                if(discharge(stats,0)) {
                    tower.attack={tower.buildingId,stats};attackBuffer_.push_back(tower.attack);
                    tower.fireCooldownRemaining=stats.attackInterval+cooldownOvershoot;
                    tower.sequenceRemaining=effectiveAttackCount(stats.quantity,1)-1;
                    tower.sequenceInterval=gunBurstTiming(stats).interval;tower.sequenceDelay=tower.sequenceInterval;
                }
            }
            continue;
        }
        const auto spawn = [&](const FinalAttackStats& eventStats, double yaw, double pitch, int instance) {
            const auto shotStats = defenseInstanceStats(eventStats, building->type, instance);
            const auto seed = ++shotSerial_ ^ (static_cast<std::uint64_t>(tower.buildingId.index) << 32U);
            // Uniform disk in the cone. Randomness changes real flight direction,
            // never whether a detected contact is allowed to deal damage.
            const double radius = std::sqrt(unitRandom(seed)) * shotStats.spreadDegrees * PiRadians / 180.0;
            const double angle = unitRandom(seed ^ 0x8a5cd789635d2dffULL) * 2.0 * PiRadians;
            const double shotYaw = yaw + std::cos(angle) * radius;
            const double shotPitch = pitch + std::sin(angle) * radius;
            const Vec3 direction{-std::sin(shotYaw) * std::cos(shotPitch),
                std::sin(shotPitch), -std::cos(shotYaw) * std::cos(shotPitch)};
            const Vec3 muzzle = missile
                ? missileLauncherMuzzleWorldPosition(buildingWorldPosition(*building), yaw, pitch, tower.nextMuzzle)
                : crossbow
                ? crossbowMuzzleWorldPosition(buildingWorldPosition(*building), yaw, pitch)
                : gunTurretMuzzleWorldPosition(buildingWorldPosition(*building), yaw, tower.nextMuzzle);
            const bool spawned = projectiles_.spawn(tower.buildingId, building->type, muzzle, direction,
                shotStats, building->baseHeight, tower.nextMuzzle, false, missile ? tower.targetId : std::nullopt);
            if (spawned) {
                launches_.push_back({tower.buildingId, building->type, muzzle, tower.nextMuzzle});
            }
            if (!crossbow) tower.nextMuzzle = static_cast<std::uint8_t>((tower.nextMuzzle + 1U) % (missile ? 4U : 2U));
            return spawned;
        };
        if (tower.sequenceRemaining > 0) {
            tower.sequenceDelay -= deltaSeconds;
            while (tower.sequenceRemaining > 0 && tower.sequenceDelay <= 1e-12) {
                if (!spawn(tower.attack.stats, tower.sequenceYaw, tower.sequencePitch, effectiveAttackCount(tower.attack.stats.quantity,1)-tower.sequenceRemaining)) {
                    // Saturation is observable in the pool. Do not spin through
                    // millions of remaining instances when no slot is available.
                    tower.sequenceRemaining = 0;
                    break;
                }
                --tower.sequenceRemaining;
                tower.sequenceDelay += tower.sequenceInterval;
            }
        }
        tower.baseYaw = smoothBuildingAngle(
            tower.baseYaw, tower.restYaw, deltaSeconds);

        if (tower.targetId) {
            const auto target = enemies.enemy(*tower.targetId);
            if (!target || (wind && !windBlowerCanTarget(*target, building->baseHeight)) || !projectileDefenseCanTarget(
                    building->type, *target, building->baseHeight) ||
                !withinRange(origin, target->position, range) ||
                (missile && withinRange(origin, target->position, MissileLauncherMinimumRange)) ||
                !directionInsideDefenseArc(
                    origin, target->position, tower.restYaw,
                    building->level)) {
                tower.targetId.reset();
            }
        }

        if (!wind && preferMarkedTargets_ && (!laser || !tower.targetId) && tower.targetSearchCooldownRemaining <= 0.0) {
            Vec3 search = origin; search.y = building->baseHeight;
            if (auto marked = markedDefenseTarget(enemies, building->type, search, range,
                    tower.restYaw, building->level, missile ? MissileLauncherMinimumRange : 0.0)) {
                tower.targetId = marked;
                tower.targetSearchCooldownRemaining = SearchInterval;
            } else if (tower.targetId) {
                tower.targetSearchCooldownRemaining = SearchInterval;
            }
        }
        if (!tower.targetId && tower.targetSearchCooldownRemaining <= 0.0) {
            Vec3 searchOrigin = origin;
            searchOrigin.y = building->baseHeight;
            if (wind) {
                if (!flameQueryReady) { flameQuery_.rebuild(enemies); flameQueryReady = true; }
                double nearest = range * range;
                flameQuery_.nearby(enemies, origin, range, [&](const EnemyInstance& enemy) {
                    if (!windBlowerCanTarget(enemy, building->baseHeight) ||
                        !directionInsideDefenseArc(origin, enemy.position, tower.restYaw, building->level)) return;
                    const double dx = enemy.position.x-origin.x, dz = enemy.position.z-origin.z;
                    const double distance = dx*dx+dz*dz;
                    if (distance < nearest) { nearest = distance; tower.targetId = enemy.id; }
                });
            } else tower.targetId = missile ? enemies.densestEnemyInArc(
                searchOrigin, range, missileExplosionRadius(stats), tower.restYaw,
                defenseAttackHalfAngleRadians(building->level), MissileLauncherMinimumRange)
                : enemies.nearestEnemyInArc(
                searchOrigin, range, tower.restYaw,
                defenseAttackHalfAngleRadians(building->level),
                building->type != BuildingType::GunTurret,
                building->type == BuildingType::GunTurret
                    ? GunTurretMaximumSurfaceHeightDifference
                    : std::numeric_limits<double>::infinity());
            tower.targetSearchCooldownRemaining = SearchInterval;
        }
        if (!tower.targetId) {
            tower.flameRemaining = 0.0;
            tower.laserFocusTarget.reset(); tower.laserFocusSeconds = 0;
            defense_runtime::holdAimWithoutTarget(tower, deltaSeconds);
            continue;
        }

        const auto target = enemies.enemy(*tower.targetId);
        if (!target) {
            tower.targetId.reset();
            continue;
        }
        const Vec3 aimPosition = attackTargetPosition(*target);
        tower.manualTurnPending = false;
        const double deltaX = aimPosition.x - origin.x;
        const double deltaY = aimPosition.y - origin.y;
        const double deltaZ = aimPosition.z - origin.z;
        const double desiredYaw = std::atan2(-deltaX, -deltaZ);
        const double desiredPitch = missile
            ? MissileLauncherMaximumPitch
            : laser
            ? std::clamp(std::atan2(deltaY, std::hypot(deltaX, deltaZ)), LaserMinimumPitch, LaserMaximumPitch)
            : flame
            ? std::clamp(std::atan2(deltaY, std::hypot(deltaX, deltaZ)),
                FlamethrowerMinimumPitch, FlamethrowerMaximumPitch)
            : crossbow ? std::atan2(deltaY, std::hypot(deltaX, deltaZ)) : 0.0;
        tower.yaw = advanceDefenseAim(
            tower.yaw, desiredYaw, tower.yawVelocity,
            deltaSeconds, aimResponse, aimTurnSpeed, true);
        tower.pitch = advanceDefenseAim(
            tower.pitch, desiredPitch, tower.pitchVelocity,
            deltaSeconds, 10.0, aimPitchSpeed, false);
        if (laser) {
            const bool aligned = std::abs(wrapBuildingAngle(desiredYaw - tower.yaw)) <= AimTolerance &&
                std::abs(desiredPitch - tower.pitch) <= AimTolerance;
            if (!aligned || tower.laserFocusTarget != tower.targetId) {
                tower.laserFocusTarget = aligned ? tower.targetId : std::nullopt;
                tower.laserFocusSeconds = 0;
            }
            if (!aligned) continue;
            // Duration means sustained focus: longer duration reaches the same
            // maximum ramp damage sooner while staying locked on one target.
            tower.laserFocusSeconds = std::min(LaserFocusRampSeconds,
                tower.laserFocusSeconds + deltaSeconds *
                    stats.duration / baseStats(BuildingType::LaserTower, building->level).duration);
            tower.laserOrigin = laserMuzzleWorldPosition(buildingWorldPosition(*building), tower.yaw, tower.pitch);
            tower.laserEnd = aimPosition;
            tower.laserRemaining = .12;
            for(auto& beam:laserBeams_)if(beam.sourceId==tower.buildingId) {
                beam.remaining=.12;beam.focus=tower.laserFocusSeconds/LaserFocusRampSeconds;
            }
            if (tower.fireCooldownRemaining > 0) continue;
            auto focused = stats;
            const double focusMultiplier = laserFocusMultiplier(tower.laserFocusSeconds);
            focused.damage *= focusMultiplier;
            tower.attack = {tower.buildingId, focused};
            attackBuffer_.push_back(tower.attack);
            tower.fireCooldownRemaining = stats.attackInterval + cooldownOvershoot;
            if (!teslaQueryReady) {teslaQuery_.rebuild(enemies);teslaQueryReady=true;}
            std::erase_if(laserBeams_,[&](const auto& beam){return beam.sourceId==tower.buildingId;});
            const int rays=effectiveAttackCount(focused.quantity,1);
            const EntityId primary=*tower.targetId;
            bool primaryKilled=false;
            for(int ray=0;ray<rays && enemies.enemy(primary);++ray) {
                const auto rayStats = defenseInstanceStats(focused, building->type, ray);
                std::optional<EntityId> fromTarget;
                applyTeslaChain(enemies,teslaQuery_,primary,tower.laserOrigin,rayStats,
                    ++shotSerial_ ^ tower.buildingId.index,teslaVisited_,
                    [&](const AttackHit& hit,Vec3 origin,Vec3 end,int jump) {
                        auto hitStats=focused;hitStats.damage=hit.payloadDamage;
                        shotBuffer_.push_back({.towerId=tower.buildingId,.targetId=hit.result.id,
                            .targetType=hit.result.type,.targetEliteAffixes=hit.result.eliteAffixes,
                            .origin=origin,.hitPosition=end,.type=BuildingType::LaserTower,
                            .damage=hit.result.damage,.payloadDamage=hit.payloadDamage,
                            .secondaryImpact=jump>0,.critical=hit.critical,.killed=hit.result.killed,.stats=hitStats});
                        if(laserBeams_.size()<MaximumLaserVisualSegments)
                            laserBeams_.push_back({tower.buildingId,fromTarget,hit.result.id,origin,end,
                                tower.laserFocusSeconds/LaserFocusRampSeconds,.12,ray,rays});
                        fromTarget=hit.result.id;
                        primaryKilled|=hit.result.id==primary && hit.result.killed;
                    },&teslaTargetCache_,BuildingType::LaserTower,1.0/focusMultiplier);
                // Pierce continues on the original beam axis beyond the focus
                // target. Unlike a bounce, it only hits bodies intersecting
                // that straight line and never retargets a nearby enemy.
                const Vec3 beam = geometry::subtract(aimPosition, tower.laserOrigin);
                const double beamLength = geometry::length(beam);
                if (beamLength < 1e-6 || beamLength >= rayStats.range) continue;
                const Vec3 direction = geometry::scale(beam, 1.0 / beamLength);
                const Vec3 end = geometry::add(tower.laserOrigin,
                    geometry::scale(direction, rayStats.range));
                Vec3 pierceStart = aimPosition;
                std::optional<EntityId> previous = primary;
                for (int pierce = 0; pierce < effectiveAttackCount(rayStats.pierce); ++pierce) {
                    const auto contact = teslaQuery_.sweep(enemies, pierceStart, end,
                        0.16, BuildingType::LaserTower, building->baseHeight, teslaVisited_);
                    if (!contact) break;
                    teslaVisited_.push_back(contact->id);
                    const auto contactStats = defenseContactStats(rayStats, BuildingType::LaserTower,
                        0, pierce + 1);
                    if (const auto hit = applyAttackHit(enemies, contact->id, contactStats,
                            direction, ++shotSerial_ ^ tower.buildingId.index)) {
                        shotBuffer_.push_back({.towerId=tower.buildingId,.targetId=hit->result.id,
                            .targetType=hit->result.type,.targetEliteAffixes=hit->result.eliteAffixes,
                            .origin=pierceStart,.hitPosition=contact->position,
                            .type=BuildingType::LaserTower,.damage=hit->result.damage,
                            .payloadDamage=hit->payloadDamage,.secondaryImpact=true,
                            .critical=hit->critical,.killed=hit->result.killed,.stats=contactStats});
                        if (laserBeams_.size() < MaximumLaserVisualSegments)
                            laserBeams_.push_back({tower.buildingId,previous,contact->id,
                                pierceStart,contact->position,
                                tower.laserFocusSeconds/LaserFocusRampSeconds,.12,ray,rays});
                    }
                    previous = contact->id;
                    pierceStart = geometry::add(contact->position, geometry::scale(direction, .05));
                }
            }
            if(primaryKilled) {tower.laserFocusTarget.reset();tower.laserFocusSeconds=0;}
            continue;
        }
        if (std::abs(wrapBuildingAngle(
                desiredYaw - tower.yaw)) >
                AimTolerance ||
            std::abs(desiredPitch - tower.pitch) > AimTolerance ||
            tower.fireCooldownRemaining > 0.0 || tower.sequenceRemaining > 0) {
            continue;
        }
        tower.attack = {tower.buildingId, stats};
        attackBuffer_.push_back(tower.attack);
        // Carry the sub-step deadline instead of losing a fraction of every period.
        tower.fireCooldownRemaining = stats.attackInterval + cooldownOvershoot;
        if (wind) {
            if (!flameQueryReady) { flameQuery_.rebuild(enemies); flameQueryReady = true; }
            tower.windAge = 0;
            tower.windYaw = tower.yaw;
            tower.windOrigin = windBlowerMuzzle(buildingWorldPosition(*building), tower.yaw);
            const Vec3 direction{-std::sin(tower.yaw), 0, -std::cos(tower.yaw)};
            flameQuery_.nearby(enemies, origin, range + 2.0, [&](const EnemyInstance& enemy) {
                if (!windBlowerTouches(enemy, buildingWorldPosition(*building), tower.yaw, range)) return;
                // Bound overlapping fans: a gust never stacks additional speed
                // onto an enemy already travelling at its full impulse speed.
                const double along = enemy.knockbackVelocity.x*direction.x + enemy.knockbackVelocity.z*direction.z;
                const double strength = std::max(0.0, stats.knockback - std::max(0.0, along));
                enemies.applyKnockback(enemy.id, direction, strength);
            });
            launches_.push_back({tower.buildingId, building->type, tower.windOrigin, 0});
        } else if (flame) {
            if (!flameQueryReady) {
                flameQuery_.rebuild(enemies);
                flameQueryReady = true;
            }
            const Vec3 muzzle = flamethrowerMuzzleWorldPosition(
                buildingWorldPosition(*building), tower.yaw, tower.pitch);
            const Vec3 direction{-std::sin(tower.yaw) * std::cos(tower.pitch),
                std::sin(tower.pitch), -std::cos(tower.yaw) * std::cos(tower.pitch)};
            if (tower.flameTailRemaining <= 0) tower.flameAge = 0;
            tower.flameRemaining = std::max(0.12, stats.attackInterval * 1.2);
            tower.flameTailRemaining = flamethrowerTailSeconds(stats);
            tower.flameOrigin = muzzle;
            const std::uint64_t pulseSeed = ++shotSerial_ ^
                (static_cast<std::uint64_t>(tower.buildingId.index) << 32U);
            flameQuery_.nearby(enemies, origin, range + 2.0, [&](const EnemyInstance& enemy) {
                if (!flamethrowerTouchesEnemy(enemy, muzzle, direction,
                        flamethrowerStreamLength(stats))) return;
                const auto hit = applyAttackHit(enemies, enemy.id, stats, direction,
                    pulseSeed ^ enemy.id.index);
                if (!hit) return;
                shotBuffer_.push_back({.towerId = tower.buildingId,
                    .targetId = hit->result.id, .targetType = hit->result.type,
                    .targetEliteAffixes = hit->result.eliteAffixes, .origin = muzzle,
                    .hitPosition = hit->result.position, .type = BuildingType::Flamethrower,
                    .damage = hit->result.damage, .payloadDamage = stats.damage,
                    .critical = hit->critical, .killed = hit->result.killed, .stats = stats});
                if (!hit->result.killed) applyFlameBurn(hit->result.id, tower.buildingId, stats, enemies);
            });
        } else if (crossbow) {
            const int count = effectiveAttackCount(stats.quantity, 1);
            const double fanStep = count > 1
                ? std::min(stats.fanStepDegrees, 24.0 / (count - 1)) : 0.0;
            for (int instance = 0; instance < count; ++instance) {
                if (!spawn(stats, tower.yaw + fanAngleDegrees(instance, count, fanStep) * PiRadians / 180.0,
                    tower.pitch, instance == count / 2 ? 0 : 1)) break;
            }
        } else {
            // One snapshot and sequence per building. Compression leaves at least
            // 40% of the event period for the pause, independent of Quantity.
            tower.sequenceYaw = tower.yaw;
            tower.sequencePitch = missile ? tower.pitch : std::atan2(deltaY, std::hypot(deltaX, deltaZ));
            tower.sequenceRemaining = spawn(stats, tower.sequenceYaw, tower.sequencePitch, 0)
                ? effectiveAttackCount(stats.quantity, 1) - 1 : 0;
            tower.sequenceInterval = gunBurstTiming(stats).interval;
            tower.sequenceDelay = tower.sequenceInterval;
        }
    }
    for (const auto& impact : projectiles_.tick(deltaSeconds, enemies)) {
        shotBuffer_.push_back({
            .towerId = impact.sourceId, .targetId = impact.hit.result.id,
            .targetType = impact.hit.result.type, .targetEliteAffixes = impact.hit.result.eliteAffixes,
            .origin = impact.launchOrigin, .hitPosition = impact.position, .type = impact.type,
            .damage = impact.hit.result.damage, .payloadDamage = impact.payloadDamage,
            .muzzleIndex = impact.muzzleIndex, .secondaryImpact = impact.secondary,
            .critical = impact.hit.critical, .killed = impact.hit.result.killed, .stats = impact.stats, .shrapnel = impact.shrapnel,
        });
    }
    // Death-triggered shards are physical instances of the same projectile
    // implementation. No secondary instantaneous damage or duplicate hit flow.
    if (shrapnelCount_ > 0 && std::ranges::any_of(shotBuffer_, [](const TowerShot& hit) {
            return hit.type == BuildingType::GunTurret && hit.killed;
        })) {
        shrapnelQuery_.rebuild(enemies);
        // shotBuffer holds stable hit snapshots while spawning can grow the pool.
        for (const auto& hit : shotBuffer_) {
            if (!hit.killed || hit.type != BuildingType::GunTurret) continue;
            const auto source = std::ranges::find(buildings, hit.towerId, &BuildingInstance::id);
            if (source == buildings.end()) continue;
            auto shardStats = hit.stats;
            shardStats.damage = hit.payloadDamage * 0.45;
            shardStats.size *= 0.56;
            std::vector<EntityId> selected{hit.targetId};
            for (int shard = 0; shard < shrapnelCount_; ++shard) {
                const auto target = shrapnelQuery_.bounceTarget(enemies, hit.hitPosition, 5.0,
                    hit.type, source->baseHeight, selected);
                if (!target) break;
                selected.push_back(target->id);
                projectiles_.spawn(hit.towerId, hit.type, hit.hitPosition,
                    geometry::subtract(attackTargetPosition(*target), hit.hitPosition), shardStats, source->baseHeight, 0, true);
            }
        }
    }
    return shotBuffer_;
}

void TowerSystem::applyFlameBurn(EntityId target, EntityId source, const FinalAttackStats& stats,
    const EnemySystem& enemies) {
    const auto slot = enemies.slotIndex(target);
    if (!slot || *slot >= flameBurnSlots_.size()) return;
    const int index = flameBurnSlots_[*slot];
    FlameBurn* existing = index >= 0 && static_cast<std::size_t>(index) < flameBurns_.size() &&
        flameBurns_[static_cast<std::size_t>(index)].targetId == target
        ? &flameBurns_[static_cast<std::size_t>(index)] : nullptr;
    auto burnStats = stats;
    burnStats.damage *= 0.25; // Two ticks/second: afterburn DPS = half one heat pulse.
    burnStats.knockback = 0.0;
    if (!existing) {
        flameBurnSlots_[*slot] = static_cast<int>(flameBurns_.size());
        flameBurns_.push_back({target, source, burnStats, stats.duration, FlamethrowerBurnInterval});
    } else {
        existing->remaining = std::max(existing->remaining, stats.duration);
        // Multiple jets refresh one strongest dose; never allocate one burn per hit.
        if (burnStats.damage >= existing->stats.damage) {
            existing->stats = burnStats;
            existing->sourceId = source;
        }
    }
}

void TowerSystem::updateFlameBurns(double deltaSeconds, EnemySystem& enemies) {
    if (flameBurns_.empty()) return;
    for (auto& burn : flameBurns_) {
        if (!enemies.enemy(burn.targetId)) { burn.remaining = 0.0; continue; }
        const double activeSeconds = std::min(std::max(0.0, deltaSeconds), burn.remaining);
        burn.remaining -= activeSeconds;
        burn.tickRemaining -= activeSeconds;
        while (burn.tickRemaining <= 1e-10) {
            const auto hit = applyAttackHit(enemies, burn.targetId, burn.stats, {},
                ++shotSerial_ ^ burn.targetId.index);
            if (!hit) { burn.remaining = 0.0; break; }
            shotBuffer_.push_back({.towerId = burn.sourceId, .targetId = hit->result.id,
                .targetType = hit->result.type, .targetEliteAffixes = hit->result.eliteAffixes,
                .hitPosition = hit->result.position, .type = BuildingType::Flamethrower,
                .damage = hit->result.damage, .payloadDamage = burn.stats.damage,
                .secondaryImpact = true, .critical = hit->critical, .killed = hit->result.killed,
                .stats = burn.stats, .periodicBurn = true});
            burn.tickRemaining += FlamethrowerBurnInterval;
            if (hit->result.killed) { burn.remaining = 0.0; break; }
        }
    }
    std::erase_if(flameBurns_, [](const auto& burn) { return burn.remaining <= 1e-10; });
    flameBurnSlots_.fill(-1);
    for (std::size_t index = 0; index < flameBurns_.size(); ++index) {
        const auto slot = enemies.slotIndex(flameBurns_[index].targetId);
        if (slot && *slot < flameBurnSlots_.size()) flameBurnSlots_[*slot] = static_cast<int>(index);
    }
}

const std::vector<TowerRuntime>& TowerSystem::towers() const {
    return towers_;
}

} // namespace ian
