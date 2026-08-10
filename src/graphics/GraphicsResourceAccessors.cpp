#include "graphics/GraphicsResources.hpp"

namespace ian {

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

ShaderResource& GraphicsResources::iceMagicShader() {
    return iceMagicShader_;
}

const ShaderResource& GraphicsResources::iceMagicShader() const {
    return iceMagicShader_;
}

ShaderResource& GraphicsResources::coinOutlineShader() {
    return coinOutlineShader_;
}

const ShaderResource& GraphicsResources::coinOutlineShader() const {
    return coinOutlineShader_;
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

ModelResource& GraphicsResources::iceWandModel() {
    return iceWandModel_;
}

ModelResource& GraphicsResources::woodenChestModel() {
    return woodenChestModel_;
}

ModelResource& GraphicsResources::stoneChestModel() {
    return stoneChestModel_;
}

ModelResource& GraphicsResources::coinModel() {
    return coinModel_;
}

ModelResource& GraphicsResources::appleLootModel() {
    return appleLootModel_;
}

ModelResource& GraphicsResources::breadLootModel() {
    return breadLootModel_;
}

ModelResource& GraphicsResources::ironBarLootModel() {
    return ironBarLootModel_;
}

ModelResource& GraphicsResources::fuelJerrycanLootModel() {
    return fuelJerrycanLootModel_;
}

ModelResource& GraphicsResources::compassLootModel() {
    return compassLootModel_;
}

ModelResource& GraphicsResources::nailLootModel() {
    return nailLootModel_;
}

ModelResource& GraphicsResources::keyLootModel() {
    return keyLootModel_;
}

ModelResource& GraphicsResources::mapLootModel() {
    return mapLootModel_;
}

ModelResource& GraphicsResources::anvilLootModel() {
    return anvilLootModel_;
}

ModelResource& GraphicsResources::sawLootModel() {
    return sawLootModel_;
}

ModelResource& GraphicsResources::potionLootModel() {
    return potionLootModel_;
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

ModelResource& GraphicsResources::spikeTrapModel() {
    return spikeTrapModel_;
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

const ModelResource& GraphicsResources::enemyMinionModel() const {
    return enemyMinionModel_;
}

ModelResource& GraphicsResources::enemyRogueModel() {
    return enemyRogueModel_;
}

const ModelResource& GraphicsResources::enemyRogueModel() const {
    return enemyRogueModel_;
}

ModelResource& GraphicsResources::enemyWarriorModel() {
    return enemyWarriorModel_;
}

const ModelResource& GraphicsResources::enemyWarriorModel() const {
    return enemyWarriorModel_;
}

ModelResource& GraphicsResources::enemyMageModel() {
    return enemyMageModel_;
}

const ModelResource& GraphicsResources::enemyMageModel() const {
    return enemyMageModel_;
}

ModelResource& GraphicsResources::enemySapperModel() {
    return enemySapperModel_;
}

const ModelResource& GraphicsResources::enemySapperModel() const {
    return enemySapperModel_;
}

ModelResource& GraphicsResources::enemyFlyingModel() {
    return enemyFlyingModel_;
}

const ModelResource& GraphicsResources::enemyFlyingModel() const {
    return enemyFlyingModel_;
}

ModelResource& GraphicsResources::enemyBossModel() {
    return enemyBossModel_;
}

const ModelResource& GraphicsResources::enemyBossModel() const {
    return enemyBossModel_;
}

ModelResource& GraphicsResources::enemySplitterModel() {
    return enemySplitterModel_;
}

const ModelResource& GraphicsResources::enemySplitterModel() const {
    return enemySplitterModel_;
}

ModelResource& GraphicsResources::enemySplitlingModel() {
    return enemySplitlingModel_;
}

const ModelResource& GraphicsResources::enemySplitlingModel() const {
    return enemySplitlingModel_;
}

const ModelAnimationsResource&
GraphicsResources::enemyGeneralAnimations() const {
    return enemyGeneralAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemyPinkBlobAnimations() const {
    return enemyPinkBlobAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemyNinjaAnimations() const {
    return enemyNinjaAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemyMushroomKingAnimations() const {
    return enemyMushroomKingAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemySplitterAnimations() const {
    return enemySplitterAnimations_;
}

const ModelAnimationsResource&
GraphicsResources::enemySplitlingAnimations() const {
    return enemySplitlingAnimations_;
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
