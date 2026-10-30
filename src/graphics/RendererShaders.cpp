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

double Renderer::framePacingMilliseconds() const {
    return framePacingMilliseconds_;
}

double Renderer::frameSwapMilliseconds() const {
    return frameSwapMilliseconds_;
}

void Renderer::resolveAuxiliaryShaderLocations() {
    if (resources_.viewModelShader().valid()) {
        Shader& shader = resources_.viewModelShader().get();
        shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(shader, "mvp");
        shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(shader, "matModel");
        shader.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(shader, "matNormal");
        shader.locs[SHADER_LOC_MAP_DIFFUSE] =
            GetShaderLocation(shader, "texture0");
        shader.locs[SHADER_LOC_COLOR_DIFFUSE] =
            GetShaderLocation(shader, "colDiffuse");
        viewModelSunDirectionLocation_ =
            GetShaderLocation(shader, "sunDirection");
        viewModelSunColorLocation_ =
            GetShaderLocation(shader, "sunColor");
        viewModelSunIntensityLocation_ =
            GetShaderLocation(shader, "sunIntensity");
        viewModelSkyAmbientColorLocation_ =
            GetShaderLocation(shader, "skyAmbientColor");
        viewModelGroundAmbientColorLocation_ =
            GetShaderLocation(shader, "groundAmbientColor");
        viewModelAmbientIntensityLocation_ =
            GetShaderLocation(shader, "ambientIntensity");
        viewModelDayNightTintLocation_ =
            GetShaderLocation(shader, "dayNightTint");
        viewModelExposureLocation_ =
            GetShaderLocation(shader, "exposure");
    }
    if (resources_.iceMagicShader().valid()) {
        Shader& shader = resources_.iceMagicShader().get();
        shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(shader, "mvp");
        iceMagicTimeLocation_ =
            GetShaderLocation(shader, "timeSeconds");
        iceMagicTintLocation_ =
            GetShaderLocation(shader, "tint");
        iceMagicIntensityLocation_ =
            GetShaderLocation(shader, "intensity");
    }
    if (resources_.viewModelCompositeShader().valid()) {
        const Shader& shader =
            resources_.viewModelCompositeShader().get();
        viewModelTexelSizeLocation_ =
            GetShaderLocation(shader, "texelSize");
        viewModelOutlineEnabledLocation_ =
            GetShaderLocation(shader, "outlineEnabled");
        viewModelOutlineWidthLocation_ =
            GetShaderLocation(shader, "outlineWidth");
        viewModelOutlineStrengthLocation_ =
            GetShaderLocation(shader, "outlineStrength");
        viewModelRimStrengthLocation_ =
            GetShaderLocation(shader, "rimStrength");
        viewModelBrightnessLocation_ =
            GetShaderLocation(shader, "brightness");
        viewModelSaturationLocation_ =
            GetShaderLocation(shader, "saturation");
    }
    if (resources_.cloudShader().valid()) {
        Shader& shader = resources_.cloudShader().get();
        shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(shader, "mvp");
        shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(shader, "matModel");
        shader.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(shader, "matNormal");
        shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
            GetShaderLocationAttrib(shader, "instanceTransform");
        cloudCameraPositionLocation_ =
            GetShaderLocation(shader, "cameraPosition");
        cloudSunDirectionLocation_ =
            GetShaderLocation(shader, "sunDirection");
        cloudSunColorLocation_ =
            GetShaderLocation(shader, "sunColor");
        cloudSunIntensityLocation_ =
            GetShaderLocation(shader, "sunIntensity");
        cloudAmbientColorLocation_ =
            GetShaderLocation(shader, "ambientColor");
        cloudVisibilityLocation_ =
            GetShaderLocation(shader, "visibility");
        cloudNightAmountLocation_ =
            GetShaderLocation(shader, "nightAmount");
    }
    if (resources_.waterShader().valid()) {
        Shader& shader = resources_.waterShader().get();
        shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(shader, "mvp");
        waterCameraPositionLocation_ =
            GetShaderLocation(shader, "cameraPosition");
        waterShoreColorLocation_ =
            GetShaderLocation(shader, "shoreColor");
        waterShallowColorLocation_ =
            GetShaderLocation(shader, "shallowColor");
        waterMidColorLocation_ =
            GetShaderLocation(shader, "midColor");
        waterDeepColorLocation_ =
            GetShaderLocation(shader, "deepColor");
        waterSkyColorLocation_ =
            GetShaderLocation(shader, "skyColor");
        waterSunDirectionLocation_ =
            GetShaderLocation(shader, "sunDirection");
        waterSunColorLocation_ =
            GetShaderLocation(shader, "sunColor");
        waterFogColorLocation_ =
            GetShaderLocation(shader, "fogColor");
        waterDayNightTintLocation_ =
            GetShaderLocation(shader, "dayNightTint");
        waterFogStartLocation_ =
            GetShaderLocation(shader, "fogStart");
        waterFogEndLocation_ =
            GetShaderLocation(shader, "fogEnd");
        waterExposureLocation_ =
            GetShaderLocation(shader, "exposure");
        waterTimeLocation_ =
            GetShaderLocation(shader, "timeSeconds");
        waterWaveSpeedLocation_ =
            GetShaderLocation(shader, "waveSpeed");
        waterShoreWidthLocation_ =
            GetShaderLocation(shader, "shoreWidth");
        waterDepthFalloffLocation_ =
            GetShaderLocation(shader, "depthFalloff");
        waterPatternScaleLocation_ =
            GetShaderLocation(shader, "patternScale");
        waterPatternStrengthLocation_ =
            GetShaderLocation(shader, "patternStrength");
        waterPatternSpeedLocation_ =
            GetShaderLocation(shader, "patternSpeed");
        waterFresnelStrengthLocation_ =
            GetShaderLocation(shader, "fresnelStrength");
        waterQualityLocation_ =
            GetShaderLocation(shader, "qualityLevel");
    }
    tacticalSectorShapeLocation_ = GetShaderLocation(
        resources_.tacticalSectorShader().get(), "sectorShape");
    if (resources_.selectionOutlineShader().valid()) {
        selectionOutlineTexelSizeLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(), "texelSize");
        selectionOutlineRadiusLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(),
            "outlineRadius");
        selectionOutlineSceneDepthLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(), "sceneDepth");
        selectionOutlineSelectionDepthLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(), "selectionDepth");
        selectionOutlineEffectCoverageLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(), "effectCoverage");
        selectionOutlineEffectCoverageEnabledLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(), "effectCoverageEnabled");
        selectionOutlineInverseProjectionLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(),
            "inverseProjection");
        selectionOutlineDepthOcclusionEnabledLocation_ =
            GetShaderLocation(
                resources_.selectionOutlineShader().get(),
                "depthOcclusionEnabled");
    }
    if (resources_.selectionMaskShader().valid()) {
        Shader& shader = resources_.selectionMaskShader().get();
        configureSkinningLocations(shader);
        shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(shader, "matModel");
        selectionMaskTimeLocation_ =
            GetShaderLocation(shader, "timeSeconds");
        selectionMaskWindLocation_ =
            GetShaderLocation(shader, "windAmount");
        selectionMaskColorLocation_ =
            GetShaderLocation(shader, "maskColor");
        selectionMaskSkinningEnabledLocation_ =
            GetShaderLocation(shader, "skinningEnabled");
    }
    if (resources_.shadowShader().valid()) {
        Shader& shader = resources_.shadowShader().get();
        configureSkinningLocations(shader);
        shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
            GetShaderLocationAttrib(shader, "instanceTransform");
        shadowSkinningEnabledLocation_ =
            GetShaderLocation(shader, "skinningEnabled");
        shadowInstancingEnabledLocation_ =
            GetShaderLocation(shader, "instancingEnabled");
    }
    if (resources_.grassShader().valid()) {
        Shader& shader = resources_.grassShader().get();
        shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(shader, "mvp");
        grassTintLocation_ =
            GetShaderLocation(shader, "grassTint");
        grassTimeLocation_ =
            GetShaderLocation(shader, "timeSeconds");
        grassCameraPositionLocation_ =
            GetShaderLocation(shader, "cameraPosition");
        grassSunDirectionLocation_ =
            GetShaderLocation(shader, "sunDirection");
        grassSunColorLocation_ =
            GetShaderLocation(shader, "sunColor");
        grassSunIntensityLocation_ =
            GetShaderLocation(shader, "sunIntensity");
        grassSkyAmbientColorLocation_ =
            GetShaderLocation(shader, "skyAmbientColor");
        grassGroundAmbientColorLocation_ =
            GetShaderLocation(shader, "groundAmbientColor");
        grassAmbientIntensityLocation_ =
            GetShaderLocation(shader, "ambientIntensity");
        grassFogColorLocation_ =
            GetShaderLocation(shader, "fogColor");
        grassFogStartLocation_ =
            GetShaderLocation(shader, "fogStart");
        grassFogEndLocation_ =
            GetShaderLocation(shader, "fogEnd");
        grassFogBandsEnabledLocation_ =
            GetShaderLocation(shader, "fogBandsEnabled");
        grassFogBandCountLocation_ =
            GetShaderLocation(shader, "fogBandCount");
        grassDayNightTintLocation_ =
            GetShaderLocation(shader, "dayNightTint");
        grassExposureLocation_ =
            GetShaderLocation(shader, "exposure");
        grassSaturationLocation_ =
            GetShaderLocation(shader, "saturation");
    }
    if (resources_.shockwaveShader().valid()) {
        Shader& shader = resources_.shockwaveShader().get();
        shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(shader, "matModel");
        shader.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(shader, "matNormal");
        shockwaveCameraLocation_ =
            GetShaderLocation(shader, "cameraPosition");
        shockwaveProgressLocation_ =
            GetShaderLocation(shader, "progress");
        shockwaveTimeLocation_ =
            GetShaderLocation(shader, "timeSeconds");
    }
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
    worldMaterialUniforms_.reset();
    if (!resources_.worldShader().valid()) {
        return;
    }

    auto& shader = resources_.worldShader().get();
    configureSkinningLocations(shader);
    worldSkinningEnabledLocation_ =
        GetShaderLocation(shader, "skinningEnabled");
    worldInstancingEnabledLocation_ =
        GetShaderLocation(shader, "instancingEnabled");
    worldBonePoseOffsetLocation_ = GetShaderLocation(shader, "bonePoseOffset");
    worldBonePoseBlendLocation_ = GetShaderLocation(shader, "bonePoseBlend");
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
        .groundTextureScale =
            GetShaderLocation(shader, "groundTextureScale"),
        .groundTextureStrength =
            GetShaderLocation(shader, "groundTextureStrength"),
        .groundSaturation =
            GetShaderLocation(shader, "groundSaturation"),
        .groundBrightness =
            GetShaderLocation(shader, "groundBrightness"),
        .groundTextureTint =
            GetShaderLocation(shader, "groundTextureTint"),
        .groundTextureContrast =
            GetShaderLocation(shader, "groundTextureContrast"),
        .groundHighlightCompression =
            GetShaderLocation(shader, "groundHighlightCompression"),
        .groundSecondaryBlend =
            GetShaderLocation(shader, "groundSecondaryBlend"),
        .groundMacroVariation =
            GetShaderLocation(shader, "groundMacroVariation"),
        .timeSeconds = GetShaderLocation(shader, "timeSeconds"),
        .windAmount = GetShaderLocation(shader, "windAmount"),
        .localWindHeight = GetShaderLocation(shader, "localWindHeight"),
        .distantFadeAmount =
            GetShaderLocation(shader, "distantFadeAmount"),
        .vegetationAmount =
            GetShaderLocation(shader, "vegetationAmount"),
        .treeAmount = GetShaderLocation(shader, "treeAmount"),
        .treeLeafShadowColor =
            GetShaderLocation(shader, "treeLeafShadowColor"),
        .treeLeafMidColor =
            GetShaderLocation(shader, "treeLeafMidColor"),
        .treeLeafLightColor =
            GetShaderLocation(shader, "treeLeafLightColor"),
        .treeTrunkShadowColor =
            GetShaderLocation(shader, "treeTrunkShadowColor"),
        .treeTrunkMidColor =
            GetShaderLocation(shader, "treeTrunkMidColor"),
        .treeTrunkLightColor =
            GetShaderLocation(shader, "treeTrunkLightColor"),
        .treeShadowThreshold =
            GetShaderLocation(shader, "treeShadowThreshold"),
        .treeHighlightThreshold =
            GetShaderLocation(shader, "treeHighlightThreshold"),
        .treeRampSmoothness =
            GetShaderLocation(shader, "treeRampSmoothness"),
        .treeHighlightStrength =
            GetShaderLocation(shader, "treeHighlightStrength"),
        .treeTextureVariation =
            GetShaderLocation(shader, "treeTextureVariation"),
        .treeLeafFlatness =
            GetShaderLocation(shader, "treeLeafFlatness"),
        .bushAmount = GetShaderLocation(shader, "bushAmount"),
        .bushShadowColor =
            GetShaderLocation(shader, "bushShadowColor"),
        .bushMidColor = GetShaderLocation(shader, "bushMidColor"),
        .bushLightColor = GetShaderLocation(shader, "bushLightColor"),
        .bushShadowThreshold =
            GetShaderLocation(shader, "bushShadowThreshold"),
        .bushHighlightThreshold =
            GetShaderLocation(shader, "bushHighlightThreshold"),
        .bushRampSmoothness =
            GetShaderLocation(shader, "bushRampSmoothness"),
        .bushHighlightStrength =
            GetShaderLocation(shader, "bushHighlightStrength"),
        .coniferAmount = GetShaderLocation(shader, "coniferAmount"),
        .plantBiomeAnchor = GetShaderLocation(shader, "plantBiomeAnchor"),
        .coniferShadowColor =
            GetShaderLocation(shader, "coniferShadowColor"),
        .coniferMidColor =
            GetShaderLocation(shader, "coniferMidColor"),
        .coniferLightColor =
            GetShaderLocation(shader, "coniferLightColor"),
        .coniferShadowThreshold =
            GetShaderLocation(shader, "coniferShadowThreshold"),
        .coniferHighlightThreshold =
            GetShaderLocation(shader, "coniferHighlightThreshold"),
        .coniferRampSmoothness =
            GetShaderLocation(shader, "coniferRampSmoothness"),
        .coniferHighlightStrength =
            GetShaderLocation(shader, "coniferHighlightStrength"),
        .coniferFacetBlend =
            GetShaderLocation(shader, "coniferFacetBlend"),
        .hitFlashAmount = GetShaderLocation(shader, "hitFlashAmount"),
        .enemyWhiteFlashAmount =
            GetShaderLocation(shader, "enemyWhiteFlashAmount"),
        .enemyPresentationAmount =
            GetShaderLocation(shader, "enemyPresentationAmount"),
        .hitFlashPosition = GetShaderLocation(shader, "hitFlashPosition"),
        .hitFlashRadius = GetShaderLocation(shader, "hitFlashRadius"),
        .buildingFeedbackSweep = GetShaderLocation(shader, "buildingFeedbackSweep"),
        .buildingFeedbackTint = GetShaderLocation(shader, "buildingFeedbackTint"),
        .selectionAmount = GetShaderLocation(shader, "selectionAmount"),
        .selectionTint = GetShaderLocation(shader, "selectionTint"),
        .ghostAmount = GetShaderLocation(shader, "ghostAmount"),
        .ghostTint = GetShaderLocation(shader, "ghostTint"),
        .ghostOpacity = GetShaderLocation(shader, "ghostOpacity"),
        .outlineMaterialClassOverride = GetShaderLocation(
            shader, "outlineMaterialClassOverride"),
        .shadowMap = GetShaderLocation(shader, "shadowMap"),
        .lightViewProjection = GetShaderLocation(shader, "lightViewProjection"),
        .shadowsEnabled = GetShaderLocation(shader, "shadowsEnabled"),
        .constantBias = GetShaderLocation(shader, "constantBias"),
        .slopeBias = GetShaderLocation(shader, "slopeBias"),
        .shadowStrength = GetShaderLocation(shader, "shadowStrength"),
        .shadowMapTexelSize =
            GetShaderLocation(shader, "shadowMapTexelSize"),
        .shadowSampleCount =
            GetShaderLocation(shader, "shadowSampleCount"),
        .instancingEnabled = worldInstancingEnabledLocation_,
        .inkOutlineEligible = GetShaderLocation(
            shader, "inkOutlineEligible"),
        .inkOccluder = GetShaderLocation(shader, "inkOccluder"),
        .unlitAmount = GetShaderLocation(shader, "unlitAmount"),
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
    if (resources_.worldShader().valid() && location == worldSkinningEnabledLocation_ &&
        shader.id == resources_.worldShader().get().id) {
        const int noInterpolation = 0;
        SetShaderValue(shader, worldBonePoseOffsetLocation_, &noInterpolation, SHADER_UNIFORM_INT);
    }
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
        .hueSinCos =
            GetShaderLocation(shader, "hueSinCos"),
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
        .outlinePixelScale =
            GetShaderLocation(shader, "outlinePixelScale"),
        .outlineFogFadeStrength =
            GetShaderLocation(shader, "outlineFogFadeStrength"),
        .outlineFogColor =
            GetShaderLocation(shader, "outlineFogColor"),
        .outlineFogStart =
            GetShaderLocation(shader, "outlineFogStart"),
        .outlineFogEnd =
            GetShaderLocation(shader, "outlineFogEnd"),
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
        .selectionMask = GetShaderLocation(shader, "selectionMask"),
        .inverseProjection = GetShaderLocation(
            shader, "inverseProjection"),
        .ssaoTexelSize = GetShaderLocation(shader, "ssaoTexelSize"),
        .ssaoEnabled = GetShaderLocation(shader, "ssaoEnabled"),
        .ssaoStrength = GetShaderLocation(shader, "ssaoStrength"),
        .selectionMaskEnabled =
            GetShaderLocation(shader, "selectionMaskEnabled"),
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
    const float hueRadians = settings_.hueDegrees * DEG2RAD;
    const Vector2 hueSinCos{
        std::sin(hueRadians), std::cos(hueRadians)};
    SetShaderValue(
        shader, postProcessLocations_.hueSinCos,
        &hueSinCos, SHADER_UNIFORM_VEC2);
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
    const RenderTexture2D& sceneTarget = resources_.sceneTarget();
    const float outlinePixelScale = std::min(
        static_cast<float>(sceneTarget.texture.width) /
            static_cast<float>(std::max(GetScreenWidth(), 1)),
        static_cast<float>(sceneTarget.texture.height) /
            static_cast<float>(std::max(GetScreenHeight(), 1)));
    upload(postProcessLocations_.outlinePixelScale,
           outlinePixelScale);
    upload(postProcessLocations_.outlineFogFadeStrength,
           settings_.outlineFogFadeStrength);
    SetShaderValue(shader, postProcessLocations_.outlineFogColor,
                   &postProcessFogColor_, SHADER_UNIFORM_VEC3);
    upload(postProcessLocations_.outlineFogStart,
           postProcessFogStart_);
    upload(postProcessLocations_.outlineFogEnd,
           postProcessFogEnd_);
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
    const float selectionMaskEnabled =
        selectionMaskReady_ && resources_.selectionMaskValid()
            ? 1.0F
            : 0.0F;
    upload(postProcessLocations_.selectionMaskEnabled,
           selectionMaskEnabled);
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
    if (selectionMaskEnabled > 0.5F) {
        SetShaderValueTexture(
            shader, postProcessLocations_.selectionMask,
            resources_.selectionMask().texture);
    }
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
    postProcessFogColor_ = lighting.fogColor;
    postProcessFogStart_ = fogStart;
    postProcessFogEnd_ = fogEnd;
    const float shadowsEnabled =
        settings_.shadows && shadowFrameValid_ &&
                resources_.shadowMap().valid()
            ? 1.0F
            : 0.0F;
    const float shadowMapTexelSize =
        resources_.shadowMap().valid()
            ? 1.0F / static_cast<float>(resources_.shadowMap().size())
            : 0.0F;
    const int shadowSampleCount =
        settings_.quality == GraphicsQuality::High
            ? 9
            : (settings_.quality == GraphicsQuality::Medium ? 5 : 1);

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
    SetShaderValue(shader, worldShaderLocations_.shadowSampleCount,
                   &shadowSampleCount, SHADER_UNIFORM_INT);
    SetShaderValueMatrix(shader, worldShaderLocations_.lightViewProjection,
                         lightViewProjection_);
}

void Renderer::uploadWorldMaterial(const WorldMaterialState& material) {
    auto& shader = resources_.worldShader().get();
    // Track actual GPU values, including temporary tree/bush overrides that
    // intentionally do not change the logical worldMaterial_ state.
    const auto upload = [&](int location, const auto* value, int type) {
        std::array<float, 4> components{};
        std::size_t count = 1U;
        if constexpr (requires { value->w; }) {
            components = {value->x, value->y, value->z, value->w};
            count = 4U;
        } else if constexpr (requires { value->z; }) {
            components = {value->x, value->y, value->z, 0.0F};
            count = 3U;
        } else {
            components[0] = *value;
        }
        if (worldMaterialUniforms_.update(
                location, std::span<const float>(components.data(), count))) {
            SetShaderValue(shader, location, value, type);
        }
    };
    upload(worldShaderLocations_.baseColor,
                   &material.baseColor, SHADER_UNIFORM_VEC4);
    upload(worldShaderLocations_.bakedAo,
                   &material.bakedAo, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.vertexAoAmount,
                   &material.vertexAoAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.screenAoAmount,
                   &material.screenAoAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.terrainAmount,
                   &material.terrainAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.terrainGrassTint,
                   &material.terrainGrassTint, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.terrainDirtTint,
                   &material.terrainDirtTint, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.groundTextureScale,
                   &material.groundTextureScale, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.groundTextureStrength,
                   &material.groundTextureStrength, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.groundSaturation,
                   &material.groundSaturation, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.groundBrightness,
                   &material.groundBrightness, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.groundTextureTint,
                   &material.groundTextureTint, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.groundTextureContrast,
                   &material.groundTextureContrast, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.groundHighlightCompression,
                   &material.groundHighlightCompression,
                   SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.groundSecondaryBlend,
                   &material.groundSecondaryBlend, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.groundMacroVariation,
                   &material.groundMacroVariation, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.windAmount,
                   &material.windAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.localWindHeight,
                   &material.localWindHeight, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.distantFadeAmount,
                   &material.distantFadeAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.vegetationAmount,
                   &material.vegetationAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.treeAmount,
                   &material.treeAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.treeLeafShadowColor,
                   &material.treeLeafShadowColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.treeLeafMidColor,
                   &material.treeLeafMidColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.treeLeafLightColor,
                   &material.treeLeafLightColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.treeTrunkShadowColor,
                   &material.treeTrunkShadowColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.treeTrunkMidColor,
                   &material.treeTrunkMidColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.treeTrunkLightColor,
                   &material.treeTrunkLightColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.treeShadowThreshold,
                   &material.treeShadowThreshold, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.treeHighlightThreshold,
                   &material.treeHighlightThreshold, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.treeRampSmoothness,
                   &material.treeRampSmoothness, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.treeHighlightStrength,
                   &material.treeHighlightStrength, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.treeTextureVariation,
                   &material.treeTextureVariation, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.treeLeafFlatness,
                   &material.treeLeafFlatness, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.bushAmount,
                   &material.bushAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.bushShadowColor,
                   &material.bushShadowColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.bushMidColor,
                   &material.bushMidColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.bushLightColor,
                   &material.bushLightColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.bushShadowThreshold,
                   &material.bushShadowThreshold, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.bushHighlightThreshold,
                   &material.bushHighlightThreshold, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.bushRampSmoothness,
                   &material.bushRampSmoothness, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.bushHighlightStrength,
                   &material.bushHighlightStrength, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.coniferAmount,
                   &material.coniferAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.plantBiomeAnchor,
                   &material.plantBiomeAnchor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.coniferShadowColor,
                   &material.coniferShadowColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.coniferMidColor,
                   &material.coniferMidColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.coniferLightColor,
                   &material.coniferLightColor, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.coniferShadowThreshold,
                   &material.coniferShadowThreshold, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.coniferHighlightThreshold,
                   &material.coniferHighlightThreshold, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.coniferRampSmoothness,
                   &material.coniferRampSmoothness, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.coniferHighlightStrength,
                   &material.coniferHighlightStrength, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.coniferFacetBlend,
                   &material.coniferFacetBlend, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.hitFlashAmount,
                   &material.hitFlashAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.enemyWhiteFlashAmount,
                   &material.enemyWhiteFlashAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.enemyPresentationAmount,
                   &material.enemyPresentationAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.hitFlashPosition,
                   &material.hitFlashPosition, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.hitFlashRadius,
                   &material.hitFlashRadius, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.buildingFeedbackSweep,
        &material.buildingFeedback.sweep, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.buildingFeedbackTint,
        &material.buildingFeedback.tint, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.selectionAmount,
                   &material.selectionAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.selectionTint,
                   &material.selectionTint, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.ghostAmount,
                   &material.ghostAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.ghostTint,
                   &material.ghostTint, SHADER_UNIFORM_VEC3);
    upload(worldShaderLocations_.ghostOpacity,
                   &material.ghostOpacity, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.outlineMaterialClassOverride,
        &material.outlineMaterialClassOverride,
        SHADER_UNIFORM_FLOAT);
    const float inkOutlineEligible = material.inkOutlineEligible
        ? 1.0F : 0.0F;
    upload(worldShaderLocations_.inkOutlineEligible,
                   &inkOutlineEligible, SHADER_UNIFORM_FLOAT);
    const float inkOccluder = material.inkOccluder ? 1.0F : 0.0F;
    upload(worldShaderLocations_.inkOccluder,
                   &inkOccluder, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.unlitAmount,
                   &material.unlitAmount, SHADER_UNIFORM_FLOAT);
    upload(worldShaderLocations_.aoStrength,
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
