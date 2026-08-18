#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingStats.hpp"
#include "buildings/FoundationSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/BombSystem.hpp"
#include "combat/IceWandSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "core/Types.hpp"
#include "economy/CrystalMineSystem.hpp"
#include "economy/CoinPickupSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameEvent.hpp"
#include "game/GameBalance.hpp"
#include "game/LootChestSystem.hpp"
#include "resources/ResourceSystem.hpp"
#include "progression/SkillTree.hpp"
#include "progression/InsightSystem.hpp"
#include "progression/ObjectiveSystem.hpp"
#include "world/CollisionWorld.hpp"
#include "world/MapDefinition.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldConfig.hpp"
#include "waves/WaveDirector.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_set>
#include <vector>

namespace ian {

inline constexpr double ResourcePickupFlightSeconds = 0.78;

struct StartWaveEarlyCommand {};
struct EnableUnlimitedResourcesCommand {};
struct ToggleWeaponCommand {};
struct SelectWeaponCommand {
    PlayerWeapon weapon{PlayerWeapon::BareHands};
};
struct UpgradeWeaponCommand {};
struct UseConsumableCommand {};
struct InteractCommand {};
struct RerollChestCommand { EntityId chestId; };
struct RepairAllBuildingsCommand {};
struct PurchaseBombBundleCommand {};
struct RevealNearestChestCommand {};
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
    EliteAffixMask eliteAffixes{};
};
struct CastChainLightningCommand {
    std::optional<EntityId> firstTarget;
    std::optional<EntityId> excludedTarget;
    std::optional<Vec3> sourcePosition;
    double damage{28.0};
    double jumpRadius{6.5};
    double damageFalloff{0.82};
    int maximumTargets{6};
};
struct ToggleGateCommand {
    EntityId gateId;
};
struct RotatePlacedBuildingCommand {
    EntityId buildingId;
    int steps{1};
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
    bool overrideAimedResource{};
    std::optional<EntityId> aimedResourceOverride;
    bool overrideAimedModularBuilding{};
    std::optional<EntityId> aimedModularBuildingOverride;
    bool overrideAimedWorldLandmark{};
    std::optional<EntityId> aimedWorldLandmarkOverride;
    bool jump{};
    bool sprint{};
    bool dash{};
    bool usePickaxe{};
    bool fireRifle{};
    bool fireIceWand{};
    bool fireFireWand{};
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
    std::optional<SelectWeaponCommand> selectWeapon;
    std::optional<UpgradeWeaponCommand> upgradeWeapon;
    std::optional<UseConsumableCommand> useConsumable;
    std::optional<InteractCommand> interact;
    std::optional<RerollChestCommand> rerollChest;
    std::optional<RepairAllBuildingsCommand> repairAllBuildings;
    std::optional<PurchaseBombBundleCommand> purchaseBombBundle;
    std::optional<RevealNearestChestCommand> revealNearestChest;
    std::optional<DefeatAllEnemiesCommand> defeatAllEnemies;
    std::optional<ToggleInvulnerabilityCommand> toggleInvulnerability;
    std::optional<DamageCoreCommand> damageCore;
    std::optional<DamagePlayerCommand> damagePlayer;
    std::optional<SpawnEnemyCommand> spawnEnemy;
    std::optional<CastChainLightningCommand> castChainLightning;
    std::optional<ToggleGateCommand> toggleGate;
    std::optional<RotatePlacedBuildingCommand>
        rotatePlacedBuilding;
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

enum class ChallengeColumnState {
    Dormant,
    Active,
    Completed,
};

struct ChallengeColumnInstance {
    EntityId id;
    Vec3 position;
    double yaw{};
    ChallengeColumnState state{ChallengeColumnState::Dormant};
    double completionProgress{};
    double fenceProgress{};
    int enemyBudget{};
};

enum class WorldLandmarkType {
    Mine,
    LumberMill,
};

struct WorldLandmarkInstance {
    EntityId id;
    WorldLandmarkType type{WorldLandmarkType::Mine};
    Vec3 position;
    double yaw{};
    double collisionRadius{};
    int activationCoinCost{};
    bool activated{};
    double productionProgress{};
};

enum class TutorialObjective {
    BareHandsTraining,
    MineWood,
    PlaceCore,
    MineStone,
    BuildCrystalMine,
    PrepareForNight,
    SurviveFirstWave,
};

enum class SkillPointSource { IntroObjective, Wave, Boss, Event, Elite };

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
    Vec3 playerHorizontalVelocity;
    bool dashUnlocked{};
    bool dashing{};
    double dashCooldownRemaining{};
    double dashCooldownDuration{};
    double playerVerticalVelocity;
    double playerHealth;
    double playerMaxHealth;
    bool playerRespawning;
    double playerRespawnTimeRemaining;
    double playerRespawnDuration;
    int deathLostWood;
    int deathLostStone;
    int deathLostCrystals;
    int wood;
    int stone;
    int crystals;
    int woodCapacity;
    int stoneCapacity;
    int crystalCapacity;
    bool crystalStorageFull;
    int coins;
    std::span<const CoinPickup> coinPickups;
    std::optional<EntityId> aimedChest;
    std::optional<EntityId> aimedLoot;
    std::span<const LootChestInstance> lootChests;
    std::optional<EntityId> aimedChallengeColumn;
    std::span<const ChallengeColumnInstance> challengeColumns;
    std::optional<EntityId> aimedWorldLandmark;
    std::span<const WorldLandmarkInstance> worldLandmarks;
    std::optional<Vec3> activeChallengeCenter;
    double activeChallengeRadius{};
    std::optional<Vec3> nearestChestPosition;
    double nearestChestDistance{};
    std::array<int, LootUpgradeEffectCount> lootStacks;
    double playerDamageMultiplier;
    double playerMoveSpeedMultiplier;
    double playerArmorMultiplier;
    double playerTemporaryHealth;
    double playerRecoverableArmor;
    double playerMaxRecoverableArmor;
    double playerArmorRechargeDelayRemaining;
    bool battlePotionAvailable{};
    double battlePotionBerserkRemaining{};
    double battlePotionBerserkDuration{};
    double chestOpeningCostMultiplier;
    bool freeChestOpeningAvailable{};
    int chestOpeningCostSurcharge;
    double pickaxeCooldownRemaining;
    std::optional<EntityId> aimedResource;
    double aimedResourceEfficiency{1.0};
    std::span<const ResourceNode> resourceNodes;
    double worldLimit;
    double worldCellSize;
    std::uint32_t terrainSeed;
    int terrainResolution;
    double terrainWorldSize;
    std::span<const float> terrainSamples;
    std::span<const PondDefinition> ponds;
    std::span<const MapObstacle> mapObstacles;
    std::span<const CollisionBox> collisionBoxes;
    std::span<const FlowDebugVector> flowDebugVectors;
    std::optional<BuildingType> selectedBuilding;
    std::array<ResourceCost, GameBalance::BuildingTypeCount>
        buildingCosts;
    std::array<bool, GameBalance::BuildingTypeCount>
        unlockedBuildings;
    std::array<ResourceCost, ModularBuildPieceCount>
        modularBuildingCosts;
    std::optional<BuildingPreview> buildingPreview;
    std::span<const BuildingInstance> buildings;
    std::span<const PlatformFrameInstance> platformFrames;
    std::span<const SharedSupport> sharedSupports;
    std::span<const WallInstance> modularWalls;
    std::span<const RampInstance> ramps;
    std::optional<EntityId> aimedModularBuilding;
    std::optional<EntityId> aimedModularBuildingCandidate;
    std::optional<EntityId> aimedEnemy;
    std::optional<EntityId> aimedBuilding;
    std::optional<ResourceCost> aimedBuildingUpgradeCost;
    std::optional<BuildingStatComparison> aimedBuildingStats;
    std::span<const EnemyInstance> enemies;
    std::span<const EnemyProjectile> enemyProjectiles;
    std::span<const TowerRuntime> towers;
    std::span<const CannonRuntime> cannons;
    std::span<const TrapRuntime> traps;
    std::span<const CannonProjectile> cannonProjectiles;
    std::span<const BombProjectile> bombProjectiles;
    std::span<const IceWandProjectile> iceWandProjectiles;
    double iceWandChargeRemaining{};
    double iceWandChargeDuration{};
    double iceWandCooldownRemaining{};
    std::span<const IceWandProjectile> fireWandProjectiles;
    double fireWandChargeRemaining{};
    double fireWandChargeDuration{};
    double fireWandCooldownRemaining{};
    std::size_t activeEnemyCount;
    std::size_t pendingEnemyCount;
    std::array<int, GameBalance::EnemyTypeCount>
        upcomingEnemyCounts;
    bool upcomingWaveHasBoss;
    std::optional<AttackDirection> upcomingAttackDirection;
    double phaseTimeRemaining;
    double phaseDuration;
    int earlyWaveBonus;
    int earlyWaveCoinBonus;
    int earlyWaveInsightBonus;
    int wave;
    int bestWave;
    double coreHealth;
    double coreMaxHealth;
    std::optional<EntityId> coreId;
    std::uint8_t coreLevel;
    bool unlimitedResources;
    bool playerInvulnerable;
    bool automaticToolSwitch;
    bool holdToGather;
    std::array<bool, PlayerWeaponCount> unlockedWeapons;
    PlayerWeapon selectedWeapon;
    double selectedWeaponDamage;
    int rifleLevel;
    int rifleAmmunition;
    int rifleMagazineSize;
    int rifleUpgradeCrystalCost;
    bool rifleReloading;
    double rifleReloadRemaining;
    double rifleReloadDuration;
    int bombsRemaining;
    int bombPurchaseCoinCost;
    int bombPurchaseAmount;
    int chestRerollCoinCost;
    int repairAllCoinCost;
    int chestRevealCoinCost;
    int waveCompletionReward;
    int tutorialWoodTarget;
    int tutorialStoneTarget;
    std::optional<TutorialObjective> tutorialObjective;
    int skillPoints;
    double currentInsight;
    double requiredInsight;
    double totalInsightEarned;
    int totalTreePointsEarned;
    std::span<const ObjectiveStatus> objectives;
    std::array<int, 3> recommendedObjectives;
    int bareHandsWoodGathered;
    int bareHandsStoneGathered;
    bool introSkillObjectiveCompleted;
};

struct ProgressionRunState {
    SkillTreeRunState skillTree;
    InsightRunState insight;
    ObjectiveRunState objectives;
};

class Simulation {
  public:
    static constexpr std::size_t MaximumActiveEnemies =
        EnemySystem::MaxActiveEnemies;

    explicit Simulation(GameBalance balance = GameBalance::defaults(),
                        MapDefinition map = MapDefinition::defaults(),
                        WorldConfig worldConfig =
                            WorldConfig::defaults(),
                        std::vector<SkillNodeDefinition> skills =
                            SkillTree::defaultDefinitions(),
                        InsightConfig insightConfig =
                            InsightConfig::defaults(),
                        std::vector<ObjectiveDefinition> objectives =
                            ObjectiveSystem::defaultDefinitions());

    void startRun();
    void restartRun();
    void returnToMainMenu();
    void togglePause();
    void tick(double deltaSeconds, const PlayerCommand& command = {});

    // Reference remains valid until Simulation is mutated.
    [[nodiscard]] const SimulationSnapshot& snapshot() const;
    [[nodiscard]] const EnemyPerformanceStats&
    enemyPerformanceStats() const;
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
    [[nodiscard]] PlatformFramePlacement
    previewFoundationAtHeight(
        Vec3 terrainHit, double floorHeight) const;
    [[nodiscard]] std::optional<PlatformFrameInstance>
    placeFoundation(Vec3 terrainHit);
    [[nodiscard]] std::optional<PlatformFrameInstance>
    placeFoundationAtHeight(
        Vec3 terrainHit, double floorHeight);
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
    void beginModularPlacementBatch();
    void endModularPlacementBatch();
    void setStructuralCollapseEnabled(bool enabled);
    [[nodiscard]] bool structuralCollapseEnabled() const;
    [[nodiscard]] std::vector<EntityId> structuralCollapseRisk(
        std::span<const EntityId> supports) const;
    [[nodiscard]] std::uint64_t structuralRevision() const;
    [[nodiscard]] std::size_t clearModularBuildings();
    std::vector<GameEvent> takeEvents();
    [[nodiscard]] const SkillTree& skillTree() const;
    [[nodiscard]] SkillPurchaseError purchaseSkill(std::size_t index);
    void grantSkillPoints(int amount, SkillPointSource source);
    void grantLootUpgrade(
        LootUpgradeEffect effect,
        LootRarity rarity = LootRarity::Common);
    [[nodiscard]] SkillTreeRunState saveSkillTreeState() const;
    [[nodiscard]] bool loadSkillTreeState(const SkillTreeRunState& state);
    [[nodiscard]] ProgressionRunState saveProgressionState() const;
    [[nodiscard]] bool loadProgressionState(const ProgressionRunState& state);
    [[nodiscard]] const InsightSystem& insightSystem() const;
    [[nodiscard]] const ObjectiveSystem& objectiveSystem() const;

  private:
    [[nodiscard]] static Vec3 lookDirection(double yaw,
                                            double pitch);
    void resetRun(GameEventType eventType);
    void updatePlayer(double deltaSeconds,
                      const PlayerCommand& command);
    void processDebugCommands(const PlayerCommand& command);
    void castChainLightning(
        const CastChainLightningCommand& command);
    void registerNailHit(
        EntityId primaryTarget, Vec3 impactPosition,
        double directHitDamage);
    void processBuildingCommands(const PlayerCommand& command);
    void processBuildingActions(const PlayerCommand& command);
    void updatePlayerActions(double deltaSeconds,
                             const PlayerCommand& command);
    void updatePendingResourceGrants(double deltaSeconds);
    void launchSawSplinters(EntityId sourceId, Vec3 origin,
                            int chainDepth = 0);
    void updateSawSplinters(double deltaSeconds);
    void updateRunPhase(double deltaSeconds,
                        const PlayerCommand& command);
    [[nodiscard]] int earlyWaveBonus() const;
    [[nodiscard]] int earlyWaveCoinBonus() const;
    [[nodiscard]] int earlyWaveInsightBonus() const;
    void updateCombat(double deltaSeconds);
    void updateEliteEffects(double deltaSeconds);
    void collectEliteEnemyEvents();
    void updateTrapCombat(double deltaSeconds);
    void updateTowerCombat(double deltaSeconds);
    void updateCannonCombat(double deltaSeconds);
    [[nodiscard]] PlacementResult validatePlacement(BuildingType type,
                                                    GridPosition position) const;
    [[nodiscard]] PlacementResult validatePlacement(
        BuildingType type, GridPosition position,
        const BuildingPlatformSurface& surface) const;
    [[nodiscard]] PlacementResult
    previewPlacementWithOptionalHeight(
        BuildingType type, GridPosition position,
        std::optional<double> preferredHeight) const;
    [[nodiscard]] bool buildingUnlocked(BuildingType type) const;
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
        std::optional<double> preferredHeight =
            std::nullopt) const;
    [[nodiscard]] bool foundationAddsPlacementCost(
        const PlatformFramePlacement& placement) const;
    [[nodiscard]] bool rectangleHasDeepWater(
        double minimumX, double maximumX,
        double minimumZ, double maximumZ) const;
    void raisePlayerOntoGroundFrame(
        const PlatformFrameInstance& frame);
    [[nodiscard]] bool shouldAutoJumpGroundFrame(
        Vec3 movement) const;
    [[nodiscard]] bool
    modularRemovalWouldDestroyCore(
        EntityId id) const;
    void syncBuildingRuntimeSystems();
    void syncWorldStructures();
    void syncModularStructures();
    void removeUnsupportedPlatformBuildings();
    void damagePlayer(
        double damage, std::optional<EntityId> attackerId,
        Vec3 attackPosition, bool ignoreArmor = false);
    void applyFallDamage(double landingSpeed);
    void beginPlayerRespawn(
        std::optional<EntityId> attackerId);
    void updatePlayerRespawn(double deltaSeconds);
    void respawnPlayer();
    void prepareWave(const WavePlan& plan, GridPosition corePosition,
                     std::size_t firstAnchorIndex);
    void beginPreparedWave();
    void tickWaveSpawning(double deltaSeconds);
    void completeWave();
    void cycleUnlockedTool();
    void updateFortifications(double deltaSeconds);
    void applyLootPickup(const LootPickup& pickup);
    void applyPotionWaveStart();
    void updateLootEffects(double deltaSeconds,
                           std::size_t firstGameplayEvent);
    void resetChallengeColumns();
    void resetWorldLandmarks();
    void syncWorldLandmarkColliders();
    void updateWorldLandmarks(
        double deltaSeconds, const PlayerCommand& command);
    void updateChallengeColumns(
        double deltaSeconds, const PlayerCommand& command);
    void activateChallengeColumn(EntityId id);
    void failActiveChallenge();
    void constrainPlayerToChallengeArena();
    [[nodiscard]] bool challengeActive() const;
    void updateCoinPickups(double deltaSeconds);
    [[nodiscard]] double resourceToolEfficiency(
        PlayerWeapon tool, ResourceType resource) const;
    [[nodiscard]] int resourceCapacity(
        BuildingType storageType) const;
    void clampResourcesToCapacity();
    void addWood(int amount);
    void addStone(int amount);
    void addCrystals(int amount);
    [[nodiscard]] bool hasStorageSpace(
        ResourceType resource) const;
    [[nodiscard]] double playerPermanentMaxHealth() const;
    [[nodiscard]] bool isFortified(EntityId id) const;
    [[nodiscard]] double repairCooldownRemaining(EntityId id) const;
    void startRepairCooldown(EntityId id);
    [[nodiscard]] std::optional<TutorialObjective> tutorialObjective() const;
    void invalidateSnapshotCache();
    [[nodiscard]] bool resourceGroundPositionIsSafe(
        double x, double z, double radius) const;
    void processInsightEvents(std::size_t firstEvent, bool suppressEnemyRewards);
    void processInsightEvent(const GameEvent& event);
    void processObjectiveEvents(std::size_t firstEvent);
    void grantConfiguredInsight(double amount, InsightSource source,
                                InsightCategory category,
                                const InsightGrantContext& context);
    void grantBlueprintInsightForType(
        BuildingType type, int blueprintStackOrdinal);
    void grantBlueprintInsightForExistingBuildings(
        int blueprintStackOrdinal);
    void refreshSkillRuntimeEffects();
    [[nodiscard]] std::uint32_t nextRunTerrainSeed();

    RunState state_{RunState::MainMenu};
    RunState stateBeforePause_{RunState::Gathering};
    std::uint64_t tick_{};
    double elapsedSeconds_{};
    MapDefinition map_;
    WorldConfig worldConfig_;
    TerrainHeightfield terrain_;
    std::uint64_t runSeedState_{};
    FoundationSystem foundations_;
    int modularPlacementBatchDepth_{};
    bool modularStructuresDirty_{};
    GlbCollisionAsset platformCollisionAsset_;
    GlbCollisionAsset rampCollisionAsset_;
    std::array<GlbCollisionAsset, TreeVisualVariantCount>
        treeCollisionAssets_;
    std::array<GlbCollisionAsset, StoneVisualVariantCount>
        stoneCollisionAssets_;
    Vec3 playerPosition_{0.0, 1.7, 6.0};
    Vec3 playerHorizontalVelocity_{};
    double verticalVelocity_{};
    double coyoteTimeRemaining_{};
    double jumpBufferRemaining_{};
    double dashRemaining_{};
    double dashCooldownRemaining_{};
    Vec3 dashDirection_{};
    double autoJumpAssistRemaining_{};
    Vec3 autoJumpAssistDirection_{};
    double edgeSupportGraceRemaining_{};
    double lastGroundSurfaceHeight_{};
    double playerYaw_{};
    double playerPitch_{};
    bool playerGrounded_{true};
    double playerHealth_{100.0};
    bool playerRespawning_{};
    double playerRespawnTimeRemaining_{};
    int deathLostWood_{};
    int deathLostStone_{};
    int deathLostCrystals_{};
    int wood_{};
    int stone_{};
    int crystals_{};
    bool crystalStorageFullNotified_{};
    int coins_{};
    CoinPickupSystem coinPickups_;
    std::unordered_set<std::uint64_t> rewardedEnemyCoins_;
    std::array<ResourceCost, ModularBuildPieceCount>
        modularBuildingCosts_;
    struct PendingResourceGrant {
        ResourceType type;
        Vec3 position;
        int amount;
        double remaining;
    };
    std::vector<PendingResourceGrant> pendingResourceGrants_;
    struct PendingSawSplinter {
        EntityId sourceId;
        EntityId targetId;
        Vec3 origin;
        Vec3 targetPosition;
        double damage{};
        double remaining{};
        int chainDepth{};
    };
    std::vector<PendingSawSplinter> pendingSawSplinters_;
    bool unlimitedResources_{};
    bool playerInvulnerable_{};
    std::uint64_t debugSpawnSequence_{};
    std::uint64_t pickaxeAttackSequence_{};
    std::uint64_t powerSwingResourceHits_{};
    std::uint64_t nailHitCounter_{};
    double pickaxeCooldownRemaining_{};
    double pickaxeInputBufferRemaining_{};
    std::optional<EntityId> aimedResource_;
    ResourceSystem resources_;
    std::optional<EntityId> aimedChest_;
    std::optional<EntityId> aimedLoot_;
    LootChestSystem lootChests_;
    std::vector<ChallengeColumnInstance> challengeColumns_;
    std::vector<WorldLandmarkInstance> worldLandmarks_;
    std::optional<EntityId> aimedWorldLandmark_;
    std::optional<EntityId> aimedChallengeColumn_;
    std::optional<std::size_t> activeChallengeColumn_;
    std::uint32_t challengeRunGeneration_{};
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
    SkillTree skillTree_;
    InsightSystem insight_;
    ObjectiveSystem objectives_;
    std::unordered_set<std::uint64_t> insightRewardedEnemyIds_;
    BombSystem bombs_;
    IceWandSystem iceWand_;
    IceWandSystem fireWand_;
    CrystalMineSystem crystalMines_;
    WaveDirector waveDirector_;
    EconomyBalanceDefinition economy_;
    GameplayBalanceDefinition gameplay_;
    ClubBalanceDefinition club_;
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
    bool currentWaveHasBoss_{};
    double playerDamageMultiplier_{1.0};
    double playerMoveSpeedMultiplier_{1.0};
    double playerArmorMultiplier_{1.0};
    double playerMaxHealthMultiplier_{1.0};
    double buildingMaxHealthMultiplier_{1.0};
    double productionSpeedMultiplier_{1.0};
    double woodYieldMultiplier_{1.0};
    double chestOpeningCostMultiplier_{1.0};
    double playerBonusMaxHealth_{};
    double playerTemporaryHealth_{};
    double playerRecoverableArmor_{};
    double playerMaxRecoverableArmor_{};
    double secondsSincePlayerDamage_{};
    bool battlePotionAvailable_{};
    double battlePotionBerserkRemaining_{};
    double battlePotionBerserkDuration_{};
    double battlePotionLifestealRemaining_{};
    bool freeChestOpeningAvailable_{};
    std::array<int, LootUpgradeEffectCount> lootStacks_{};
    int bareHandsWoodGathered_{};
    int bareHandsStoneGathered_{};
    bool introSkillObjectiveCompleted_{};
    struct ActiveFortification { EntityId id; double remaining; };
    std::vector<ActiveFortification> activeFortifications_;
    struct ActiveRepairCooldown { EntityId id; double remaining; };
    std::vector<ActiveRepairCooldown> activeRepairCooldowns_;
    std::vector<EnemyStructureTarget> modularTargetBuffer_;
    struct PendingEliteExplosion {
        EntityId sourceId;
        Vec3 position;
        double remaining{1.15};
    };
    std::vector<PendingEliteExplosion> pendingEliteExplosions_;
    std::vector<GameEvent> events_;
    mutable std::optional<SimulationSnapshot> snapshotCache_;
    std::uint64_t structuralRevision_{};
};

} // namespace ian
