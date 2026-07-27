#include "graphics/GraphicsResources.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace ian {

ShaderResource::~ShaderResource() {
    unload();
}

bool ShaderResource::load(const char* vertexPath, const char* fragmentPath) {
    unload();
    shader_ = LoadShader(vertexPath, fragmentPath);
    loaded_ = IsShaderValid(shader_);
    return loaded_;
}

void ShaderResource::unload() {
    if (loaded_) {
        UnloadShader(shader_);
        shader_ = {};
        loaded_ = false;
    }
}

bool ShaderResource::valid() const {
    return loaded_;
}

Shader& ShaderResource::get() {
    return shader_;
}

const Shader& ShaderResource::get() const {
    return shader_;
}

TextureResource::~TextureResource() {
    unload();
}

bool TextureResource::load(const char* path) {
    unload();
    texture_ = LoadTexture(path);
    loaded_ = IsTextureValid(texture_);
    return loaded_;
}

void TextureResource::unload() {
    if (loaded_) {
        UnloadTexture(texture_);
        texture_ = {};
        loaded_ = false;
    }
}

bool TextureResource::valid() const {
    return loaded_;
}

Texture2D& TextureResource::get() {
    return texture_;
}

const Texture2D& TextureResource::get() const {
    return texture_;
}

ModelResource::~ModelResource() {
    unload();
}

bool ModelResource::load(const char* path) {
    unload();
    model_ = LoadModel(path);
    loaded_ = IsModelValid(model_);
    return loaded_;
}

void ModelResource::unload() {
    if (loaded_) {
        UnloadModel(model_);
        model_ = {};
        loaded_ = false;
    }
}

bool ModelResource::valid() const {
    return loaded_;
}

Model& ModelResource::get() {
    return model_;
}

const Model& ModelResource::get() const {
    return model_;
}

RenderTextureResource::~RenderTextureResource() {
    unload();
}

bool RenderTextureResource::load(int width, int height) {
    unload();
    if (width <= 0 || height <= 0) {
        return false;
    }

    target_ = LoadRenderTexture(width, height);
    loaded_ = IsRenderTextureValid(target_);
    if (loaded_) {
        SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
    }
    return loaded_;
}

void RenderTextureResource::unload() {
    if (loaded_) {
        UnloadRenderTexture(target_);
        target_ = {};
        loaded_ = false;
    }
}

bool RenderTextureResource::valid() const {
    return loaded_;
}

RenderTexture2D& RenderTextureResource::get() {
    return target_;
}

const RenderTexture2D& RenderTextureResource::get() const {
    return target_;
}

ShadowMapResource::~ShadowMapResource() {
    unload();
}

bool ShadowMapResource::load(int size) {
    unload();
    if (size <= 0) {
        return false;
    }

    target_.id = rlLoadFramebuffer();
    target_.texture = {
        .id = rlLoadTexture(nullptr, size, size,
                            RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1),
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    target_.depth = {
        .id = rlLoadTextureDepth(size, size, false),
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R32,
    };

    if (target_.id == 0U || target_.texture.id == 0U ||
        target_.depth.id == 0U) {
        if (target_.id != 0U) {
            rlUnloadFramebuffer(target_.id);
        }
        if (target_.texture.id != 0U) {
            rlUnloadTexture(target_.texture.id);
        }
        if (target_.depth.id != 0U) {
            rlUnloadTexture(target_.depth.id);
        }
        target_ = {};
        return false;
    }

    rlFramebufferAttach(target_.id, target_.texture.id,
                        RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target_.id, target_.depth.id, RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    loaded_ = rlFramebufferComplete(target_.id);
    if (!loaded_) {
        unload();
    }
    return loaded_;
}

void ShadowMapResource::unload() {
    if (target_.id != 0U) {
        rlUnloadFramebuffer(target_.id);
        target_.id = 0U;
        target_.depth.id = 0U;
    } else if (target_.depth.id != 0U) {
        rlUnloadTexture(target_.depth.id);
        target_.depth.id = 0U;
    }
    if (target_.texture.id != 0U) {
        rlUnloadTexture(target_.texture.id);
    }
    target_ = {};
    loaded_ = false;
}

bool ShadowMapResource::valid() const {
    return loaded_;
}

int ShadowMapResource::size() const {
    return target_.texture.width;
}

const RenderTexture2D& ShadowMapResource::target() const {
    return target_;
}

const Texture2D& ShadowMapResource::depthTexture() const {
    return target_.depth;
}

GraphicsResources::~GraphicsResources() {
    shutdown();
}

void GraphicsResources::initialize(const GraphicsSettings& settings) {
    initialized_ = true;
    worldShader_.load("assets/shaders/world.vs", "assets/shaders/world.fs");
    shadowShader_.load("assets/shaders/shadow.vs", "assets/shaders/shadow.fs");
    shadowDebugShader_.load(nullptr, "assets/shaders/shadow_debug.fs");
    skyShader_.load(nullptr, "assets/shaders/sky.fs");
    cannonModel_.load("assets/models/cannon.glb");
    cannonballModel_.load("assets/models/cannonball.glb");
    arrowModel_.load("assets/models/arrow.glb");
    crossbowModel_.load("assets/models/crossbow.glb");
    coreModel_.load("assets/models/core.glb");
    rockModel_.load("assets/models/rock.glb");
    treeModel_.load("assets/models/tree.glb");
    updateFramebuffer(settings);
    updateShadowMap(settings);
}

void GraphicsResources::updateFramebuffer(const GraphicsSettings& settings) {
    if (!initialized_) {
        return;
    }

    const int framebufferWidth = GetRenderWidth();
    const int framebufferHeight = GetRenderHeight();
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    const float scale = std::clamp(settings.renderScale, 0.5F, 1.0F);
    const int desiredWidth =
        std::max(1, static_cast<int>(std::lround(static_cast<float>(framebufferWidth) * scale)));
    const int desiredHeight =
        std::max(1, static_cast<int>(std::lround(static_cast<float>(framebufferHeight) * scale)));
    if (desiredWidth == requestedSceneWidth_ && desiredHeight == requestedSceneHeight_) {
        return;
    }

    requestedSceneWidth_ = desiredWidth;
    requestedSceneHeight_ = desiredHeight;
    sceneTarget_.load(desiredWidth, desiredHeight);
}

void GraphicsResources::updateShadowMap(const GraphicsSettings& settings) {
    if (!initialized_) {
        return;
    }
    if (!settings.shadows) {
        shadowMap_.unload();
        requestedShadowMapSize_ = 0;
        return;
    }

    const int desiredSize = std::clamp(settings.shadowMapSize, 512, 4096);
    if (desiredSize == requestedShadowMapSize_) {
        return;
    }
    requestedShadowMapSize_ = desiredSize;
    shadowMap_.load(desiredSize);
}

void GraphicsResources::shutdown() {
    shadowMap_.unload();
    treeModel_.unload();
    rockModel_.unload();
    coreModel_.unload();
    crossbowModel_.unload();
    arrowModel_.unload();
    cannonballModel_.unload();
    cannonModel_.unload();
    placeholderModel_.unload();
    fallbackTexture_.unload();
    skyShader_.unload();
    shadowDebugShader_.unload();
    shadowShader_.unload();
    worldShader_.unload();
    sceneTarget_.unload();
    requestedSceneWidth_ = 0;
    requestedSceneHeight_ = 0;
    requestedShadowMapSize_ = 0;
    initialized_ = false;
}

bool GraphicsResources::sceneTargetValid() const {
    return sceneTarget_.valid();
}

const RenderTexture2D& GraphicsResources::sceneTarget() const {
    return sceneTarget_.get();
}

int GraphicsResources::sceneWidth() const {
    return requestedSceneWidth_;
}

int GraphicsResources::sceneHeight() const {
    return requestedSceneHeight_;
}

ShaderResource& GraphicsResources::worldShader() {
    return worldShader_;
}

const ShaderResource& GraphicsResources::worldShader() const {
    return worldShader_;
}

ShaderResource& GraphicsResources::shadowShader() {
    return shadowShader_;
}

const ShaderResource& GraphicsResources::shadowShader() const {
    return shadowShader_;
}

ShaderResource& GraphicsResources::shadowDebugShader() {
    return shadowDebugShader_;
}

const ShaderResource& GraphicsResources::shadowDebugShader() const {
    return shadowDebugShader_;
}

ShaderResource& GraphicsResources::skyShader() {
    return skyShader_;
}

const ShaderResource& GraphicsResources::skyShader() const {
    return skyShader_;
}

TextureResource& GraphicsResources::fallbackTexture() {
    return fallbackTexture_;
}

ModelResource& GraphicsResources::placeholderModel() {
    return placeholderModel_;
}

ModelResource& GraphicsResources::cannonModel() {
    return cannonModel_;
}

ModelResource& GraphicsResources::cannonballModel() {
    return cannonballModel_;
}

ModelResource& GraphicsResources::arrowModel() {
    return arrowModel_;
}

ModelResource& GraphicsResources::crossbowModel() {
    return crossbowModel_;
}

ModelResource& GraphicsResources::coreModel() {
    return coreModel_;
}

ModelResource& GraphicsResources::rockModel() {
    return rockModel_;
}

ModelResource& GraphicsResources::treeModel() {
    return treeModel_;
}

ShadowMapResource& GraphicsResources::shadowMap() {
    return shadowMap_;
}

const ShadowMapResource& GraphicsResources::shadowMap() const {
    return shadowMap_;
}

} // namespace ian
