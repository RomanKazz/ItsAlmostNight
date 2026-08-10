#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"

#include <optional>
#include <span>
#include <vector>

namespace ian {

struct CannonRuntime {
    EntityId buildingId;
    std::optional<EntityId> targetId;
    double yaw{};
    double pitch{};
    double fireCooldownRemaining{};
    double targetSearchCooldownRemaining{};
};

struct CannonProjectile {
    EntityId id;
    EntityId cannonId;
    Vec3 position;
    Vec3 velocity;
    Vec3 targetPosition;
    double fuseRemaining{};
    double explosionRadius{};
    double explosionDamage{};
    double explosionImpulse{};
    bool active{};
};

struct CannonExplosion {
    EntityId projectileId;
    Vec3 position;
    double radius;
    int hitCount;
    int killedCount;
};

struct CannonShot {
    EntityId cannonId;
    EntityId projectileId;
    Vec3 position;
};

class CannonSystem {
  public:
    CannonSystem();

    [[nodiscard]] static double attackRange(std::uint8_t level);
    [[nodiscard]] static double fireInterval(std::uint8_t level);
    [[nodiscard]] static double explosionRadius(std::uint8_t level);
    [[nodiscard]] static double explosionDamage(std::uint8_t level);

    void reset();
    void clearProjectiles();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const CannonExplosion> tick(double deltaSeconds,
                                          const std::vector<BuildingInstance>& buildings,
                                          EnemySystem& enemies);

    [[nodiscard]] const std::vector<CannonProjectile>& projectiles() const;
    [[nodiscard]] const std::vector<CannonRuntime>& cannons() const;
    [[nodiscard]] std::span<const CannonShot> shots() const;

  private:
    void launch(const BuildingInstance& cannon, Vec3 targetPosition);
    void explode(CannonProjectile& projectile, EnemySystem& enemies);

    std::vector<CannonRuntime> cannons_;
    std::vector<CannonProjectile> projectiles_;
    std::vector<CannonExplosion> explosionBuffer_;
    std::vector<CannonShot> shotBuffer_;
    std::uint32_t nextProjectileIndex_{4000};
};

} // namespace ian
