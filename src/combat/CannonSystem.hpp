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
    BuildingType type{BuildingType::Cannon};
    std::optional<EntityId> targetId;
    std::optional<Vec3> pendingTargetPosition;
    double restYaw{};
    double baseYaw{};
    double yaw{};
    double pitch{};
    double fireCooldownRemaining{};
    double targetSearchCooldownRemaining{};
    double firingAnimationRemaining{};
    bool loaded{true};
};

struct CannonProjectile {
    EntityId id;
    EntityId cannonId;
    BuildingType type{BuildingType::Cannon};
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
    BuildingType type{BuildingType::Cannon};
};

struct CannonHit {
    EntityId cannonId;
    EnemyDamageResult result;
    BuildingType type{BuildingType::Cannon};
};

class CannonSystem {
  public:
    CannonSystem();

    [[nodiscard]] static double attackRange(std::uint8_t level);
    [[nodiscard]] static double attackRange(
        BuildingType type, std::uint8_t level);
    [[nodiscard]] static double minimumRange(
        BuildingType type, std::uint8_t level);
    [[nodiscard]] static double fireInterval(std::uint8_t level);
    [[nodiscard]] static double fireInterval(
        BuildingType type, std::uint8_t level);
    [[nodiscard]] static double explosionRadius(std::uint8_t level);
    [[nodiscard]] static double explosionRadius(
        BuildingType type, std::uint8_t level);
    [[nodiscard]] static double explosionDamage(std::uint8_t level);
    [[nodiscard]] static double explosionDamage(
        BuildingType type, std::uint8_t level);

    void reset();
    void setSkillModifiers(double damage, double radius,
                           double fireRate, double highGroundDamage);
    void clearProjectiles();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    std::span<const CannonExplosion> tick(double deltaSeconds,
                                          const std::vector<BuildingInstance>& buildings,
                                          EnemySystem& enemies);

    [[nodiscard]] const std::vector<CannonProjectile>& projectiles() const;
    [[nodiscard]] const std::vector<CannonRuntime>& cannons() const;
    [[nodiscard]] std::span<const CannonShot> shots() const;
    [[nodiscard]] std::span<const CannonHit> hits() const;

  private:
    void launch(const BuildingInstance& cannon, Vec3 targetPosition,
                double yawRadians, double pitchRadians);
    void explode(CannonProjectile& projectile, EnemySystem& enemies);

    std::vector<CannonRuntime> cannons_;
    std::vector<CannonProjectile> projectiles_;
    std::vector<CannonExplosion> explosionBuffer_;
    std::vector<CannonShot> shotBuffer_;
    std::vector<CannonHit> hitBuffer_;
    std::uint32_t nextProjectileIndex_{4000};
    double damageMultiplier_{1.0};
    double radiusMultiplier_{1.0};
    double fireRateMultiplier_{1.0};
    double highGroundDamageMultiplier_{1.0};
};

} // namespace ian
