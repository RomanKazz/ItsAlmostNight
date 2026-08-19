#pragma once

#include "core/Types.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace ian {

class TerrainHeightfield;
struct BuildingInstance;
struct MapObstacle;

inline constexpr std::size_t IceWandMaximumProjectiles = 12;
inline constexpr std::size_t IceWandTrailPointCount = 18;

enum class WandElement {
    Ice,
    Fire,
};

struct IceWandProjectile {
    EntityId id{};
    Vec3 previousPosition{};
    Vec3 position{};
    Vec3 velocity{};
    std::array<Vec3, IceWandTrailPointCount> trail{};
    std::size_t trailCount{};
    double age{};
    double lifetime{};
    double radius{};
    WandElement element{WandElement::Ice};
    bool active{};
};

struct IceWandLaunch {
    EntityId projectileId{};
    Vec3 position{};
};

struct IceWandHit {
    EntityId projectileId{};
    EntityId enemyId{};
    Vec3 position{};
    double damage{};
    bool killed{};
    bool alreadyFrozen{};
    bool periodicBurn{};
};

struct IceWandImpact {
    EntityId projectileId{};
    Vec3 position{};
    int hitCount{};
    int killedCount{};
};

class IceWandSystem {
  public:
    explicit IceWandSystem(
        IceWandBalanceDefinition definition =
            GameBalance::defaults().weapons.iceWand);
    explicit IceWandSystem(FireWandBalanceDefinition definition);

    void reset();
    void setSkillModifiers(double damage, double radius,
                           double statusDuration, double burnDamage,
                           double thermalShockDamage);
    void setCastSpeedMultiplier(double multiplier);
    bool requestFire(Vec3 origin, Vec3 direction);
    void tick(double deltaSeconds, EnemySystem& enemies,
              const TerrainHeightfield* terrain,
              std::span<const BuildingInstance> buildings,
              std::span<const MapObstacle> obstacles = {});
    void clearProjectiles();
    void ignite(EntityId enemyId, EntityId sourceId,
                const EnemySystem& enemies, double duration,
                double damagePerSecond);

    [[nodiscard]] std::span<const IceWandProjectile> projectiles() const;
    [[nodiscard]] std::span<const IceWandLaunch> launches() const;
    [[nodiscard]] std::span<const IceWandHit> hits() const;
    [[nodiscard]] std::span<const IceWandImpact> impacts() const;
    [[nodiscard]] double chargeRemaining() const;
    [[nodiscard]] double chargeDuration() const;
    [[nodiscard]] double cooldownRemaining() const;
    [[nodiscard]] double directDamage() const;
    [[nodiscard]] double maximumRange() const;
    [[nodiscard]] double burnDuration() const;

  private:
    struct EnemySweepHit {
        EntityId id{};
        double time{};
        Vec3 position{};
    };

    struct BurningEnemy {
        EntityId enemyId{};
        EntityId sourceProjectileId{};
        double remaining{};
        double tickRemaining{};
        double damagePerSecond{};
        bool active{};
    };

    [[nodiscard]] std::optional<EnemySweepHit> sweepEnemy(
        Vec3 start, Vec3 end, double radius,
        const EnemySystem& enemies) const;
    [[nodiscard]] std::optional<double> sweepBuilding(
        Vec3 start, Vec3 end, double radius,
        std::span<const BuildingInstance> buildings) const;
    [[nodiscard]] std::optional<double> sweepTerrain(
        Vec3 start, Vec3 end, double radius,
        const TerrainHeightfield* terrain) const;
    [[nodiscard]] std::optional<double> sweepObstacles(
        Vec3 start, Vec3 end, double radius,
        std::span<const MapObstacle> obstacles) const;
    void spawnProjectile();
    void impactProjectile(
        IceWandProjectile& projectile,
        Vec3 impactPosition,
        std::optional<EntityId> directTarget,
        EnemySystem& enemies);
    void recordHit(const IceWandProjectile& projectile,
                   const EnemyDamageResult& result,
                   bool alreadyFrozen);
    void updateBurning(double deltaSeconds, EnemySystem& enemies);
    void applyBurn(EntityId enemyId, EntityId sourceProjectileId,
                   const EnemySystem& enemies);
    [[nodiscard]] bool isBurning(EntityId enemyId) const;

    std::array<IceWandProjectile, IceWandMaximumProjectiles> projectiles_{};
    std::array<std::uint32_t, IceWandMaximumProjectiles> generations_{};
    std::array<IceWandLaunch, IceWandMaximumProjectiles> launchBuffer_{};
    std::array<IceWandHit, EnemySystem::MaxActiveEnemies> hitBuffer_{};
    std::array<IceWandImpact, IceWandMaximumProjectiles> impactBuffer_{};
    std::array<BurningEnemy, EnemySystem::MaxActiveEnemies> burningEnemies_{};
    std::size_t launchCount_{};
    std::size_t hitCount_{};
    std::size_t impactCount_{};
    std::size_t nextSlot_{};
    IceWandBalanceDefinition definition_;
    FireWandBalanceDefinition fireDefinition_{};
    WandElement element_{WandElement::Ice};
    std::uint32_t firstProjectileIndex_{6000};
    double cooldownRemaining_{};
    double chargeRemaining_{};
    Vec3 chargeOrigin_{};
    Vec3 chargeDirection_{0.0, 0.0, -1.0};
    bool charging_{};
    double damageMultiplier_{1.0};
    double radiusMultiplier_{1.0};
    double statusDurationMultiplier_{1.0};
    double burnDamageMultiplier_{1.0};
    double thermalShockDamage_{};
    double castSpeedMultiplier_{1.0};
};

} // namespace ian
