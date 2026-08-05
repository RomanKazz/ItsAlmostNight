#pragma once

#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"

#include <span>
#include <vector>

namespace ian {

class TerrainHeightfield;

struct BombProjectile {
    EntityId id;
    Vec3 position;
    Vec3 velocity;
    Vec3 rotation;
    Vec3 angularVelocity;
    double fuseRemaining{};
    double fuseDuration{};
    bool grounded{};
    bool active{};
};

struct BombExplosion {
    EntityId projectileId;
    Vec3 position;
    int hitCount;
    int killedCount;
};

class BombSystem {
  public:
    explicit BombSystem(
        BombBalanceDefinition definition = GameBalance::defaults().weapons.bomb);

    void reset();
    bool throwBomb(Vec3 origin, Vec3 direction,
                   bool consumeBomb = true);
    std::span<const BombExplosion> tick(
        double deltaSeconds, EnemySystem& enemies,
        const TerrainHeightfield* terrain = nullptr);
    void clearProjectiles();

    [[nodiscard]] int remainingBombs() const;
    [[nodiscard]] const std::vector<BombProjectile>& projectiles() const;

  private:
    void explode(BombProjectile& projectile, EnemySystem& enemies);

    std::vector<BombProjectile> projectiles_;
    std::vector<BombExplosion> explosionBuffer_;
    std::uint32_t nextProjectileIndex_{5000};
    BombBalanceDefinition definition_;
    int remainingBombs_{};
};

} // namespace ian
