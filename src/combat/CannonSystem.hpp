#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/CannonAttack.hpp"
#include "combat/AttackProjectile.hpp"
#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"

#include <array>
#include <cstddef>
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
    double yawVelocity{};
    double pitchVelocity{};
    double previousYaw{};
    double previousPitch{};
    bool manualTurnPending{};
    double fireCooldownRemaining{};
    double targetSearchCooldownRemaining{};
    double firingAnimationRemaining{};
    bool loaded{true};
    AttackEvent attack;
};

struct CannonProjectile {
    EntityId id;
    EntityId cannonId;
    BuildingType type{BuildingType::Cannon};
    Vec3 position;
    Vec3 velocity;
    Vec3 targetPosition;
    double fuseRemaining{};
    // Bounded history of actual positions: the trail follows the ballistic arc.
    static constexpr std::size_t TrailCapacity = 16;
    std::array<Vec3, TrailCapacity> trailPositions{};
    std::size_t trailHead{};
    std::size_t trailPointCount{};
    double explosionRadius{};
    double explosionDamage{};
    double explosionImpulse{};
    double visualScale{1.0};
    int clusterExplosionCount{};
    bool active{};
    ProjectileConfiguration configuration;
    ProjectileRuntimeState runtime;
    void resetForReuse() {
        const EntityId previousId = id;
        auto history = std::move(runtime.alreadyHitTargets);
        *this = {}; id = previousId;
        runtime.alreadyHitTargets = std::move(history);
        runtime.reset();
    }
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
    bool critical{};
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

    [[nodiscard]] static AttackStats baseStats(BuildingType type, std::uint8_t level);
    [[nodiscard]] FinalAttackStats finalStats(const BuildingInstance& building) const;
    void setPreferMarkedTargets(bool enabled) { preferMarkedTargets_ = enabled; }
    void setAttackModifiers(const AttackModifiers& modifiers) { modifiers_ = modifiers; }
    [[nodiscard]] std::size_t droppedProjectiles() const { return pool_.dropped(); }
    void reset();
    void setAttackStyle(CannonAttackStyle style);
    void setTypeBonuses(BuildingType type, double damage, double fireRate);
    void setModifiers(double damage, double radius,
                           double fireRate, double highGroundDamage,
                           int clusterExplosionCount = 0);
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
                double yawRadians, double pitchRadians, const FinalAttackStats& stats);
    bool launchProjectile(const BuildingInstance& cannon, Vec3 origin,
                          Vec3 targetPosition, const CannonAttackProfile& profile, const FinalAttackStats& stats, bool recordShot);
    void explode(CannonProjectile& projectile, EnemySystem& enemies);
    bool bounce(CannonProjectile& projectile, EnemySystem& enemies);

    std::vector<CannonRuntime> cannons_;
    ProjectilePool<CannonProjectile> pool_{200000};
    AttackCollisionQuery query_;
    std::vector<CannonExplosion> explosionBuffer_;
    std::vector<CannonShot> shotBuffer_;
    std::vector<CannonHit> hitBuffer_;
    CannonAttackStyle attackStyle_{CannonAttackStyle::Standard};
    BuildingType bonusType_{BuildingType::Core};
    AttackModifiers modifiers_;
    bool preferMarkedTargets_{};
    AttackModifiers typeModifiers_;
    double highGroundDamageMultiplier_{1.0};
    int clusterExplosionCount_{};
};

} // namespace ian
