#pragma once

#include "buildings/BuildingSystem.hpp"
#include "combat/IceWandSystem.hpp"
#include "core/PerformanceStats.hpp"
#include "economy/CoinPickupSystem.hpp"
#include "graphics/DecorationExclusionMap.hpp"
#include "graphics/GraphicsResources.hpp"
#include "game/LootChestSystem.hpp"
#include "graphics/GraphicsSettings.hpp"
#include "graphics/TerrainRenderer.hpp"
#include "resources/ResourceSystem.hpp"

#include <raylib.h>

#include <array>
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
    float bakedAo{1.0F};
    float vertexAoAmount{};
    float screenAoAmount{1.0F};
    float terrainAmount{};
    Vector3 terrainGrassTint{0.22F, 0.34F, 0.14F};
    Vector3 terrainDirtTint{0.46F, 0.32F, 0.18F};
    float windAmount{};
    float localWindHeight{};
    float distantFadeAmount{};
    float vegetationAmount{};
    float hitFlashAmount{};
    float selectionAmount{};
    Vector3 selectionTint{1.0F, 0.72F, 0.2F};
    float ghostAmount{};
    Vector3 ghostTint{0.18F, 0.72F, 1.0F};
    float ghostOpacity{0.46F};
    // The post-process ink mask is carried by the world pass alpha.  This is
    // a material/render-type flag, never an instance-name convention.
    bool inkOutlineEligible{true};
};

struct RendererPerformanceStats {
    PerformanceMetric instancedEnemyDraw;
    std::size_t instancedEnemyCount{};
    std::size_t enemyBatchCount{};
    std::size_t lowDetailEnemyCount{};
    std::size_t blobShadowCount{};
    std::size_t blobShadowTriangles{};
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
    Vector3 position{0.47401315F, -0.409843F, -0.7427701F};
    Vector3 rotation{19.27515F, -72.26504F, 5.6109023F};
    float scale{0.9171346F};
    float windupDegrees{34.0625F};
    float strikeDegrees{-75.38535F};
    float depthPush{-0.098322F};
    float swingDuration{0.48F};
    float hitProgress{0.42F};
    float movementBob{0.5010769F};
    float swapDuration{0.32F};
    float swapDrop{0.72F};
    bool outlineEnabled{true};
    float outlineWidth{3.3623266F};
    float outlineStrength{0.3901942F};
    float rimStrength{0.4286008F};
    float brightness{1.2466959F};
    float saturation{1.1411145F};
};

[[nodiscard]] bool loadFirstPersonToolTuning(
    std::string_view path, FirstPersonToolTuning& tuning);
[[nodiscard]] bool saveFirstPersonToolTuning(
    std::string_view path, const FirstPersonToolTuning& tuning);

struct EnemyDrawInstance {
    EnemyModelVisual modelVisual{EnemyModelVisual::Minion};
    EnemyAnimationVisual animationVisual{EnemyAnimationVisual::Idle};
    float animationSeconds{};
    Vector3 position{};
    float yawRadians{};
    Color tint{WHITE};
    float scale{1.0F};
    bool loop{true};
    bool lowDetail{};
    bool inkOutlineEligible{true};
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
    void processInput();
    [[nodiscard]] bool graphicsPanelVisible() const;
    void setGraphicsPanelVisible(bool visible);
    [[nodiscard]] bool shadowMapVisible() const;
    void setShadowMapVisible(bool visible);
    void cycleAoStrength();
    void cycleQuality();
    void cycleShadowQuality();
    void cycleFrameRateLimit();
    void applyFrameRateLimit() const;
    void adjustPixelSize(int direction);
    void setLowHealthEffect(float amount,
                            bool reduceFlashes);

    void beginWorldPass(Color clearColor, const Camera3D& camera);
    void drawSky(const SkyState& sky);
    void drawClouds(Vector3 cameraPosition, float nightAmount,
                    const WorldLighting& lighting);
    void endWorldPass();
    void beginUiOnlyFrame(Color clearColor);
    void endFrame();
    void drawScenePreview(Rectangle bounds);
    [[nodiscard]] bool beginFirstPersonToolPass();
    void endFirstPersonToolPass(
        const FirstPersonToolTuning& tuning);

    [[nodiscard]] bool beginShadowPass(const WorldLighting& lighting,
                                       Vector3 focus);
    void endShadowPass();
    [[nodiscard]] bool beginSelectionMaskPass(const Camera3D& camera);
    void setSelectionMaskWind(float amount);
    void setSelectionMaskColor(Color color);
    void setSelectionOutlineTint(Color tint);
    void setSelectionOutlineBounds(BoundingBox worldBounds);
    void endSelectionMaskPass();
    void clearSelectionOutline();
    void drawSelectionOutline();
    [[nodiscard]] bool shadowCasterVisible(Vector3 position,
                                           float radius = 0.0F) const;

    void beginWorldShader(const WorldLighting& lighting);
    void setWorldMaterial(const WorldMaterialState& material);
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
        bool wireframe = false);
    void drawPondDecor();
    void drawPondShoreRocks();
    void drawPondSurfaceDecor();
    void drawWater(Vector3 cameraPosition,
                   const WorldLighting& lighting);
    [[nodiscard]] std::optional<double>
    buildingRaycastDistance(
        const BuildingInstance& building,
        std::span<const BuildingInstance> buildings,
        Ray ray, double maxDistance,
        float defensiveYaw = 0.0F,
        float cannonPitch = 0.0F);
    [[nodiscard]] std::optional<double>
    resourceRaycastDistance(
        ResourceType type, Vector3 position,
        Ray ray, double maxDistance,
        std::size_t visualVariant = 0U,
        float visualScale = 1.0F,
        float yawRadians = 0.0F);
    [[nodiscard]] std::optional<double>
    platformFrameRaycastDistance(
        Vector3 topCenter, float scale,
        const std::array<float, 4>& supportLengths,
        Ray ray, double maxDistance);
    [[nodiscard]] bool drawCannon(Vector3 position, float yawRadians,
                                  float pitchRadians,
                                  Color tint = WHITE,
                                  float scale = 1.0F);
    [[nodiscard]] bool drawCannonball(Vector3 position,
                                      Color tint = WHITE);
    [[nodiscard]] bool drawArrow(Vector3 position, Vector3 direction,
                                 Color tint = WHITE);
    [[nodiscard]] bool drawCrossbow(Vector3 position, float yawRadians,
                                    Color tint = WHITE,
                                    float scale = 1.0F);
    [[nodiscard]] bool drawCore(Vector3 position, float yawRadians = 0.0F,
                                Color tint = WHITE,
                                float scale = 1.0F);
    [[nodiscard]] bool drawChallengeColumn(
        Vector3 position, float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawChallengeArenaPeg(
        Vector3 position, float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawFirstPersonTool(
        FirstPersonToolVisual visual, float swingProgress,
        float movementPhase, float movementAmount,
        const FirstPersonToolTuning& tuning,
        float iceChargeProgress = 0.0F,
        float iceRecoilProgress = 0.0F);
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
        float yawRadians, float scale) const;
    [[nodiscard]] bool drawPlatformFrameModel(
        Vector3 topCenter, Color tint = WHITE,
        float scale = 1.0F,
        const std::array<float, 4>& supportLengths = {});
    [[nodiscard]] bool drawRampModel(
        Vector3 footprintCenter, float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawMine(Vector3 position,
                                float yawRadians = 0.0F,
                                Color tint = WHITE,
                                float scale = 1.0F);
    [[nodiscard]] bool drawResourceProducer(
        BuildingType type, Vector3 position,
        float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawSpikeTrap(
        Vector3 position, float yawRadians = 0.0F,
        float animationSeconds = -1.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawRock(Vector3 position,
                                Color tint = WHITE,
                                float scale = 1.0F,
                                std::size_t visualVariant = 0U,
                                float yawRadians = 0.0F);
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
                                float scale = 1.0F);
    [[nodiscard]] bool drawEnemy(
        EnemyModelVisual modelVisual,
        EnemyAnimationVisual animationVisual,
        float animationSeconds, Vector3 position,
        float yawRadians, Color tint = WHITE,
        float scale = 1.0F, bool loop = true,
        bool inkOutlineEligible = true,
        bool quantizeCrowdPose = false);
    [[nodiscard]] bool drawEnemiesInstanced(
        std::span<const EnemyDrawInstance> instances);
    void drawGrassInstances(Vector3 cameraPosition,
                            float worldLimit, float nightAmount,
                            const WorldLighting& lighting,
                            std::span<const GrassClearArea>
                                clearAreas = {});
    void drawDecorativeRocks(
        Vector3 cameraPosition, float worldLimit,
        std::span<const GrassClearArea> clearAreas = {});
    void drawDecorativeRockAo(
        Vector3 cameraPosition, float worldLimit,
        std::span<const GrassClearArea> clearAreas = {});
    void drawBoundaryForest();
    void setWorldReveal(Vector2 origin,
                        float elapsedSeconds);
    [[nodiscard]] float worldRevealScaleAt(
        Vector2 position) const;
    void drawUpgradeEffect(Vector3 position, float progress,
                           float scale = 1.0F);
    [[nodiscard]] bool beginBlobShadowBatch(Vector3 cameraPosition);
    void drawBlobShadow(Vector3 groundPosition, float radiusX, float radiusZ,
                        float opacity, int segmentCountOverride = 0);
    void endBlobShadowBatch();

    [[nodiscard]] GraphicsSettings& settings();
    [[nodiscard]] const GraphicsSettings& settings() const;
    [[nodiscard]] const RendererPerformanceStats& performanceStats() const;

  private:
    struct EnemyBatchKey {
        EnemyModelVisual model{};
        EnemyAnimationVisual animation{};
        int frame{};
        std::uint32_t tint{};
        int scale{};
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
        int timeSeconds{-1};
        int windAmount{-1};
        int localWindHeight{-1};
        int distantFadeAmount{-1};
        int vegetationAmount{-1};
        int hitFlashAmount{-1};
        int selectionAmount{-1};
        int selectionTint{-1};
        int ghostAmount{-1};
        int ghostTint{-1};
        int ghostOpacity{-1};
        int shadowMap{-1};
        int lightViewProjection{-1};
        int shadowsEnabled{-1};
        int constantBias{-1};
        int slopeBias{-1};
        int shadowStrength{-1};
        int shadowMapTexelSize{-1};
        int instancingEnabled{-1};
        int inkOutlineEligible{-1};
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
        int paperGrainEnabled{-1};
        int paperGrainStrength{-1};
        int sceneDepth{-1};
        int sceneNormal{-1};
        int ssaoTexture{-1};
        int inverseProjection{-1};
        int ssaoTexelSize{-1};
        int ssaoEnabled{-1};
        int ssaoStrength{-1};
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
    void drawSsaoPass();
    void uploadPostProcessSettings();
    void uploadWorldLighting(const WorldLighting& lighting);
    void uploadWorldMaterial(const WorldMaterialState& material);
    void bindTerrainTexture();
    void bindShadowMap();
    void rebuildPondDecorInstances();
    void drawPondDecorInstances(std::size_t beginVariant,
                                std::size_t endVariant);
    void drawPondShoreRockInstances();
    void setSkinningEnabled(Shader& shader, bool enabled);
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
    GraphicsResources resources_;
    TerrainRenderer terrainRenderer_;
    DecorationExclusionMap decorationExclusionMap_;
    const TerrainHeightfield* terrainHeightfield_{};
    WorldShaderLocations worldShaderLocations_;
    SkyShaderLocations skyShaderLocations_;
    PostProcessLocations postProcessLocations_;
    SsaoLocations ssaoLocations_;
    WorldMaterialState worldMaterial_;
    std::optional<WorldMaterialState>
        ghostPreviewRestoreMaterial_;
    int selectionOutlineTexelSizeLocation_{-1};
    int selectionOutlineRadiusLocation_{-1};
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
    int waterCameraPositionLocation_{-1};
    int waterShallowColorLocation_{-1};
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
    int upgradeEffectOriginLocation_{-1};
    int upgradeEffectHeightLocation_{-1};
    int upgradeEffectProgressLocation_{-1};
    int upgradeEffectTimeLocation_{-1};
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
    Color selectionOutlineTint_{WHITE};
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
    std::array<std::vector<Matrix>, 4>
        pondShoreRockTransforms_;
    bool boundaryForestCached_{};
    Vector2 worldRevealOrigin_{};
    float worldRevealElapsed_{1000.0F};
    std::vector<Transform> enemyAnimationPose_;
    std::vector<Transform*> enemyAnimationFrames_;
    std::array<std::array<std::vector<int>, 2>, 7>
        enemyBoneMappings_;
    std::map<EnemyBatchKey, EnemyBatch> enemyBatches_;
    std::vector<EnemyBatch*> activeEnemyBatches_;
    std::map<EnemyPoseKey, std::vector<Matrix>> enemyBonePoseCache_;
    Model enemyCrowdLodModel_{};
    RendererPerformanceStats performanceStats_{};
    double instancedEnemyMillisecondsThisFrame_{};
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
