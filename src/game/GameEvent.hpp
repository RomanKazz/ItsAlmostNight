#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/FoundationSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "core/Types.hpp"
#include "resources/ResourceSystem.hpp"
#include "game/LootChestSystem.hpp"
#include "progression/InsightSystem.hpp"

#include <optional>
#include <cstdint>
#include <string>

namespace ian {

enum class EnemyType : std::uint8_t;

enum class GameEventType {
    RunStarted,
    RunRestarted,
    PauseChanged,
    ResourceHit,
    ResourceCollected,
    ResourceGatherMissed,
    ResourceGranted,
    ResourceStorageFull,
    SawSplinterLaunched,
    PickaxeHit,
    BuildingPlaced,
    BuildingRejected,
    BuildingDestroyed,
    BuildingDamaged,
    ModularBuildingDestroyed,
    ModularBuildingDamaged,
    CoreDamaged,
    BossRamImpact,
    BossPhaseChanged,
    BossGroundSlam,
    BossWarCry,
    PlayerDamaged,
    RopeFallSaved,
    AppleConsumed,
    BreadWellFed,
    IronArmorBroken,
    BattlePotionActivated,
    BloodHarvestTriggered,
    OverkillHit,
    PlayerDied,
    PlayerRespawned,
    EnemyKilled,
    EnemyBanished,
    EnemySplit,
    EliteEnemySpawned,
    EliteVolatilePrimed,
    SunsetStarted,
    AttackDirectionWarned,
    WaveStarted,
    EarlyWaveBonusGranted,
    WaveCompleted,
    StageCleared,
    FinalNightStarted,
    WaveRewardGranted,
    RunEnded,
    ProjectileHit,
    CrystalProduced,
    CoinCollected,
    BuildingUpgraded,
    BuildingUpgradeRejected,
    BuildingRepaired,
    ModularBuildingRepaired,
    AnvilRepairShockwave,
    BuildingRepairRejected,
    BuildingSold,
    BuildingSellRejected,
    Explosion,
    IceWandChargeStarted,
    IceWandFired,
    IceWandImpact,
    IceWandHit,
    FireWandChargeStarted,
    FireWandFired,
    FireWandImpact,
    FireWandHit,
    ChainLightningHit,
    TrapActivated,
    TrapHit,
    CannonHit,
    WeaponFired,
    CannonFired,
    WeaponUpgraded,
    WeaponUpgradeRejected,
    ConsumableUsed,
    GateToggled,
    GateToggleRejected,
    PlayerLanded,
    PlayerDashed,
    ModularBuildingPlaced,
    InsightGranted,
    ObjectiveCompleted,
    BuildingFortified,
    ChestOpened,
    ChestOpenRejected,
    ChestRerolled,
    ChestRerollAlreadyUsed,
    ChestRerollUnavailable,
    ChestRevealed,
    BombPurchased,
    AllBuildingsRepaired,
    EconomyPurchaseRejected,
    WorldLandmarkActivated,
    CrystalStorageFull,
    LootCollected,
    ChallengeCompleted,
    TowerFired,
    PoisonHit,
    EnemyMagicFired,
    EnemyMagicImpact,
    BossAttackTelegraph,
    BossRoyalImpact,
    BossSummonAppeared,
    BossAttackInterrupted,
    MissileExplosion,
};

struct GameEvent {
    GameEventType type;
    std::optional<EntityId> entityId;
    std::optional<EntityId> sourceId;
    std::optional<ResourceType> resourceType;
    std::optional<BuildingType> buildingType;
    std::optional<EnemyType> enemyType;
    std::uint8_t enemyEliteAffixes{};
    std::optional<BuildingInstance> building;
    std::optional<PlatformFrameInstance> platformFrame;
    std::optional<WallInstance> modularWall;
    std::optional<RampInstance> ramp;
    std::optional<PlacementError> placementError;
    std::optional<UpgradeError> upgradeError;
    std::optional<BuildingActionError> buildingActionError;
    std::optional<WeaponUpgradeError> weaponUpgradeError;
    Vec3 position;
    std::optional<Vec3> targetPosition;
    int amount{};
    int coinAmount{};
    double damage{};
    // Outgoing shot damage before enemy mitigation and remaining-health
    // clipping. Presentation must not shrink a finishing projectile.
    double projectileDamage{};
    double intensity{};
    bool critical{};
    bool matchingTool{};
    bool largeDeposit{};
    bool night{};
    bool secondaryImpact{};
    bool structuralCollapse{};
    std::optional<std::string> objectiveId;
    std::optional<InsightSource> insightSource;
    double insightAmount{};
    double insightBefore{};
    double insightAfter{};
    double insightRequirement{};
    double insightDiminishingMultiplier{1.0};
    int levelsGranted{};
    double duration{};
    std::optional<LootRarity> lootRarity;
    std::optional<LootUpgradeEffect> lootUpgradeEffect;
};

// Keep kill attribution identical across direct hits, secondary attacks and
// damage-over-time. Callers retain responsibility for detecting a new kill.
[[nodiscard]] inline GameEvent enemyKilledEvent(
    std::optional<EntityId> id, std::optional<EnemyType> type,
    std::uint8_t eliteAffixes, Vec3 position,
    std::optional<EntityId> source = {},
    std::optional<BuildingType> buildingType = {}) {
    return {
        .type = GameEventType::EnemyKilled,
        .entityId = id,
        .sourceId = source,
        .buildingType = buildingType,
        .enemyType = type,
        .enemyEliteAffixes = eliteAffixes,
        .position = position,
    };
}

} // namespace ian
