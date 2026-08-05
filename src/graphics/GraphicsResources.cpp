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
    if ((vertexPath != nullptr && !FileExists(vertexPath)) ||
        (fragmentPath != nullptr && !FileExists(fragmentPath))) {
        return false;
    }
    shader_ = LoadShader(vertexPath, fragmentPath);
    loaded_ = IsShaderValid(shader_);
    if (!loaded_) {
        UnloadShader(shader_);
        shader_ = {};
    }
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
    if (!loaded_ && texture_.id != 0U) {
        UnloadTexture(texture_);
        texture_ = {};
    }
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
    collisionAsset_ = loadGlbCollisionAsset(path);
    model_ = LoadModel(path);
    loaded_ = IsModelValid(model_);
    if (!loaded_) {
        // LoadModel creates a default material even when no mesh could be
        // loaded, so an invalid model can still own heap allocations.
        UnloadModel(model_);
        model_ = {};
    } else {
        for (auto iterator =
                 collisionAsset_.renderMeshIndices.rbegin();
             iterator !=
                 collisionAsset_.renderMeshIndices.rend();
             ++iterator) {
            const std::size_t meshIndex = *iterator;
            if (meshIndex >=
                static_cast<std::size_t>(model_.meshCount)) {
                continue;
            }
            UnloadMesh(model_.meshes[meshIndex]);
            for (int index =
                     static_cast<int>(meshIndex);
                 index + 1 < model_.meshCount; ++index) {
                model_.meshes[index] =
                    model_.meshes[index + 1];
                model_.meshMaterial[index] =
                    model_.meshMaterial[index + 1];
            }
            --model_.meshCount;
        }
        meshBounds_.reserve(
            static_cast<std::size_t>(model_.meshCount));
        for (int index = 0; index < model_.meshCount; ++index) {
            const BoundingBox bounds =
                GetMeshBoundingBox(model_.meshes[index]);
            meshBounds_.push_back(bounds);
            if (index == 0) {
                visualBounds_ = bounds;
            } else {
                visualBounds_.min.x = std::min(
                    visualBounds_.min.x, bounds.min.x);
                visualBounds_.min.y = std::min(
                    visualBounds_.min.y, bounds.min.y);
                visualBounds_.min.z = std::min(
                    visualBounds_.min.z, bounds.min.z);
                visualBounds_.max.x = std::max(
                    visualBounds_.max.x, bounds.max.x);
                visualBounds_.max.y = std::max(
                    visualBounds_.max.y, bounds.max.y);
                visualBounds_.max.z = std::max(
                    visualBounds_.max.z, bounds.max.z);
            }
        }
    }
    return loaded_;
}

void ModelResource::unload() {
    if (loaded_) {
        UnloadModel(model_);
        model_ = {};
        loaded_ = false;
    }
    collisionAsset_ = {};
    visualBounds_ = {};
    meshBounds_.clear();
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

const GlbCollisionAsset&
ModelResource::collisionAsset() const {
    return collisionAsset_;
}

const BoundingBox& ModelResource::visualBounds() const {
    return visualBounds_;
}

std::span<const BoundingBox>
ModelResource::meshBounds() const {
    return meshBounds_;
}

ModelAnimationsResource::~ModelAnimationsResource() {
    unload();
}

bool ModelAnimationsResource::load(const char* path) {
    unload();
    animations_ = LoadModelAnimations(path, &count_);
    return animations_ != nullptr && count_ > 0;
}

void ModelAnimationsResource::unload() {
    if (animations_ != nullptr) {
        UnloadModelAnimations(animations_, count_);
    }
    animations_ = nullptr;
    count_ = 0;
}

const ModelAnimation* ModelAnimationsResource::find(
    std::string_view name) const {
    for (int index = 0; index < count_; ++index) {
        if (name == animations_[index].name) {
            return &animations_[index];
        }
    }
    return nullptr;
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
    if (!loaded_) {
        if (target_.id != 0U) {
            UnloadRenderTexture(target_);
        } else {
            if (target_.texture.id != 0U) {
                rlUnloadTexture(target_.texture.id);
            }
            if (target_.depth.id != 0U) {
                rlUnloadTexture(target_.depth.id);
            }
        }
        target_ = {};
    } else {
        SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
        SetTextureWrap(target_.texture, TEXTURE_WRAP_CLAMP);
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
    cloudShader_.load(
        "assets/shaders/cloud.vs", "assets/shaders/cloud.fs");
    waterShader_.load(
        "assets/shaders/water.vs", "assets/shaders/water.fs");
    selectionMaskShader_.load("assets/shaders/selection_mask.vs",
                              "assets/shaders/selection_mask.fs");
    selectionOutlineShader_.load(nullptr,
                                 "assets/shaders/selection_outline.fs");
    postProcessShader_.load(
        nullptr, "assets/shaders/postprocess.fs");
    viewModelCompositeShader_.load(
        nullptr, "assets/shaders/viewmodel_composite.fs");
    grassShader_.load("assets/shaders/grass_instanced.vs",
                      "assets/shaders/grass_instanced.fs");
    upgradeEffectShader_.load("assets/shaders/upgrade_effect.vs",
                              "assets/shaders/upgrade_effect.fs");
    terrainTexture_.load("assets/textures/grass_watercolor.png");
    if (terrainTexture_.valid()) {
        Texture2D& texture = terrainTexture_.get();
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    }
    cannonModel_.load("assets/models/cannon.glb");
    cannonballModel_.load("assets/models/cannonball.glb");
    arrowModel_.load("assets/models/arrow.glb");
    crossbowModel_.load("assets/models/crossbow.glb");
    coreModel_.load("assets/models/core.glb");
    axeModel_.load("assets/models/tools/axe.glb");
    pickaxeModel_.load("assets/models/tools/pickaxe.glb");
    clubModel_.load("assets/models/tools/club.glb");
    hammerModel_.load("assets/models/tools/hammer.glb");
    woodenChestModel_.load("assets/models/wooden_chest.glb");
    stoneChestModel_.load("assets/models/stone_chest.glb");
    platformModel_.load("assets/models/platform.glb");
    rampModel_.load("assets/models/ramp.glb");
    mineModel_.load("assets/models/mine.glb");
    lumberMillModel_.load(
        "assets/models/lumber_mill.glb");
    quarryModel_.load("assets/models/quarry.glb");
    rockModel_.load("assets/models/rock.glb");
    treeModels_[0].load("assets/models/tree.glb");
    treeModels_[1].load("assets/models/tree_b.glb");
    treeModels_[2].load("assets/models/tree_c.glb");
    boundaryTreeModels_[0].load(
        "assets/models/boundary_forest/tree.glb");
    boundaryTreeModels_[1].load(
        "assets/models/boundary_forest/tree_high.glb");
    decorativeRockModels_[0].load(
        "assets/models/decor/rock_small_a.glb");
    decorativeRockModels_[1].load(
        "assets/models/decor/rock_small_b.glb");
    decorativeRockModels_[2].load(
        "assets/models/decor/rock_small_c.glb");
    decorativeRockModels_[3].load(
        "assets/models/decor/rock_small_d.glb");
    decorativeBushModels_[0].load(
        "assets/models/decor/bushes/a.glb");
    decorativeBushModels_[1].load(
        "assets/models/decor/bushes/b.glb");
    decorativeBushModels_[2].load(
        "assets/models/decor/bushes/c.glb");
    decorativeBushModels_[3].load(
        "assets/models/decor/bushes/d.glb");
    decorativeBushModels_[4].load(
        "assets/models/decor/bushes/e.glb");
    decorativeBushModels_[5].load(
        "assets/models/decor/bushes/f.glb");
    pondDecorModels_[0].load(
        "assets/models/decor/water/waterlily_A.gltf");
    pondDecorModels_[1].load(
        "assets/models/decor/water/waterlily_B.gltf");
    pondDecorModels_[2].load(
        "assets/models/decor/water/waterplant_A.gltf");
    pondDecorModels_[3].load(
        "assets/models/decor/water/waterplant_B.gltf");
    pondDecorModels_[4].load(
        "assets/models/decor/water/waterplant_C.gltf");
    cloudModels_[0].load("assets/models/clouds/small.glb");
    cloudModels_[1].load("assets/models/clouds/big.glb");
    wallIsolatedModel_.load("assets/models/walls/isolated.glb");
    wallEndModel_.load("assets/models/walls/end.glb");
    wallCornerModel_.load("assets/models/walls/corner.glb");
    wallTModel_.load("assets/models/walls/t.glb");
    wallCrossModel_.load("assets/models/walls/cross.glb");
    grassModelB_.load(
        "assets/models/grass/Grass_2_B_Singlesided_Color1.gltf");
    grassModelC_.load(
        "assets/models/grass/Grass_2_C_Singlesided_Color1.gltf");
    grassModelD_.load(
        "assets/models/grass/Grass_2_D_Singlesided_Color1.gltf");
    enemyMinionModel_.load(
        "assets/models/enemies/minion.glb");
    enemyRogueModel_.load(
        "assets/models/enemies/rogue.glb");
    enemyWarriorModel_.load(
        "assets/models/enemies/warrior.glb");
    enemyMageModel_.load(
        "assets/models/enemies/mage.glb");
    enemySapperModel_.load(
        "assets/models/enemies/ultimate/sapper.gltf");
    enemyFlyingModel_.load(
        "assets/models/enemies/ultimate/flying.gltf");
    enemyBossModel_.load(
        "assets/models/enemies/ultimate/boss.gltf");
    enemyGeneralAnimations_.load(
        "assets/models/enemies/animations/general.glb");
    enemyMovementAnimations_.load(
        "assets/models/enemies/animations/movement.glb");
    enemySapperAnimations_.load(
        "assets/models/enemies/ultimate/sapper.gltf");
    enemyFlyingAnimations_.load(
        "assets/models/enemies/ultimate/flying.gltf");
    enemyBossAnimations_.load(
        "assets/models/enemies/ultimate/boss.gltf");
    updateFramebuffer(settings);
    updateViewModelTarget();
    updateSelectionMask(settings);
    updateShadowMap(settings);
}

void GraphicsResources::updateViewModelTarget() {
    if (!initialized_) {
        return;
    }
    const int width = GetRenderWidth();
    const int height = GetRenderHeight();
    if (width <= 0 || height <= 0 ||
        (width == requestedViewModelWidth_ &&
         height == requestedViewModelHeight_)) {
        return;
    }
    requestedViewModelWidth_ = width;
    requestedViewModelHeight_ = height;
    viewModelTarget_.load(width, height);
    if (viewModelTarget_.valid()) {
        SetTextureFilter(viewModelTarget_.get().texture,
                         TEXTURE_FILTER_BILINEAR);
    }
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

    const int pixelSize = std::clamp(settings.pixelSize, 1, 8);
    const int desiredWidth =
        std::max(1, (framebufferWidth + pixelSize - 1) / pixelSize);
    const int desiredHeight =
        std::max(1, (framebufferHeight + pixelSize - 1) / pixelSize);
    if (desiredWidth == requestedSceneWidth_ && desiredHeight == requestedSceneHeight_) {
        return;
    }

    requestedSceneWidth_ = desiredWidth;
    requestedSceneHeight_ = desiredHeight;
    sceneTarget_.load(desiredWidth, desiredHeight);
}

void GraphicsResources::updateSelectionMask(
    const GraphicsSettings& settings) {
    if (!initialized_) {
        return;
    }

    const bool useSceneTarget =
        settings.postProcessing && sceneTarget_.valid();
    const int desiredWidth =
        useSceneTarget ? requestedSceneWidth_ : GetScreenWidth();
    const int desiredHeight =
        useSceneTarget ? requestedSceneHeight_ : GetScreenHeight();
    if (desiredWidth <= 0 || desiredHeight <= 0 ||
        (desiredWidth == requestedSelectionMaskWidth_ &&
         desiredHeight == requestedSelectionMaskHeight_)) {
        return;
    }

    requestedSelectionMaskWidth_ = desiredWidth;
    requestedSelectionMaskHeight_ = desiredHeight;
    selectionMaskTarget_.load(desiredWidth, desiredHeight);
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
    viewModelTarget_.unload();
    shadowMap_.unload();
    enemyBossAnimations_.unload();
    enemyFlyingAnimations_.unload();
    enemySapperAnimations_.unload();
    enemyMovementAnimations_.unload();
    enemyGeneralAnimations_.unload();
    enemyBossModel_.unload();
    enemyFlyingModel_.unload();
    enemySapperModel_.unload();
    enemyMageModel_.unload();
    enemyWarriorModel_.unload();
    enemyRogueModel_.unload();
    enemyMinionModel_.unload();
    grassModelD_.unload();
    grassModelC_.unload();
    grassModelB_.unload();
    wallCrossModel_.unload();
    wallTModel_.unload();
    wallCornerModel_.unload();
    wallEndModel_.unload();
    wallIsolatedModel_.unload();
    for (auto& treeModel : treeModels_) {
        treeModel.unload();
    }
    for (auto& treeModel : boundaryTreeModels_) {
        treeModel.unload();
    }
    for (auto& rockModel : decorativeRockModels_) {
        rockModel.unload();
    }
    for (auto& bushModel : decorativeBushModels_) {
        bushModel.unload();
    }
    for (auto& pondDecorModel : pondDecorModels_) {
        pondDecorModel.unload();
    }
    for (auto& cloudModel : cloudModels_) {
        cloudModel.unload();
    }
    rockModel_.unload();
    quarryModel_.unload();
    lumberMillModel_.unload();
    mineModel_.unload();
    rampModel_.unload();
    platformModel_.unload();
    hammerModel_.unload();
    clubModel_.unload();
    stoneChestModel_.unload();
    woodenChestModel_.unload();
    pickaxeModel_.unload();
    axeModel_.unload();
    coreModel_.unload();
    crossbowModel_.unload();
    arrowModel_.unload();
    cannonballModel_.unload();
    cannonModel_.unload();
    placeholderModel_.unload();
    terrainTexture_.unload();
    fallbackTexture_.unload();
    upgradeEffectShader_.unload();
    grassShader_.unload();
    postProcessShader_.unload();
    viewModelCompositeShader_.unload();
    selectionOutlineShader_.unload();
    selectionMaskShader_.unload();
    skyShader_.unload();
    cloudShader_.unload();
    waterShader_.unload();
    shadowDebugShader_.unload();
    shadowShader_.unload();
    worldShader_.unload();
    sceneTarget_.unload();
    selectionMaskTarget_.unload();
    requestedSceneWidth_ = 0;
    requestedSceneHeight_ = 0;
    requestedViewModelWidth_ = 0;
    requestedViewModelHeight_ = 0;
    requestedSelectionMaskWidth_ = 0;
    requestedSelectionMaskHeight_ = 0;
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

bool GraphicsResources::selectionMaskValid() const {
    return selectionMaskTarget_.valid();
}

const RenderTexture2D& GraphicsResources::selectionMask() const {
    return selectionMaskTarget_.get();
}

int GraphicsResources::selectionMaskWidth() const {
    return requestedSelectionMaskWidth_;
}

int GraphicsResources::selectionMaskHeight() const {
    return requestedSelectionMaskHeight_;
}

bool GraphicsResources::viewModelTargetValid() const {
    return viewModelTarget_.valid();
}

const RenderTexture2D& GraphicsResources::viewModelTarget() const {
    return viewModelTarget_.get();
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

ShaderResource& GraphicsResources::cloudShader() {
    return cloudShader_;
}

const ShaderResource& GraphicsResources::cloudShader() const {
    return cloudShader_;
}

ShaderResource& GraphicsResources::waterShader() {
    return waterShader_;
}

const ShaderResource& GraphicsResources::waterShader() const {
    return waterShader_;
}

ShaderResource& GraphicsResources::selectionMaskShader() {
    return selectionMaskShader_;
}

const ShaderResource& GraphicsResources::selectionMaskShader() const {
    return selectionMaskShader_;
}

ShaderResource& GraphicsResources::selectionOutlineShader() {
    return selectionOutlineShader_;
}

const ShaderResource& GraphicsResources::selectionOutlineShader() const {
    return selectionOutlineShader_;
}

ShaderResource& GraphicsResources::postProcessShader() {
    return postProcessShader_;
}

const ShaderResource& GraphicsResources::postProcessShader() const {
    return postProcessShader_;
}

ShaderResource& GraphicsResources::viewModelCompositeShader() {
    return viewModelCompositeShader_;
}

const ShaderResource& GraphicsResources::viewModelCompositeShader() const {
    return viewModelCompositeShader_;
}

ShaderResource& GraphicsResources::grassShader() {
    return grassShader_;
}

const ShaderResource& GraphicsResources::grassShader() const {
    return grassShader_;
}

ShaderResource& GraphicsResources::upgradeEffectShader() {
    return upgradeEffectShader_;
}

const ShaderResource& GraphicsResources::upgradeEffectShader() const {
    return upgradeEffectShader_;
}

TextureResource& GraphicsResources::fallbackTexture() {
    return fallbackTexture_;
}

TextureResource& GraphicsResources::terrainTexture() {
    return terrainTexture_;
}

const TextureResource& GraphicsResources::terrainTexture() const {
    return terrainTexture_;
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

ModelResource& GraphicsResources::axeModel() {
    return axeModel_;
}

ModelResource& GraphicsResources::pickaxeModel() {
    return pickaxeModel_;
}

ModelResource& GraphicsResources::clubModel() {
    return clubModel_;
}

ModelResource& GraphicsResources::hammerModel() {
    return hammerModel_;
}

ModelResource& GraphicsResources::woodenChestModel() {
    return woodenChestModel_;
}

ModelResource& GraphicsResources::stoneChestModel() {
    return stoneChestModel_;
}

ModelResource& GraphicsResources::platformModel() {
    return platformModel_;
}

ModelResource& GraphicsResources::rampModel() {
    return rampModel_;
}

ModelResource& GraphicsResources::mineModel() {
    return mineModel_;
}

ModelResource& GraphicsResources::lumberMillModel() {
    return lumberMillModel_;
}

ModelResource& GraphicsResources::quarryModel() {
    return quarryModel_;
}

ModelResource& GraphicsResources::rockModel() {
    return rockModel_;
}

ModelResource& GraphicsResources::treeModel(std::size_t variant) {
    return treeModels_[variant % treeModels_.size()];
}

ModelResource& GraphicsResources::boundaryTreeModel(
    std::size_t variant) {
    return boundaryTreeModels_[
        variant % boundaryTreeModels_.size()];
}

ModelResource& GraphicsResources::decorativeRockModel(
    std::size_t variant) {
    return decorativeRockModels_[
        variant % decorativeRockModels_.size()];
}

ModelResource& GraphicsResources::decorativeBushModel(
    std::size_t variant) {
    return decorativeBushModels_[
        variant % decorativeBushModels_.size()];
}

ModelResource& GraphicsResources::pondDecorModel(
    std::size_t variant) {
    return pondDecorModels_[variant % pondDecorModels_.size()];
}

ModelResource& GraphicsResources::cloudModel(std::size_t variant) {
    return cloudModels_[variant % cloudModels_.size()];
}

ModelResource& GraphicsResources::wallIsolatedModel() {
    return wallIsolatedModel_;
}

ModelResource& GraphicsResources::wallEndModel() {
    return wallEndModel_;
}

ModelResource& GraphicsResources::wallCornerModel() {
    return wallCornerModel_;
}

ModelResource& GraphicsResources::wallTModel() {
    return wallTModel_;
}

ModelResource& GraphicsResources::wallCrossModel() {
    return wallCrossModel_;
}

ModelResource& GraphicsResources::grassModelB() {
    return grassModelB_;
}

ModelResource& GraphicsResources::grassModelC() {
    return grassModelC_;
}

ModelResource& GraphicsResources::grassModelD() {
    return grassModelD_;
}

ModelResource& GraphicsResources::enemyMinionModel() {
    return enemyMinionModel_;
}

ModelResource& GraphicsResources::enemyRogueModel() {
    return enemyRogueModel_;
}

ModelResource& GraphicsResources::enemyWarriorModel() {
    return enemyWarriorModel_;
}

ModelResource& GraphicsResources::enemyMageModel() {
    return enemyMageModel_;
}

ModelResource& GraphicsResources::enemySapperModel() {
    return enemySapperModel_;
}

ModelResource& GraphicsResources::enemyFlyingModel() {
    return enemyFlyingModel_;
}

ModelResource& GraphicsResources::enemyBossModel() {
    return enemyBossModel_;
}

const ModelAnimationsResource&
GraphicsResources::enemyGeneralAnimations() const {
    return enemyGeneralAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemyMovementAnimations() const {
    return enemyMovementAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemySapperAnimations() const {
    return enemySapperAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemyFlyingAnimations() const {
    return enemyFlyingAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemyBossAnimations() const {
    return enemyBossAnimations_;
}

ShadowMapResource& GraphicsResources::shadowMap() {
    return shadowMap_;
}

const ShadowMapResource& GraphicsResources::shadowMap() const {
    return shadowMap_;
}

} // namespace ian
