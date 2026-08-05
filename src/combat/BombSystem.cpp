#include "combat/BombSystem.hpp"
#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr std::size_t MaxProjectiles = 32;
constexpr double MaximumPhysicsStep = 1.0 / 120.0;
constexpr double Restitution = 0.28;
constexpr double BounceTangentRetention = 0.62;
constexpr double RollingFriction = 1.8;
constexpr double Pi = 3.14159265358979323846;

double dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 subtract(Vec3 value, Vec3 direction, double amount) {
    return {
        value.x - direction.x * amount,
        value.y - direction.y * amount,
        value.z - direction.z * amount,
    };
}

Vec3 scale(Vec3 value, double amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vec3 moveToward(Vec3 value, Vec3 target, double amount) {
    const double blend = std::clamp(amount, 0.0, 1.0);
    return {
        value.x + (target.x - value.x) * blend,
        value.y + (target.y - value.y) * blend,
        value.z + (target.z - value.z) * blend,
    };
}

void advanceRotation(BombProjectile& projectile, double step) {
    projectile.rotation.x = std::fmod(
        projectile.rotation.x + projectile.angularVelocity.x * step,
        2.0 * Pi);
    projectile.rotation.y = std::fmod(
        projectile.rotation.y + projectile.angularVelocity.y * step,
        2.0 * Pi);
    projectile.rotation.z = std::fmod(
        projectile.rotation.z + projectile.angularVelocity.z * step,
        2.0 * Pi);
}

} // namespace

BombSystem::BombSystem(BombBalanceDefinition definition)
    : definition_(definition), remainingBombs_(definition.startingBombs) {
    projectiles_.reserve(MaxProjectiles);
    explosionBuffer_.reserve(MaxProjectiles);
}

void BombSystem::reset() {
    projectiles_.clear();
    explosionBuffer_.clear();
    remainingBombs_ = definition_.startingBombs;
}

bool BombSystem::throwBomb(Vec3 origin, Vec3 direction) {
    if (remainingBombs_ <= 0) {
        return false;
    }

    BombProjectile* projectile = nullptr;
    for (auto& candidate : projectiles_) {
        if (!candidate.active) {
            projectile = &candidate;
            ++projectile->id.generation;
            break;
        }
    }
    if (projectile == nullptr) {
        if (projectiles_.size() >= MaxProjectiles) {
            return false;
        }
        projectiles_.push_back({});
        projectile = &projectiles_.back();
        projectile->id = {nextProjectileIndex_++, 1};
    }

    projectile->position = origin;
    projectile->velocity = {
        direction.x * definition_.throwSpeed,
        direction.y * definition_.throwSpeed + definition_.upwardSpeed,
        direction.z * definition_.throwSpeed,
    };
    projectile->rotation = {};
    projectile->angularVelocity = {
        direction.z * 12.0,
        3.5,
        -direction.x * 12.0,
    };
    projectile->fuseRemaining = definition_.fuseDuration;
    projectile->fuseDuration = definition_.fuseDuration;
    projectile->grounded = false;
    projectile->active = true;
    --remainingBombs_;
    return true;
}

std::span<const BombExplosion> BombSystem::tick(
    double deltaSeconds, EnemySystem& enemies,
    const TerrainHeightfield* terrain) {
    explosionBuffer_.clear();
    for (auto& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }

        projectile.fuseRemaining -= deltaSeconds;
        const int stepCount = std::clamp(
            static_cast<int>(std::ceil(deltaSeconds / MaximumPhysicsStep)),
            1, 16);
        const double step = deltaSeconds / static_cast<double>(stepCount);
        for (int physicsStep = 0; physicsStep < stepCount; ++physicsStep) {
            projectile.velocity.y -= definition_.gravity * step;
            projectile.position.x += projectile.velocity.x * step;
            projectile.position.y += projectile.velocity.y * step;
            projectile.position.z += projectile.velocity.z * step;

            const bool terrainAvailable = terrain != nullptr &&
                terrain->isInside(projectile.position.x, projectile.position.z);
            const double surfaceHeight = terrainAvailable
                ? terrain->getHeight(projectile.position.x, projectile.position.z)
                : 0.0;
            const double contactHeight = surfaceHeight + definition_.groundHeight;
            if (projectile.position.y > contactHeight) {
                projectile.grounded = false;
                projectile.angularVelocity = scale(
                    projectile.angularVelocity,
                    std::exp(-0.08 * step));
                advanceRotation(projectile, step);
                continue;
            }

            projectile.position.y = contactHeight;
            const Vec3 normal = terrainAvailable
                ? terrain->getNormal(projectile.position.x, projectile.position.z)
                : Vec3{0.0, 1.0, 0.0};
            const double normalVelocity = dot(projectile.velocity, normal);
            if (normalVelocity < 0.0) {
                const double impactSpeed = -normalVelocity;
                if (impactSpeed > 0.9) {
                    projectile.velocity = subtract(
                        projectile.velocity, normal,
                        (1.0 + Restitution) * normalVelocity);
                    const double outgoingNormal = dot(projectile.velocity, normal);
                    const Vec3 tangent = subtract(
                        projectile.velocity, normal, outgoingNormal);
                    projectile.velocity = {
                        normal.x * outgoingNormal +
                            tangent.x * BounceTangentRetention,
                        normal.y * outgoingNormal +
                            tangent.y * BounceTangentRetention,
                        normal.z * outgoingNormal +
                            tangent.z * BounceTangentRetention,
                    };
                    projectile.grounded = false;
                } else {
                    projectile.velocity = subtract(
                        projectile.velocity, normal, normalVelocity);
                    projectile.grounded = true;
                }
            }

            if (projectile.grounded) {
                const double residualNormal = dot(projectile.velocity, normal);
                projectile.velocity = subtract(
                    projectile.velocity, normal, residualNormal);
                projectile.velocity = scale(
                    projectile.velocity,
                    std::exp(-RollingFriction * step));
                const double tangentSpeed = std::sqrt(
                    dot(projectile.velocity, projectile.velocity));
                const double slope = std::hypot(normal.x, normal.z);
                if (tangentSpeed < 0.06 && slope < 0.035) {
                    projectile.velocity = {};
                }

                const Vec3 rollingAngularVelocity = scale(
                    cross(normal, projectile.velocity),
                    1.0 / std::max(definition_.groundHeight, 0.01));
                projectile.angularVelocity = moveToward(
                    projectile.angularVelocity, rollingAngularVelocity,
                    1.0 - std::exp(-12.0 * step));
            } else {
                projectile.angularVelocity = scale(
                    projectile.angularVelocity,
                    std::exp(-0.08 * step));
            }

            advanceRotation(projectile, step);
        }
        if (projectile.fuseRemaining <= 0.0) {
            explode(projectile, enemies);
        }
    }
    return explosionBuffer_;
}

void BombSystem::clearProjectiles() {
    for (auto& projectile : projectiles_) {
        projectile.active = false;
    }
}

int BombSystem::remainingBombs() const {
    return remainingBombs_;
}

const std::vector<BombProjectile>& BombSystem::projectiles() const {
    return projectiles_;
}

void BombSystem::explode(BombProjectile& projectile, EnemySystem& enemies) {
    const auto damage =
        enemies.damageInRadius(projectile.position, definition_.explosionRadius,
                               definition_.explosionDamage, definition_.knockbackStrength);
    int killedCount = 0;
    for (const auto& result : damage) {
        if (result.killed) {
            ++killedCount;
        }
    }
    explosionBuffer_.push_back({
        .projectileId = projectile.id,
        .position = projectile.position,
        .hitCount = static_cast<int>(damage.size()),
        .killedCount = killedCount,
    });
    projectile.active = false;
}

} // namespace ian
