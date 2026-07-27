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
    int hitCount;
    int killedCount;
};

class CannonSystem {
  public:
    CannonSystem();

    void reset();
    void clearProjectiles();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const CannonExplosion> tick(double deltaSeconds,
                                          const std::vector<BuildingInstance>& buildings,
                                          EnemySystem& enemies);

    [[nodiscard]] const std::vector<CannonProjectile>& projectiles() const;
    [[nodiscard]] const std::vector<CannonRuntime>& cannons() const;

  private:
    void launch(const BuildingInstance& cannon, Vec3 targetPosition);
    void explode(CannonProjectile& projectile, EnemySystem& enemies);

    std::vector<CannonRuntime> cannons_;
    std::vector<CannonProjectile> projectiles_;
    std::vector<CannonExplosion> explosionBuffer_;
    std::uint32_t nextProjectileIndex_{4000};
};

} // namespace ian
