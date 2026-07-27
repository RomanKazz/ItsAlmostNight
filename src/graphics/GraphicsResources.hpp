#pragma once

#include "graphics/GraphicsSettings.hpp"

#include <raylib.h>

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
    void updateShadowMap(const GraphicsSettings& settings);
    void shutdown();

    [[nodiscard]] bool sceneTargetValid() const;
    [[nodiscard]] const RenderTexture2D& sceneTarget() const;
    [[nodiscard]] int sceneWidth() const;
    [[nodiscard]] int sceneHeight() const;

    [[nodiscard]] ShaderResource& worldShader();
    [[nodiscard]] const ShaderResource& worldShader() const;
    [[nodiscard]] ShaderResource& shadowShader();
    [[nodiscard]] const ShaderResource& shadowShader() const;
    [[nodiscard]] ShaderResource& shadowDebugShader();
    [[nodiscard]] const ShaderResource& shadowDebugShader() const;
    [[nodiscard]] ShaderResource& skyShader();
    [[nodiscard]] const ShaderResource& skyShader() const;
    [[nodiscard]] TextureResource& fallbackTexture();
    [[nodiscard]] ModelResource& placeholderModel();
    [[nodiscard]] ModelResource& cannonModel();
    [[nodiscard]] ModelResource& cannonballModel();
    [[nodiscard]] ModelResource& arrowModel();
    [[nodiscard]] ModelResource& crossbowModel();
    [[nodiscard]] ModelResource& coreModel();
    [[nodiscard]] ModelResource& rockModel();
    [[nodiscard]] ModelResource& treeModel();
    [[nodiscard]] ShadowMapResource& shadowMap();
    [[nodiscard]] const ShadowMapResource& shadowMap() const;

  private:
    ShaderResource worldShader_;
    ShaderResource shadowShader_;
    ShaderResource shadowDebugShader_;
    ShaderResource skyShader_;
    TextureResource fallbackTexture_;
    ModelResource placeholderModel_;
    ModelResource cannonModel_;
    ModelResource cannonballModel_;
    ModelResource arrowModel_;
    ModelResource crossbowModel_;
    ModelResource coreModel_;
    ModelResource rockModel_;
    ModelResource treeModel_;
    RenderTextureResource sceneTarget_;
    ShadowMapResource shadowMap_;
    int requestedSceneWidth_{};
    int requestedSceneHeight_{};
    int requestedShadowMapSize_{};
    bool initialized_{};
};

} // namespace ian
