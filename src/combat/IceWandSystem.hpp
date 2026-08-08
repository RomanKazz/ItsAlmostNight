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

    void reset();
    bool requestFire(Vec3 origin, Vec3 direction);
    void tick(double deltaSeconds, EnemySystem& enemies,
              const TerrainHeightfield* terrain,
              std::span<const BuildingInstance> buildings,
              std::span<const MapObstacle> obstacles = {});
    void clearProjectiles();

    [[nodiscard]] std::span<const IceWandProjectile> projectiles() const;
    [[nodiscard]] std::span<const IceWandLaunch> launches() const;
    [[nodiscard]] std::span<const IceWandHit> hits() const;
    [[nodiscard]] std::span<const IceWandImpact> impacts() const;
    [[nodiscard]] double chargeRemaining() const;
    [[nodiscard]] double chargeDuration() const;
    [[nodiscard]] double cooldownRemaining() const;
    [[nodiscard]] double directDamage() const;
    [[nodiscard]] double maximumRange() const;

  private:
    struct EnemySweepHit {
        EntityId id{};
        double time{};
        Vec3 position{};
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

    std::array<IceWandProjectile, IceWandMaximumProjectiles> projectiles_{};
    std::array<std::uint32_t, IceWandMaximumProjectiles> generations_{};
    std::array<IceWandLaunch, IceWandMaximumProjectiles> launchBuffer_{};
    std::array<IceWandHit, EnemySystem::MaxActiveEnemies> hitBuffer_{};
    std::array<IceWandImpact, IceWandMaximumProjectiles> impactBuffer_{};
    std::size_t launchCount_{};
    std::size_t hitCount_{};
    std::size_t impactCount_{};
    std::size_t nextSlot_{};
    IceWandBalanceDefinition definition_;
    double cooldownRemaining_{};
    double chargeRemaining_{};
    Vec3 chargeOrigin_{};
    Vec3 chargeDirection_{0.0, 0.0, -1.0};
    bool charging_{};
};

} // namespace ian
