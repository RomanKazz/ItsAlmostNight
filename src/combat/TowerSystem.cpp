#include "combat/TowerSystem.hpp"
#include "combat/DefenseTargeting.hpp"
#include "combat/DefenseRuntimeSupport.hpp"
#include "combat/DefenseAiming.hpp"
#include "combat/AttackPattern.hpp"
#include "combat/FlamethrowerAttack.hpp"
#include "combat/TeslaAttack.hpp"
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
    return type == BuildingType::Turret || type == BuildingType::GunTurret ||
        type == BuildingType::Flamethrower || type == BuildingType::MissileLauncher || type == BuildingType::TeslaTower;
}

double attackRange(BuildingType type, std::uint8_t level) {
    level = std::clamp<std::uint8_t>(level, 1, MaxBuildingLevel);
    if (type == BuildingType::TeslaTower) return 8.0 + 0.35 * (level - 1);
    if (type == BuildingType::MissileLauncher) return 15.0 + 0.5 * (level - 1);
    if (type == BuildingType::Flamethrower) return 4.0 + 0.2 * (level - 1);
    const double base = type == BuildingType::GunTurret ? 10.0 : BaseRange;
    return base + 0.5 * static_cast<double>(level - 1);
}

double attackDamage(BuildingType type, std::uint8_t level) {
    if (type == BuildingType::TeslaTower) return 1.2 + 0.3 * (std::clamp<int>(level, 1, MaxBuildingLevel) - 1);
    if (type == BuildingType::MissileLauncher) return 2.0 + 0.45 * (std::clamp<int>(level, 1, MaxBuildingLevel) - 1);
    constexpr std::array<double, MaxBuildingLevel> FlameDamage{
        0.6, 0.85, 1.1, 1.4, 1.7, 2.0, 2.35, 2.7};
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
    stats.knockback = type == BuildingType::GunTurret ? 0.25 : 0.8;
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
    if (type == BuildingType::TeslaTower) {
        stats.projectileSpeed = 0; stats.spreadDegrees = 0; stats.knockback = 0.18;
        stats.bounces = 2 + (level - 1) / 3; stats.bounceRange = 4.5; stats.burstInterval = 0.12;
    }
    return stats;
}

FinalAttackStats TowerSystem::finalStats(const BuildingInstance& building) const {
    if (building.type == BuildingType::Flamethrower || building.type == BuildingType::MissileLauncher || building.type == BuildingType::TeslaTower) {
        return resolveBuildingAttackStats(baseStats(building.type, building.level), building,
            compatibleAttackModifiers(building.type, modifiers_), bonusType_,
            compatibleAttackModifiers(building.type, typeModifiers_), highGroundDamageMultiplier_);
    }
    return resolveBuildingAttackStats(baseStats(building.type, building.level), building,
        modifiers_, bonusType_, typeModifiers_, highGroundDamageMultiplier_);
}

void TowerSystem::clearProjectiles() {
    projectiles_.reset();
    flameBurns_.clear();
    teslaVisited_.clear();
    flameBurnSlots_.fill(-1);
    for (auto& tower : towers_) {
        tower.flameRemaining = tower.flameAge = tower.flameTailRemaining = 0.0;
    }
    for (auto& tower : towers_) { tower.sequenceRemaining = 0; tower.sequenceDelay = 0.0; tower.sequenceInterval = 0.0; }
}
void TowerSystem::reset() {
    clearProjectiles(); modifiers_ = {}; typeModifiers_ = {};
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
}

std::span<const TowerShot> TowerSystem::tick(double deltaSeconds,
                                             const std::vector<BuildingInstance>& buildings,
                                             EnemySystem& enemies) {
    shotBuffer_.clear();
    attackBuffer_.clear();
    launches_.clear();
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

        tower.previousYaw = tower.yaw;
        tower.previousPitch = tower.pitch;
        const double cooldownOvershoot = std::min(0.0, tower.fireCooldownRemaining - deltaSeconds);
        tower.fireCooldownRemaining = std::max(0.0, tower.fireCooldownRemaining - deltaSeconds);
        tower.targetSearchCooldownRemaining =
            std::max(0.0, tower.targetSearchCooldownRemaining - deltaSeconds);
        const bool crossbow = building->type == BuildingType::Turret;
        const bool missile = building->type == BuildingType::MissileLauncher;
        const bool flame = building->type == BuildingType::Flamethrower;
        if (tower.flameTailRemaining > 0) tower.flameAge += deltaSeconds;
        tower.flameRemaining = std::max(0.0, tower.flameRemaining - deltaSeconds);
        tower.flameTailRemaining = std::max(0.0, tower.flameTailRemaining - deltaSeconds);
        if (tower.flameRemaining > 0) tower.flameTailRemaining = flamethrowerTailSeconds(tower.attack.stats);
        const double aimResponse = crossbow ? 10.0 : 12.0;
        const double aimTurnSpeed = crossbow ? 4.0 : TurnSpeed;
        const double aimPitchSpeed = crossbow ? 2.8 : PitchSpeed;
        const Vec3 origin = towerPosition(*building);
        const auto stats = finalStats(*building);
        const double range = flame ? flamethrowerRange(stats) : stats.range;
        if (building->type == BuildingType::TeslaTower) {
            // Static, omnidirectional building. Quantity means one controlled
            // chain-discharge sequence, with independent visited IDs per bolt.
            const Vec3 base = buildingWorldPosition(*building);
            const Vec3 energy{base.x,base.y+TeslaEnergyHeight,base.z};
            const auto discharge = [&](const FinalAttackStats& shotStats) {
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
                        shotBuffer_.push_back({.towerId=tower.buildingId,.targetId=hit.result.id,
                            .targetType=hit.result.type,.targetEliteAffixes=hit.result.eliteAffixes,
                            .origin=source,.hitPosition=end,.type=BuildingType::TeslaTower,
                            .damage=hit.result.damage,.payloadDamage=shotStats.damage,
                            .muzzleIndex=static_cast<std::uint8_t>(std::min(jump,255)),
                            .secondaryImpact=jump>0,.critical=hit.critical,.killed=hit.result.killed,.stats=shotStats});
                    }, &teslaTargetCache_);
                return true;
            };
            if (tower.sequenceRemaining>0) {
                tower.sequenceDelay-=deltaSeconds;
                while(tower.sequenceRemaining>0 && tower.sequenceDelay<=1e-12 && teslaDischarges<TeslaDischargesPerTick) {
                    if(!discharge(tower.attack.stats)) {tower.sequenceRemaining=0;break;}
                    --tower.sequenceRemaining;tower.sequenceDelay+=tower.sequenceInterval;
                }
            }
            if(tower.fireCooldownRemaining<=0 && tower.sequenceRemaining==0 && tower.targetSearchCooldownRemaining<=0 && teslaDischarges<TeslaDischargesPerTick) {
                if(discharge(stats)) {
                    tower.attack={tower.buildingId,stats};attackBuffer_.push_back(tower.attack);
                    tower.fireCooldownRemaining=stats.attackInterval+cooldownOvershoot;
                    tower.sequenceRemaining=effectiveAttackCount(stats.quantity,1)-1;
                    tower.sequenceInterval=gunBurstTiming(stats).interval;tower.sequenceDelay=tower.sequenceInterval;
                }
            }
            continue;
        }
        const auto spawn = [&](const FinalAttackStats& shotStats, double yaw, double pitch) {
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
                if (!spawn(tower.attack.stats, tower.sequenceYaw, tower.sequencePitch)) {
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
            if (!target || !projectileDefenseCanTarget(
                    building->type, *target, building->baseHeight) ||
                !withinRange(origin, target->position, range) ||
                (missile && withinRange(origin, target->position, MissileLauncherMinimumRange)) ||
                !directionInsideDefenseArc(
                    origin, target->position, tower.restYaw,
                    building->level)) {
                tower.targetId.reset();
            }
        }

        if (preferMarkedTargets_ && tower.targetSearchCooldownRemaining <= 0.0) {
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
            tower.targetId = missile ? enemies.densestEnemyInArc(
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
        if (flame) {
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
            for (int instance = 0; instance < effectiveAttackCount(stats.quantity, 1); ++instance) {
                if (!spawn(stats, tower.yaw + fanAngleDegrees(instance, effectiveAttackCount(stats.quantity, 1), stats.fanStepDegrees) * PiRadians / 180.0,
                    tower.pitch)) break;
            }
        } else {
            // One snapshot and sequence per building. Compression leaves at least
            // 40% of the event period for the pause, independent of Quantity.
            tower.sequenceYaw = tower.yaw;
            tower.sequencePitch = missile ? tower.pitch : std::atan2(deltaY, std::hypot(deltaX, deltaZ));
            tower.sequenceRemaining = spawn(stats, tower.sequenceYaw, tower.sequencePitch)
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
