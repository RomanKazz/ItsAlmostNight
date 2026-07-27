#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/BombSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "core/Types.hpp"
#include "economy/GoldMineSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/CollisionWorld.hpp"
#include "world/MapDefinition.hpp"
#include "waves/WaveDirector.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ian {

struct StartWaveEarlyCommand {};
struct EnableUnlimitedResourcesCommand {};
struct ToggleWeaponCommand {};
struct UpgradeWeaponCommand {};
struct UseConsumableCommand {};
struct DefeatAllEnemiesCommand {};
struct ToggleInvulnerabilityCommand {};
struct DamageCoreCommand {
    double amount{50.0};
};
struct SpawnEnemyCommand {
    EnemyType type{EnemyType::Basic};
};
struct ToggleGateCommand {
    EntityId gateId;
};

struct PlayerCommand {
    double moveForward{};
    double moveRight{};
    double lookYaw{};
    double lookPitch{};
    bool jump{};
    bool sprint{};
    bool usePickaxe{};
    bool fireRifle{};
    std::optional<BuildingType> selectBuilding;
    bool cancelBuilding{};
    std::optional<PlaceBuildingCommand> placeBuilding;
    int rotateBuilding{};
    std::optional<StartWaveEarlyCommand> startWaveEarly;
    std::optional<EnableUnlimitedResourcesCommand> enableUnlimitedResources;
    std::optional<UpgradeBuildingCommand> upgradeBuilding;
    std::optional<RepairBuildingCommand> repairBuilding;
    std::optional<SellBuildingCommand> sellBuilding;
    std::optional<ToggleWeaponCommand> toggleWeapon;
    std::optional<UpgradeWeaponCommand> upgradeWeapon;
    std::optional<UseConsumableCommand> useConsumable;
    std::optional<DefeatAllEnemiesCommand> defeatAllEnemies;
    std::optional<ToggleInvulnerabilityCommand> toggleInvulnerability;
    std::optional<DamageCoreCommand> damageCore;
    std::optional<SpawnEnemyCommand> spawnEnemy;
    std::optional<ToggleGateCommand> toggleGate;
};

enum class RunState {
    MainMenu,
    Gathering,
    BuildPhase,
    Sunset,
    Wave,
    WaveComplete,
    Victory,
    Defeat,
    Paused,
};

enum class TutorialObjective {
    MineWood,
    PlaceCore,
    MineStone,
    BuildGoldMine,
    PrepareForNight,
    SurviveFirstWave,
};

enum class AttackDirection {
    North,
    East,
    South,
    West,
};

enum class GameEventType {
    RunStarted,
    RunRestarted,
    PauseChanged,
    ResourceHit,
    ResourceCollected,
    BuildingPlaced,
    BuildingRejected,
    BuildingDestroyed,
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
    BuildingRepairRejected,
    BuildingSold,
    BuildingSellRejected,
    Explosion,
    TrapActivated,
    WeaponFired,
    WeaponUpgraded,
    WeaponUpgradeRejected,
    ConsumableUsed,
    GateToggled,
    GateToggleRejected,
};

struct GameEvent {
    GameEventType type;
    std::optional<EntityId> entityId;
    std::optional<EntityId> sourceId;
    std::optional<ResourceType> resourceType;
    std::optional<BuildingType> buildingType;
    std::optional<PlacementError> placementError;
    std::optional<UpgradeError> upgradeError;
    std::optional<BuildingActionError> buildingActionError;
    std::optional<WeaponUpgradeError> weaponUpgradeError;
    Vec3 position;
    int amount{};
};

struct SimulationSnapshot {
    RunState state;
    std::uint64_t tick;
    double elapsedSeconds;
    Vec3 playerPosition;
    double playerYaw;
    double playerPitch;
    bool playerGrounded;
    double playerHealth;
    double playerMaxHealth;
    int wood;
    int stone;
    int gold;
    double pickaxeCooldownRemaining;
    std::optional<EntityId> aimedResource;
    std::span<const ResourceNode> resourceNodes;
    double worldLimit;
    std::span<const MapObstacle> mapObstacles;
    std::span<const CollisionBox> collisionBoxes;
    std::span<const FlowDebugVector> flowDebugVectors;
    std::optional<BuildingType> selectedBuilding;
    std::optional<BuildingPreview> buildingPreview;
    std::span<const BuildingInstance> buildings;
    std::optional<EntityId> aimedEnemy;
    std::optional<EntityId> aimedBuilding;
    std::optional<ResourceCost> aimedBuildingUpgradeCost;
    std::span<const EnemyInstance> enemies;
    std::span<const TowerRuntime> towers;
    std::span<const CannonRuntime> cannons;
    std::span<const CannonProjectile> cannonProjectiles;
    std::span<const BombProjectile> bombProjectiles;
    std::size_t activeEnemyCount;
    std::size_t pendingEnemyCount;
    std::optional<AttackDirection> upcomingAttackDirection;
    double phaseTimeRemaining;
    double phaseDuration;
    int wave;
    double coreHealth;
    double coreMaxHealth;
    std::optional<EntityId> coreId;
    std::uint8_t coreLevel;
    bool unlimitedResources;
    bool playerInvulnerable;
    PlayerWeapon selectedWeapon;
    int rifleLevel;
    int rifleAmmunition;
    int rifleMagazineSize;
    int rifleUpgradeGoldCost;
    bool rifleReloading;
    double rifleReloadRemaining;
    int bombsRemaining;
    int waveCompletionReward;
    int tutorialWoodTarget;
    int tutorialStoneTarget;
    std::optional<TutorialObjective> tutorialObjective;
};

class Simulation {
  public:
    explicit Simulation(GameBalance balance = GameBalance::defaults(),
                        MapDefinition map = MapDefinition::defaults());

    void startRun();
    void restartRun();
    void togglePause();
    void tick(double deltaSeconds, const PlayerCommand& command = {});

    [[nodiscard]] SimulationSnapshot snapshot() const;
    std::vector<GameEvent> takeEvents();

  private:
    [[nodiscard]] PlacementResult validatePlacement(BuildingType type,
                                                    GridPosition position) const;
    void syncWorldStructures();
    void respawnPlayer();
    void prepareWave(const WavePlan& plan, GridPosition corePosition,
                     std::size_t firstAnchorIndex);
    void beginPreparedWave();
    void tickWaveSpawning(double deltaSeconds);
    void completeWave();
    [[nodiscard]] std::optional<TutorialObjective> tutorialObjective() const;

    RunState state_{RunState::MainMenu};
    RunState stateBeforePause_{RunState::Gathering};
    std::uint64_t tick_{};
    double elapsedSeconds_{};
    MapDefinition map_;
    Vec3 playerPosition_{0.0, 1.7, 6.0};
    double verticalVelocity_{};
    double playerYaw_{};
    double playerPitch_{};
    bool playerGrounded_{true};
    double playerHealth_{100.0};
    int wood_{};
    int stone_{};
    int gold_{};
    bool unlimitedResources_{};
    bool playerInvulnerable_{};
    std::uint64_t debugSpawnSequence_{};
    double pickaxeCooldownRemaining_{};
    std::optional<EntityId> aimedResource_;
    ResourceSystem resources_;
    std::optional<BuildingType> selectedBuilding_;
    std::uint8_t buildingRotation_{};
    std::optional<BuildingPreview> buildingPreview_;
    BuildingSystem buildings_;
    CollisionWorld collisionWorld_;
    FlowField flowField_;
    std::vector<FlowDebugVector> flowDebugVectors_;
    std::optional<EntityId> aimedEnemy_;
    std::optional<EntityId> aimedBuilding_;
    EnemySystem enemies_;
    TowerSystem towers_;
    CannonSystem cannons_;
    TrapSystem traps_;
    PlayerWeaponSystem playerWeapons_;
    BombSystem bombs_;
    GoldMineSystem goldMines_;
    WaveDirector waveDirector_;
    EconomyBalanceDefinition economy_;
    GameplayBalanceDefinition gameplay_;
    double phaseTimeRemaining_{};
    double phaseDuration_{};
    int wave_{};
    std::vector<EnemySpawn> waveSpawnQueue_;
    std::size_t nextWaveSpawnIndex_{};
    int waveSpawnGroupSize_{1};
    double waveSpawnInterval_{1.0};
    double waveSpawnTimeRemaining_{};
    std::optional<AttackDirection> upcomingAttackDirection_;
    std::vector<GameEvent> events_;
};

} // namespace ian
