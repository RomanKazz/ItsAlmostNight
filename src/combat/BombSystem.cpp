#include "combat/BombSystem.hpp"

#include <algorithm>

namespace ian {
namespace {

constexpr std::size_t MaxProjectiles = 32;

} // namespace

BombSystem::BombSystem(BombBalanceDefinition definition)
    : definition_(definition), remainingBombs_(definition.startingBombs) {
    projectiles_.reserve(MaxProjectiles);
    explosionBuffer_.reserve(MaxProjectiles);
}

void BombSystem::reset() {
    projectiles_.clear();
    explosionBuffer_.clear();
    nextProjectileIndex_ = 5000;
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
    projectile->fuseRemaining = definition_.fuseDuration;
    projectile->active = true;
    --remainingBombs_;
    return true;
}

std::span<const BombExplosion> BombSystem::tick(double deltaSeconds, EnemySystem& enemies) {
    explosionBuffer_.clear();
    for (auto& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }

        projectile.fuseRemaining -= deltaSeconds;
        projectile.velocity.y -= definition_.gravity * deltaSeconds;
        projectile.position.x += projectile.velocity.x * deltaSeconds;
        projectile.position.y += projectile.velocity.y * deltaSeconds;
        projectile.position.z += projectile.velocity.z * deltaSeconds;
        if (projectile.position.y < definition_.groundHeight) {
            projectile.position.y = definition_.groundHeight;
            if (projectile.velocity.y < 0.0) {
                projectile.velocity.y = -projectile.velocity.y * 0.35;
            }
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
