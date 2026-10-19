#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "combat/IceWandSystem.hpp"
#include "core/PerformanceStats.hpp"
#include "economy/CoinPickupSystem.hpp"
#include "graphics/DecorationExclusionMap.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/InstanceBufferPool.hpp"
#include "game/LootChestSystem.hpp"
#include "graphics/GraphicsSettings.hpp"
#include "graphics/TerrainRenderer.hpp"
#include "resources/ResourceSystem.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ian {

[[nodiscard]] Color lootRarityColor(LootRarity rarity);

struct WorldLighting {
    Vector3 cameraPosition{};
    Vector3 sunDirection{-0.45F, -1.0F, -0.25F};
    Vector3 sunColor{1.0F, 0.86F, 0.68F};
    float sunIntensity{1.0F};
    Vector3 skyAmbientColor{0.42F, 0.52F, 0.68F};
    Vector3 groundAmbientColor{0.16F, 0.21F, 0.17F};
    float ambientIntensity{0.65F};
    float cloudShadowStrength{0.18F};
    Vector3 fogColor{0.25F, 0.33F, 0.44F};
    float fogStart{30.0F};
    float fogEnd{75.0F};
    Vector3 dayNightTint{1.0F, 1.0F, 1.0F};
    float exposure{1.0F};
    float saturation{1.0F};
};

struct WorldMaterialState {
    Vector4 baseColor{1.0F, 1.0F, 1.0F, 1.0F};
    float unlitAmount{};
    float bakedAo{1.0F};
    float vertexAoAmount{};
    float screenAoAmount{1.0F};
    float terrainAmount{};
    Vector3 terrainGrassTint{0.22F, 0.34F, 0.14F};
    Vector3 terrainDirtTint{0.46F, 0.32F, 0.18F};
    // World-space terrain texture controls. Scale is texture repeats/metre.
    float groundTextureScale{0.050F};
    float groundTextureStrength{1.0F};
    float groundSaturation{0.60F};
    float groundBrightness{0.91F};
    Vector3 groundTextureTint{0.87F, 1.0F, 1.10F};
    float groundTextureContrast{0.80F};
    float groundHighlightCompression{0.18F};
    float groundSecondaryBlend{0.18F};
    float groundMacroVariation{0.075F};
    float windAmount{};
    float localWindHeight{};
    float distantFadeAmount{};
    float vegetationAmount{};
    float treeAmount{};
    Vector3 treeLeafShadowColor{0.192F, 0.345F, 0.275F};
    Vector3 treeLeafMidColor{0.400F, 0.553F, 0.243F};
    Vector3 treeLeafLightColor{0.659F, 0.776F, 0.325F};
    Vector3 treeTrunkShadowColor{0.286F, 0.216F, 0.184F};
    Vector3 treeTrunkMidColor{0.608F, 0.357F, 0.196F};
    Vector3 treeTrunkLightColor{0.820F, 0.541F, 0.302F};
    float treeShadowThreshold{0.38F};
    float treeHighlightThreshold{0.88F};
    float treeRampSmoothness{0.19F};
    float treeHighlightStrength{0.90F};
    float treeTextureVariation{0.08F};
    float treeLeafFlatness{0.68F};
    float bushAmount{};
    Vector3 bushShadowColor{0.157F, 0.325F, 0.247F};
    Vector3 bushMidColor{0.333F, 0.498F, 0.224F};
    Vector3 bushLightColor{0.525F, 0.678F, 0.259F};
    float bushShadowThreshold{0.28F};
    float bushHighlightThreshold{0.84F};
    float bushRampSmoothness{0.10F};
    float bushHighlightStrength{0.82F};
    float coniferAmount{};
    // x/z world root encoded in xy; z enables the fixed palette anchor.
    Vector3 plantBiomeAnchor{};
    Vector3 coniferShadowColor{0.129F, 0.302F, 0.263F};
    Vector3 coniferMidColor{0.275F, 0.482F, 0.259F};
    Vector3 coniferLightColor{0.475F, 0.663F, 0.294F};
    float coniferShadowThreshold{0.28F};
    float coniferHighlightThreshold{0.84F};
    float coniferRampSmoothness{0.11F};
    float coniferHighlightStrength{0.84F};
    float coniferFacetBlend{0.58F};
    float hitFlashAmount{};
    float enemyWhiteFlashAmount{};
    float enemyPresentationAmount{};
    Vector3 hitFlashPosition{};
    float hitFlashRadius{};
    float selectionAmount{};
    Vector3 selectionTint{1.0F, 0.72F, 0.2F};
    float ghostAmount{};
    Vector3 ghostTint{0.18F, 0.72F, 1.0F};
    float ghostOpacity{0.46F};
    // Values above zero replace the automatic terrain/prop/vegetation class
    // stored in the outline buffer. Short-lived fragments use their own
    // class so their silhouette remains visible over the source object.
    float outlineMaterialClassOverride{};
    // The post-process ink mask is carried by the world pass alpha.  This is
    // a material/render-type flag, never an instance-name convention.
    bool inkOutlineEligible{true};
    // Thin foreground geometry can hide ink belonging to outlined surfaces
    // behind it without receiving a contour of its own.
    bool inkOccluder{};
};

struct RendererPerformanceStats {
    PerformanceMetric instancedEnemyDraw;
    PerformanceMetric gpuWorld;
    PerformanceMetric gpuPostProcess;
    PerformanceMetric gpuViewModel;
    PerformanceMetric gpuViewModelTarget;
    PerformanceMetric gpuViewModelComposite;
    PerformanceMetric gpuUi;
    bool gpuTimersSupported{};
    std::size_t gpuQueriesIssued{};
    std::size_t terrainChunksSubmitted{};
    std::size_t terrainChunksCulled{};
    std::size_t instancedEnemyCount{};
    std::size_t enemyBatchCount{};
    std::size_t lowDetailEnemyCount{};
    std::size_t blobShadowCount{};
    std::size_t blobShadowTriangles{};
    // Per-frame diagnostics, sampled only while performance logging is on.
    double finalBatchSubmissionMs{};
    double presentThreadCpuMs{};
    double presentNotRunningMs{};
    bool presentThreadCpuValid{};
};

enum class RendererGpuStage : std::size_t {
    World,
    PostProcess,
    ViewModelTarget,
    ViewModelComposite,
    Ui,
    Count,
};

struct GrassClearArea {
    Vector2 center{};
    float innerRadius{};
    float amount{};
};

struct SkyState {
    Vector3 cameraForward{0.0F, 0.0F, -1.0F};
    Vector3 cameraRight{1.0F, 0.0F, 0.0F};
    Vector3 cameraUp{0.0F, 1.0F, 0.0F};
    float verticalFovDegrees{75.0F};
    Vector3 zenithColor{0.18F, 0.32F, 0.53F};
    Vector3 horizonColor{0.61F, 0.75F, 0.85F};
    Vector3 lowerSkyColor{0.31F, 0.43F, 0.49F};
    Vector3 celestialDirection{0.4F, 0.8F, 0.25F};
    Vector3 celestialColor{1.0F, 0.84F, 0.55F};
    float celestialIntensity{1.0F};
    float nightAmount{};
    float timeSeconds{};
    float exposure{1.0F};
    float saturation{1.0F};
};

enum class EnemyModelVisual {
    Minion,
    Rogue,
    Warrior,
    Mage,
    Sapper,
    Flying,
    Boss,
    Splitter,
    Splitling,
    BossSummon,
    Bee,
    BeeElite,
    AirTank,
};

enum class EnemyAnimationVisual {
    Idle,
    Walk,
    Run,
    MeleeAttack,
    RangedAttack,
    SapperAttack,
    Hit,
    Death,
    Spawn,
};

enum class FirstPersonToolVisual {
    None,
    Axe,
    Pickaxe,
    Club,
    IceWand,
    FireWand,
    Hammer,
    Bomb,
};

struct FirstPersonToolTuning {
    Vector3 position{0.55401313F, -0.349843F, -0.7427701F};
    Vector3 rotation{22.0F, -60.0F, 7.0F};
    float scale{0.74F};
    float windupDegrees{34.0625F};
    float strikeDegrees{-75.38535F};
    float depthPush{-0.098322F};
    float swingDuration{0.48F};
    float hitProgress{0.42F};
    float movementBob{0.5010769F};
    float swapDuration{0.32F};
    float swapDrop{0.72F};
    bool outlineEnabled{true};
    float outlineWidth{2.48F};
    float outlineStrength{0.78F};
    float rimStrength{0.30F};
    float brightness{1.12F};
    float saturation{0.96F};
};

[[nodiscard]] inline FirstPersonToolTuning
defaultFirstPersonToolTuning(FirstPersonToolVisual visual) {
    FirstPersonToolTuning tuning;
    if (visual == FirstPersonToolVisual::Axe ||
        visual == FirstPersonToolVisual::Club) {
        tuning.position.x = 0.39473686F;
    }
    if (visual == FirstPersonToolVisual::Axe ||
        visual == FirstPersonToolVisual::Pickaxe ||
        visual == FirstPersonToolVisual::Club) {
        constexpr float AnimationFrames = 60.0F;
        constexpr float ImpactFrame = 20.0F;
        constexpr float PlaybackSpeed = 1.8F;
        tuning.swingDuration = AnimationFrames / (60.0F * PlaybackSpeed);
        tuning.hitProgress = ImpactFrame / AnimationFrames;
    }
    return tuning;
}

struct EnemyDrawInstance {
    EnemyModelVisual modelVisual{EnemyModelVisual::Minion};
    EnemyAnimationVisual animationVisual{EnemyAnimationVisual::Idle};
    float animationSeconds{};
    Vector3 position{};
    float yawRadians{};
    Color tint{WHITE};
    float whiteFlashAmount{};
    float scale{1.0F};
    bool loop{true};
    bool lowDetail{};
    bool inkOutlineEligible{true};
    std::uint8_t eliteAffixes{};
    Vector3 bodyStretch{1, 1, 1};
    float bodyRoll{};
};

struct LootChestWorldTransform {
    Matrix baseTransform{};
    Matrix lidTransform{};
    BoundingBox worldBounds{};
    bool hasLid{};
    bool valid{};
};

struct TreeDrawInstance {
    Vector3 position{};
    float yawRadians{};
    float scale{1.0F};
    std::size_t visualVariant{};
};

struct RockDrawInstance {
    Vector3 position{};
    float yawRadians{};
    float scale{1.0F};
    std::size_t visualVariant{};
};

class Renderer {
  public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void initialize();
    void shutdown();
    void beginFrame();
    void processInput();
    [[nodiscard]] bool graphicsPanelVisible() const;
    void setGraphicsPanelVisible(bool visible);
    [[nodiscard]] bool shadowMapVisible() const;
    void setShadowMapVisible(bool visible);
    void cycleAoStrength();
    void cycleQuality();
    void cycleShadowQuality();
    void cycleFrameRateLimit();
    void applyFrameRateLimit();
    void adjustPixelSize(int direction);
    void setLowHealthEffect(float amount,
                            bool reduceFlashes);
    void setMenuDepthOfField(bool enabled,
                             float focusDistance = 18.0F,
                             float focusRange = 3.0F,
                             float maximumBlurPixels = 7.0F);

    void beginWorldPass(Color clearColor, const Camera3D& camera);
    void drawSky(const SkyState& sky);
    void drawClouds(const Camera3D& camera, float nightAmount,
                    const WorldLighting& lighting);
    void endWorldPass();
    void beginUiOnlyFrame(Color clearColor);
    void endFrame();
    void drawScenePreview(Rectangle bounds);
    [[nodiscard]] bool beginFirstPersonToolPass(
        const WorldLighting* lighting = nullptr,
        const Camera3D* worldCamera = nullptr);
    void endFirstPersonToolPass(
        const FirstPersonToolTuning& tuning,
        const WorldLighting* lighting = nullptr);

    // Prepare offscreen tools before composing the world to the window.
    // endFirstPersonToolPass remains the immediate-mode convenience path.
    void finishFirstPersonToolTarget();
    void compositeFirstPersonTool(const FirstPersonToolTuning& tuning);

    [[nodiscard]] bool beginShadowPass(const WorldLighting& lighting,
                                       Vector3 focus);
    void endShadowPass();
    [[nodiscard]] bool beginSelectionMaskPass(const Camera3D& camera);
    void setSelectionMaskWind(float amount);
    void setSelectionMaskColor(Color color);
    void setSelectionOutlineTint(Color tint);
    void setSelectionOutlineRadius(float radiusPixels);
    void setSelectionOutlineDepthOcclusion(bool enabled);
    void setSelectionOutlineBounds(BoundingBox worldBounds);
    void endSelectionMaskPass();
    void clearSelectionOutline();
    void drawSelectionOutline();
    [[nodiscard]] bool shadowCasterVisible(Vector3 position,
                                           float radius = 0.0F) const;

    void beginWorldShader(const WorldLighting& lighting);
    void setWorldMaterial(const WorldMaterialState& material);
    [[nodiscard]] float buildingVisualHeight(BuildingType type, int level = 1);
    void drawTacticalGroundSector(
        Vector3 center, float radius, float yawRadians,
        float arcDegrees, Color color, float innerRadius = 0.0F);
    void beginColorOnlyWorldEffects();
    void endColorOnlyWorldEffects();
    void beginGhostPreviewMaterial();
    void endGhostPreviewMaterial();
    void endWorldShader();
    void rebuildTerrain(
        const TerrainHeightfield& terrain,
        std::span<const DecorationExclusion> exclusions = {});
    void rebuildDecorationExclusions(
        std::span<const DecorationExclusion> exclusions);
    void drawTerrain(
        Color tint, Vector3 focusPosition,
        bool wireframe = false, float mountainGrowth = 1.0F, float lowerMountainGrowth = 1.0F);
    void drawPondDecor(const Camera3D& camera);
    void drawPondShoreRocks(const Camera3D& camera);
    void drawPondSurfaceDecor(const Camera3D& camera);
    void drawWater(Vector3 cameraPosition,
                   const WorldLighting& lighting);
    [[nodiscard]] std::optional<double>
    buildingRaycastDistance(
        const BuildingInstance& building,
        std::span<const BuildingInstance> buildings,
        Ray ray, double maxDistance,
        float defensiveYaw = 0.0F,
        float cannonPitch = 0.0F,
        float gateOpenAmount = 1.0F,
        float spikeAnimationSeconds = -1.0F,
        std::optional<float> defenseBaseYaw = std::nullopt, float windRotor = 0);
    [[nodiscard]] std::optional<double>
    resourceRaycastDistance(
        ResourceType type, Vector3 position,
        Ray ray, double maxDistance,
        std::size_t visualVariant = 0U,
        float visualScale = 1.0F,
        float yawRadians = 0.0F);
    [[nodiscard]] std::optional<double>
    worldLandmarkRaycastDistance(
        std::size_t variant, Vector3 position,
        float yawRadians, Ray ray, double maxDistance);
    [[nodiscard]] std::optional<double>
    platformFrameRaycastDistance(
        Vector3 topCenter, float scale,
        const std::array<float, 4>& supportLengths,
        Ray ray, double maxDistance,
        const std::array<bool, 4>& supportVisibility =
            AllPlatformSupportsVisible);
    [[nodiscard]] std::optional<double>
    rampRaycastDistance(
        Vector3 footprintCenter, float yawRadians,
        float scale, Ray ray, double maxDistance);
    [[nodiscard]] bool drawCannon(Vector3 position, float yawRadians,
                                  float pitchRadians,
                                  Color tint = WHITE,
                                  float scale = 1.0F, int level = 1);
    [[nodiscard]] bool drawCannonball(Vector3 position,
                                      Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawWindBlower(Vector3 position, float baseYaw, Color tint = WHITE, float scale = 1, float headYaw = 0, float rotor = 0, int level = 1);
    [[nodiscard]] bool drawDecoyBeacon(Vector3 position, float yawRadians, Color tint = WHITE, float scale = 1, float lightStrength = .5F, int level = 1);
    [[nodiscard]] bool drawRepairStation(Vector3 position, float yawRadians, Color tint = WHITE, float scale = 1, int level = 1);
    [[nodiscard]] bool drawMortar(Vector3 position, float yawRadians, Color tint = WHITE,
        float scale = 1.0F, float pitchRadians = 0.0F,
        std::optional<float> headYaw = std::nullopt, int level = 1);
    [[nodiscard]] bool drawLaserTower(Vector3 position, float yawRadians, Color tint = WHITE,
        float scale = 1.0F, float pitchRadians = 0.0F,
        std::optional<float> headYaw = std::nullopt, int level = 1);
    [[nodiscard]] bool drawTeslaTower(Vector3 position, float yawRadians, Color tint = WHITE,
        float scale = 1.0F, float pitchRadians = 0.0F,
        std::optional<float> headYaw = std::nullopt, int level = 1);
    [[nodiscard]] bool drawMissileLauncher(Vector3 position, float yawRadians, Color tint = WHITE,
        float scale = 1.0F, float pitchRadians = 0.0F,
        std::optional<float> headYaw = std::nullopt, int level = 1);
    [[nodiscard]] bool drawFlamethrower(Vector3 position, float yawRadians,
                                              Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawCatapult(
        Vector3 position, float yawRadians, float armPitchRadians,
        bool loaded = true, Color tint = WHITE,
        float scale = 1.0F, int level = 1);
    [[nodiscard]] bool drawCatapultBall(Vector3 position,
                                        Color tint = WHITE);
    [[nodiscard]] bool drawArrow(Vector3 position, Vector3 direction,
                                 Color tint = WHITE);
    [[nodiscard]] bool drawCrossbow(Vector3 position, float yawRadians,
                                    Color tint = WHITE,
                                    float scale = 1.0F,
                                    float pitchRadians = 0.0F, int level = 1);
    [[nodiscard]] bool drawGunTurret(
        Vector3 position, float baseYawRadians,
        float barrelYawRadians, Color tint = WHITE,
        float scale = 1.0F,
        Vec3 barrelRecoilOffset = {}, int level = 1);
    [[nodiscard]] bool drawTurretBullet(
        Vector3 position, Vector3 direction,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] Vec3 gunTurretMuzzlePosition(
        Vec3 position, float yawRadians,
        std::size_t muzzleIndex);
    [[nodiscard]] Vec3 crossbowMuzzlePosition(
        Vec3 position, float yawRadians,
        float pitchRadians) const;
    [[nodiscard]] bool drawCore(Vector3 position, float yawRadians = 0.0F,
                                Color tint = WHITE,
                                float scale = 1.0F, int level = 1);
    [[nodiscard]] bool drawChallengeColumn(
        Vector3 position, float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawChallengeArenaPeg(
        Vector3 position, float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawEnemyPortal(
        Vector3 position, float yawRadians,
        float matterRotationRadians, const WorldLighting& lighting,
        float scale = 1.0F, float arrivalProgress = -1.0F,
        float openingProgress = 1.0F, bool detailedArrival = true);
    [[nodiscard]] bool drawWorldLandmark(
        std::size_t variant, Vector3 position,
        float yawRadians = 0.0F, Color tint = WHITE,
        float scale = 1.0F);
    [[nodiscard]] BoundingBox worldLandmarkWorldBounds(
        std::size_t variant, Vector3 position,
        float yawRadians, float scale = 1.0F);
    [[nodiscard]] bool drawFirstPersonTool(
        FirstPersonToolVisual visual, float swingProgress,
        float movementPhase, float movementAmount,
        const FirstPersonToolTuning& tuning,
        float iceChargeProgress = 0.0F,
        float iceRecoilProgress = 0.0F,
        bool alternateHarvestSwing = false, int toolLevel = 1);
    void drawFirstPersonSwingTrail(
        FirstPersonToolVisual visual, float swingProgress,
        float movementPhase, float movementAmount,
        const FirstPersonToolTuning& tuning,
        const Camera3D& viewModelCamera,
        bool alternateHarvestSwing = false);
    [[nodiscard]] bool hasAlternateHarvestSwingAnimation() const;
    void drawIceWandProjectile(
        const IceWandProjectile& projectile,
        Vector3 cameraPosition, float timeSeconds,
        float interpolationAlpha = 1.0F);
    [[nodiscard]] bool drawLootChest(
        LootChestType type, Vector3 position, float yawRadians,
        float openingProgress, Color tint = WHITE,
        float scale = 1.0F);
    void drawCoin(CoinType type, Vector3 position, float rotationRadians,
                  float scale = 1.0F);
    [[nodiscard]] bool drawPowerupPickup(std::size_t type, Vector3 position,
                                        float rotationRadians, float scale = 1.0F,
                                        Color tint = WHITE);
    void drawExperienceOctahedron(
        Vector3 position, float rotationRadians, float scale = 1.0F,
        int experienceValue = 1);
    void drawHeart(Vector3 position, float rotationRadians,
                   float scale = 1.0F);
    void drawSawBladeProjectile(
        Vector3 position, Vector3 direction,
        float spinDegrees, float scale = 1.0F,
        Color tint = WHITE);
    [[nodiscard]] bool drawDestructibleProp(
        ResourceType type, Vector3 position, float yawRadians,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] BoundingBox destructiblePropWorldBounds(
        ResourceType type, Vector3 position, float yawRadians,
        float scale = 1.0F);
    [[nodiscard]] LootChestWorldTransform lootChestWorldTransform(
        LootChestType type, Vector3 position, float yawRadians,
        float openingProgress, float scale = 1.0F);
    void drawLootItem(
        Vector3 position, LootUpgradeEffect effect,
        LootRarity rarity, float rotationRadians,
        Color tint = WHITE, float scale = 1.0F,
        Vector3 surfaceNormal = {0.0F, 1.0F, 0.0F});
    [[nodiscard]] BoundingBox lootItemWorldBounds(
        Vector3 position, LootUpgradeEffect effect,
        float rotationRadians, float scale,
        Vector3 surfaceNormal = {0.0F, 1.0F, 0.0F});
    [[nodiscard]] BoundingBox treeWorldBounds(
        Vector3 position, float scale, std::size_t visualVariant,
        float yawRadians);
    [[nodiscard]] BoundingBox enemyWorldBounds(
        EnemyModelVisual modelVisual, Vector3 position,
        float yawRadians, float scale, bool eliteDetails = false) const;
    [[nodiscard]] bool drawPlatformFrameModel(
        Vector3 topCenter, Color tint = WHITE,
        float scale = 1.0F,
        const std::array<float, 4>& supportLengths = {},
        const std::array<bool, 4>& supportVisibility =
            AllPlatformSupportsVisible,
        const std::array<bool, 4>& edgeVisibility =
            AllPlatformEdgesVisible);
    [[nodiscard]] bool drawRampModel(
        Vector3 footprintCenter, float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawRampSupportBeam(
        Vector3 baseCenter, float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawMine(Vector3 position,
                                float yawRadians = 0.0F,
                                Color tint = WHITE,
                                float scale = 1.0F);
    [[nodiscard]] bool drawResourceProducer(
        BuildingType type, Vector3 position,
        float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F, int level = 1,
        double animationSeconds = -1.0);
    [[nodiscard]] bool drawSpikeTrap(
        Vector3 position, float yawRadians = 0.0F,
        float animationSeconds = -1.0F,
        Color tint = WHITE, float scale = 1.0F, int level = 1);
    [[nodiscard]] bool drawRock(Vector3 position,
                                Color tint = WHITE,
                                float scale = 1.0F,
                                std::size_t visualVariant = 0U,
                                float yawRadians = 0.0F);
    [[nodiscard]] bool drawCrystalResource(
        Vector3 position, Color tint = WHITE,
        float scale = 1.0F, float yawRadians = 0.0F,
        Vector3 surfaceNormal = {0.0F, 1.0F, 0.0F});
    [[nodiscard]] bool drawRocksInstanced(
        std::span<const RockDrawInstance> instances);
    [[nodiscard]] bool drawTree(Vector3 position,
                                Color tint = WHITE,
                                float scale = 1.0F,
                                std::size_t visualVariant = 0U,
                                float yawRadians = 0.0F);
    [[nodiscard]] bool drawTreesInstanced(
        std::span<const TreeDrawInstance> instances);
    [[nodiscard]] bool drawWall(Vector3 position,
                                std::uint8_t connectionMask,
                                float yawRadians = 0.0F,
                                Color tint = WHITE,
                                float scale = 1.0F, int level = 1);
    [[nodiscard]] bool drawGate(
        Vector3 position, float yawRadians,
        float openAmount = 0.0F,
        Color tint = WHITE, float scale = 1.0F, int level = 1);
    [[nodiscard]] bool drawEnemy(
        EnemyModelVisual modelVisual,
        EnemyAnimationVisual animationVisual,
        float animationSeconds, Vector3 position,
        float yawRadians, Color tint = WHITE,
        float scale = 1.0F, bool loop = true,
        bool inkOutlineEligible = true,
        bool quantizeCrowdPose = false,
        std::uint8_t eliteAffixes = 0,
        Vector3 bodyStretch = {1, 1, 1}, float bodyRoll = 0);
    [[nodiscard]] bool drawEnemiesInstanced(
        std::span<const EnemyDrawInstance> instances);
    void drawGrassInstances(Vector3 cameraPosition,
                            float worldLimit, float nightAmount,
                            const WorldLighting& lighting,
                            std::span<const GrassClearArea>
                                clearAreas = {});
    void drawDecorativeRocks(
        const Camera3D& camera, float worldLimit,
        std::span<const GrassClearArea> clearAreas = {});
    [[nodiscard]] bool drawDecorativeRockFragment(
        Vector3 position, Vector3 rotationAxis,
        float rotationDegrees, float scale,
        std::size_t variant, Color tint = WHITE);
    [[nodiscard]] bool drawWoodFragment(
        Vector3 position, Vector3 rotationAxis,
        float rotationDegrees, float scale,
        Color tint = WHITE);
    [[nodiscard]] bool drawCrystalFragment(
        Vector3 position, Vector3 rotationAxis,
        float rotationDegrees, float scale,
        Color tint = WHITE);
    void drawDecorativeRockAo(
        const Camera3D& camera, float worldLimit,
        std::span<const GrassClearArea> clearAreas = {});
    void drawBoundaryForest(const Camera3D& camera);
    void setWorldReveal(Vector2 origin,
                        float elapsedSeconds);
    [[nodiscard]] float worldRevealScaleAt(
        Vector2 position) const;
    void drawUpgradeEffect(Vector3 position, float progress,
                           float scale = 1.0F);
    void drawRepairShockwave(Vector3 position, Vector3 cameraPosition,
                             float progress, float radius);
    [[nodiscard]] bool beginBlobShadowBatch(Vector3 cameraPosition);
    void drawBlobShadow(Vector3 groundPosition, float radiusX, float radiusZ,
                        float opacity, int segmentCountOverride = 0);
    void endBlobShadowBatch();

    [[nodiscard]] GraphicsSettings& settings();
    [[nodiscard]] const GraphicsSettings& settings() const;
    [[nodiscard]] const RendererPerformanceStats& performanceStats() const;
    [[nodiscard]] double framePacingMilliseconds() const;
    [[nodiscard]] double frameSwapMilliseconds() const;
    void setGpuProfilingEnabled(bool enabled);
    void beginGpuTimer(RendererGpuStage stage);
    void endGpuTimer(RendererGpuStage stage);

  private:
    struct EnemyBatchKey {
        EnemyModelVisual model{};
        EnemyAnimationVisual animation{};
        int frame{};
        std::uint32_t tint{};
        std::uint8_t whiteFlashLevel{};
        bool loop{};
        bool lowDetail{};
        bool inkOutlineEligible{};

        auto operator<=>(const EnemyBatchKey&) const = default;
    };

    struct EnemyPoseKey {
        EnemyModelVisual model{};
        EnemyAnimationVisual animation{};
        int frame{};

        auto operator<=>(const EnemyPoseKey&) const = default;
    };

    struct EnemyBatch {
        EnemyDrawInstance representative{};
        std::vector<Matrix> transforms{};
        std::uint64_t lastUsedFrame{};
    };

    struct CachedInstance {
        Matrix transform{};
        Vector2 position{};
        float scale{};
        float groundHeight{};
    };

    void ensureGrassClearAreaIndex(
        std::span<const GrassClearArea> clearAreas);
    [[nodiscard]] Vector3 terrainSurfaceNormal(
        float worldX, float worldZ) const;
    [[nodiscard]] Matrix terrainAlignedRotation(
        float worldX, float worldZ, float yawRadians) const;
    void drawTerrainAlignedModel(
        Model& model, Vector3 position, float yawRadians,
        Vector3 scale, Color tint) const;
    void paceFrame();
    [[nodiscard]] float clearAreaVisibility(
        Vector2 position,
        std::span<const GrassClearArea> clearAreas,
        float feather, float innerPadding = 0.0F) const;

    struct WorldShaderLocations {
        int baseColor{-1};
        int cameraPosition{-1};
        int sunDirection{-1};
        int sunColor{-1};
        int sunIntensity{-1};
        int skyAmbientColor{-1};
        int groundAmbientColor{-1};
        int ambientIntensity{-1};
        int cloudShadowStrength{-1};
        int fogColor{-1};
        int fogStart{-1};
        int fogEnd{-1};
        int fogBandsEnabled{-1};
        int fogBandCount{-1};
        int dayNightTint{-1};
        int exposure{-1};
        int saturation{-1};
        int toonShadingEnabled{-1};
        int toonLightSteps{-1};
        int bakedAo{-1};
        int vertexAoAmount{-1};
        int screenAoAmount{-1};
        int aoStrength{-1};
        int terrainAmount{-1};
        int terrainGrassTint{-1};
        int terrainDirtTint{-1};
        int terrainTexture{-1};
        int terrainTextureEnabled{-1};
        int groundTextureScale{-1};
        int groundTextureStrength{-1};
        int groundSaturation{-1};
        int groundBrightness{-1};
        int groundTextureTint{-1};
        int groundTextureContrast{-1};
        int groundHighlightCompression{-1};
        int groundSecondaryBlend{-1};
        int groundMacroVariation{-1};
        int timeSeconds{-1};
        int windAmount{-1};
        int localWindHeight{-1};
        int distantFadeAmount{-1};
        int vegetationAmount{-1};
        int treeAmount{-1};
        int treeLeafShadowColor{-1};
        int treeLeafMidColor{-1};
        int treeLeafLightColor{-1};
        int treeTrunkShadowColor{-1};
        int treeTrunkMidColor{-1};
        int treeTrunkLightColor{-1};
        int treeShadowThreshold{-1};
        int treeHighlightThreshold{-1};
        int treeRampSmoothness{-1};
        int treeHighlightStrength{-1};
        int treeTextureVariation{-1};
        int treeLeafFlatness{-1};
        int bushAmount{-1};
        int bushShadowColor{-1};
        int bushMidColor{-1};
        int bushLightColor{-1};
        int bushShadowThreshold{-1};
        int bushHighlightThreshold{-1};
        int bushRampSmoothness{-1};
        int bushHighlightStrength{-1};
        int coniferAmount{-1};
        int plantBiomeAnchor{-1};
        int coniferShadowColor{-1};
        int coniferMidColor{-1};
        int coniferLightColor{-1};
        int coniferShadowThreshold{-1};
        int coniferHighlightThreshold{-1};
        int coniferRampSmoothness{-1};
        int coniferHighlightStrength{-1};
        int coniferFacetBlend{-1};
        int hitFlashAmount{-1};
        int enemyWhiteFlashAmount{-1};
        int enemyPresentationAmount{-1};
        int hitFlashPosition{-1};
        int hitFlashRadius{-1};
        int selectionAmount{-1};
        int selectionTint{-1};
        int ghostAmount{-1};
        int ghostTint{-1};
        int ghostOpacity{-1};
        int outlineMaterialClassOverride{-1};
        int shadowMap{-1};
        int lightViewProjection{-1};
        int shadowsEnabled{-1};
        int constantBias{-1};
        int slopeBias{-1};
        int shadowStrength{-1};
        int shadowMapTexelSize{-1};
        int shadowSampleCount{-1};
        int instancingEnabled{-1};
        int inkOutlineEligible{-1};
        int inkOccluder{-1};
        int unlitAmount{-1};
    };

    struct SkyShaderLocations {
        int viewportSize{-1};
        int cameraForward{-1};
        int cameraRight{-1};
        int cameraUp{-1};
        int tanHalfFov{-1};
        int aspectRatio{-1};
        int zenithColor{-1};
        int horizonColor{-1};
        int lowerSkyColor{-1};
        int celestialDirection{-1};
        int celestialColor{-1};
        int celestialIntensity{-1};
        int nightAmount{-1};
        int daySkybox{-1};
        int morningSkybox{-1};
        int nightSkybox{-1};
        int skyboxEnabled{-1};
        int timeSeconds{-1};
        int exposure{-1};
        int saturation{-1};
    };

    struct PostProcessLocations {
        int exposure{-1};
        int brightness{-1};
        int contrast{-1};
        int saturation{-1};
        int hueDegrees{-1};
        int temperature{-1};
        int tint{-1};
        int gamma{-1};
        int blackPoint{-1};
        int curveShadows{-1};
        int curveMidtones{-1};
        int curveHighlights{-1};
        int sharpness{-1};
        int vignette{-1};
        int lowHealthAmount{-1};
        int lowHealthTime{-1};
        int lowHealthPulse{-1};
        int paletteEnabled{-1};
        int paletteLevels{-1};
        int ditherEnabled{-1};
        int ditherStrength{-1};
        int bloomEnabled{-1};
        int bloomStrength{-1};
        int inkOutlinesEnabled{-1};
        int outlineStrength{-1};
        int outlineWidth{-1};
        int outlinePixelScale{-1};
        int outlineFogFadeStrength{-1};
        int outlineFogColor{-1};
        int outlineFogStart{-1};
        int outlineFogEnd{-1};
        int paperGrainEnabled{-1};
        int paperGrainStrength{-1};
        int menuDofEnabled{-1};
        int menuDofFocusDistance{-1};
        int menuDofFocusRange{-1};
        int menuDofMaximumBlurPixels{-1};
        int sceneDepth{-1};
        int sceneNormal{-1};
        int ssaoTexture{-1};
        int selectionMask{-1};
        int inverseProjection{-1};
        int ssaoTexelSize{-1};
        int ssaoEnabled{-1};
        int ssaoStrength{-1};
        int selectionMaskEnabled{-1};
    };

    struct SsaoLocations {
        int sceneDepth{-1};
        int sceneNormal{-1};
        int projection{-1};
        int inverseProjection{-1};
        int viewMatrix{-1};
        int texelSize{-1};
        int radius{-1};
        int bias{-1};
        int fadeStart{-1};
        int fadeEnd{-1};
        int sampleCount{-1};
    };

    void resolveWorldShaderLocations();
    void resolveSkyShaderLocations();
    void resolvePostProcessLocations();
    void resolveSsaoLocations();
    void resolveAuxiliaryShaderLocations();
    void drawSsaoPass();
    void uploadPostProcessSettings();
    void uploadWorldLighting(const WorldLighting& lighting);
    void uploadWorldMaterial(const WorldMaterialState& material);
    void bindTerrainTexture();
    void bindShadowMap();
    void rebuildPondDecorInstances();
    void drawPondDecorInstances(const Camera3D& camera,
                                std::size_t beginVariant,
                                std::size_t endVariant);
    void drawPondShoreRockInstances(const Camera3D& camera);
    void setSkinningEnabled(Shader& shader, bool enabled);
    Shader* activeModelShader(bool selectionEligible = true);
    Shader* prepareModelShader(Model& model, bool selectionEligible = true);
    void drawEliteEnemyDetails(const EnemyDrawInstance& instance);
    [[nodiscard]] const std::vector<int>& enemyBoneMapping(
        EnemyModelVisual visual, const Model& model,
        const std::array<const char*, 23>& sourceBones);
    void drawShadowMapDebug() const;
    void drawIceMagicSphere(Vector3 position, float radius,
                            float timeSeconds, float intensity,
                            Color tint);
    GraphicsSettings settings_;
    float lowHealthEffect_{};
    float lowHealthPulse_{1.0F};
    bool menuDepthOfFieldEnabled_{};
    float menuDepthOfFieldFocusDistance_{18.0F};
    float menuDepthOfFieldFocusRange_{3.0F};
    float menuDepthOfFieldMaximumBlurPixels_{7.0F};
    Vector3 postProcessFogColor_{0.25F, 0.33F, 0.44F};
    float postProcessFogStart_{30.0F};
    float postProcessFogEnd_{75.0F};
    GraphicsResources resources_;
    InstanceBufferPool instanceBuffers_;
    TerrainRenderer terrainRenderer_;
    float lowerMountainGrowth_{1.0F};
    DecorationExclusionMap decorationExclusionMap_;
    const TerrainHeightfield* terrainHeightfield_{};
    WorldShaderLocations worldShaderLocations_;
    SkyShaderLocations skyShaderLocations_;
    PostProcessLocations postProcessLocations_;
    SsaoLocations ssaoLocations_;
    WorldMaterialState worldMaterial_;
    std::optional<WorldMaterialState>
        ghostPreviewRestoreMaterial_;
    int tacticalSectorShapeLocation_{-1};
    int selectionOutlineTexelSizeLocation_{-1};
    int selectionOutlineRadiusLocation_{-1};
    int selectionOutlineSceneDepthLocation_{-1};
    int selectionOutlineSelectionDepthLocation_{-1};
    int selectionOutlineEffectCoverageLocation_{-1};
    int selectionOutlineEffectCoverageEnabledLocation_{-1};
    RenderTextureResource selectionEffectCoverage_;
    bool selectionEffectCoverageReady_{};
    int selectionOutlineInverseProjectionLocation_{-1};
    int selectionOutlineDepthOcclusionEnabledLocation_{-1};
    int selectionMaskTimeLocation_{-1};
    int selectionMaskWindLocation_{-1};
    int selectionMaskColorLocation_{-1};
    int worldSkinningEnabledLocation_{-1};
    int worldInstancingEnabledLocation_{-1};
    int shadowSkinningEnabledLocation_{-1};
    int shadowInstancingEnabledLocation_{-1};
    int selectionMaskSkinningEnabledLocation_{-1};
    int viewModelTexelSizeLocation_{-1};
    int viewModelOutlineEnabledLocation_{-1};
    int viewModelOutlineWidthLocation_{-1};
    int viewModelOutlineStrengthLocation_{-1};
    int viewModelRimStrengthLocation_{-1};
    int viewModelBrightnessLocation_{-1};
    int viewModelSaturationLocation_{-1};
    int viewModelSunDirectionLocation_{-1};
    int viewModelSunColorLocation_{-1};
    int viewModelSunIntensityLocation_{-1};
    int viewModelSkyAmbientColorLocation_{-1};
    int viewModelGroundAmbientColorLocation_{-1};
    int viewModelAmbientIntensityLocation_{-1};
    int viewModelDayNightTintLocation_{-1};
    int viewModelExposureLocation_{-1};
    int grassTintLocation_{-1};
    int grassTimeLocation_{-1};
    int grassCameraPositionLocation_{-1};
    int grassSunDirectionLocation_{-1};
    int grassSunColorLocation_{-1};
    int grassSunIntensityLocation_{-1};
    int grassSkyAmbientColorLocation_{-1};
    int grassGroundAmbientColorLocation_{-1};
    int grassAmbientIntensityLocation_{-1};
    int grassFogColorLocation_{-1};
    int grassFogStartLocation_{-1};
    int grassFogEndLocation_{-1};
    int grassFogBandsEnabledLocation_{-1};
    int grassFogBandCountLocation_{-1};
    int grassDayNightTintLocation_{-1};
    int grassExposureLocation_{-1};
    int grassSaturationLocation_{-1};
    int cloudCameraPositionLocation_{-1};
    int cloudSunDirectionLocation_{-1};
    int cloudSunColorLocation_{-1};
    int cloudSunIntensityLocation_{-1};
    int cloudAmbientColorLocation_{-1};
    int cloudVisibilityLocation_{-1};
    int cloudNightAmountLocation_{-1};
    int waterCameraPositionLocation_{-1};
    int waterShoreColorLocation_{-1};
    int waterShallowColorLocation_{-1};
    int waterMidColorLocation_{-1};
    int waterDeepColorLocation_{-1};
    int waterSkyColorLocation_{-1};
    int waterSunDirectionLocation_{-1};
    int waterSunColorLocation_{-1};
    int waterFogColorLocation_{-1};
    int waterDayNightTintLocation_{-1};
    int waterFogStartLocation_{-1};
    int waterFogEndLocation_{-1};
    int waterExposureLocation_{-1};
    int waterTimeLocation_{-1};
    int waterWaveSpeedLocation_{-1};
    int waterShoreWidthLocation_{-1};
    int waterDepthFalloffLocation_{-1};
    int waterPatternScaleLocation_{-1};
    int waterPatternStrengthLocation_{-1};
    int waterPatternSpeedLocation_{-1};
    int waterFresnelStrengthLocation_{-1};
    int waterQualityLocation_{-1};
    int upgradeEffectOriginLocation_{-1};
    int upgradeEffectHeightLocation_{-1};
    int upgradeEffectProgressLocation_{-1};
    int upgradeEffectTimeLocation_{-1};
    int shockwaveCameraLocation_{-1};
    int shockwaveProgressLocation_{-1};
    int shockwaveTimeLocation_{-1};
    int iceMagicTimeLocation_{-1};
    int iceMagicTintLocation_{-1};
    int iceMagicIntensityLocation_{-1};
    Matrix lightViewProjection_{};
    Matrix ssaoProjection_{};
    Matrix ssaoInverseProjection_{};
    Matrix ssaoViewMatrix_{};
    Vector3 shadowFocus_{};
    Vector3 blobShadowCamera_{};
    Camera3D selectionMaskCamera_{};
    std::optional<Rectangle> selectionOutlineBounds_;
    std::optional<Rectangle> firstPersonCompositeBounds_;
    Color selectionOutlineTint_{WHITE};
    float selectionOutlineRadiusPixels_{6.0F};
    bool selectionOutlineDepthOcclusion_{true};
    bool showDebugPanel_{};
    bool showShadowMap_{};
    bool frameOpen_{};
    bool worldPassOpen_{};
    bool usingOffscreenTarget_{};
    bool ssaoFrameReady_{};
    bool worldShaderActive_{};
    bool shadowPassOpen_{};
    bool shadowFrameValid_{};
    // The shadow map is intentionally refreshed at a small fixed cadence.
    // Re-rendering the complete caster set every frame is one of the most
    // expensive passes on large maps, while a 30 Hz cache is visually
    // indistinguishable during normal camera movement.
    bool shadowCacheInitialized_{};
    double shadowLastUpdateTime_{-1.0};
    Vector3 shadowLastFocus_{};
    Vector3 shadowLastSunDirection_{};
    float shadowLastDistance_{-1.0F};
    int shadowLastMapSize_{-1};
    bool selectionMaskPassOpen_{};
    bool selectionMaskReady_{};
    bool blobShadowBatchOpen_{};
    std::array<std::vector<Matrix>, 2>
        boundaryForestTransforms_;
    std::array<std::vector<Matrix>, 2>
        boundaryForestRevealTransforms_;
    std::array<std::vector<Matrix>, 2>
        boundaryForestVisibleTransforms_;
    std::array<std::vector<Matrix>, TreeVisualVariantCount>
        resourceTreeTransforms_;
    std::array<std::vector<Matrix>, StoneVisualVariantCount>
        resourceRockTransforms_;
    std::array<std::vector<Matrix>, 4>
        decorativeRockTransforms_;
    std::array<std::vector<Matrix>, 9>
        decorativeBushTransforms_;
    // Procedural decoration is rebuilt only when the camera crosses a cache
    // cell or when terrain/reveal inputs change. Dynamic exclusions and clear
    // areas are filtered without invalidating candidate generation.
    std::array<std::vector<CachedInstance>, 3>
        grassInstanceCandidates_;
    std::array<std::vector<Matrix>, 3>
        grassInstanceTransforms_;
    std::array<std::vector<CachedInstance>, 4>
        decorativeRockCandidates_;
    std::array<std::vector<CachedInstance>, 9>
        decorativeBushCandidates_;
    bool grassInstanceCacheValid_{};
    bool decorativeInstanceCacheValid_{};
    int grassCacheCameraCellX_{};
    int grassCacheCameraCellZ_{};
    int decorativeCacheCameraCellX_{};
    int decorativeCacheCameraCellZ_{};
    GraphicsQuality grassCacheQuality_{GraphicsQuality::High};
    GraphicsQuality decorativeCacheQuality_{GraphicsQuality::High};
    float grassCacheWorldLimit_{-1.0F};
    float decorativeCacheWorldLimit_{-1.0F};
    const TerrainHeightfield* grassCacheTerrain_{};
    const TerrainHeightfield* decorativeCacheTerrain_{};
    Vector2 grassCacheRevealOrigin_{};
    Vector2 decorativeCacheRevealOrigin_{};
    std::array<std::vector<Matrix>, 5>
        pondDecorTransforms_;
    std::array<std::vector<Matrix>, 5>
        visiblePondDecorTransforms_;
    std::array<std::vector<Matrix>, 4>
        pondShoreRockTransforms_;
    std::array<std::vector<Matrix>, 4>
        visiblePondShoreRockTransforms_;
    bool boundaryForestCached_{};
    Vector2 worldRevealOrigin_{};
    float worldRevealElapsed_{1000.0F};
    std::vector<Transform> enemyAnimationPose_;
    std::vector<Transform*> enemyAnimationFrames_;
    std::array<std::array<std::vector<int>, 2>, 13>
        enemyBoneMappings_;
    std::map<EnemyBatchKey, EnemyBatch> enemyBatches_;
    std::vector<EnemyBatch*> activeEnemyBatches_;
    std::uint64_t enemyBatchFrame_{};
    std::map<EnemyPoseKey, std::vector<Matrix>> enemyBonePoseCache_;
    Model enemyCrowdLodModel_{};
    Model rampSupportBeamModel_{};
    static constexpr std::size_t PlatformTopPartCount = 5U;
    std::array<Mesh, PlatformTopPartCount>
        platformTopPartMeshes_{};
    bool platformTopPartMeshesReady_{};
    RendererPerformanceStats performanceStats_{};
    static constexpr std::size_t GpuQueryBufferCount = 4U;
    struct GpuQuerySlot {
        unsigned int id{};
        bool pending{};
    };
    std::array<
        std::array<GpuQuerySlot, GpuQueryBufferCount>,
        static_cast<std::size_t>(RendererGpuStage::Count)>
        gpuQueries_{};
    std::array<
        std::size_t,
        static_cast<std::size_t>(RendererGpuStage::Count)>
        gpuQueryWriteIndices_{};
    std::optional<RendererGpuStage> activeGpuStage_;
    std::size_t activeGpuQueryIndex_{};
    std::optional<RendererGpuStage> sampledGpuStage_;
    std::uint64_t gpuProfilingFrameCount_{};
    bool gpuProfilingEnabled_{};
    bool gpuTimersInitialized_{};
    void warmCombatRendering();
    void initializeGpuTimers();
    void shutdownGpuTimers();
    void initializePlatformTopPartMeshes();
    void shutdownPlatformTopPartMeshes();
    void pollGpuTimers();
    double instancedEnemyMillisecondsThisFrame_{};
    double framePacingMilliseconds_{};
    double frameSwapMilliseconds_{};
#if defined(__APPLE__)
    int pacedFrameRate_{};
    std::uint64_t frameDeadline_{};
    std::uint32_t machTimebaseNumer_{};
    std::uint32_t machTimebaseDenom_{};
#endif
    std::vector<std::vector<std::uint32_t>>
        grassClearAreaCells_;
    const GrassClearArea* indexedGrassClearAreaData_{};
    std::size_t indexedGrassClearAreaCount_{};
    std::size_t grassClearAreaContentHash_{};
    float grassClearAreaMinimum_{};
    float grassClearAreaCellSize_{8.0F};
    int grassClearAreaDimension_{};
};

} // namespace ian
