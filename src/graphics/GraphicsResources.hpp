#pragma once

#include "graphics/GraphicsSettings.hpp"

#include <raylib.h>

#include <string_view>

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

  private:
    Model model_{};
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

    [[nodiscard]] ShaderResource& worldShader();
    [[nodiscard]] const ShaderResource& worldShader() const;
    [[nodiscard]] ShaderResource& shadowShader();
    [[nodiscard]] const ShaderResource& shadowShader() const;
    [[nodiscard]] ShaderResource& shadowDebugShader();
    [[nodiscard]] const ShaderResource& shadowDebugShader() const;
    [[nodiscard]] ShaderResource& skyShader();
    [[nodiscard]] const ShaderResource& skyShader() const;
    [[nodiscard]] ShaderResource& selectionMaskShader();
    [[nodiscard]] const ShaderResource& selectionMaskShader() const;
    [[nodiscard]] ShaderResource& selectionOutlineShader();
    [[nodiscard]] const ShaderResource& selectionOutlineShader() const;
    [[nodiscard]] ShaderResource& postProcessShader();
    [[nodiscard]] const ShaderResource& postProcessShader() const;
    [[nodiscard]] ShaderResource& grassShader();
    [[nodiscard]] const ShaderResource& grassShader() const;
    [[nodiscard]] ShaderResource& upgradeEffectShader();
    [[nodiscard]] const ShaderResource& upgradeEffectShader() const;
    [[nodiscard]] TextureResource& fallbackTexture();
    [[nodiscard]] TextureResource& terrainTexture();
    [[nodiscard]] const TextureResource& terrainTexture() const;
    [[nodiscard]] ModelResource& placeholderModel();
    [[nodiscard]] ModelResource& cannonModel();
    [[nodiscard]] ModelResource& cannonballModel();
    [[nodiscard]] ModelResource& arrowModel();
    [[nodiscard]] ModelResource& crossbowModel();
    [[nodiscard]] ModelResource& coreModel();
    [[nodiscard]] ModelResource& platformModel();
    [[nodiscard]] ModelResource& rampModel();
    [[nodiscard]] ModelResource& mineModel();
    [[nodiscard]] ModelResource& lumberMillModel();
    [[nodiscard]] ModelResource& quarryModel();
    [[nodiscard]] ModelResource& rockModel();
    [[nodiscard]] ModelResource& treeModel();
    [[nodiscard]] ModelResource& wallIsolatedModel();
    [[nodiscard]] ModelResource& wallEndModel();
    [[nodiscard]] ModelResource& wallCornerModel();
    [[nodiscard]] ModelResource& wallTModel();
    [[nodiscard]] ModelResource& wallCrossModel();
    [[nodiscard]] ModelResource& grassModelB();
    [[nodiscard]] ModelResource& grassModelC();
    [[nodiscard]] ModelResource& grassModelD();
    [[nodiscard]] ModelResource& enemyMinionModel();
    [[nodiscard]] ModelResource& enemyRogueModel();
    [[nodiscard]] ModelResource& enemyWarriorModel();
    [[nodiscard]] ModelResource& enemyMageModel();
    [[nodiscard]] ModelResource& enemySapperModel();
    [[nodiscard]] ModelResource& enemyFlyingModel();
    [[nodiscard]] ModelResource& enemyBossModel();
    [[nodiscard]] const ModelAnimationsResource&
        enemyGeneralAnimations() const;
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
    ShaderResource selectionMaskShader_;
    ShaderResource selectionOutlineShader_;
    ShaderResource postProcessShader_;
    ShaderResource grassShader_;
    ShaderResource upgradeEffectShader_;
    TextureResource fallbackTexture_;
    TextureResource terrainTexture_;
    ModelResource placeholderModel_;
    ModelResource cannonModel_;
    ModelResource cannonballModel_;
    ModelResource arrowModel_;
    ModelResource crossbowModel_;
    ModelResource coreModel_;
    ModelResource platformModel_;
    ModelResource rampModel_;
    ModelResource mineModel_;
    ModelResource lumberMillModel_;
    ModelResource quarryModel_;
    ModelResource rockModel_;
    ModelResource treeModel_;
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
    ModelAnimationsResource enemyMovementAnimations_;
    ModelAnimationsResource enemySapperAnimations_;
    ModelAnimationsResource enemyFlyingAnimations_;
    ModelAnimationsResource enemyBossAnimations_;
    RenderTextureResource sceneTarget_;
    RenderTextureResource selectionMaskTarget_;
    ShadowMapResource shadowMap_;
    int requestedSceneWidth_{};
    int requestedSceneHeight_{};
    int requestedSelectionMaskWidth_{};
    int requestedSelectionMaskHeight_{};
    int requestedShadowMapSize_{};
    bool initialized_{};
};

} // namespace ian
