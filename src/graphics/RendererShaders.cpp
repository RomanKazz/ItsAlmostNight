#include "graphics/Renderer.hpp"

#include "buildings/BuildingSystem.hpp"
#include "ui/UiText.hpp"
#include "world/PondDecorationLayout.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ian {
namespace {

void configureSkinningLocations(Shader& shader) {
    shader.locs[SHADER_LOC_VERTEX_BONEIDS] =
        GetShaderLocationAttrib(shader, "vertexBoneIndices");
    shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] =
        GetShaderLocationAttrib(shader, "vertexBoneWeights");
    shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] =
        GetShaderLocation(shader, "boneMatrices");
}

} // namespace

GraphicsSettings& Renderer::settings() {
    return settings_;
}

const GraphicsSettings& Renderer::settings() const {
    return settings_;
}

const RendererPerformanceStats& Renderer::performanceStats() const {
    return performanceStats_;
}

void Renderer::setLowHealthEffect(
    float amount, bool reduceFlashes) {
    lowHealthEffect_ = std::clamp(amount, 0.0F, 1.0F);
    lowHealthPulse_ = reduceFlashes ? 0.0F : 1.0F;
}

void Renderer::setMenuDepthOfField(
    bool enabled, float focusDistance, float focusRange,
    float maximumBlurPixels) {
    menuDepthOfFieldEnabled_ = enabled;
    menuDepthOfFieldFocusDistance_ =
        std::max(focusDistance, 0.1F);
    menuDepthOfFieldFocusRange_ =
        std::max(focusRange, 0.1F);
    menuDepthOfFieldMaximumBlurPixels_ =
        std::clamp(maximumBlurPixels, 0.0F, 12.0F);
}

void Renderer::resolveWorldShaderLocations() {
    if (!resources_.worldShader().valid()) {
        return;
    }

    auto& shader = resources_.worldShader().get();
    configureSkinningLocations(shader);
    worldSkinningEnabledLocation_ =
        GetShaderLocation(shader, "skinningEnabled");
    worldInstancingEnabledLocation_ =
        GetShaderLocation(shader, "instancingEnabled");
    shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
        GetShaderLocationAttrib(shader, "instanceTransform");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    worldShaderLocations_ = {
        .baseColor = GetShaderLocation(shader, "baseColor"),
        .cameraPosition = GetShaderLocation(shader, "cameraPosition"),
        .sunDirection = GetShaderLocation(shader, "sunDirection"),
        .sunColor = GetShaderLocation(shader, "sunColor"),
        .sunIntensity = GetShaderLocation(shader, "sunIntensity"),
        .skyAmbientColor = GetShaderLocation(shader, "skyAmbientColor"),
        .groundAmbientColor = GetShaderLocation(shader, "groundAmbientColor"),
        .ambientIntensity = GetShaderLocation(shader, "ambientIntensity"),
        .cloudShadowStrength =
            GetShaderLocation(shader, "cloudShadowStrength"),
        .fogColor = GetShaderLocation(shader, "fogColor"),
        .fogStart = GetShaderLocation(shader, "fogStart"),
        .fogEnd = GetShaderLocation(shader, "fogEnd"),
        .fogBandsEnabled =
            GetShaderLocation(shader, "fogBandsEnabled"),
        .fogBandCount =
            GetShaderLocation(shader, "fogBandCount"),
        .dayNightTint = GetShaderLocation(shader, "dayNightTint"),
        .exposure = GetShaderLocation(shader, "exposure"),
        .saturation = GetShaderLocation(shader, "saturation"),
        .toonShadingEnabled =
            GetShaderLocation(shader, "toonShadingEnabled"),
        .toonLightSteps =
            GetShaderLocation(shader, "toonLightSteps"),
        .bakedAo = GetShaderLocation(shader, "bakedAo"),
        .vertexAoAmount = GetShaderLocation(shader, "vertexAoAmount"),
        .screenAoAmount = GetShaderLocation(shader, "screenAoAmount"),
        .aoStrength = GetShaderLocation(shader, "aoStrength"),
        .terrainAmount = GetShaderLocation(shader, "terrainAmount"),
        .terrainGrassTint =
            GetShaderLocation(shader, "terrainGrassTint"),
        .terrainDirtTint =
            GetShaderLocation(shader, "terrainDirtTint"),
        .terrainTexture =
            GetShaderLocation(shader, "terrainTexture"),
        .terrainTextureEnabled =
            GetShaderLocation(shader, "terrainTextureEnabled"),
        .timeSeconds = GetShaderLocation(shader, "timeSeconds"),
        .windAmount = GetShaderLocation(shader, "windAmount"),
        .localWindHeight = GetShaderLocation(shader, "localWindHeight"),
        .distantFadeAmount =
            GetShaderLocation(shader, "distantFadeAmount"),
        .vegetationAmount =
            GetShaderLocation(shader, "vegetationAmount"),
        .hitFlashAmount = GetShaderLocation(shader, "hitFlashAmount"),
        .selectionAmount = GetShaderLocation(shader, "selectionAmount"),
        .selectionTint = GetShaderLocation(shader, "selectionTint"),
        .ghostAmount = GetShaderLocation(shader, "ghostAmount"),
        .ghostTint = GetShaderLocation(shader, "ghostTint"),
        .ghostOpacity = GetShaderLocation(shader, "ghostOpacity"),
        .shadowMap = GetShaderLocation(shader, "shadowMap"),
        .lightViewProjection = GetShaderLocation(shader, "lightViewProjection"),
        .shadowsEnabled = GetShaderLocation(shader, "shadowsEnabled"),
        .constantBias = GetShaderLocation(shader, "constantBias"),
        .slopeBias = GetShaderLocation(shader, "slopeBias"),
        .shadowStrength = GetShaderLocation(shader, "shadowStrength"),
        .shadowMapTexelSize =
            GetShaderLocation(shader, "shadowMapTexelSize"),
        .instancingEnabled = worldInstancingEnabledLocation_,
        .inkOutlineEligible = GetShaderLocation(
            shader, "inkOutlineEligible"),
    };
}

void Renderer::setSkinningEnabled(
    Shader& shader, bool enabled) {
    int location = -1;
    if (resources_.worldShader().valid() &&
        shader.id == resources_.worldShader().get().id) {
        location = worldSkinningEnabledLocation_;
    } else if (
        resources_.shadowShader().valid() &&
        shader.id == resources_.shadowShader().get().id) {
        location = shadowSkinningEnabledLocation_;
    } else if (
        resources_.selectionMaskShader().valid() &&
        shader.id ==
            resources_.selectionMaskShader().get().id) {
        location =
            selectionMaskSkinningEnabledLocation_;
    }
    if (location < 0) {
        return;
    }
    const int value = enabled ? 1 : 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, location, &value, SHADER_UNIFORM_INT);
}

void Renderer::resolveSkyShaderLocations() {
    if (!resources_.skyShader().valid()) {
        return;
    }

    const auto& shader = resources_.skyShader().get();
    skyShaderLocations_ = {
        .viewportSize = GetShaderLocation(shader, "viewportSize"),
        .cameraForward = GetShaderLocation(shader, "cameraForward"),
        .cameraRight = GetShaderLocation(shader, "cameraRight"),
        .cameraUp = GetShaderLocation(shader, "cameraUp"),
        .tanHalfFov = GetShaderLocation(shader, "tanHalfFov"),
        .aspectRatio = GetShaderLocation(shader, "aspectRatio"),
        .zenithColor = GetShaderLocation(shader, "zenithColor"),
        .horizonColor = GetShaderLocation(shader, "horizonColor"),
        .lowerSkyColor = GetShaderLocation(shader, "lowerSkyColor"),
        .celestialDirection =
            GetShaderLocation(shader, "celestialDirection"),
        .celestialColor = GetShaderLocation(shader, "celestialColor"),
        .celestialIntensity =
            GetShaderLocation(shader, "celestialIntensity"),
        .nightAmount = GetShaderLocation(shader, "nightAmount"),
        .daySkybox = GetShaderLocation(shader, "daySkybox"),
        .morningSkybox = GetShaderLocation(shader, "morningSkybox"),
        .nightSkybox = GetShaderLocation(shader, "nightSkybox"),
        .skyboxEnabled = GetShaderLocation(shader, "skyboxEnabled"),
        .timeSeconds = GetShaderLocation(shader, "timeSeconds"),
        .exposure = GetShaderLocation(shader, "exposure"),
        .saturation = GetShaderLocation(shader, "saturation"),
    };
}

void Renderer::resolvePostProcessLocations() {
    if (!resources_.postProcessShader().valid()) {
        return;
    }
    const auto& shader =
        resources_.postProcessShader().get();
    postProcessLocations_ = {
        .exposure =
            GetShaderLocation(shader, "postExposure"),
        .brightness =
            GetShaderLocation(shader, "brightness"),
        .contrast =
            GetShaderLocation(shader, "contrast"),
        .saturation =
            GetShaderLocation(shader, "saturation"),
        .hueDegrees =
            GetShaderLocation(shader, "hueDegrees"),
        .temperature =
            GetShaderLocation(shader, "temperature"),
        .tint = GetShaderLocation(shader, "tint"),
        .gamma = GetShaderLocation(shader, "gammaValue"),
        .blackPoint =
            GetShaderLocation(shader, "blackPoint"),
        .curveShadows =
            GetShaderLocation(shader, "curveShadows"),
        .curveMidtones =
            GetShaderLocation(shader, "curveMidtones"),
        .curveHighlights =
            GetShaderLocation(shader, "curveHighlights"),
        .sharpness =
            GetShaderLocation(shader, "sharpness"),
        .vignette =
            GetShaderLocation(shader, "vignette"),
        .lowHealthAmount =
            GetShaderLocation(shader, "lowHealthAmount"),
        .lowHealthTime =
            GetShaderLocation(shader, "lowHealthTime"),
        .lowHealthPulse =
            GetShaderLocation(shader, "lowHealthPulse"),
        .paletteEnabled =
            GetShaderLocation(shader, "paletteEnabled"),
        .paletteLevels =
            GetShaderLocation(shader, "paletteLevels"),
        .ditherEnabled =
            GetShaderLocation(shader, "ditherEnabled"),
        .ditherStrength =
            GetShaderLocation(shader, "ditherStrength"),
        .bloomEnabled =
            GetShaderLocation(shader, "bloomEnabled"),
        .bloomStrength =
            GetShaderLocation(shader, "bloomStrength"),
        .inkOutlinesEnabled = GetShaderLocation(
            shader, "inkOutlinesEnabled"),
        .outlineStrength =
            GetShaderLocation(shader, "outlineStrength"),
        .outlineWidth =
            GetShaderLocation(shader, "outlineWidth"),
        .paperGrainEnabled =
            GetShaderLocation(shader, "paperGrainEnabled"),
        .paperGrainStrength = GetShaderLocation(
            shader, "paperGrainStrength"),
        .menuDofEnabled =
            GetShaderLocation(shader, "menuDofEnabled"),
        .menuDofFocusDistance =
            GetShaderLocation(shader, "menuDofFocusDistance"),
        .menuDofFocusRange =
            GetShaderLocation(shader, "menuDofFocusRange"),
        .menuDofMaximumBlurPixels = GetShaderLocation(
            shader, "menuDofMaximumBlurPixels"),
        .sceneDepth = GetShaderLocation(shader, "sceneDepth"),
        .sceneNormal = GetShaderLocation(shader, "sceneNormal"),
        .ssaoTexture = GetShaderLocation(shader, "ssaoTexture"),
        .inverseProjection = GetShaderLocation(
            shader, "inverseProjection"),
        .ssaoTexelSize = GetShaderLocation(shader, "ssaoTexelSize"),
        .ssaoEnabled = GetShaderLocation(shader, "ssaoEnabled"),
        .ssaoStrength = GetShaderLocation(shader, "ssaoStrength"),
    };
}

void Renderer::resolveSsaoLocations() {
    if (!resources_.ssaoShader().valid()) {
        return;
    }
    const Shader& shader = resources_.ssaoShader().get();
    ssaoLocations_ = {
        .sceneDepth = GetShaderLocation(shader, "sceneDepth"),
        .sceneNormal = GetShaderLocation(shader, "sceneNormal"),
        .projection = GetShaderLocation(shader, "projection"),
        .inverseProjection = GetShaderLocation(
            shader, "inverseProjection"),
        .viewMatrix = GetShaderLocation(shader, "viewMatrix"),
        .texelSize = GetShaderLocation(shader, "texelSize"),
        .radius = GetShaderLocation(shader, "radius"),
        .bias = GetShaderLocation(shader, "bias"),
        .fadeStart = GetShaderLocation(shader, "fadeStart"),
        .fadeEnd = GetShaderLocation(shader, "fadeEnd"),
        .sampleCount = GetShaderLocation(shader, "sampleCount"),
    };
}

void Renderer::uploadPostProcessSettings() {
    if (!resources_.postProcessShader().valid()) {
        return;
    }
    auto& shader = resources_.postProcessShader().get();
    const auto upload =
        [&shader](int location, const float& value) {
            SetShaderValue(
                shader, location, &value,
                SHADER_UNIFORM_FLOAT);
        };
    upload(postProcessLocations_.exposure,
           settings_.postExposure);
    upload(postProcessLocations_.brightness,
           settings_.brightness);
    upload(postProcessLocations_.contrast,
           settings_.contrast);
    upload(postProcessLocations_.saturation,
           settings_.colorSaturation);
    upload(postProcessLocations_.hueDegrees,
           settings_.hueDegrees);
    upload(postProcessLocations_.temperature,
           settings_.temperature);
    upload(postProcessLocations_.tint, settings_.tint);
    upload(postProcessLocations_.gamma, settings_.gamma);
    upload(postProcessLocations_.blackPoint,
           settings_.blackPoint);
    upload(postProcessLocations_.curveShadows,
           settings_.curveShadows);
    upload(postProcessLocations_.curveMidtones,
           settings_.curveMidtones);
    upload(postProcessLocations_.curveHighlights,
           settings_.curveHighlights);
    upload(postProcessLocations_.sharpness,
           settings_.sharpness);
    upload(postProcessLocations_.vignette,
           settings_.vignette);
    upload(postProcessLocations_.lowHealthAmount,
           lowHealthEffect_);
    const float lowHealthTime =
        static_cast<float>(GetTime());
    upload(postProcessLocations_.lowHealthTime,
           lowHealthTime);
    upload(postProcessLocations_.lowHealthPulse,
           lowHealthPulse_);
    upload(postProcessLocations_.paletteEnabled,
           settings_.paletteQuantization ? 1.0F : 0.0F);
    upload(postProcessLocations_.paletteLevels,
           settings_.paletteLevels);
    upload(postProcessLocations_.ditherEnabled,
           settings_.dithering ? 1.0F : 0.0F);
    upload(postProcessLocations_.ditherStrength,
           settings_.ditherStrength);
    upload(postProcessLocations_.bloomEnabled,
           settings_.bloom ? 1.0F : 0.0F);
    upload(postProcessLocations_.bloomStrength,
           settings_.bloomStrength);
    upload(postProcessLocations_.inkOutlinesEnabled,
           settings_.inkOutlines ? 1.0F : 0.0F);
    upload(postProcessLocations_.outlineStrength,
           settings_.outlineStrength);
    upload(postProcessLocations_.outlineWidth,
           settings_.outlineWidth);
    upload(postProcessLocations_.paperGrainEnabled,
           settings_.paperGrain ? 1.0F : 0.0F);
    upload(postProcessLocations_.paperGrainStrength,
           settings_.paperGrainStrength);
    upload(postProcessLocations_.menuDofEnabled,
           menuDepthOfFieldEnabled_ ? 1.0F : 0.0F);
    upload(postProcessLocations_.menuDofFocusDistance,
           menuDepthOfFieldFocusDistance_);
    upload(postProcessLocations_.menuDofFocusRange,
           menuDepthOfFieldFocusRange_);
    upload(postProcessLocations_.menuDofMaximumBlurPixels,
           menuDepthOfFieldMaximumBlurPixels_);
    const float ssaoEnabled =
        ssaoFrameReady_ && resources_.ssaoTargetValid() ? 1.0F : 0.0F;
    const float ssaoStrength = settings_.aoStrength*1.15F;
    upload(postProcessLocations_.ssaoEnabled, ssaoEnabled);
    upload(postProcessLocations_.ssaoStrength, ssaoStrength);
    SetShaderValueMatrix(
        shader, postProcessLocations_.inverseProjection,
        ssaoInverseProjection_);
    // Distance-aware ink outlines also consume scene depth, even when SSAO
    // itself is disabled by the active quality preset.
    SetShaderValueTexture(
        shader, postProcessLocations_.sceneDepth,
        resources_.sceneTarget().depth);
    SetShaderValueTexture(
        shader, postProcessLocations_.sceneNormal,
        resources_.sceneNormalTexture());
    if (ssaoEnabled > 0.5F) {
        const Vector2 ssaoTexelSize{
            1.0F / static_cast<float>(
                       std::max(resources_.ssaoWidth(), 1)),
            1.0F / static_cast<float>(
                       std::max(resources_.ssaoHeight(), 1)),
        };
        SetShaderValue(shader, postProcessLocations_.ssaoTexelSize,
                       &ssaoTexelSize, SHADER_UNIFORM_VEC2);
        SetShaderValueTexture(
            shader, postProcessLocations_.ssaoTexture,
            resources_.ssaoTarget().texture);
    }
}

void Renderer::uploadWorldLighting(const WorldLighting& lighting) {
    auto& shader = resources_.worldShader().get();
    const float fogStart = settings_.fog ? lighting.fogStart : 1000000.0F;
    const float fogEnd = settings_.fog ? lighting.fogEnd : 1000001.0F;
    const float shadowsEnabled =
        settings_.shadows && shadowFrameValid_ &&
                resources_.shadowMap().valid()
            ? 1.0F
            : 0.0F;
    const float shadowMapTexelSize =
        resources_.shadowMap().valid()
            ? 1.0F / static_cast<float>(resources_.shadowMap().size())
            : 0.0F;

    SetShaderValue(shader, worldShaderLocations_.cameraPosition,
                   &lighting.cameraPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.sunDirection,
                   &lighting.sunDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.sunColor,
                   &lighting.sunColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.sunIntensity,
                   &lighting.sunIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.skyAmbientColor,
                   &lighting.skyAmbientColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.groundAmbientColor,
                   &lighting.groundAmbientColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.ambientIntensity,
                   &lighting.ambientIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.cloudShadowStrength,
                   &lighting.cloudShadowStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.fogColor,
                   &lighting.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.fogStart,
                   &fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.fogEnd,
                   &fogEnd, SHADER_UNIFORM_FLOAT);
    const float fogBandsEnabled =
        settings_.fogBands ? 1.0F : 0.0F;
    SetShaderValue(shader,
                   worldShaderLocations_.fogBandsEnabled,
                   &fogBandsEnabled, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader,
                   worldShaderLocations_.fogBandCount,
                   &settings_.fogBandCount,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.dayNightTint,
                   &lighting.dayNightTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.exposure,
                   &lighting.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.saturation,
                   &lighting.saturation, SHADER_UNIFORM_FLOAT);
    const float toonShadingEnabled =
        settings_.posterizedLighting ? 1.0F : 0.0F;
    SetShaderValue(shader,
                   worldShaderLocations_.toonShadingEnabled,
                   &toonShadingEnabled, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.toonLightSteps,
                   &settings_.lightingSteps, SHADER_UNIFORM_FLOAT);
    const float timeSeconds = static_cast<float>(GetTime());
    SetShaderValue(shader, worldShaderLocations_.timeSeconds,
                   &timeSeconds, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.shadowsEnabled,
                   &shadowsEnabled, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.constantBias,
                   &settings_.constantBias, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.slopeBias,
                   &settings_.slopeBias, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.shadowStrength,
                   &settings_.shadowStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.shadowMapTexelSize,
                   &shadowMapTexelSize, SHADER_UNIFORM_FLOAT);
    SetShaderValueMatrix(shader, worldShaderLocations_.lightViewProjection,
                         lightViewProjection_);
}

void Renderer::uploadWorldMaterial(const WorldMaterialState& material) {
    auto& shader = resources_.worldShader().get();
    SetShaderValue(shader, worldShaderLocations_.baseColor,
                   &material.baseColor, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, worldShaderLocations_.bakedAo,
                   &material.bakedAo, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.vertexAoAmount,
                   &material.vertexAoAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.screenAoAmount,
                   &material.screenAoAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.terrainAmount,
                   &material.terrainAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.terrainGrassTint,
                   &material.terrainGrassTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.terrainDirtTint,
                   &material.terrainDirtTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.windAmount,
                   &material.windAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.localWindHeight,
                   &material.localWindHeight, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.distantFadeAmount,
                   &material.distantFadeAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.vegetationAmount,
                   &material.vegetationAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.hitFlashAmount,
                   &material.hitFlashAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.selectionAmount,
                   &material.selectionAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.selectionTint,
                   &material.selectionTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.ghostAmount,
                   &material.ghostAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.ghostTint,
                   &material.ghostTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.ghostOpacity,
                   &material.ghostOpacity, SHADER_UNIFORM_FLOAT);
    const float inkOutlineEligible = material.inkOutlineEligible
        ? 1.0F : 0.0F;
    SetShaderValue(shader, worldShaderLocations_.inkOutlineEligible,
                   &inkOutlineEligible, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.aoStrength,
                   &settings_.aoStrength, SHADER_UNIFORM_FLOAT);
}

void Renderer::bindTerrainTexture() {
    if (!worldShaderActive_) {
        return;
    }

    constexpr int TerrainTextureSlot = 9;
    auto& shader = resources_.worldShader().get();
    const int textureSlot = TerrainTextureSlot;
    const float enabled =
        resources_.terrainTexture().valid() ? 1.0F : 0.0F;
    SetShaderValue(shader, worldShaderLocations_.terrainTexture,
                   &textureSlot, SHADER_UNIFORM_INT);
    SetShaderValue(shader,
                   worldShaderLocations_.terrainTextureEnabled,
                   &enabled, SHADER_UNIFORM_FLOAT);
    if (enabled > 0.5F) {
        rlActiveTextureSlot(TerrainTextureSlot);
        rlEnableTexture(resources_.terrainTexture().get().id);
        rlActiveTextureSlot(0);
    }
}

void Renderer::bindShadowMap() {
    if (!worldShaderActive_ || !settings_.shadows ||
        !shadowFrameValid_ || !resources_.shadowMap().valid()) {
        return;
    }
    constexpr int ShadowTextureSlot = 10;
    auto& shader = resources_.worldShader().get();
    rlEnableShader(shader.id);
    rlActiveTextureSlot(ShadowTextureSlot);
    rlEnableTexture(resources_.shadowMap().depthTexture().id);
    rlSetUniform(worldShaderLocations_.shadowMap, &ShadowTextureSlot,
                 SHADER_UNIFORM_INT, 1);
    rlActiveTextureSlot(0);
}

} // namespace ian
