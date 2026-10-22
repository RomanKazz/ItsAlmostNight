#include "combat/AttackProjectile.hpp"
#include "combat/DefenseTomeScaling.hpp"

namespace ian {
namespace {
Vec3 steerMissile(Vec3 current, Vec3 desired, double response, double maximumRate, double seconds) {
    const Vec3 from = geometry::normalizedOr(current), to = geometry::normalizedOr(desired);
    const double cosine = std::clamp(geometry::dot(from,to), -1.0, 1.0);
    const double angle = std::acos(cosine);
    // Ease out as the heading converges instead of snapping onto the last straight
    // segment. The angular cap keeps the opening turn broad and readable.
    const double turn = std::min(angle * -std::expm1(-response * seconds), maximumRate * seconds);
    if (turn <= 1e-12) return from;
    Vec3 axis = geometry::cross(from,to);
    if (geometry::lengthSquared(axis) < 1e-12) {
        axis = geometry::cross(from, std::abs(from.y)<.9 ? Vec3{0,1,0} : Vec3{1,0,0});
    }
    axis = geometry::normalizedOr(axis);
    // Rodrigues rotation also handles a target exactly behind the rocket.
    return geometry::normalizedOr(geometry::add(geometry::scale(from,std::cos(turn)),
        geometry::scale(geometry::cross(axis,from),std::sin(turn))));
}
}
bool AttackProjectileSystem::spawn(EntityId source, BuildingType type, Vec3 origin,
    Vec3 direction, const FinalAttackStats& stats, double surfaceHeight, std::uint8_t muzzle, bool shrapnel, std::optional<EntityId> target) {
    auto* projectile = pool_.acquire();
    if (!projectile) return false;
    projectile->sourceId = source; projectile->type = type;
    projectile->configuration = {stats, defenseProjectileHitRadius(type == BuildingType::Turret) * stats.size, surfaceHeight};
    projectile->position = projectile->previousPosition = projectile->launchOrigin = origin;
    projectile->shrapnel = shrapnel;
    projectile->runtime.currentTarget = target;
    projectile->runtime.velocity = geometry::scale(geometry::normalizedOr(direction), stats.projectileSpeed);
    projectile->runtime.remainingBounces = effectiveAttackCount(stats.bounces);
    projectile->runtime.remainingPierce = effectiveAttackCount(stats.pierce);
    projectile->muzzleIndex = muzzle;
    return true;
}

std::span<const ProjectileImpact> AttackProjectileSystem::tick(double deltaSeconds, EnemySystem& enemies) {
    impacts_.clear();
    blasts_.clear();
    query_.rebuild(enemies);
    auto& items = pool_.items();
    for (std::size_t index = 0; index < items.size(); ++index) {
        auto& projectile = items[index];
        if (!projectile.active) continue;
        auto& state = projectile.runtime;
        const auto& config = projectile.configuration;
        const auto& stats = config.stats;
        projectile.previousPosition = projectile.position;
        if (projectile.type == BuildingType::MissileLauncher) {
            state.targetSearchRemaining -= std::max(0.0, deltaSeconds);
            if (state.currentTarget && !enemies.enemy(*state.currentTarget)) state.currentTarget.reset();
            if (!state.currentTarget && state.targetSearchRemaining <= 0) {
                state.targetSearchRemaining = 0.12;
                if (const auto next = query_.bounceTarget(enemies, projectile.position, 8.0,
                        projectile.type, config.sourceSurfaceHeight, state.alreadyHitTargets)) state.currentTarget = next->id;
            }
        }
        // Duration is shared with other attack effects and is much longer than
        // the useful flight of a straight shot. Keep a little room for targets
        // near the range boundary and for rockets to turn onto moving targets.
        const double maximumTravel = projectile.type == BuildingType::MissileLauncher
            ? stats.range * 1.75 + 2.0 : stats.range + 2.0;
        const double remainingTravel = std::max(0.0, maximumTravel - state.travelledDistance);
        double remainingTime = std::min({std::max(0.0, deltaSeconds),
            std::max(0.0, stats.duration - state.elapsedLifetime),
            remainingTravel / std::max(1e-9, stats.projectileSpeed)});
        state.previousTravelledDistance = state.travelledDistance;
        // Flight speed stays constant through pierce/bounce. Keep accumulated
        // distance: displacement from the muzzle shrinks when a shot bounces back.
        state.travelledDistance += remainingTime * stats.projectileSpeed;
        state.elapsedLifetime += remainingTime;
        bool finished = false;
        while (remainingTime > 1e-9 && !finished) {
            // Rockets first leave raised tubes, then converge along distinct arcs.
            // Ordinary bullets keep their existing optional homing behavior.
            const bool rocket = projectile.type == BuildingType::MissileLauncher;
            if (stats.homing > 0.0 && state.currentTarget) {
                const auto target = enemies.enemy(*state.currentTarget);
                if (target) {
                    Vec3 aim = attackTargetPosition(*target);
                    if (rocket) {
                        const double arcProgress = std::clamp(state.elapsedLifetime / 1.05, 0.0, 1.0);
                        const double envelope = 1.0 - arcProgress * arcProgress * (3.0 - 2.0 * arcProgress);
                        const auto seed = (static_cast<std::uint64_t>(projectile.id.index) << 32U) ^ projectile.id.generation;
                        const double variation = unitRandom(seed);
                        const Vec3 toTarget = geometry::subtract(aim, projectile.launchOrigin);
                        const double horizontal = std::max(0.001, std::hypot(toTarget.x, toTarget.z));
                        const double side = (variation - 0.5) * 2.0 * envelope;
                        aim.x += -toTarget.z / horizontal * side;
                        aim.z += toTarget.x / horizontal * side;
                        aim.y += envelope * (1.0 + variation);
                    }
                    const auto desired = geometry::scale(geometry::normalizedOr(
                        geometry::subtract(aim, projectile.position)), stats.projectileSpeed);
                    Vec3 heading;
                    if (rocket) {
                        const double targetDistance = geometry::length(geometry::subtract(aim, projectile.launchOrigin));
                        const double launchWindow = (0.42 + 0.025 * projectile.muzzleIndex) *
                            std::clamp(targetDistance / 8.0, 0.35, 1.0);
                        const double launchProgress = std::clamp((state.elapsedLifetime - 0.04) /
                            launchWindow, 0.0, 1.0);
                        const double guidance = launchProgress * launchProgress * (3.0 - 2.0 * launchProgress);
                        const double distance = geometry::length(geometry::subtract(aim, projectile.position));
                        // Tighten continuously on approach so nearby/moving targets
                        // are intercepted rather than orbited, including at high speed.
                        const double turnRate = std::max(stats.projectileSpeed / 4.0,
                            stats.projectileSpeed / (0.8 + 0.6 * distance));
                        const double response = stats.homing * std::max(1.0, stats.projectileSpeed / 12.0) *
                            std::max(1.0, 4.0 / (0.4 + distance));
                        heading = steerMissile(state.velocity, desired, response * guidance,
                            turnRate * guidance, remainingTime);
                    } else {
                        heading = geometry::normalizedOr(geometry::lerpClamped(state.velocity, desired, stats.homing * remainingTime));
                    }
                    state.velocity = geometry::scale(heading, stats.projectileSpeed);
                }
            }
            const Vec3 end = geometry::add(projectile.position, geometry::scale(state.velocity, remainingTime));
            const auto contact = query_.sweep(enemies, projectile.position, end, config.hitRadius,
                projectile.type, config.sourceSurfaceHeight, state.alreadyHitTargets);
            if (!contact) { projectile.position = end; break; }
            projectile.position = contact->position;
            remainingTime -= contact->distance / std::max(1e-9, stats.projectileSpeed);
            const bool secondary = !state.alreadyHitTargets.empty();
            state.alreadyHitTargets.push_back(contact->id);
            const auto contactStats = defenseContactStats(stats, projectile.type,
                effectiveAttackCount(stats.bounces) - state.remainingBounces,
                effectiveAttackCount(stats.pierce) - state.remainingPierce);
            const auto hit = applyAttackHit(enemies, contact->id, contactStats, state.velocity,
                (static_cast<std::uint64_t>(projectile.id.index) << 32U) ^ projectile.id.generation ^
                (static_cast<std::uint64_t>(contact->id.index) * 0x9e3779b97f4a7c15ULL));
            if (hit) impacts_.push_back({projectile.sourceId, projectile.type, *hit,
                projectile.position, contactStats.damage, projectile.muzzleIndex, secondary || projectile.shrapnel, contactStats, projectile.launchOrigin, projectile.shrapnel});
            if (projectile.type == BuildingType::MissileLauncher) {
                const double radius = missileExplosionRadius(stats);
                blasts_.push_back({projectile.sourceId, projectile.position, radius});
                query_.nearby(enemies, projectile.position, radius + 2.0, [&](const EnemyInstance& enemy) {
                    if (enemy.id == contact->id || !projectileDefenseCanTarget(projectile.type, enemy, config.sourceSurfaceHeight)) return;
                    const Vec3 delta = geometry::subtract(attackTargetPosition(enemy), projectile.position);
                    if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z > radius * radius) return;
                    const auto splash = applyAttackHit(enemies, enemy.id, stats, delta,
                        (static_cast<std::uint64_t>(projectile.id.index) << 32U) ^ projectile.id.generation ^
                        (static_cast<std::uint64_t>(enemy.id.index) * 0x9e3779b97f4a7c15ULL));
                    if (splash) impacts_.push_back({projectile.sourceId, projectile.type, *splash,
                        attackTargetPosition(enemy), stats.damage, projectile.muzzleIndex, true, stats, projectile.launchOrigin, false});
                });
                finished = true;
                break;
            }
            // Pierce spends its budget first, keeping the current direction.
            // Then bounce spends its own budget. Each target is hit at most once.
            if (state.remainingPierce > 0) { --state.remainingPierce; continue; }
            if (state.remainingBounces > 0) {
                const auto target = query_.bounceTarget(enemies, projectile.position, stats.bounceRange,
                    projectile.type, config.sourceSurfaceHeight, state.alreadyHitTargets);
                if (target) {
                    --state.remainingBounces; state.currentTarget = target->id;
                    state.velocity = geometry::scale(geometry::normalizedOr(geometry::subtract(
                        attackTargetPosition(*target), projectile.position)), stats.projectileSpeed);
                    continue;
                }
            }
            finished = true;
        }
        if (projectile.type == BuildingType::MissileLauncher && !finished) {
            state.trailElapsed += std::max(0.0, deltaSeconds);
            if (state.trailCount == 0 || state.trailElapsed >= 0.045) {
                state.trailElapsed = std::fmod(state.trailElapsed, 0.045);
                state.trailHead = (state.trailHead + 1) % state.trailPositions.size();
                state.trailPositions[state.trailHead] = projectile.position;
                state.trailCount = std::min(state.trailCount + 1, state.trailPositions.size());
            }
        }
        if (finished || state.elapsedLifetime >= stats.duration ||
            state.travelledDistance >= maximumTravel - 1e-9) pool_.release(index);
    }
    return impacts_;
}
} // namespace ian
