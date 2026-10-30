#pragma once

#include "progression/ContentUnlocks.hpp"

#include "buildings/BuildingSystem.hpp"
#include "buildings/RepairStation.hpp"
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
#include "economy/PowerupSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameEvent.hpp"
#include "game/GameBalance.hpp"
#include "game/PlayerClass.hpp"
#include "game/PlayerClassProgression.hpp"
#include "game/LootChestSystem.hpp"
#include "game/ChestItemCombatSystem.hpp"
#include "game/RelicState.hpp"
#include <unordered_map>
#include "resources/ResourceSystem.hpp"
#include "progression/InsightSystem.hpp"
#include "progression/ObjectiveSystem.hpp"
#include "progression/RunUpgrade.hpp"
#include "progression/BuildingStatUpgrades.hpp"
#include "progression/RunShop.hpp"
#include "progression/Shrine.hpp"
#include "progression/MetaShop.hpp"
#include "world/CollisionWorld.hpp"
#include "world/MapDefinition.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldConfig.hpp"
#include "world/WorldScenery.hpp"
#include "waves/WaveDirector.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ian {

inline constexpr double ResourcePickupFlightSeconds = 0.78;
inline constexpr std::size_t MaximumAttackPortals = 8U;

struct FloorPlatformFoundationPlacement {
    PlatformFramePlacement foundation;
    PlatformFramePlacement floor;

    [[nodiscard]] bool valid() const {
        return foundation.valid() && floor.valid();
    }
};

struct FloorPlatformFoundationBuild {
    PlatformFrameInstance foundation;
    PlatformFrameInstance floor;
};

struct RampFoundationPlacement {
    std::array<PlatformFramePlacement, 2> foundations;
    RampPlacement ramp;

    [[nodiscard]] bool valid() const {
        return foundations[0].valid() &&
               foundations[1].valid() && ramp.valid();
    }
};

struct RampFoundationBuild {
    std::array<PlatformFrameInstance, 2> foundations;
    RampInstance ramp;
};

struct StartWaveEarlyCommand {};
struct EnableUnlimitedResourcesCommand {};
struct ToggleWeaponCommand {};
struct SelectWeaponCommand {
    PlayerWeapon weapon{PlayerWeapon::Axe};
};
struct UpgradeWeaponCommand {};
struct UseConsumableCommand {};
struct InteractCommand {};
struct RevealNearestChestCommand {};
struct DefeatAllEnemiesCommand {};
struct ToggleInvulnerabilityCommand {};
struct GrantInsightCommand {
    double amount{100.0};
};
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
struct CollectProducerCommand {
    BuildingType type;
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
    std::optional<Vec3> aimedResourceHitPositionOverride;
    bool overrideAimedModularBuilding{};
    std::optional<EntityId> aimedModularBuildingOverride;
    bool overrideAimedWorldLandmark{};
    std::optional<EntityId> aimedWorldLandmarkOverride;
    bool jump{};
    bool jumpHeld{};
    bool sprint{}; // Stable legacy input field: descend during sandbox flight only.
    bool usePickaxe{};
    bool fireRifle{};
    bool fireIceWand{};
    bool fireFireWand{};
    std::optional<BuildingType> selectBuilding;
    bool cancelBuilding{};
    std::optional<PlaceBuildingCommand> placeBuilding;
    std::vector<PlaceBuildingCommand> placeBuildingBatch;
    int rotateBuilding{};
    std::optional<std::uint8_t> setBuildingRotation;
    std::optional<StartWaveEarlyCommand> startWaveEarly;
    std::optional<EnableUnlimitedResourcesCommand> enableUnlimitedResources;
    std::optional<UpgradeBuildingCommand> upgradeBuilding;
    std::optional<CollectProducerCommand> collectProducer;
    std::optional<RepairBuildingCommand> repairBuilding;
    std::optional<SellBuildingCommand> sellBuilding;
    std::optional<ToggleWeaponCommand> toggleWeapon;
    std::optional<SelectWeaponCommand> selectWeapon;
    std::optional<UpgradeWeaponCommand> upgradeWeapon;
    std::optional<UseConsumableCommand> useConsumable;
    std::optional<InteractCommand> interact;
    std::optional<RevealNearestChestCommand> revealNearestChest;
    std::optional<DefeatAllEnemiesCommand> defeatAllEnemies;
    std::optional<ToggleInvulnerabilityCommand> toggleInvulnerability;
    std::optional<GrantInsightCommand> grantInsight;
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

struct DefenseCardStatus {
    EntityId buildingId;
    UpgradeError upgradeError{UpgradeError::Unsupported};
    ResourceCost upgradeCost;
    BuildingStatComparison stats;
};

struct ProducerCardStatus {
    EntityId buildingId;
    BuildingStatComparison stats;
    ResourceCost upgradeCost;
    UpgradeError upgradeError{UpgradeError::Unsupported};
};

enum class RunState {
    MainMenu,
    Gathering,
    BuildPhase,
    Sunset,
    Wave,
    WaveComplete,
    StageClear,
    Victory,
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
    PlayerShrine,
    BuildingShrine,
};

[[nodiscard]] constexpr ShrineKind landmarkShrineKind(WorldLandmarkType type) {
    return type == WorldLandmarkType::PlayerShrine ? ShrineKind::Player
        : type == WorldLandmarkType::BuildingShrine ? ShrineKind::Buildings : ShrineKind::None;
}

struct WorldLandmarkInstance {
    EntityId id;
    WorldLandmarkType type{WorldLandmarkType::Mine};
    Vec3 position;
    double yaw{};
    double collisionRadius{};
    int activationCoinCost{};
    bool activated{};
    double productionProgress{};
    int level{1};
    int upgradeCoinCost{};
    double incomePerMinute{};
    double nextIncomePerMinute{};
};

enum class TutorialObjective {
    MineWood,
    PlaceCore,
    PrepareForNight,
    SurviveFirstWave,
    OpenBuildMode,
};

enum class AttackDirection {
    North,
    East,
    South,
    West,
};

struct RunCombatStatistics {
    int wavesSurvived{};
    int enemiesDefeated{};
    double playerDamageDealt{};
    double defenseDamageDealt{};
    int woodAcquired{};
    int stoneAcquired{};
    int crystalsAcquired{};
    int coinsCollected{};
    int structuresBuilt{};
    int structuresLost{};
};

struct RunUpgradeStats {
    double defenseDamage{1.0};
    double defenseFireRate{1.0};
    double buildingHealth{1.0};
    double productionSpeed{1.0};
    double experienceGain{1.0};
    double resourceYield{1.0};
    double resourceCapacity{1.0};
    double playerAttackSpeed{1.0};
    double toolAttackSpeed{1.0};
    double luck{};
    double healthRegeneration{};
    double projectileCountBonus{};
    double pierce{};
    double bounces{};
    double clusterExplosions{};
    double projectileSpeed{1.0};
    double attackDuration{1.0};
    double attackSize{1.0};
    double critChance{};
    double critDamage{2.0};
    double knockback{1.0};
    CannonAttackStyle cannonAttackStyle{CannonAttackStyle::Standard};
};

struct SimulationSnapshot {
    RunState state;
    RunState stateBeforePause{RunState::Gathering};
    bool sandboxMode{};
    PlayerClass playerClass{PlayerClass::None};
    std::uint64_t tick;
    double elapsedSeconds;
    Vec3 playerPosition;
    double playerYaw;
    double playerPitch;
    bool playerGrounded;
    Vec3 playerHorizontalVelocity;
    double playerVerticalVelocity;
    double playerHealth;
    double playerMaxHealth;
    bool playerRespawning;
    double playerRespawnTimeRemaining;
    double playerRespawnDuration;
    double playerRespawnProtectionRemaining{};
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
    PowerupRunState powerups;
    std::optional<EntityId> aimedChest;
    std::optional<EntityId> aimedLoot;
    std::span<const LootChestInstance> lootChests;
    std::span<const ItemPoisonCloud> itemPoisonClouds;
    std::span<const RelicGhost> relicGhosts;
    std::span<const BuildingInstance> relicCombatBuildings;
    std::optional<EntityId> aimedChallengeColumn;
    std::span<const ChallengeColumnInstance> challengeColumns;
    std::optional<EntityId> aimedWorldLandmark;
    std::span<const WorldLandmarkInstance> worldLandmarks;
    std::span<const WorldSceneryInstance> worldScenery;
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
    bool appleAvailable{};
    bool breadWellFed{};
    bool stageCleared{};
    bool finalNight{};
    bool overtimeActive{};
    double overtimeSeconds{};
    OvertimeDifficulty overtime{};
    double chestOpeningCostMultiplier;
    bool freeChestOpeningAvailable{};
    int freeChestRerollsRemaining{};
    int chestOpeningCostSurcharge;
    int openedPaidChestCount{};
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
    // Presentation reads the existing world roads without generating a second terrain.
    std::span<const TerrainPathDefinition> terrainPaths;
    std::span<const PondDefinition> ponds;
    std::span<const MapObstacle> mapObstacles;
    std::span<const CollisionBox> collisionBoxes;
    std::span<const FlowDebugVector> flowDebugVectors;
    std::optional<BuildingType> selectedBuilding;
    std::array<ResourceCost, GameBalance::BuildingTypeCount>
        buildingCosts;
    std::array<std::uint8_t, GameBalance::BuildingTypeCount> buildingBlueprintLevels{};
    double wallBaseMaxHealth{};
    std::array<double, GameBalance::BuildingTypeCount> producerRatesPerSecond{};
    std::array<double, GameBalance::BuildingTypeCount> nextProducerRatesPerSecond{};
    std::array<int, GameBalance::BuildingTypeCount> producerAmountsPerCycle{};
    std::array<int, GameBalance::BuildingTypeCount> nextProducerAmountsPerCycle{};
    std::array<bool, GameBalance::BuildingTypeCount>
        unlockedBuildings;
    int coreBuildRadius{};
    std::array<double, 4> coreCardPowers{};
    int coreHealthBaseLevel{1};
    int coreBuildingBaseLevel{1};
    int coreStorageBaseLevel{1};
    int placedBuildingCount{};
    int totalBuildingLimit{3};
    int buildingLimitBonus{};
    std::array<ResourceCost, ModularBuildPieceCount>
        modularBuildingCosts;
    std::optional<BuildingPreview> buildingPreview;
    std::optional<FinalAttackStats> buildingPreviewFinalAttackStats;
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
    std::vector<DefenseCardStatus> defenseCards;
    std::vector<ProducerCardStatus> producerCards;
    std::span<const EnemyInstance> enemies;
    std::span<const EnemyProjectile> enemyProjectiles;
    std::span<const DecoyBeaconRuntime> decoyBeacons;
    double repairStationPulseAge{};
    std::span<const RepairStationLink> repairStationLinks;
    std::span<const TowerRuntime> towers;
    std::span<const LaserBeamSegment> laserBeams;
    std::span<const AttackProjectile> defenseProjectiles;
    std::optional<FinalAttackStats> aimedFinalAttackStats;
    std::size_t droppedDefenseProjectiles{};
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
    NightPattern upcomingNightPattern{NightPattern::Horde};
    NightPattern currentNightPattern{NightPattern::Horde};
    int currentNightAct{};
    std::optional<AttackDirection> upcomingAttackDirection;
    std::array<bool, 4> upcomingAttackDirections{};
    std::array<std::optional<Vec3>, MaximumAttackPortals>
        attackPortalPositions{};
    double phaseTimeRemaining;
    double phaseDuration;
    int earlyWaveBonus;
    int earlyWaveCoinBonus;
    int earlyWaveInsightBonus;
    int wave;
    int bestWave;
    RunCombatStatistics runStatistics;
    double coreHealth;
    double coreMaxHealth;
    std::optional<EntityId> coreId;
    std::uint8_t coreLevel;
    bool unlimitedResources;
    bool playerInvulnerable;
    bool automaticToolSwitch;
    bool holdToGather;
    std::array<bool, PlayerWeaponCount> unlockedWeapons;
    std::array<int, PlayerWeaponCount> weaponLevels{};
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
    double bombCooldownRemaining{};
    double bombCooldownDuration{};
    int bombPurchaseCoinCost;
    int bombPurchaseAmount;
    int chestRerollCoinCost;
    int chestRevealCoinCost;
    int waveCompletionReward;
    int tutorialWoodTarget;
    std::optional<TutorialObjective> tutorialObjective;
    int playerLevel;
    double currentInsight;
    double requiredInsight;
    double totalInsightEarned;
    std::span<const ObjectiveStatus> objectives;
    std::array<int, 3> recommendedObjectives;
    std::size_t buildingArsenalCount{};
    int buildingSlotLimit{BaseRunBuildingSlots};
    int tomeCount{};
    int tomeSlotLimit{BaseRunTomeSlots};
    int weaponSlotLimit{BaseRunWeaponSlots};
    bool runUpgradeChoicePending{};
    RunShopState runShop{};
    ShrineRunState shrines{};
    std::array<bool, RunShopSlotCount> runShopBuyable{};
    std::size_t runUpgradeChoiceCount{};
    int runUpgradePaidChoicePrice{};
    std::array<RunUpgradeEffect, MaximumRunUpgradeChoices>
        runUpgradeChoices{};
    std::array<int, RunUpgradeEffectCount> runUpgradeStacks{};
    std::array<double, RunUpgradeEffectCount> runUpgradePower{};
    std::array<RunUpgradeRarity, MaximumRunUpgradeChoices> runUpgradeRarities{};
    std::array<std::uint64_t, MaximumRunUpgradeChoices> runUpgradeRollSeeds{};
    BuildingUpgradePowers buildingUpgradePowers{};
    RunUpgradeStats upgradeStats;
    PlayerClassAbilityState classAbilityState;
    int runUpgradeSelectionsRemaining{};
    int runUpgradeRerollTokens{};
    int runUpgradeSkipTokens{};
    int runUpgradeBanishTokens{};
    std::array<bool,RunUpgradeEffectCount> banishedRunUpgrades{};
    bool runUpgradeLockUnlocked{};
    std::optional<RunUpgradeEffect> lockedRunUpgrade;
};

struct ProgressionRunState {
    InsightRunState insight;
    ObjectiveRunState objectives;
};

// Fast, safe resume checkpoint. Transient combat state is intentionally not
// included; loading resumes at the start of a build phase.
struct SuspendedRunState {
    int buildingArsenalVersion{};
    PlayerClass playerClass{PlayerClass::None};
    MetaRunBonuses metaBonuses;
    std::uint32_t terrainSeed{};
    RunState resumeState{RunState::BuildPhase};
    std::uint64_t tick{};
    double elapsedSeconds{};
    double overtimeSeconds{};
    double phaseTimeRemaining{};
    double phaseDuration{};
    int wave{};
    int bestWave{};
    Vec3 playerPosition{};
    double playerYaw{};
    double playerPitch{};
    double playerHealth{};
    int wood{};
    int stone{};
    int crystals{};
    int coins{};
    int bombs{};
    double bombCooldownRemaining{};
    PlayerWeapon selectedWeapon{PlayerWeapon::Axe};
    int rifleLevel{1};
    PlayerClassAbilityState classAbilityState;
    PowerupRunState powerups;
    std::vector<CoinPickup> metaSilverPickups;
    std::array<int, LootUpgradeEffectCount> lootStacks{};
    RelicState relics;
    std::array<int, RunUpgradeEffectCount> runUpgradeStacks{};
    std::array<double, RunUpgradeEffectCount> runUpgradePower{};
    BuildingUpgradePowers buildingUpgradePowers{};
    double repairStationCycleRemaining{RepairStationInterval};
    std::vector<BuildingInstance> buildings;
    std::vector<CrystalMineRuntime> producerRuntimes;
    std::vector<PlatformFrameInstance> platformFrames;
    std::vector<WallInstance> modularWalls;
    std::vector<RampInstance> ramps;
    std::vector<ResourceNode> resourceNodes;
    std::vector<LootChestInstance> lootChests;
    std::vector<ChallengeColumnInstance> challengeColumns;
    std::vector<WorldLandmarkInstance> worldLandmarks;
    std::array<std::uint8_t, GameBalance::BuildingTypeCount>
        buildingBlueprintLevels{};
    int coreBuildRadius{};
    int coreBuildRadiusLevel{1};
    std::array<double, 4> coreCardPowers{};
    int coreHealthBaseLevel{1};
    int coreBuildingBaseLevel{1};
    int coreStorageBaseLevel{1};
    int playerWeaponArsenalVersion{};
    int resourceNodeCapacityVersion{};
    double playerDamageMultiplier{1.0};
    double runPlayerDamageMultiplier{1.0};
    double playerAttackSpeedMultiplier{1.0};
    double toolAttackSpeedMultiplier{1.0};
    double playerMoveSpeedMultiplier{1.0};
    double runPlayerMoveSpeedMultiplier{1.0};
    double playerArmorMultiplier{1.0};
    double playerMaxHealthMultiplier{1.0};
    double buildingMaxHealthMultiplier{1.0};
    double runBuildingMaxHealthMultiplier{1.0};
    double productionSpeedMultiplier{1.0};
    double runProductionSpeedMultiplier{1.0};
    double defenseDamageMultiplier{1.0};
    double defenseFireRateMultiplier{1.0};
    double woodYieldMultiplier{1.0};
    double chestOpeningCostMultiplier{1.0};
    int openedPaidChestCount{};
    double playerBonusMaxHealth{};
    double playerTemporaryHealth{};
    double playerRecoverableArmor{};
    double playerMaxRecoverableArmor{};
    bool appleAvailable{};
    bool breadWellFed{};
    bool battlePotionAvailable{};
    bool freeChestOpeningAvailable{};
    double battlePotionBerserkRemaining{};
    double battlePotionBerserkDuration{};
    double battlePotionLifestealRemaining{};
    int freeChestRerollsRemaining{};
    int runNightlyBombBonus{};
    int runUpgradeRerollTokens{};
    int runUpgradeSkipTokens{};
    int runUpgradeBanishTokens{};
    std::array<bool,RunUpgradeEffectCount> banishedRunUpgrades{};
    bool runUpgradeLockUnlocked{};
    std::optional<RunUpgradeEffect> lockedRunUpgrade;
    int bonusSelectionsNextReward{};
    int riskyInvestmentPending{}; // Legacy checkpoint field; ignored and saved as zero.
    int bloodHarvestKillProgress{};
    InsightRunState insight;
    ObjectiveRunState objectives;
    bool runUpgradeChoicePending{};
    RunShopState runShop{};
    ShrineRunState shrines{};
    std::size_t runUpgradeChoiceCount{};
    int runUpgradePaidChoicePrice{};
    std::array<RunUpgradeEffect, MaximumRunUpgradeChoices>
        runUpgradeChoices{};
    std::array<RunUpgradeRarity, MaximumRunUpgradeChoices> runUpgradeRarities{};
    std::array<std::uint64_t, MaximumRunUpgradeChoices> runUpgradeRollSeeds{};
    int runUpgradeSelectionsRemaining{};
    std::uint32_t runUpgradeOfferGeneration{};
    bool stageCleared{};
    bool finalNight{};
    RunCombatStatistics runStatistics;
};

class Simulation {
    friend struct RelicTestAccess;
    friend struct BasicClassesTestAccess;
  public:
    static constexpr std::size_t MaximumActiveEnemies =
        EnemySystem::MaxActiveEnemies;
    static constexpr int StageClearWave = 10;

    explicit Simulation(GameBalance balance = GameBalance::defaults(),
                        MapDefinition map = MapDefinition::defaults(),
                        WorldConfig worldConfig =
                            WorldConfig::defaults(),
                        InsightConfig insightConfig =
                            InsightConfig::defaults(),
                        std::vector<ObjectiveDefinition> objectives =
                            ObjectiveSystem::defaultDefinitions());

    [[nodiscard]] bool configureNextRunBonuses(const MetaRunBonuses& bonuses);
    void configureNextRunContent(const RunContentAccess& access);
    void startRun(PlayerClass playerClass = PlayerClass::None);
    void startSandboxRun(PlayerClass playerClass = PlayerClass::None);
    void startRunFromSeed(PlayerClass playerClass, std::uint32_t terrainSeed);
    void restartRun();
    void returnToMainMenu();
    void togglePause();
    [[nodiscard]] bool enterFinalNight();
    [[nodiscard]] bool bankStageClear();
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
    [[nodiscard]] std::vector<BuildingPreview> previewBuildingLine(
        BuildingType type, std::span<const GridPosition> cells,
        std::uint8_t rotation, double preferredHeight) const;
    [[nodiscard]] BuildingPlatformSurface
    previewPlacementSurface(
        BuildingType type, GridPosition position) const;
    [[nodiscard]] BuildingPlatformSurface
    previewPlacementSurface(
        BuildingType type, GridPosition position,
        double preferredHeight) const;
    [[nodiscard]] const TerrainHeightfield& terrain() const;
    [[nodiscard]] Vec3 playerSpawn() const { return map_.playerSpawn; }
    [[nodiscard]] std::optional<PlatformFramePlacement> previewBuildingFoundation(
        BuildingType type, GridPosition position,
        std::optional<double> preferredHeight = std::nullopt) const;
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
    [[nodiscard]] FloorPlatformFoundationPlacement
    previewFloorPlatformWithFoundation(
        Vec3 terrainHit,
        std::optional<double> foundationHeight =
            std::nullopt) const;
    [[nodiscard]] std::optional<
        FloorPlatformFoundationBuild>
    placeFloorPlatformWithFoundation(
        Vec3 terrainHit,
        std::optional<double> foundationHeight =
            std::nullopt);
    [[nodiscard]] WallPlacement previewWall(
        Vec3 terrainHit, Rotation rotation) const;
    [[nodiscard]] std::optional<WallInstance>
    placeWall(Vec3 terrainHit, Rotation rotation);
    [[nodiscard]] RampPlacement previewRamp(
        Vec3 terrainHit, Rotation rotation) const;
    [[nodiscard]] std::optional<RampInstance>
    placeRamp(Vec3 terrainHit, Rotation rotation);
    [[nodiscard]] RampFoundationPlacement
    previewRampWithFoundation(
        Vec3 terrainHit, Rotation rotation,
        std::optional<double> foundationHeight =
            std::nullopt) const;
    [[nodiscard]] std::optional<RampFoundationBuild>
    placeRampWithFoundation(
        Vec3 terrainHit, Rotation rotation,
        std::optional<double> foundationHeight =
            std::nullopt);
    void beginModularPlacementBatch();
    void endModularPlacementBatch();
    void setStructuralCollapseEnabled(bool enabled);
    [[nodiscard]] bool structuralCollapseEnabled() const;
    [[nodiscard]] std::vector<EntityId> structuralCollapseRisk(
        std::span<const EntityId> supports) const;
    [[nodiscard]] std::uint64_t structuralRevision() const;
    [[nodiscard]] std::size_t clearModularBuildings();
    void takeEvents(std::vector<GameEvent>& destination);
    std::vector<GameEvent> takeEvents();
    [[nodiscard]] bool startDebugEndlessNight();
    [[nodiscard]] bool grantDebugRunUpgrade(
        RunUpgradeEffect effect, RunUpgradeRarity rarity = RunUpgradeRarity::Common);
    // Debug-only bulk grants bypass offer/level/arsenal limits; unlocks stay unique.
    [[nodiscard]] bool grantDebugRunUpgradeBatch(RunUpgradeEffect effect, int count,
        RunUpgradeRarity rarity = RunUpgradeRarity::Common);
    [[nodiscard]] bool grantDebugPowerup(PowerupType type);
    [[nodiscard]] bool selectRunUpgrade(std::size_t choiceIndex);
    [[nodiscard]] bool rerollRunUpgrades();
    [[nodiscard]] bool skipRunUpgrade();
    [[nodiscard]] bool banishRunUpgrade(std::size_t choiceIndex);
    [[nodiscard]] bool openRunShop(bool debugAccess = false);
    [[nodiscard]] bool closeRunShop();
    [[nodiscard]] bool buyRunShopOffer(std::size_t index);
    [[nodiscard]] bool rerollRunShop();
    [[nodiscard]] bool toggleRunShopLock(std::size_t index);
    void grantLootUpgrade(
        LootUpgradeEffect effect,
        LootRarity rarity = LootRarity::Common);
    [[nodiscard]] ProgressionRunState saveProgressionState() const;
    [[nodiscard]] bool loadProgressionState(const ProgressionRunState& state);
    [[nodiscard]] const InsightSystem& insightSystem() const;
    [[nodiscard]] SuspendedRunState saveSuspendedRunState() const;
    [[nodiscard]] bool loadSuspendedRunState(
        const SuspendedRunState& state);

  private:
    [[nodiscard]] static Vec3 lookDirection(double yaw,
                                            double pitch);
    [[nodiscard]] bool markEnemyRewarded(
        std::vector<EntityId>& rewarded,
        EntityId id) const;
    void resetRun(GameEventType eventType, bool includeShrines = true);
    void recordRunStatistics(std::span<const GameEvent> events);
    void updatePlayer(double deltaSeconds,
                      const PlayerCommand& command);
    void processDebugCommands(const PlayerCommand& command);
    void castChainLightning(
        const CastChainLightningCommand& command);
    void processBuildingCommands(const PlayerCommand& command);
    void processBuildingActions(const PlayerCommand& command);
    void triggerAnvilShockwave(EntityId sourceId, Vec3 position);
    void updatePlayerActions(double deltaSeconds,
                             const PlayerCommand& command);
    void advanceBuildingProduction(double deltaSeconds);
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
    void updateRepairStations(double deltaSeconds);
    void updateEliteEffects(double deltaSeconds);
    void collectEliteEnemyEvents();
    void updateTrapCombat(double deltaSeconds);
    void updateTowerCombat(double deltaSeconds);
    void updateCannonCombat(double deltaSeconds);
    [[nodiscard]] PlacementResult validatePlacement(BuildingType type,
                                                    GridPosition position) const;
    [[nodiscard]] PlacementResult validatePlacement(
        BuildingType type, GridPosition position,
        const BuildingPlatformSurface& surface,
        const FoundationSystem* foundations = nullptr,
        const BuildingSystem* buildings = nullptr,
        std::optional<ResourceCost> available = std::nullopt) const;
    [[nodiscard]] BuildingPreview previewBuildingPlacement(
        BuildingType type, GridPosition position, std::uint8_t rotation,
        std::optional<double> preferredHeight,
        const FoundationSystem& foundations, const BuildingSystem& buildings,
        ResourceCost available,
        std::optional<GridCoord> foundationAnchor = std::nullopt) const;
    [[nodiscard]] PlacementResult
    previewPlacementWithOptionalHeight(
        BuildingType type, GridPosition position,
        std::optional<double> preferredHeight) const;
    [[nodiscard]] bool buildingUnlocked(BuildingType type) const;
    [[nodiscard]] bool ownsRunBuilding(BuildingType type) const;
    [[nodiscard]] std::size_t runBuildingArsenalCount() const;
    [[nodiscard]] std::size_t runWeaponArsenalCount() const;
    [[nodiscard]] bool runUpgradeAvailable(RunUpgradeEffect effect) const;
    [[nodiscard]] bool playerWeaponUnlocked(PlayerWeapon weapon) const;
    [[nodiscard]] int playerWeaponLevel(PlayerWeapon weapon) const;
    [[nodiscard]] BuildingPlatformSurface
    placementSurface(BuildingType type,
                     GridPosition position,
                     const FoundationSystem* foundations = nullptr) const;
    [[nodiscard]] BuildingPlatformSurface
    placementSurfaceWithPreferredHeight(
        BuildingType type, GridPosition position,
        double preferredHeight,
        const FoundationSystem* foundations = nullptr) const;
    [[nodiscard]] std::optional<
        PlatformFramePlacement>
    automaticFoundationPlacement(
        BuildingType type, GridPosition position,
        std::optional<double> preferredHeight =
            std::nullopt,
        const FoundationSystem* foundations = nullptr,
        std::optional<GridCoord> foundationAnchor = std::nullopt) const;
    [[nodiscard]] PlatformFramePlacement validateFoundationAtHeight(
        PlatformFramePlacement placement) const;
    [[nodiscard]] PlatformFramePlacement
    validateFloorPlatformPlacement(
        PlatformFramePlacement placement,
        bool checkResourceCost) const;
    [[nodiscard]] RampPlacement
    validateRampPlacement(
        RampPlacement placement,
        bool checkResourceCost) const;
    [[nodiscard]] bool foundationAddsPlacementCost(
        const PlatformFramePlacement& placement) const;
    [[nodiscard]] bool foundationInsideCoreBuildRadius(
        GridCoord anchor) const;
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
    void rebuildFlowField();
    void rebuildFlowFieldIfDirty();
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
                     std::size_t firstAnchorIndex,
                     double healthScale = 1.0,
                     double damageScale = 1.0);
    void beginPreparedWave();
    [[nodiscard]] bool beginNextFinalNightWave();
    void refillWaveSpawnQueue();
    void resetNightEncounter();
    void tickWaveSpawning(double deltaSeconds);
    void completeWave();
    void cycleUnlockedTool();
    void updateFortifications(double deltaSeconds);
    void applyLootPickup(const LootPickup& pickup);
    void applyPotionWaveStart();
    void tryConsumeApple();
    [[nodiscard]] double temporaryAttackSpeedMultiplier() const;
    [[nodiscard]] double playerClassDamageMultiplier() const;
    [[nodiscard]] PlayerClassLevelBonuses classLevelBonuses() const;
    void updatePlayerClassAbilities(double deltaSeconds);
    void registerStumpMeleeHit(Vec3 direction, double damage);
    void updateStumpRootWaves(double deltaSeconds);
    void chainHarvestResources(EntityId depleted, ResourceType type, Vec3 center, double damage);
    [[nodiscard]] double playerArmorCapacity() const;
    void applyPlayerClassLevelGrowth(int levelsGranted);
    void updatePlayerClassEffects(
        std::size_t firstGameplayEvent, bool suppressKillEffects);
    void updateLootEffects(double deltaSeconds,
                           std::size_t firstGameplayEvent);
    void resetChallengeColumns();
    void resetWorldScenery();
    void syncWorldSceneryColliders();
    [[nodiscard]] bool activateShrine(WorldLandmarkInstance& landmark);
    void generateShrineChoices();
    [[nodiscard]] bool selectShrineBlessing(std::size_t choice);
    void resetWorldLandmarks(bool includeShrines = true);
    void syncWorldLandmarkColliders();
    void updateWorldLandmarks(
        double deltaSeconds, const PlayerCommand& command);
    void refreshWorldLandmarkEconomy();
    void advanceWorldLandmarkProduction(double deltaSeconds);
    void updateChallengeColumns(
        double deltaSeconds, const PlayerCommand& command);
    void activateChallengeColumn(EntityId id);
    void failActiveChallenge();
    void constrainPlayerToChallengeArena();
    [[nodiscard]] bool challengeActive() const;
    void updateCoinPickups(double deltaSeconds, bool suppressEnemyRewards);
    [[nodiscard]] double resourceToolEfficiency(
        PlayerWeapon tool, ResourceType resource) const;
    [[nodiscard]] int resourceCapacity(
        ResourceCapacityType resource) const;
    void clampResourcesToCapacity();
    void addWood(int amount);
    void addStone(int amount);
    void addCrystals(int amount);
    [[nodiscard]] bool hasStorageSpace(
        ResourceType resource) const;
    void tickRelics(double seconds);
    void processRelicEvents(std::size_t firstEvent);
    void relicNightfall();
    void relicDawn();
    void rememberDestroyedTower(const std::optional<BuildingInstance>& building);
    [[nodiscard]] std::optional<BuildingDamageResult> damageBuildingWithRelics(EntityId id, double damage);
    [[nodiscard]] bool rollManualRelic(double baseChance, int stacks);
    [[nodiscard]] int relicStacks(LootUpgradeEffect effect) const;
    [[nodiscard]] double relicProductionMultiplier(ResourceType type) const;
    [[nodiscard]] bool mirrorStatCard(RunUpgradeEffect effect);
    void rebuildRelicCombatBuildings();
    void dropPropLoot(const ResourceHit& hit, Vec3 position, std::uint64_t seed);
    [[nodiscard]] double playerPermanentMaxHealth() const;
    [[nodiscard]] bool isFortified(EntityId id) const;
    [[nodiscard]] double repairCooldownRemaining(EntityId id) const;
    void startRepairCooldown(EntityId id);
    [[nodiscard]] std::optional<TutorialObjective> tutorialObjective() const;
    void invalidateSnapshotCache();
    [[nodiscard]] bool resourceGroundPositionIsSafe(
        double x, double z, double radius) const;
    void processInsightEvents(std::size_t firstEvent);
    void processInsightEvent(const GameEvent& event);
    void grantConfiguredInsight(double amount, InsightSource source,
                                InsightCategory category,
                                const InsightGrantContext& context);
    void grantBlueprintInsightForType(
        BuildingType type, int blueprintStackOrdinal);
    void grantBlueprintInsightForExistingBuildings(
        int blueprintStackOrdinal);
    void refreshRunUpgradeRuntimeEffects();
    void prepareRunUpgradeChoices(int selections = 1);
    void generateRunUpgradeChoices();
    void prepareRunShop();
    void generateRunShopOffers();
    [[nodiscard]] bool applyRunUpgradeEffect(
        RunUpgradeEffect effect, RunUpgradeRarity rarity, int copies = 1,
        bool debug = false, std::uint64_t rollSeed = 0);
    [[nodiscard]] double runUpgradePower(RunUpgradeEffect effect) const;
    [[nodiscard]] double playerPassivePower(RunUpgradeEffect effect) const {
        return runUpgradePower(effect) + shrineBonusPower(shrines_,ShrineKind::Player,effect);
    }
    [[nodiscard]] int runUpgradeCount(RunUpgradeEffect effect) const;
    void processRunUpgradeCombatEffects(std::size_t firstGameplayEvent);
    void salvageDestroyedBuilding(BuildingType type, Vec3 position);
    void salvageDestroyedModularBuilding(
        const ModularBuildingDamageResult& result, Vec3 position);
    void salvageDestroyedCost(ResourceCost cost, Vec3 position);
    [[nodiscard]] std::uint32_t nextRunTerrainSeed();

    RunState state_{RunState::MainMenu};
    bool sandboxMode_{};
    PlayerClass playerClass_{PlayerClass::None};
    RunState stateBeforePause_{RunState::Gathering};
    std::uint64_t tick_{};
    double elapsedSeconds_{};
    MapDefinition map_;
    WorldConfig worldConfig_;
    TerrainHeightfield terrain_;
    std::uint64_t runSeedState_{};
    std::optional<std::uint32_t> forcedRunTerrainSeed_;
    FoundationSystem foundations_;
    int modularPlacementBatchDepth_{};
    bool modularStructuresDirty_{};
    GlbCollisionAsset platformCollisionAsset_;
    GlbCollisionAsset rampCollisionAsset_;
    std::array<GlbCollisionAsset, TreeVisualVariantCount>
        treeCollisionAssets_;
    std::array<GlbCollisionAsset, StoneVisualVariantCount>
        stoneCollisionAssets_;
    std::array<GlbCollisionAsset, 1> crystalCollisionAssets_;
    Vec3 playerPosition_{0.0, 1.7, 6.0};
    Vec3 playerHorizontalVelocity_{};
    double verticalVelocity_{};
    double coyoteTimeRemaining_{};
    double jumpBufferRemaining_{};
    double flightTapRemaining_{};
    bool playerFlying_{};
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
    double playerRespawnProtectionRemaining_{};
    int deathLostWood_{};
    int deathLostStone_{};
    int deathLostCrystals_{};
    int wood_{};
    int stone_{};
    int crystals_{};
    bool crystalStorageFullNotified_{};
    int coins_{};
    CoinPickupSystem coinPickups_;
    PowerupSystem powerups_;
    std::vector<EntityId> rewardedEnemyCoins_;
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
    struct PendingSawChainLaunch {
        EntityId sourceId;
        Vec3 position;
        int depth{};
    };
    std::vector<PendingSawChainLaunch> sawChainLaunchBuffer_;
    std::vector<const ResourceNode*> sawCandidateBuffer_;
    struct RecentPlayerHit {
        EntityId target;
        double damage{};
    };
    std::vector<RecentPlayerHit> recentPlayerHitBuffer_;
    bool unlimitedResources_{};
    bool playerInvulnerable_{};
    std::uint64_t debugSpawnSequence_{};
    std::uint64_t pickaxeAttackSequence_{};
    double pickaxeCooldownRemaining_{};
    double pickaxeInputBufferRemaining_{};
    std::optional<EntityId> aimedResource_;
    std::vector<WorldSceneryInstance> worldScenery_;
    ResourceSystem resources_;
    std::optional<EntityId> aimedChest_;
    std::optional<EntityId> aimedLoot_;
    LootChestSystem lootChests_;
    ChestItemCombatSystem chestItems_;
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
    bool flowFieldDirty_{};
    std::optional<EntityId> aimedEnemy_;
    std::optional<EntityId> aimedBuilding_;
    std::optional<EntityId> aimedModularBuilding_;
    EnemySystem enemies_;
    TowerSystem towers_;
    CannonSystem cannons_;
    TrapSystem traps_;
    PlayerWeaponSystem playerWeapons_;
    InsightSystem insight_;
    ObjectiveSystem objectives_;
    std::vector<EntityId> insightRewardedEnemyIds_;
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
    std::size_t waveSpawnGroupsDue_{};
    std::uint32_t waveSpawnCycle_{};
    NightPattern wavePattern_{NightPattern::Horde};
    int waveEncounterAct_{};
    double waveCombatElapsed_{};
    double waveReplacementTime_{};
    std::size_t waveObservedActive_{};
    std::size_t waveReplacementDemand_{};
    std::uint32_t waveGroupSerial_{};
    std::optional<EnemySpawn> waveBossSpawn_;
    std::uint8_t waveSurgeWarningMask_{};
    double waveHealthScale_{1.0};
    double waveDamageScale_{1.0};
    std::optional<AttackDirection> upcomingAttackDirection_;
    std::array<bool, 4> upcomingAttackDirections_{};
    std::array<std::optional<Vec3>, MaximumAttackPortals>
        wavePortalPositions_{};
    std::size_t waveFirstAnchorIndex_{};
    bool wavePortalsActive_{};
    bool currentWaveHasBoss_{};
    bool forceWaveCompletion_{};
    bool stageCleared_{};
    bool finalNight_{};
    double overtimeSeconds_{};
    std::unique_ptr<SuspendedRunState> waveStartCheckpoint_;
    double playerDamageMultiplier_{1.0};
    double runPlayerDamageMultiplier_{1.0};
    double playerAttackSpeedMultiplier_{1.0};
    double toolAttackSpeedMultiplier_{1.0};
    double playerMoveSpeedMultiplier_{1.0};
    double runPlayerMoveSpeedMultiplier_{1.0};
    double playerArmorMultiplier_{1.0};
    double playerMaxHealthMultiplier_{1.0};
    double buildingMaxHealthMultiplier_{1.0};
    double runBuildingMaxHealthMultiplier_{1.0};
    RunContentAccess nextRunContent_;
    RunContentAccess activeRunContent_;
    MetaRunBonuses nextRunMetaBonuses_;
    MetaRunBonuses activeMetaBonuses_;
    double productionSpeedMultiplier_{1.0};
    double runProductionSpeedMultiplier_{1.0};
    double defenseDamageMultiplier_{1.0};
    double defenseFireRateMultiplier_{1.0};
    double woodYieldMultiplier_{1.0};
    double chestOpeningCostMultiplier_{1.0};
    double playerBonusMaxHealth_{};
    double playerTemporaryHealth_{};
    double playerRecoverableArmor_{};
    double playerMaxRecoverableArmor_{};
    struct StumpRootWave {
        Vec3 origin;
        Vec3 direction;
        double damage{};
        double scale{1};
        double elapsed{};
        std::vector<EntityId> hitEnemies;
    };
    std::vector<StumpRootWave> stumpRootWaves_;
    PlayerClassAbilityState classAbilityState_;
    double secondsSincePlayerDamage_{};
    bool appleAvailable_{};
    bool breadWellFed_{};
    bool battlePotionAvailable_{};
    double battlePotionBerserkRemaining_{};
    double battlePotionBerserkDuration_{};
    double battlePotionLifestealRemaining_{};
    bool freeChestOpeningAvailable_{};
    int freeChestRerollsRemaining_{};
    std::array<int, LootUpgradeEffectCount> lootStacks_{};
    RelicState relics_;
    std::unordered_map<std::uint64_t,std::size_t> relicTowerClockIndices_;
    std::vector<BuildingInstance> relicCombatBuildings_;
    bool runUpgradeChoicePending_{};
    RunShopState runShop_{};
    ShrineRunState shrines_{};
    std::size_t runUpgradeChoiceCount_{};
    int runUpgradePaidChoicePrice_{};
    std::array<RunUpgradeEffect, MaximumRunUpgradeChoices>
        runUpgradeChoices_{};
    std::array<int, RunUpgradeEffectCount> runUpgradeStacks_{};
    std::array<double, 4> coreCardPowers_{};
    int coreHealthBaseLevel_{1};
    int coreBuildingBaseLevel_{1};
    int coreStorageBaseLevel_{1};
    std::array<double, RunUpgradeEffectCount> runUpgradePower_{};
    BuildingUpgradePowers buildingUpgradePowers_{};
    std::array<RunUpgradeRarity, MaximumRunUpgradeChoices> runUpgradeRarities_{};
    std::array<std::uint64_t, MaximumRunUpgradeChoices> runUpgradeRollSeeds_{};
    int runNightlyBombBonus_{};
    int runUpgradeSelectionsRemaining_{};
    int runUpgradeRerollTokens_{};
    int runUpgradeSkipTokens_{};
    int runUpgradeBanishTokens_{};
    std::array<bool,RunUpgradeEffectCount> banishedRunUpgrades_{};
    bool runUpgradeLockUnlocked_{};
    std::optional<RunUpgradeEffect> lockedRunUpgrade_;
    int bonusSelectionsNextReward_{};
    int bloodHarvestKillProgress_{};
    std::uint32_t runUpgradeOfferGeneration_{};
    struct ActiveFortification { EntityId id; double remaining; };
    std::vector<ActiveFortification> activeFortifications_;
    struct ActiveRepairCooldown { EntityId id; double remaining; };
    std::vector<ActiveRepairCooldown> activeRepairCooldowns_;
    double repairStationCycleRemaining_{RepairStationInterval};
    std::vector<RepairStationSource> repairStationSources_;
    std::vector<RepairStationLink> repairStationLinks_;
    std::vector<EnemyStructureTarget> modularTargetBuffer_;
    struct PendingEliteExplosion {
        EntityId sourceId;
        Vec3 position;
        double remaining{1.15};
    };
    std::vector<PendingEliteExplosion> pendingEliteExplosions_;
    std::vector<GameEvent> events_;
    RunCombatStatistics runStatistics_;
    mutable std::optional<SimulationSnapshot> snapshotCache_;
    mutable std::vector<DefenseCardStatus> defenseCardBuffer_;
    mutable std::vector<ProducerCardStatus> producerCardBuffer_;
    std::uint64_t structuralRevision_{};
};

} // namespace ian
