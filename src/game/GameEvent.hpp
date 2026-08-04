#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/FoundationSystem.hpp"
#include "core/Types.hpp"
#include "resources/ResourceSystem.hpp"

#include <optional>

namespace ian {

enum class GameEventType {
    RunStarted,
    RunRestarted,
    PauseChanged,
    ResourceHit,
    ResourceCollected,
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
    SunsetStarted,
    AttackDirectionWarned,
    WaveStarted,
    WaveCompleted,
    WaveRewardGranted,
    RunEnded,
    ProjectileHit,
    GoldProduced,
    BuildingUpgraded,
    BuildingUpgradeRejected,
    BuildingRepaired,
    ModularBuildingRepaired,
    BuildingRepairRejected,
    BuildingSold,
    BuildingSellRejected,
    Explosion,
    TrapActivated,
    WeaponFired,
    CannonFired,
    WeaponUpgraded,
    WeaponUpgradeRejected,
    ConsumableUsed,
    GateToggled,
    GateToggleRejected,
    PlayerLanded,
    SkillPointsGranted,
    SkillUnlocked,
    IntroSkillObjectiveCompleted,
    BuildingFortified,
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
};

} // namespace ian
