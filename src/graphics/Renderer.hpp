#pragma once

#include "buildings/BuildingSystem.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/GraphicsSettings.hpp"
#include "graphics/TerrainRenderer.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ian {

struct WorldLighting {
    Vector3 cameraPosition{};
    Vector3 sunDirection{-0.45F, -1.0F, -0.25F};
    Vector3 sunColor{1.0F, 0.86F, 0.68F};
    float sunIntensity{1.0F};
    Vector3 skyAmbientColor{0.42F, 0.52F, 0.68F};
    Vector3 groundAmbientColor{0.16F, 0.21F, 0.17F};
    float ambientIntensity{0.65F};
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
    float terrainAmount{};
    Vector3 terrainPrimaryTint{0.78F, 1.08F, 0.72F};
    Vector3 terrainSecondaryTint{1.10F, 0.98F, 0.62F};
    Vector3 terrainPatchTint{0.78F, 0.70F, 0.50F};
    float windAmount{};
    float hitFlashAmount{};
    float selectionAmount{};
    Vector3 selectionTint{1.0F, 0.72F, 0.2F};
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

struct EnemyDrawInstance {
    EnemyModelVisual modelVisual{EnemyModelVisual::Minion};
    EnemyAnimationVisual animationVisual{EnemyAnimationVisual::Idle};
    float animationSeconds{};
    Vector3 position{};
    float yawRadians{};
    Color tint{WHITE};
    float scale{1.0F};
    bool loop{true};
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
    void adjustPixelSize(int direction);

    void beginWorldPass(Color clearColor);
    void drawSky(const SkyState& sky);
    void endWorldPass();
    void beginUiOnlyFrame(Color clearColor);
    void endFrame();
    void drawScenePreview(Rectangle bounds);

    [[nodiscard]] bool beginShadowPass(const WorldLighting& lighting,
                                       Vector3 focus);
    void endShadowPass();
    [[nodiscard]] bool beginSelectionMaskPass(const Camera3D& camera);
    void setSelectionMaskWind(float amount);
    void endSelectionMaskPass();
    void clearSelectionOutline();
    void drawSelectionOutline();
    [[nodiscard]] bool shadowCasterVisible(Vector3 position,
                                           float radius = 0.0F) const;

    void beginWorldShader(const WorldLighting& lighting);
    void setWorldMaterial(const WorldMaterialState& material);
    void endWorldShader();
    void rebuildTerrain(
        const TerrainHeightfield& terrain);
    void drawTerrain(
        Color tint, bool wireframe = false);
    [[nodiscard]] std::optional<double>
    buildingRaycastDistance(
        const BuildingInstance& building,
        std::span<const BuildingInstance> buildings,
        Ray ray, double maxDistance,
        float defensiveYaw = 0.0F,
        float cannonPitch = 0.0F);
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
    [[nodiscard]] bool drawPlatformFrameModel(
        Vector3 topCenter, Color tint = WHITE,
        float scale = 1.0F,
        const std::array<float, 4>& supportLengths = {});
    [[nodiscard]] bool drawMine(Vector3 position,
                                float yawRadians = 0.0F,
                                Color tint = WHITE,
                                float scale = 1.0F);
    [[nodiscard]] bool drawResourceProducer(
        BuildingType type, Vector3 position,
        float yawRadians = 0.0F,
        Color tint = WHITE, float scale = 1.0F);
    [[nodiscard]] bool drawRock(Vector3 position,
                                Color tint = WHITE,
                                float scale = 1.0F);
    [[nodiscard]] bool drawTree(Vector3 position,
                                Color tint = WHITE,
                                float scale = 1.0F);
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
        float scale = 1.0F, bool loop = true);
    [[nodiscard]] bool drawEnemiesInstanced(
        std::span<const EnemyDrawInstance> instances);
    void drawGrassInstances(Vector3 cameraPosition,
                            float worldLimit, float nightAmount,
                            const WorldLighting& lighting,
                            std::span<const GrassClearArea>
                                clearAreas = {});
    void drawUpgradeEffect(Vector3 position, float progress,
                           float scale = 1.0F);
    [[nodiscard]] bool beginBlobShadowBatch(Vector3 cameraPosition);
    void drawBlobShadow(Vector3 groundPosition, float radiusX, float radiusZ,
                        float opacity);
    void endBlobShadowBatch();

    [[nodiscard]] GraphicsSettings& settings();
    [[nodiscard]] const GraphicsSettings& settings() const;

  private:
    struct WorldShaderLocations {
        int baseColor{-1};
        int cameraPosition{-1};
        int sunDirection{-1};
        int sunColor{-1};
        int sunIntensity{-1};
        int skyAmbientColor{-1};
        int groundAmbientColor{-1};
        int ambientIntensity{-1};
        int fogColor{-1};
        int fogStart{-1};
        int fogEnd{-1};
        int fogBandsEnabled{-1};
        int fogBandCount{-1};
        int dayNightTint{-1};
        int exposure{-1};
        int saturation{-1};
        int bakedAo{-1};
        int vertexAoAmount{-1};
        int aoStrength{-1};
        int terrainAmount{-1};
        int terrainPrimaryTint{-1};
        int terrainSecondaryTint{-1};
        int terrainPatchTint{-1};
        int terrainTexture{-1};
        int terrainTextureEnabled{-1};
        int timeSeconds{-1};
        int windAmount{-1};
        int hitFlashAmount{-1};
        int selectionAmount{-1};
        int selectionTint{-1};
        int shadowMap{-1};
        int lightViewProjection{-1};
        int shadowsEnabled{-1};
        int constantBias{-1};
        int slopeBias{-1};
        int shadowStrength{-1};
        int shadowMapTexelSize{-1};
        int instancingEnabled{-1};
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
        int paletteEnabled{-1};
        int paletteLevels{-1};
        int ditherEnabled{-1};
        int ditherStrength{-1};
        int posterizedLightingEnabled{-1};
        int lightingSteps{-1};
        int bloomEnabled{-1};
        int bloomStrength{-1};
        int inkOutlinesEnabled{-1};
        int outlineStrength{-1};
        int paperGrainEnabled{-1};
        int paperGrainStrength{-1};
    };

    void resolveWorldShaderLocations();
    void resolveSkyShaderLocations();
    void resolvePostProcessLocations();
    void uploadPostProcessSettings();
    void uploadWorldLighting(const WorldLighting& lighting);
    void uploadWorldMaterial(const WorldMaterialState& material);
    void bindTerrainTexture();
    void bindShadowMap();
    void setSkinningEnabled(Shader& shader, bool enabled);
    void drawShadowMapDebug() const;
    GraphicsSettings settings_;
    GraphicsResources resources_;
    TerrainRenderer terrainRenderer_;
    const TerrainHeightfield* terrainHeightfield_{};
    WorldShaderLocations worldShaderLocations_;
    SkyShaderLocations skyShaderLocations_;
    PostProcessLocations postProcessLocations_;
    WorldMaterialState worldMaterial_;
    int selectionOutlineTexelSizeLocation_{-1};
    int selectionOutlineRadiusLocation_{-1};
    int selectionMaskTimeLocation_{-1};
    int selectionMaskWindLocation_{-1};
    int worldSkinningEnabledLocation_{-1};
    int worldInstancingEnabledLocation_{-1};
    int shadowSkinningEnabledLocation_{-1};
    int selectionMaskSkinningEnabledLocation_{-1};
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
    int upgradeEffectOriginLocation_{-1};
    int upgradeEffectHeightLocation_{-1};
    int upgradeEffectProgressLocation_{-1};
    int upgradeEffectTimeLocation_{-1};
    Matrix lightViewProjection_{};
    Vector3 shadowFocus_{};
    Vector3 blobShadowCamera_{};
    bool showDebugPanel_{};
    bool showShadowMap_{};
    bool frameOpen_{};
    bool worldPassOpen_{};
    bool usingOffscreenTarget_{};
    bool worldShaderActive_{};
    bool shadowPassOpen_{};
    bool shadowFrameValid_{};
    bool selectionMaskPassOpen_{};
    bool selectionMaskReady_{};
    bool blobShadowBatchOpen_{};
    std::vector<Transform> enemyAnimationPose_;
    std::vector<Transform*> enemyAnimationFrames_;
};

} // namespace ian
