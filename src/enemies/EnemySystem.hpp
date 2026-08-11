#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/FoundationSystem.hpp"
#include "core/PerformanceStats.hpp"
#include "core/Types.hpp"
#include "game/GameBalance.hpp"
#include "navigation/FlowField.hpp"
#include "world/SpatialHash.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ian {

class TerrainHeightfield;
class CollisionWorld;

enum class EnemyType {
    Basic,
    Fast,
    Heavy,
    Boss,
    Ranged,
    Sapper,
    Flying,
    Splitter,
    Splitling,
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

[[nodiscard]] bool enemyUsesForwardSurfaceProbe(
    EnemyState state);

enum class StatusEffectType {
    Freeze,
    Slow,
};

struct EnemyStatusEffect {
    StatusEffectType type{StatusEffectType::Freeze};
    std::optional<EntityId> source;
    double remaining{};
    double intensity{};
    double immunityRemaining{};
    double visualParameter{};
};

struct StatusEffectRules {
    double eliteDurationMultiplier{0.65};
    double bossSlowAmount{0.35};
    double repeatedApplicationMultiplier{0.55};
    double immunityWindowFraction{0.35};
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
    double spawnAnimationRemaining;
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
    double aiUpdateRemaining{};
    // Height above terrain supplied by modular floors/ramps. Kept separate
    // from authored model/capsule Y so existing combat dimensions stay valid.
    double surfaceHeightOffset{};
    double worldSurfaceHeight{};
    std::array<EnemyStatusEffect, 2> statusEffects{{
        EnemyStatusEffect{.type = StatusEffectType::Freeze},
        EnemyStatusEffect{.type = StatusEffectType::Slow},
    }};
};

struct EnemySpawn {
    EnemyType type;
    Vec3 position;
    double healthMultiplier{1.0};
    double damageMultiplier{1.0};
    Vec3 initialKnockbackVelocity{};
};

struct EnemySplitResult {
    EntityId parentId;
    Vec3 position;
    int childCount{};
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
    // Walkable floor/ramp geometry can still receive structural damage, but
    // must not repel enemies while serving as navigation surface.
    bool traversable{};
    double minimumEnemySurfaceHeight{
        -std::numeric_limits<double>::infinity()};
    double maximumEnemySurfaceHeight{
        std::numeric_limits<double>::infinity()};
    std::optional<double> attackSurfaceHeight;
    bool attackable{true};
};

struct EnemyNavigationView {
    std::span<const PlatformFrameInstance> platformFrames;
    std::span<const RampInstance> ramps;
    double cellSize{1.0};
    const CollisionWorld* collisionWorld{};
    std::uint64_t revision{};
};

struct EnemyDamageResult {
    EntityId id;
    Vec3 position;
    double damage;
    double remainingHealth;
    bool killed;
};

struct EnemyPlayerAttack {
    EntityId enemyId;
    double damage;
};

struct EnemyPerformanceStats {
    PerformanceMetric tick;
    PerformanceMetric collision;
    PerformanceMetric spatialRebuild;
    std::size_t activeEnemies{};
    std::size_t spatialRebuilds{};
    std::size_t structureGridRebuilds{};
    std::size_t fullAiUpdates{};
    std::size_t throttledAiMoves{};
};

class EnemySystem {
  public:
    static constexpr std::size_t MaxEnemies = 2048;
    static constexpr std::size_t MaxActiveEnemies = 512;

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
                                          additionalStructures = {},
                                      const TerrainHeightfield* terrain = nullptr,
                                      EnemyNavigationView navigation = {});

    [[nodiscard]] std::optional<EntityId> raycast(
        Vec3 origin, Vec3 direction, double maxDistance,
        const TerrainHeightfield* terrain = nullptr) const;
    std::optional<EnemyDamageResult> damage(EntityId id, double amount);
    [[nodiscard]] std::optional<EntityId> nearestEnemy(Vec3 position, double radius) const;
    [[nodiscard]] std::optional<EntityId> densestEnemy(Vec3 position, double radius,
                                                       double clusterRadius) const;
    [[nodiscard]] std::optional<EnemyInstance> enemy(EntityId id) const;
    std::span<const EnemyDamageResult> damageInRadius(
        Vec3 position, double radius, double amount,
        double knockbackStrength = 0.0,
        std::optional<Vec3> knockbackOrigin = std::nullopt,
        double maxTotalDamage = 0.0,
        std::optional<EntityId> excludedId = std::nullopt);
    [[nodiscard]] bool applyStatus(
        EntityId id, StatusEffectType type,
        std::optional<EntityId> source, double duration,
        double intensity = 1.0,
        StatusEffectRules rules = {});
    bool clearStatus(EntityId id, StatusEffectType type);
    std::span<const EntityId> applyStatusInRadius(
        Vec3 position, double radius, StatusEffectType type,
        std::optional<EntityId> source, double duration,
        double intensity = 1.0,
        StatusEffectRules rules = {});
    std::span<const EntityId> applySlowInRadius(Vec3 position, double radius, double multiplier,
                                               double duration);
    std::size_t defeatAll();
    [[nodiscard]] std::vector<EnemySplitResult> takeSplitEvents();

    [[nodiscard]] std::size_t activeCount() const;
    [[nodiscard]] const std::vector<EnemyInstance>& enemies() const;
    [[nodiscard]] std::span<const EnemyPlayerAttack> playerAttacks() const;
    [[nodiscard]] const EnemyPerformanceStats& performanceStats() const;

  private:
    static constexpr std::uint32_t FirstEnemyIndex = 2000;

    struct PendingSplit {
        EntityId id;
        Vec3 position;
        double healthMultiplier;
        double damageMultiplier;
    };

    void appendEnemy(const EnemySpawn& spawn);
    void spawnSplitlings(
        EntityId parentId, Vec3 position,
        double healthMultiplier, double damageMultiplier);
    void markEnemyDead(EnemyInstance& enemy);
    void rebuildSpatialIndex();
    [[nodiscard]] EnemyInstance* findEnemy(EntityId id);
    [[nodiscard]] const EnemyInstance* findEnemy(EntityId id) const;

    std::vector<EnemyInstance> enemies_;
    std::vector<EnemyAttack> attackBuffer_;
    std::vector<EnemyPlayerAttack> playerAttackBuffer_;
    std::vector<EnemyDamageResult> areaDamageBuffer_;
    std::vector<EntityId> statusTargetBuffer_;
    std::vector<EnemySplitResult> splitEventBuffer_;
    std::vector<EnemyStructureTarget> structureBuffer_;
    std::vector<EnemyStructureTarget> incomingStructureBuffer_;
    std::vector<int> structureNextBuffer_;
    std::vector<int> structureGridHeads_;
    std::vector<int> collisionEnemyLinks_;
    std::vector<EntityId> areaTargetBuffer_;
    std::vector<PendingSplit> pendingSplitBuffer_;
    std::size_t activeCount_{};
    std::uint32_t nextIndex_{FirstEnemyIndex};
    SpatialHash spatialHash_;
    std::array<EnemyDefinition, GameBalance::EnemyTypeCount> definitions_;
    EnemyPerformanceStats performanceStats_{};
    bool profilingTick_{};
    bool spatialHashDirty_{};
    std::size_t spatialRebuildsThisTick_{};
    double spatialRebuildMillisecondsThisTick_{};
    std::shared_ptr<void> navigationCache_;
    std::uint64_t cachedNavigationRevision_{
        std::numeric_limits<std::uint64_t>::max()};
};

[[nodiscard]] const EnemyStatusEffect& enemyStatusEffect(
    const EnemyInstance& enemy, StatusEffectType type);
[[nodiscard]] bool enemyHasStatus(
    const EnemyInstance& enemy, StatusEffectType type);

} // namespace ian
