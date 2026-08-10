#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/FoundationSystem.hpp"
#include "core/Types.hpp"
#include "resources/ResourceSystem.hpp"
#include "game/LootChestSystem.hpp"
#include "progression/InsightSystem.hpp"

#include <optional>
#include <string>

namespace ian {

enum class GameEventType {
    RunStarted,
    RunRestarted,
    PauseChanged,
    ResourceHit,
    ResourceCollected,
    ResourceGatherMissed,
    ResourceGranted,
    PickaxeHit,
    BuildingPlaced,
    BuildingRejected,
    BuildingDestroyed,
    BuildingDamaged,
    ModularBuildingDestroyed,
    ModularBuildingDamaged,
    CoreDamaged,
    BossRamImpact,
    PlayerDamaged,
    PlayerDied,
    PlayerRespawned,
    EnemyKilled,
    EnemySplit,
    SunsetStarted,
    AttackDirectionWarned,
    WaveStarted,
    EarlyWaveBonusGranted,
    WaveCompleted,
    WaveRewardGranted,
    RunEnded,
    ProjectileHit,
    GoldProduced,
    CoinCollected,
    BuildingUpgraded,
    BuildingUpgradeRejected,
    BuildingRepaired,
    ModularBuildingRepaired,
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
    TrapActivated,
    TrapHit,
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
    SkillPointsGranted,
    SkillUnlocked,
    IntroSkillObjectiveCompleted,
    BuildingFortified,
    ChestOpened,
    ChestOpenRejected,
    LootCollected,
};

struct GameEvent {
    GameEventType type;
    std::optional<EntityId> entityId;
    std::optional<EntityId> sourceId;
    std::optional<ResourceType> resourceType;
    std::optional<BuildingType> buildingType;
    std::optional<BuildingInstance> building;
    std::optional<PlatformFrameInstance> platformFrame;
    std::optional<WallInstance> modularWall;
    std::optional<RampInstance> ramp;
    std::optional<PlacementError> placementError;
    std::optional<UpgradeError> upgradeError;
    std::optional<BuildingActionError> buildingActionError;
    std::optional<WeaponUpgradeError> weaponUpgradeError;
    Vec3 position;
    int amount{};
    double damage{};
    double intensity{};
    bool critical{};
    bool bareHands{};
    bool largeDeposit{};
    bool night{};
    std::optional<std::string> objectiveId;
    std::optional<InsightSource> insightSource;
    double insightAmount{};
    double insightBefore{};
    double insightAfter{};
    double insightRequirement{};
    double insightDiminishingMultiplier{1.0};
    int treePointsGranted{};
    std::optional<LootRarity> lootRarity;
    std::optional<LootUpgradeEffect> lootUpgradeEffect;
};

} // namespace ian
