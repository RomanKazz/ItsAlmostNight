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
#include <cstdint>
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
    BossPhaseTransition,
    BossSlamWindup,
    BossWarCryWindup,
    Dead,
};

enum class BossActionType {
    PhaseChanged,
    GroundSlam,
    WarCry,
};

enum class EnemyApproachRole : std::uint8_t {
    Direct,
    FlankLeft,
    FlankRight,
};

enum class EliteAffix : std::uint8_t {
    None = 0,
    Berserker = 1U << 0U,
    Warden = 1U << 1U,
    Volatile = 1U << 2U,
};

using EliteAffixMask = std::uint8_t;

[[nodiscard]] constexpr EliteAffixMask eliteAffixMask(
    EliteAffix affix) {
    return static_cast<EliteAffixMask>(affix);
}

[[nodiscard]] constexpr bool hasEliteAffix(
    EliteAffixMask mask, EliteAffix affix) {
    return (mask & eliteAffixMask(affix)) != 0U;
}

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
    double splitAnimationRemaining;
    double ramWindup;
    double ramDamageMultiplier;
    double ramCooldown;
    double ramWindupRemaining;
    double ramCooldownRemaining;
    int bossPhase;
    double bossAbilityWindupRemaining;
    double bossAbilityCooldownRemaining;
    bool bossWarCryNext;
    double slowRemaining;
    double movementMultiplier;
    Vec3 knockbackVelocity;
    double yaw;
    double steeringTime;
    double steeringPhase;
    double steeringFrequency;
    double turnRate;
    double locomotionRate;
    EnemyApproachRole approachRole;
    EnemyState state;
    std::optional<EntityId> target;
    bool active;
    EliteAffixMask eliteAffixes{};
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
    EliteAffixMask eliteAffixes{};
};

struct EliteEnemyEvent {
    EntityId id;
    Vec3 position;
    EliteAffixMask affixes{};
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

struct BossActionEvent {
    BossActionType type;
    EntityId bossId;
    Vec3 position;
    int phase{};
    double radius{};
    double damage{};
};

struct EnemyProjectile {
    EntityId id;
    EntityId ownerId;
    std::optional<EntityId> targetId;
    Vec3 position;
    Vec3 targetPosition;
    Vec3 velocity;
    double damage{};
    double radius{0.24};
    double targetRadius{0.42};
    double lifetimeRemaining{};
    bool targetsPlayer{true};
    bool active{true};
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
                                      EnemyNavigationView navigation = {},
                                      bool prioritizePlayerTarget = false);

    [[nodiscard]] std::optional<EntityId> raycast(
        Vec3 origin, Vec3 direction, double maxDistance,
        const TerrainHeightfield* terrain = nullptr) const;
    std::optional<EnemyDamageResult> damage(EntityId id, double amount);
    [[nodiscard]] std::optional<EntityId> nearestEnemy(Vec3 position, double radius) const;
    [[nodiscard]] std::optional<EntityId> nearestEnemyInArc(
        Vec3 position, double radius, double yaw,
        double halfAngle, bool includeFlying = true,
        double maximumSurfaceHeightDifference =
            std::numeric_limits<double>::infinity()) const;
    [[nodiscard]] std::optional<EntityId> densestEnemy(Vec3 position, double radius,
                                                       double clusterRadius) const;
    [[nodiscard]] std::optional<EntityId> densestEnemyInArc(
        Vec3 position, double radius, double clusterRadius,
        double yaw, double halfAngle,
        double minimumRadius = 0.0) const;
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
    std::span<const EntityId> knockbackInRadius(
        Vec3 position, double radius, double strength);
    std::size_t defeatAll();
    [[nodiscard]] std::vector<EnemySplitResult> takeSplitEvents();
    [[nodiscard]] std::vector<EliteEnemyEvent> takeEliteSpawnEvents();
    [[nodiscard]] std::vector<EliteEnemyEvent> takeEliteDeathEvents();
    [[nodiscard]] std::vector<BossActionEvent> takeBossActionEvents();

    [[nodiscard]] std::size_t activeCount() const;
    [[nodiscard]] const std::vector<EnemyInstance>& enemies() const;
    [[nodiscard]] std::span<const EnemyProjectile> projectiles() const;
    void clearProjectiles();
    void constrainToArena(
        Vec3 center, double radius);
    [[nodiscard]] std::span<const EnemyPlayerAttack> playerAttacks() const;
    [[nodiscard]] const EnemyPerformanceStats& performanceStats() const;

  private:
    static constexpr std::uint32_t FirstEnemyIndex = 2000;

    struct PendingSplit {
        EntityId id;
        Vec3 position;
        double healthMultiplier;
        double damageMultiplier;
        double remaining{};
    };

    void appendEnemy(const EnemySpawn& spawn,
                     bool allowActiveOverflow = false);
    void spawnSplitlings(
        EntityId parentId, Vec3 position,
        double healthMultiplier, double damageMultiplier);
    void scheduleSplit(
        EntityId parentId, Vec3 position,
        double healthMultiplier, double damageMultiplier);
    void updatePendingSplits(double deltaSeconds);
    void markEnemyDead(EnemyInstance& enemy);
    [[nodiscard]] double incomingDamageMultiplier(
        const EnemyInstance& enemy) const;
    void rebuildSpatialIndex();
    [[nodiscard]] EnemyInstance* findEnemy(EntityId id);
    [[nodiscard]] const EnemyInstance* findEnemy(EntityId id) const;

    std::vector<EnemyInstance> enemies_;
    std::vector<EnemyAttack> attackBuffer_;
    std::vector<EnemyPlayerAttack> playerAttackBuffer_;
    std::vector<EnemyProjectile> projectiles_;
    std::vector<EnemyDamageResult> areaDamageBuffer_;
    std::vector<EntityId> statusTargetBuffer_;
    std::vector<EnemySplitResult> splitEventBuffer_;
    std::vector<EliteEnemyEvent> eliteSpawnEventBuffer_;
    std::vector<EliteEnemyEvent> eliteDeathEventBuffer_;
    std::vector<BossActionEvent> bossActionEventBuffer_;
    std::vector<EnemyStructureTarget> structureBuffer_;
    std::vector<EnemyStructureTarget> incomingStructureBuffer_;
    std::vector<int> structureNextBuffer_;
    std::vector<int> structureGridHeads_;
    std::vector<int> collisionEnemyLinks_;
    std::vector<EntityId> areaTargetBuffer_;
    std::vector<PendingSplit> pendingSplitBuffer_;
    std::vector<PendingSplit> delayedSplitBuffer_;
    std::size_t activeCount_{};
    std::uint32_t nextIndex_{FirstEnemyIndex};
    std::uint32_t nextProjectileIndex_{1U};
    std::optional<Vec3> previousPlayerPosition_;
    Vec3 estimatedPlayerVelocity_{};
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
