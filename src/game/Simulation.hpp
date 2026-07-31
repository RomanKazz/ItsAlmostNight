#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingStats.hpp"
#include "buildings/FoundationSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/BombSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "core/Types.hpp"
#include "economy/GoldMineSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameEvent.hpp"
#include "game/GameBalance.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/CollisionWorld.hpp"
#include "world/MapDefinition.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldConfig.hpp"
#include "waves/WaveDirector.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ian {

inline constexpr double ResourcePickupFlightSeconds = 0.78;

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
struct DamagePlayerCommand {
    double amount{25.0};
};
struct SpawnEnemyCommand {
    EnemyType type{EnemyType::Basic};
    int count{1};
};
struct ToggleGateCommand {
    EntityId gateId;
};
struct RemoveModularBuildingCommand {
    EntityId buildingId;
};

struct PlayerCommand {
    double moveForward{};
    double moveRight{};
    double lookYaw{};
    double lookPitch{};
    bool overrideAimedBuilding{};
    std::optional<EntityId> aimedBuildingOverride;
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
    std::optional<DamagePlayerCommand> damagePlayer;
    std::optional<SpawnEnemyCommand> spawnEnemy;
    std::optional<ToggleGateCommand> toggleGate;
    std::optional<RemoveModularBuildingCommand>
        removeModularBuilding;
};

enum class RunState {
    MainMenu,
    Gathering,
    BuildPhase,
    Sunset,
    Wave,
    WaveComplete,
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
    bool playerRespawning;
    double playerRespawnTimeRemaining;
    double playerRespawnDuration;
    int deathLostWood;
    int deathLostStone;
    int deathLostGold;
    int wood;
    int stone;
    int gold;
    double pickaxeCooldownRemaining;
    std::optional<EntityId> aimedResource;
    std::span<const ResourceNode> resourceNodes;
    double worldLimit;
    double worldCellSize;
    std::uint32_t terrainSeed;
    std::span<const MapObstacle> mapObstacles;
    std::span<const CollisionBox> collisionBoxes;
    std::span<const FlowDebugVector> flowDebugVectors;
    std::optional<BuildingType> selectedBuilding;
    std::array<ResourceCost, GameBalance::BuildingTypeCount>
        buildingCosts;
    std::array<ResourceCost, ModularBuildPieceCount>
        modularBuildingCosts;
    std::optional<BuildingPreview> buildingPreview;
    std::span<const BuildingInstance> buildings;
    std::span<const PlatformFrameInstance> platformFrames;
    std::span<const SharedSupport> sharedSupports;
    std::span<const WallInstance> modularWalls;
    std::span<const RampInstance> ramps;
    std::optional<EntityId> aimedModularBuilding;
    std::optional<EntityId> aimedEnemy;
    std::optional<EntityId> aimedBuilding;
    std::optional<ResourceCost> aimedBuildingUpgradeCost;
    std::optional<BuildingStatComparison> aimedBuildingStats;
    std::span<const EnemyInstance> enemies;
    std::span<const TowerRuntime> towers;
    std::span<const CannonRuntime> cannons;
    std::span<const CannonProjectile> cannonProjectiles;
    std::span<const BombProjectile> bombProjectiles;
    std::size_t activeEnemyCount;
    std::size_t pendingEnemyCount;
    std::array<int, GameBalance::EnemyTypeCount>
        upcomingEnemyCounts;
    bool upcomingWaveHasBoss;
    std::optional<AttackDirection> upcomingAttackDirection;
    double phaseTimeRemaining;
    double phaseDuration;
    int wave;
    int bestWave;
    double coreHealth;
    double coreMaxHealth;
    std::optional<EntityId> coreId;
    std::uint8_t coreLevel;
    bool unlimitedResources;
    bool playerInvulnerable;
    PlayerWeapon selectedWeapon;
    double selectedWeaponDamage;
    int rifleLevel;
    int rifleAmmunition;
    int rifleMagazineSize;
    int rifleUpgradeGoldCost;
    bool rifleReloading;
    double rifleReloadRemaining;
    double rifleReloadDuration;
    int bombsRemaining;
    int waveCompletionReward;
    int tutorialWoodTarget;
    int tutorialStoneTarget;
    std::optional<TutorialObjective> tutorialObjective;
};

class Simulation {
  public:
    static constexpr std::size_t MaximumActiveEnemies =
        EnemySystem::MaxActiveEnemies;

    explicit Simulation(GameBalance balance = GameBalance::defaults(),
                        MapDefinition map = MapDefinition::defaults(),
                        WorldConfig worldConfig =
                            WorldConfig::defaults());

    void startRun();
    void restartRun();
    void togglePause();
    void tick(double deltaSeconds, const PlayerCommand& command = {});

    [[nodiscard]] SimulationSnapshot snapshot() const;
    [[nodiscard]] PlacementResult previewPlacement(
        BuildingType type, GridPosition position) const;
    [[nodiscard]] PlacementResult previewPlacement(
        BuildingType type, GridPosition position,
        double preferredHeight) const;
    [[nodiscard]] BuildingPlatformSurface
    previewPlacementSurface(
        BuildingType type, GridPosition position) const;
    [[nodiscard]] BuildingPlatformSurface
    previewPlacementSurface(
        BuildingType type, GridPosition position,
        double preferredHeight) const;
    [[nodiscard]] const TerrainHeightfield& terrain() const;
    void regenerateTerrain(std::uint32_t seed);
    [[nodiscard]] PlatformFramePlacement
    previewFoundation(Vec3 terrainHit) const;
    [[nodiscard]] std::optional<PlatformFrameInstance>
    placeFoundation(Vec3 terrainHit);
    [[nodiscard]] PlatformFramePlacement
    previewFloorPlatform(
        GridCoord anchor, int storey,
        double floorHeight) const;
    [[nodiscard]] std::optional<PlatformFrameInstance>
    placeFloorPlatform(
        GridCoord anchor, int storey,
        double floorHeight);
    [[nodiscard]] WallPlacement previewWall(
        Vec3 terrainHit, Rotation rotation) const;
    [[nodiscard]] std::optional<WallInstance>
    placeWall(Vec3 terrainHit, Rotation rotation);
    [[nodiscard]] RampPlacement previewRamp(
        Vec3 terrainHit, Rotation rotation) const;
    [[nodiscard]] std::optional<RampInstance>
    placeRamp(Vec3 terrainHit, Rotation rotation);
    void setStructuralCollapseEnabled(bool enabled);
    [[nodiscard]] bool structuralCollapseEnabled() const;
    [[nodiscard]] std::size_t clearModularBuildings();
    std::vector<GameEvent> takeEvents();

  private:
    void resetRun(GameEventType eventType);
    void updatePlayer(double deltaSeconds,
                      const PlayerCommand& command);
    void processDebugCommands(const PlayerCommand& command);
    void processBuildingCommands(const PlayerCommand& command);
    void updatePlayerActions(double deltaSeconds,
                             const PlayerCommand& command);
    void updatePendingResourceGrants(double deltaSeconds);
    void updateRunPhase(double deltaSeconds,
                        const PlayerCommand& command);
    void updateCombat(double deltaSeconds);
    void updateTrapCombat(double deltaSeconds);
    void updateTowerCombat(double deltaSeconds);
    void updateCannonCombat(double deltaSeconds);
    [[nodiscard]] PlacementResult validatePlacement(BuildingType type,
                                                    GridPosition position) const;
    [[nodiscard]] PlacementResult validatePlacement(
        BuildingType type, GridPosition position,
        const BuildingPlatformSurface& surface) const;
    [[nodiscard]] BuildingPlatformSurface
    placementSurface(BuildingType type,
                     GridPosition position) const;
    [[nodiscard]] BuildingPlatformSurface
    placementSurfaceWithPreferredHeight(
        BuildingType type, GridPosition position,
        double preferredHeight) const;
    [[nodiscard]] std::optional<
        PlatformFramePlacement>
    automaticFoundationPlacement(
        BuildingType type, GridPosition position,
        double floorHeight) const;
    void raisePlayerOntoGroundFrame(
        const PlatformFrameInstance& frame);
    [[nodiscard]] bool shouldAutoJumpGroundFrame(
        Vec3 movement) const;
    [[nodiscard]] bool
    modularRemovalWouldDestroyCore(
        EntityId id) const;
    void syncWorldStructures();
    void syncModularStructures();
    void removeUnsupportedPlatformBuildings();
    void damagePlayer(
        double damage, std::optional<EntityId> attackerId,
        Vec3 attackPosition);
    void beginPlayerRespawn(
        std::optional<EntityId> attackerId);
    void updatePlayerRespawn(double deltaSeconds);
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
    WorldConfig worldConfig_;
    TerrainHeightfield terrain_;
    FoundationSystem foundations_;
    Vec3 playerPosition_{0.0, 1.7, 6.0};
    double verticalVelocity_{};
    double playerYaw_{};
    double playerPitch_{};
    bool playerGrounded_{true};
    double playerHealth_{100.0};
    bool playerRespawning_{};
    double playerRespawnTimeRemaining_{};
    int deathLostWood_{};
    int deathLostStone_{};
    int deathLostGold_{};
    int wood_{};
    int stone_{};
    int gold_{};
    std::array<ResourceCost, ModularBuildPieceCount>
        modularBuildingCosts_;
    struct PendingResourceGrant {
        ResourceType type;
        Vec3 position;
        int amount;
        double remaining;
    };
    std::vector<PendingResourceGrant> pendingResourceGrants_;
    bool unlimitedResources_{};
    bool playerInvulnerable_{};
    std::uint64_t debugSpawnSequence_{};
    std::uint64_t pickaxeAttackSequence_{};
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
    std::optional<EntityId> aimedModularBuilding_;
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
    int bestWave_{};
    std::vector<EnemySpawn> waveSpawnQueue_;
    std::size_t nextWaveSpawnIndex_{};
    int waveSpawnGroupSize_{1};
    double waveSpawnInterval_{1.0};
    double waveSpawnTimeRemaining_{};
    std::optional<AttackDirection> upcomingAttackDirection_;
    std::vector<GameEvent> events_;
};

} // namespace ian
