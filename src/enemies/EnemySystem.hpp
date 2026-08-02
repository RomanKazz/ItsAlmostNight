#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"
#include "game/GameBalance.hpp"
#include "navigation/FlowField.hpp"
#include "world/SpatialHash.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ian {

enum class EnemyType {
    Basic,
    Fast,
    Heavy,
    Boss,
    Ranged,
    Sapper,
    Flying,
};

enum class EnemyState {
    Spawn,
    MoveToCore,
    ChasePlayer,
    AttackBuilding,
    AttackCore,
    AttackPlayer,
    BossRamWindup,
    Dead,
};

struct EnemyInstance {
    EntityId id;
    EnemyType type;
    Vec3 position;
    double health;
    double maxHealth;
    double speed;
    double damage;
    double attackCooldownRemaining;
    double hitAnimationRemaining;
    double ramWindup;
    double ramDamageMultiplier;
    double ramCooldown;
    double ramWindupRemaining;
    double ramCooldownRemaining;
    double slowRemaining;
    double movementMultiplier;
    Vec3 knockbackVelocity;
    double yaw;
    double steeringTime;
    double steeringPhase;
    double steeringFrequency;
    double turnRate;
    double locomotionRate;
    EnemyState state;
    std::optional<EntityId> target;
    bool active;
};

struct EnemySpawn {
    EnemyType type;
    Vec3 position;
    double healthMultiplier{1.0};
    double damageMultiplier{1.0};
};

struct EnemyAttack {
    EntityId enemyId;
    EntityId targetId;
    double damage;
    bool ram;
};

struct EnemyStructureTarget {
    EntityId id;
    Vec3 position;
    double radius;
    std::optional<BuildingType> buildingType;
    bool modular{};
    std::size_t structuralImpact{};
};

struct EnemyDamageResult {
    EntityId id;
    Vec3 position;
    double remainingHealth;
    bool killed;
};

struct EnemyPlayerAttack {
    EntityId enemyId;
    double damage;
};

class EnemySystem {
  public:
    static constexpr std::size_t MaxEnemies = 2048;
    static constexpr std::size_t MaxActiveEnemies = 160;

    explicit EnemySystem(
        std::array<EnemyDefinition, GameBalance::EnemyTypeCount> definitions =
            GameBalance::defaults().enemies);

    void reset();
    void spawnWave(std::span<const Vec3> positions);
    void spawnWave(std::span<const EnemySpawn> spawns);
    void spawnGroup(std::span<const EnemySpawn> spawns);

    std::span<const EnemyAttack> tick(double deltaSeconds,
                                      const std::vector<BuildingInstance>& buildings,
                                      const FlowField& flowField,
                                      std::optional<Vec3> playerPosition = std::nullopt,
                                      std::span<const EnemyStructureTarget>
                                          additionalStructures = {});

    [[nodiscard]] std::optional<EntityId> raycast(Vec3 origin, Vec3 direction,
                                                  double maxDistance) const;
    std::optional<EnemyDamageResult> damage(EntityId id, double amount);
    [[nodiscard]] std::optional<EntityId> nearestEnemy(Vec3 position, double radius) const;
    [[nodiscard]] std::optional<EntityId> densestEnemy(Vec3 position, double radius,
                                                       double clusterRadius) const;
    [[nodiscard]] std::optional<EnemyInstance> enemy(EntityId id) const;
    std::span<const EnemyDamageResult> damageInRadius(Vec3 position, double radius, double amount,
                                                      double knockbackStrength = 0.0);
    std::span<const EntityId> applySlowInRadius(Vec3 position, double radius, double multiplier,
                                               double duration);
    std::size_t defeatAll();

    [[nodiscard]] std::size_t activeCount() const;
    [[nodiscard]] const std::vector<EnemyInstance>& enemies() const;
    [[nodiscard]] std::span<const EnemyPlayerAttack> playerAttacks() const;

  private:
    void appendEnemy(const EnemySpawn& spawn);
    void rebuildSpatialIndex();

    std::vector<EnemyInstance> enemies_;
    std::vector<EnemyAttack> attackBuffer_;
    std::vector<EnemyPlayerAttack> playerAttackBuffer_;
    std::vector<EnemyDamageResult> areaDamageBuffer_;
    std::vector<EntityId> statusTargetBuffer_;
    std::vector<EnemyStructureTarget> structureBuffer_;
    std::vector<int> structureNextBuffer_;
    std::vector<int> collisionEnemyLinks_;
    std::vector<int> collisionBuildingLinks_;
    std::array<EntityId, SpatialHash::MaxEntries> areaTargetBuffer_{};
    std::uint32_t nextIndex_{2000};
    SpatialHash spatialHash_;
    std::array<EnemyDefinition, GameBalance::EnemyTypeCount> definitions_;
};

} // namespace ian
