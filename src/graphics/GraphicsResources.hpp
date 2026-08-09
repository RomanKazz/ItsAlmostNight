#pragma once

#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsSettings.hpp"

#include <raylib.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ian {

class ShaderResource {
  public:
    ShaderResource() = default;
    ~ShaderResource();

    ShaderResource(const ShaderResource&) = delete;
    ShaderResource& operator=(const ShaderResource&) = delete;
    ShaderResource(ShaderResource&&) = delete;
    ShaderResource& operator=(ShaderResource&&) = delete;

    bool load(const char* vertexPath, const char* fragmentPath);
    void unload();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] Shader& get();
    [[nodiscard]] const Shader& get() const;

  private:
    Shader shader_{};
    bool loaded_{};
};

class TextureResource {
  public:
    TextureResource() = default;
    ~TextureResource();

    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;
    TextureResource(TextureResource&&) = delete;
    TextureResource& operator=(TextureResource&&) = delete;

    bool load(const char* path);
    void unload();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] Texture2D& get();
    [[nodiscard]] const Texture2D& get() const;

  private:
    Texture2D texture_{};
    bool loaded_{};
};

class ModelResource {
  public:
    ModelResource() = default;
    ~ModelResource();

    ModelResource(const ModelResource&) = delete;
    ModelResource& operator=(const ModelResource&) = delete;
    ModelResource(ModelResource&&) = delete;
    ModelResource& operator=(ModelResource&&) = delete;

    bool load(const char* path);
    void unload();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] Model& get();
    [[nodiscard]] const Model& get() const;
    [[nodiscard]] const GlbCollisionAsset&
    collisionAsset() const;
    [[nodiscard]] const BoundingBox& visualBounds() const;
    [[nodiscard]] std::span<const BoundingBox>
    meshBounds() const;
    [[nodiscard]] bool meshValid(std::size_t index) const;
    [[nodiscard]] bool meshHasSkinning(std::size_t index) const;
    [[nodiscard]] bool meshSkinningValid(std::size_t index) const;
    [[nodiscard]] bool gpuSkinningCompatible() const;
    [[nodiscard]] bool runtimeBoneMatricesFinite() const;

  private:
    Model model_{};
    GlbCollisionAsset collisionAsset_;
    BoundingBox visualBounds_{};
    std::vector<BoundingBox> meshBounds_;
    std::vector<bool> meshValid_;
    std::vector<bool> meshHasSkinning_;
    std::vector<bool> meshSkinningValid_;
    std::string path_;
    bool gpuSkinningCompatible_{};
    mutable bool runtimeBoneWarningLogged_{};
    bool loaded_{};
};

class ModelAnimationsResource {
  public:
    ModelAnimationsResource() = default;
    ~ModelAnimationsResource();

    ModelAnimationsResource(const ModelAnimationsResource&) = delete;
    ModelAnimationsResource& operator=(const ModelAnimationsResource&) = delete;
    ModelAnimationsResource(ModelAnimationsResource&&) = delete;
    ModelAnimationsResource& operator=(ModelAnimationsResource&&) = delete;

    bool load(const char* path);
    void unload();

    [[nodiscard]] const ModelAnimation* find(
        std::string_view name) const;

  private:
    ModelAnimation* animations_{};
    int count_{};
};

class RenderTextureResource {
  public:
    RenderTextureResource() = default;
    ~RenderTextureResource();

    RenderTextureResource(const RenderTextureResource&) = delete;
    RenderTextureResource& operator=(const RenderTextureResource&) = delete;
    RenderTextureResource(RenderTextureResource&&) = delete;
    RenderTextureResource& operator=(RenderTextureResource&&) = delete;

    bool load(int width, int height);
    void unload();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] RenderTexture2D& get();
    [[nodiscard]] const RenderTexture2D& get() const;

  private:
    RenderTexture2D target_{};
    bool loaded_{};
};

class ShadowMapResource {
  public:
    ShadowMapResource() = default;
    ~ShadowMapResource();

    ShadowMapResource(const ShadowMapResource&) = delete;
    ShadowMapResource& operator=(const ShadowMapResource&) = delete;
    ShadowMapResource(ShadowMapResource&&) = delete;
    ShadowMapResource& operator=(ShadowMapResource&&) = delete;

    bool load(int size);
    void unload();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] int size() const;
    [[nodiscard]] const RenderTexture2D& target() const;
    [[nodiscard]] const Texture2D& depthTexture() const;

  private:
    RenderTexture2D target_{};
    bool loaded_{};
};

class GraphicsResources {
  public:
    GraphicsResources() = default;
    ~GraphicsResources();

    GraphicsResources(const GraphicsResources&) = delete;
    GraphicsResources& operator=(const GraphicsResources&) = delete;
    GraphicsResources(GraphicsResources&&) = delete;
    GraphicsResources& operator=(GraphicsResources&&) = delete;

    void initialize(const GraphicsSettings& settings);
    void updateFramebuffer(const GraphicsSettings& settings);
    void updateViewModelTarget();
    void updateSelectionMask(const GraphicsSettings& settings);
    void updateShadowMap(const GraphicsSettings& settings);
    void shutdown();

    [[nodiscard]] bool sceneTargetValid() const;
    [[nodiscard]] const RenderTexture2D& sceneTarget() const;
    [[nodiscard]] int sceneWidth() const;
    [[nodiscard]] int sceneHeight() const;
    [[nodiscard]] bool selectionMaskValid() const;
    [[nodiscard]] const RenderTexture2D& selectionMask() const;
    [[nodiscard]] int selectionMaskWidth() const;
    [[nodiscard]] int selectionMaskHeight() const;
    [[nodiscard]] bool viewModelTargetValid() const;
    [[nodiscard]] const RenderTexture2D& viewModelTarget() const;

    [[nodiscard]] ShaderResource& worldShader();
    [[nodiscard]] const ShaderResource& worldShader() const;
    [[nodiscard]] ShaderResource& shadowShader();
    [[nodiscard]] const ShaderResource& shadowShader() const;
    [[nodiscard]] ShaderResource& shadowDebugShader();
    [[nodiscard]] const ShaderResource& shadowDebugShader() const;
    [[nodiscard]] ShaderResource& skyShader();
    [[nodiscard]] const ShaderResource& skyShader() const;
    [[nodiscard]] ShaderResource& cloudShader();
    [[nodiscard]] const ShaderResource& cloudShader() const;
    [[nodiscard]] ShaderResource& waterShader();
    [[nodiscard]] const ShaderResource& waterShader() const;
    [[nodiscard]] ShaderResource& selectionMaskShader();
    [[nodiscard]] const ShaderResource& selectionMaskShader() const;
    [[nodiscard]] ShaderResource& selectionOutlineShader();
    [[nodiscard]] const ShaderResource& selectionOutlineShader() const;
    [[nodiscard]] ShaderResource& postProcessShader();
    [[nodiscard]] const ShaderResource& postProcessShader() const;
    [[nodiscard]] ShaderResource& viewModelCompositeShader();
    [[nodiscard]] const ShaderResource& viewModelCompositeShader() const;
    [[nodiscard]] ShaderResource& grassShader();
    [[nodiscard]] const ShaderResource& grassShader() const;
    [[nodiscard]] ShaderResource& upgradeEffectShader();
    [[nodiscard]] const ShaderResource& upgradeEffectShader() const;
    [[nodiscard]] ShaderResource& iceMagicShader();
    [[nodiscard]] const ShaderResource& iceMagicShader() const;
    [[nodiscard]] TextureResource& fallbackTexture();
    [[nodiscard]] TextureResource& terrainTexture();
    [[nodiscard]] const TextureResource& terrainTexture() const;
    [[nodiscard]] ModelResource& placeholderModel();
    [[nodiscard]] ModelResource& cannonModel();
    [[nodiscard]] ModelResource& cannonballModel();
    [[nodiscard]] ModelResource& arrowModel();
    [[nodiscard]] ModelResource& crossbowModel();
    [[nodiscard]] ModelResource& coreModel();
    [[nodiscard]] ModelResource& axeModel();
    [[nodiscard]] ModelResource& pickaxeModel();
    [[nodiscard]] ModelResource& clubModel();
    [[nodiscard]] ModelResource& hammerModel();
    [[nodiscard]] ModelResource& iceWandModel();
    [[nodiscard]] ModelResource& woodenChestModel();
    [[nodiscard]] ModelResource& stoneChestModel();
    [[nodiscard]] ModelResource& appleLootModel();
    [[nodiscard]] ModelResource& breadLootModel();
    [[nodiscard]] ModelResource& ironBarLootModel();
    [[nodiscard]] ModelResource& fuelJerrycanLootModel();
    [[nodiscard]] ModelResource& compassLootModel();
    [[nodiscard]] ModelResource& nailLootModel();
    [[nodiscard]] ModelResource& keyLootModel();
    [[nodiscard]] ModelResource& mapLootModel();
    [[nodiscard]] ModelResource& anvilLootModel();
    [[nodiscard]] ModelResource& sawLootModel();
    [[nodiscard]] ModelResource& potionLootModel();
    [[nodiscard]] ModelResource& platformModel();
    [[nodiscard]] ModelResource& rampModel();
    [[nodiscard]] ModelResource& mineModel();
    [[nodiscard]] ModelResource& lumberMillModel();
    [[nodiscard]] ModelResource& quarryModel();
    [[nodiscard]] ModelResource& rockModel();
    [[nodiscard]] ModelResource& treeModel(std::size_t variant = 0U);
    [[nodiscard]] ModelResource& boundaryTreeModel(
        std::size_t variant = 0U);
    [[nodiscard]] ModelResource& decorativeRockModel(
        std::size_t variant);
    [[nodiscard]] ModelResource& decorativeBushModel(
        std::size_t variant);
    [[nodiscard]] ModelResource& pondDecorModel(
        std::size_t variant);
    [[nodiscard]] ModelResource& cloudModel(std::size_t variant);
    [[nodiscard]] ModelResource& wallIsolatedModel();
    [[nodiscard]] ModelResource& wallEndModel();
    [[nodiscard]] ModelResource& wallCornerModel();
    [[nodiscard]] ModelResource& wallTModel();
    [[nodiscard]] ModelResource& wallCrossModel();
    [[nodiscard]] ModelResource& grassModelB();
    [[nodiscard]] ModelResource& grassModelC();
    [[nodiscard]] ModelResource& grassModelD();
    [[nodiscard]] ModelResource& enemyMinionModel();
    [[nodiscard]] const ModelResource& enemyMinionModel() const;
    [[nodiscard]] ModelResource& enemyRogueModel();
    [[nodiscard]] const ModelResource& enemyRogueModel() const;
    [[nodiscard]] ModelResource& enemyWarriorModel();
    [[nodiscard]] const ModelResource& enemyWarriorModel() const;
    [[nodiscard]] ModelResource& enemyMageModel();
    [[nodiscard]] const ModelResource& enemyMageModel() const;
    [[nodiscard]] ModelResource& enemySapperModel();
    [[nodiscard]] const ModelResource& enemySapperModel() const;
    [[nodiscard]] ModelResource& enemyFlyingModel();
    [[nodiscard]] const ModelResource& enemyFlyingModel() const;
    [[nodiscard]] ModelResource& enemyBossModel();
    [[nodiscard]] const ModelResource& enemyBossModel() const;
    [[nodiscard]] const ModelAnimationsResource&
        enemyGeneralAnimations() const;
    [[nodiscard]] const ModelAnimationsResource&
        enemyPinkBlobAnimations() const;
    [[nodiscard]] const ModelAnimationsResource&
        enemyMovementAnimations() const;
    [[nodiscard]] const ModelAnimationsResource&
        enemySapperAnimations() const;
    [[nodiscard]] const ModelAnimationsResource&
        enemyFlyingAnimations() const;
    [[nodiscard]] const ModelAnimationsResource&
        enemyBossAnimations() const;
    [[nodiscard]] ShadowMapResource& shadowMap();
    [[nodiscard]] const ShadowMapResource& shadowMap() const;

  private:
    ShaderResource worldShader_;
    ShaderResource shadowShader_;
    ShaderResource shadowDebugShader_;
    ShaderResource skyShader_;
    ShaderResource cloudShader_;
    ShaderResource waterShader_;
    ShaderResource selectionMaskShader_;
    ShaderResource selectionOutlineShader_;
    ShaderResource postProcessShader_;
    ShaderResource viewModelCompositeShader_;
    ShaderResource grassShader_;
    ShaderResource upgradeEffectShader_;
    ShaderResource iceMagicShader_;
    TextureResource fallbackTexture_;
    TextureResource terrainTexture_;
    ModelResource placeholderModel_;
    ModelResource cannonModel_;
    ModelResource cannonballModel_;
    ModelResource arrowModel_;
    ModelResource crossbowModel_;
    ModelResource coreModel_;
    ModelResource axeModel_;
    ModelResource pickaxeModel_;
    ModelResource clubModel_;
    ModelResource hammerModel_;
    ModelResource iceWandModel_;
    ModelResource woodenChestModel_;
    ModelResource stoneChestModel_;
    ModelResource appleLootModel_;
    ModelResource breadLootModel_;
    ModelResource ironBarLootModel_;
    ModelResource fuelJerrycanLootModel_;
    ModelResource compassLootModel_;
    ModelResource nailLootModel_;
    ModelResource keyLootModel_;
    ModelResource mapLootModel_;
    ModelResource anvilLootModel_;
    ModelResource sawLootModel_;
    ModelResource potionLootModel_;
    ModelResource platformModel_;
    ModelResource rampModel_;
    ModelResource mineModel_;
    ModelResource lumberMillModel_;
    ModelResource quarryModel_;
    ModelResource rockModel_;
    std::array<ModelResource, 3> treeModels_;
    std::array<ModelResource, 2> boundaryTreeModels_;
    std::array<ModelResource, 4> decorativeRockModels_;
    std::array<ModelResource, 6> decorativeBushModels_;
    std::array<ModelResource, 5> pondDecorModels_;
    std::array<ModelResource, 2> cloudModels_;
    ModelResource wallIsolatedModel_;
    ModelResource wallEndModel_;
    ModelResource wallCornerModel_;
    ModelResource wallTModel_;
    ModelResource wallCrossModel_;
    ModelResource grassModelB_;
    ModelResource grassModelC_;
    ModelResource grassModelD_;
    ModelResource enemyMinionModel_;
    ModelResource enemyRogueModel_;
    ModelResource enemyWarriorModel_;
    ModelResource enemyMageModel_;
    ModelResource enemySapperModel_;
    ModelResource enemyFlyingModel_;
    ModelResource enemyBossModel_;
    ModelAnimationsResource enemyGeneralAnimations_;
    ModelAnimationsResource enemyPinkBlobAnimations_;
    ModelAnimationsResource enemyMovementAnimations_;
    ModelAnimationsResource enemySapperAnimations_;
    ModelAnimationsResource enemyFlyingAnimations_;
    ModelAnimationsResource enemyBossAnimations_;
    RenderTextureResource sceneTarget_;
    RenderTextureResource viewModelTarget_;
    RenderTextureResource selectionMaskTarget_;
    ShadowMapResource shadowMap_;
    int requestedSceneWidth_{};
    int requestedSceneHeight_{};
    int requestedViewModelWidth_{};
    int requestedViewModelHeight_{};
    int requestedSelectionMaskWidth_{};
    int requestedSelectionMaskHeight_{};
    int requestedShadowMapSize_{};
    bool initialized_{};
};

} // namespace ian
