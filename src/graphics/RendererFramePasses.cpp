#include "graphics/Renderer.hpp"
#include "graphics/WorldTransforms.hpp"

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
void Renderer::beginWorldPass(Color clearColor) {
    performanceStats_.instancedEnemyCount = 0U;
    performanceStats_.enemyBatchCount = 0U;
    performanceStats_.lowDetailEnemyCount = 0U;
    performanceStats_.blobShadowCount = 0U;
    performanceStats_.blobShadowTriangles = 0U;
    instancedEnemyMillisecondsThisFrame_ = 0.0;
    resources_.updateFramebuffer(settings_);
    usingOffscreenTarget_ =
        settings_.postProcessing && resources_.sceneTargetValid();
    if (usingOffscreenTarget_) {
        BeginTextureMode(resources_.sceneTarget());
    } else {
        BeginDrawing();
    }
    ClearBackground(clearColor);
    frameOpen_ = true;
    worldPassOpen_ = true;
}

void Renderer::drawSky(const SkyState& sky) {
    if (!worldPassOpen_ || !settings_.sky ||
        !resources_.skyShader().valid()) {
        return;
    }

    auto& shader = resources_.skyShader().get();
    const int width =
        usingOffscreenTarget_ ? resources_.sceneWidth() : GetRenderWidth();
    const int height =
        usingOffscreenTarget_ ? resources_.sceneHeight() : GetRenderHeight();
    const Vector2 viewportSize{
        static_cast<float>(std::max(width, 1)),
        static_cast<float>(std::max(height, 1)),
    };
    const float aspectRatio = viewportSize.x / viewportSize.y;
    const float tanHalfFov =
        std::tan(sky.verticalFovDegrees * DEG2RAD * 0.5F);

    SetShaderValue(shader, skyShaderLocations_.viewportSize,
                   &viewportSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, skyShaderLocations_.cameraForward,
                   &sky.cameraForward, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.cameraRight,
                   &sky.cameraRight, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.cameraUp,
                   &sky.cameraUp, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.tanHalfFov,
                   &tanHalfFov, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, skyShaderLocations_.aspectRatio,
                   &aspectRatio, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, skyShaderLocations_.zenithColor,
                   &sky.zenithColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.horizonColor,
                   &sky.horizonColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.lowerSkyColor,
                   &sky.lowerSkyColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.celestialDirection,
                   &sky.celestialDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.celestialColor,
                   &sky.celestialColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyShaderLocations_.celestialIntensity,
                   &sky.celestialIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, skyShaderLocations_.nightAmount,
                   &sky.nightAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, skyShaderLocations_.timeSeconds,
                   &sky.timeSeconds, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, skyShaderLocations_.exposure,
                   &sky.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, skyShaderLocations_.saturation,
                   &sky.saturation, SHADER_UNIFORM_FLOAT);

    BeginShaderMode(shader);
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    DrawRectangle(0, 0, std::max(width, 1), std::max(height, 1), WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
}

void Renderer::drawClouds(
    Vector3 cameraPosition, float nightAmount,
    const WorldLighting& lighting) {
    if (!settings_.sky || !resources_.cloudShader().valid()) {
        return;
    }

    const float night = std::clamp(nightAmount, 0.0F, 1.0F);
    const float nightFadeProgress = std::clamp(
        (night - 0.30F) / 0.48F, 0.0F, 1.0F);
    const float smoothNightFade =
        nightFadeProgress * nightFadeProgress *
        (3.0F - 2.0F * nightFadeProgress);
    const float dayVisibility = 1.0F - smoothNightFade;
    if (dayVisibility <= 0.002F) {
        return;
    }

    Shader& shader = resources_.cloudShader().get();
    SetShaderValue(
        shader, cloudCameraPositionLocation_, &cameraPosition,
        SHADER_UNIFORM_VEC3);
    SetShaderValue(
        shader, cloudSunDirectionLocation_, &lighting.sunDirection,
        SHADER_UNIFORM_VEC3);
    SetShaderValue(
        shader, cloudSunColorLocation_, &lighting.sunColor,
        SHADER_UNIFORM_VEC3);
    SetShaderValue(
        shader, cloudSunIntensityLocation_, &lighting.sunIntensity,
        SHADER_UNIFORM_FLOAT);
    SetShaderValue(
        shader, cloudAmbientColorLocation_, &lighting.skyAmbientColor,
        SHADER_UNIFORM_VEC3);

    struct CloudInstance {
        Vector3 position;
        Vector3 scale;
        float rotationDegrees{};
        float visibility{};
        float distanceSquared{};
        std::size_t variant{};
    };
    constexpr std::size_t CloudGridSize = 5U;
    constexpr std::size_t CloudCount = CloudGridSize * CloudGridSize;
    constexpr float WrapHalfExtent = 220.0F;
    constexpr float WrapExtent = WrapHalfExtent * 2.0F;
    constexpr float CloudCellSize =
        WrapExtent / static_cast<float>(CloudGridSize);
    constexpr float FadeStart = 168.0F;
    constexpr float FadeEnd = 218.0F;
    std::array<CloudInstance, CloudCount> clouds{};
    std::size_t visibleCount = 0U;
    const float timeSeconds = static_cast<float>(GetTime());
    const auto hash = [](std::uint32_t value) {
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        return value ^ (value >> 16U);
    };
    const auto unitFloat = [&hash](std::uint32_t value) {
        return static_cast<float>(hash(value) & 0xffffU) /
            65535.0F;
    };
    const auto wrapAround = [](float value, float center) {
        float relative = std::fmod(
            value - center + WrapHalfExtent, WrapExtent);
        if (relative < 0.0F) {
            relative += WrapExtent;
        }
        return center + relative - WrapHalfExtent;
    };
    const auto smoothstep = [](float edge0, float edge1, float value) {
        const float amount = std::clamp(
            (value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
        return amount * amount * (3.0F - 2.0F * amount);
    };

    for (std::size_t index = 0; index < CloudCount; ++index) {
        const std::uint32_t seed =
            static_cast<std::uint32_t>(index) * 0x9e3779b9U +
            0x51f15e1dU;
        const std::size_t column = index % CloudGridSize;
        const std::size_t row = index / CloudGridSize;
        const float jitterX =
            (unitFloat(seed + 2U) * 2.0F - 1.0F) *
            CloudCellSize * 0.27F;
        const float jitterZ =
            (unitFloat(seed + 3U) * 2.0F - 1.0F) *
            CloudCellSize * 0.27F;
        const float baseX = -WrapHalfExtent +
            (static_cast<float>(column) + 0.5F) * CloudCellSize +
            jitterX;
        const float baseZ = -WrapHalfExtent +
            (static_cast<float>(row) + 0.5F) * CloudCellSize +
            jitterZ;
        const float x = wrapAround(
            baseX + timeSeconds * 1.04F, cameraPosition.x);
        const float z = wrapAround(
            baseZ + timeSeconds * 0.22F,
            cameraPosition.z);
        const float offsetX = x - cameraPosition.x;
        const float offsetZ = z - cameraPosition.z;
        const float distanceSquared =
            offsetX * offsetX + offsetZ * offsetZ;
        const float distance = std::sqrt(distanceSquared);
        const float distanceFade = 1.0F -
            smoothstep(FadeStart, FadeEnd, distance);
        const float visibility =
            dayVisibility * distanceFade * 0.96F;
        if (visibility <= 0.002F) {
            continue;
        }

        const std::size_t variant =
            unitFloat(seed + 4U) < 0.46F ? 0U : 1U;
        const float baseScale =
            (variant == 0U ? 6.6F : 7.2F) +
            unitFloat(seed + 5U) *
                (variant == 0U ? 8.8F : 9.2F);
        clouds[visibleCount++] = {
            .position = {
                x,
                54.0F + unitFloat(seed + 6U) * 34.0F,
                z,
            },
            .scale = {
                baseScale *
                    (0.85F + unitFloat(seed + 7U) * 0.50F),
                baseScale *
                    (0.84F + unitFloat(seed + 8U) * 0.30F),
                baseScale *
                    (0.78F + unitFloat(seed + 9U) * 0.44F),
            },
            .rotationDegrees =
                (unitFloat(seed + 10U) * 2.0F - 1.0F) * 8.0F,
            .visibility = visibility,
            .distanceSquared = distanceSquared,
            .variant = variant,
        };
    }

    std::sort(
        clouds.begin(), clouds.begin() +
            static_cast<std::ptrdiff_t>(visibleCount),
        [](const CloudInstance& left, const CloudInstance& right) {
            return left.distanceSquared > right.distanceSquared;
        });

    rlDrawRenderBatchActive();
    // Blend cloud color normally, but clear the scene material-mask alpha.
    // The ink post-process uses that alpha to decide which pixels may receive
    // outlines, so clouds remain soft while retaining their distance fade.
    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
        RL_ZERO, RL_ZERO,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlDisableDepthMask();
    for (std::size_t index = 0; index < visibleCount; ++index) {
        const CloudInstance& cloud = clouds[index];
        ModelResource& resource = resources_.cloudModel(cloud.variant);
        if (!resource.valid()) {
            continue;
        }
        Model& model = resource.get();
        for (int materialIndex = 0;
             materialIndex < model.materialCount; ++materialIndex) {
            model.materials[materialIndex].shader = shader;
        }
        SetShaderValue(
            shader, cloudVisibilityLocation_, &cloud.visibility,
            SHADER_UNIFORM_FLOAT);
        DrawModelEx(
            model, cloud.position, {0.0F, 1.0F, 0.0F},
            cloud.rotationDegrees, cloud.scale, WHITE);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
}

void Renderer::endWorldPass() {
    if (!worldPassOpen_) {
        return;
    }

    if (usingOffscreenTarget_) {
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);

        const auto& target = resources_.sceneTarget();
        const Rectangle source{
            0.0F,
            0.0F,
            static_cast<float>(target.texture.width),
            -static_cast<float>(target.texture.height),
        };
        const Rectangle destination{
            0.0F,
            0.0F,
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight()),
        };
        if (resources_.postProcessShader().valid()) {
            uploadPostProcessSettings();
            BeginShaderMode(
                resources_.postProcessShader().get());
            DrawTexturePro(
                target.texture, source, destination,
                {0.0F, 0.0F}, 0.0F, WHITE);
            EndShaderMode();
        } else {
            DrawTexturePro(
                target.texture, source, destination,
                {0.0F, 0.0F}, 0.0F, WHITE);
        }
    }
    worldPassOpen_ = false;
}

bool Renderer::beginFirstPersonToolPass() {
    resources_.updateViewModelTarget();
    if (!resources_.viewModelTargetValid()) {
        return false;
    }
    BeginTextureMode(resources_.viewModelTarget());
    ClearBackground(BLANK);
    return true;
}

void Renderer::endFirstPersonToolPass(
    const FirstPersonToolTuning& tuning) {
    if (!resources_.viewModelTargetValid()) {
        return;
    }
    EndTextureMode();
    const RenderTexture2D& target =
        resources_.viewModelTarget();
    const Rectangle source{
        0.0F, 0.0F,
        static_cast<float>(target.texture.width),
        -static_cast<float>(target.texture.height),
    };
    const Rectangle destination{
        0.0F, 0.0F,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight()),
    };
    if (resources_.viewModelCompositeShader().valid()) {
        Shader& shader =
            resources_.viewModelCompositeShader().get();
        const Vector2 texelSize{
            1.0F / static_cast<float>(
                       std::max(target.texture.width, 1)),
            1.0F / static_cast<float>(
                       std::max(target.texture.height, 1)),
        };
        const float outlineEnabled =
            tuning.outlineEnabled ? 1.0F : 0.0F;
        SetShaderValue(shader, viewModelTexelSizeLocation_,
                       &texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, viewModelOutlineEnabledLocation_,
                       &outlineEnabled, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, viewModelOutlineWidthLocation_,
                       &tuning.outlineWidth, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, viewModelOutlineStrengthLocation_,
                       &tuning.outlineStrength, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, viewModelRimStrengthLocation_,
                       &tuning.rimStrength, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, viewModelBrightnessLocation_,
                       &tuning.brightness, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, viewModelSaturationLocation_,
                       &tuning.saturation, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(shader);
        DrawTexturePro(target.texture, source, destination,
                       {}, 0.0F, WHITE);
        EndShaderMode();
        return;
    }
    DrawTexturePro(target.texture, source, destination,
                   {}, 0.0F, WHITE);
}

void Renderer::drawScenePreview(Rectangle bounds) {
    if (!resources_.sceneTargetValid() ||
        bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return;
    }
    const auto& target = resources_.sceneTarget();
    const Rectangle source{
        0.0F, 0.0F,
        static_cast<float>(target.texture.width),
        -static_cast<float>(target.texture.height),
    };
    if (resources_.postProcessShader().valid()) {
        uploadPostProcessSettings();
        BeginShaderMode(
            resources_.postProcessShader().get());
        DrawTexturePro(
            target.texture, source, bounds,
            {0.0F, 0.0F}, 0.0F, WHITE);
        EndShaderMode();
    } else {
        DrawTexturePro(
            target.texture, source, bounds,
            {0.0F, 0.0F}, 0.0F, WHITE);
    }
}

void Renderer::beginUiOnlyFrame(Color clearColor) {
    performanceStats_.instancedEnemyCount = 0U;
    performanceStats_.enemyBatchCount = 0U;
    performanceStats_.lowDetailEnemyCount = 0U;
    performanceStats_.blobShadowCount = 0U;
    performanceStats_.blobShadowTriangles = 0U;
    instancedEnemyMillisecondsThisFrame_ = 0.0;
    resources_.updateFramebuffer(settings_);
    usingOffscreenTarget_ = false;
    BeginDrawing();
    ClearBackground(clearColor);
    frameOpen_ = true;
    worldPassOpen_ = false;
}

void Renderer::endFrame() {
    if (!frameOpen_) {
        return;
    }
    if (worldPassOpen_) {
        endWorldPass();
    }
    if (showShadowMap_) {
        drawShadowMapDebug();
    }
    performanceStats_.instancedEnemyDraw.sample(
        instancedEnemyMillisecondsThisFrame_);
    EndDrawing();
    frameOpen_ = false;
}

bool Renderer::beginShadowPass(const WorldLighting& lighting, Vector3 focus) {
    resources_.updateShadowMap(settings_);
    if (!settings_.shadows || !settings_.worldShader ||
        !resources_.worldShader().valid() ||
        !resources_.shadowMap().valid() ||
        !resources_.shadowShader().valid()) {
        shadowFrameValid_ = false;
        shadowCacheInitialized_ = false;
        return false;
    }

    const float shadowDistance = std::max(settings_.shadowDistance, 10.0F);
    const Vector3 sunDirection = Vector3Normalize(lighting.sunDirection);
    const int shadowMapSize = resources_.shadowMap().size();
    const double now = GetTime();
    const float focusDeltaX = focus.x - shadowLastFocus_.x;
    const float focusDeltaZ = focus.z - shadowLastFocus_.z;
    const float sunAlignment =
        Vector3DotProduct(sunDirection, shadowLastSunDirection_);
    const bool shadowSettingsChanged =
        !shadowCacheInitialized_ ||
        shadowMapSize != shadowLastMapSize_ ||
        std::abs(shadowDistance - shadowLastDistance_) > 0.01F;
    constexpr double ShadowRefreshInterval = 1.0 / 30.0;
    constexpr float FocusRefreshDistanceSquared = 1.0F;
    constexpr float SunRefreshAlignment = 0.0005F;
    const bool cacheFresh =
        shadowFrameValid_ && !shadowSettingsChanged &&
        now - shadowLastUpdateTime_ < ShadowRefreshInterval &&
        focusDeltaX * focusDeltaX + focusDeltaZ * focusDeltaZ <
            FocusRefreshDistanceSquared &&
        1.0F - sunAlignment < SunRefreshAlignment;
    if (cacheFresh) {
        return false;
    }
    shadowFrameValid_ = false;
    const Vector3 lightPosition =
        Vector3Subtract(focus, Vector3Scale(sunDirection, shadowDistance));
    const Vector3 worldUp{0.0F, 1.0F, 0.0F};
    const Vector3 fallbackUp{0.0F, 0.0F, 1.0F};
    const Vector3 up =
        std::abs(Vector3DotProduct(sunDirection, worldUp)) > 0.95F
            ? fallbackUp
            : worldUp;
    const Camera3D lightCamera{
        .position = lightPosition,
        .target = focus,
        .up = up,
        .fovy = shadowDistance * 2.0F,
        .projection = CAMERA_ORTHOGRAPHIC,
    };
    shadowFocus_ = focus;
    shadowLastFocus_ = focus;
    shadowLastSunDirection_ = sunDirection;
    shadowLastDistance_ = shadowDistance;
    shadowLastMapSize_ = shadowMapSize;
    shadowLastUpdateTime_ = now;
    shadowCacheInitialized_ = true;

    BeginTextureMode(resources_.shadowMap().target());
    ClearBackground(WHITE);
    BeginMode3D(lightCamera);
    const Matrix lightView = rlGetMatrixModelview();
    const Matrix lightProjection = rlGetMatrixProjection();
    lightViewProjection_ = MatrixMultiply(lightView, lightProjection);
    BeginShaderMode(resources_.shadowShader().get());
    shadowPassOpen_ = true;
    return true;
}

void Renderer::endShadowPass() {
    if (!shadowPassOpen_) {
        return;
    }
    EndShaderMode();
    EndMode3D();
    EndTextureMode();
    shadowPassOpen_ = false;
    shadowFrameValid_ = true;
}

bool Renderer::beginSelectionMaskPass(const Camera3D& camera) {
    resources_.updateFramebuffer(settings_);
    resources_.updateSelectionMask(settings_);
    selectionMaskReady_ = false;
    if (selectionMaskPassOpen_ ||
        !resources_.selectionMaskValid() ||
        !resources_.selectionMaskShader().valid()) {
        return false;
    }

    BeginTextureMode(resources_.selectionMask());
    ClearBackground(BLANK);
    BeginMode3D(camera);
    selectionMaskCamera_ = camera;
    BeginShaderMode(resources_.selectionMaskShader().get());
    // Depth is encoded in alpha, so source blending would corrupt it when
    // selected geometry replaces a previously drawn black occluder.
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    const float timeSeconds = static_cast<float>(GetTime());
    constexpr float NoWind = 0.0F;
    constexpr Vector4 WhiteMask{1.0F, 1.0F, 1.0F, 1.0F};
    SetShaderValue(resources_.selectionMaskShader().get(),
                   selectionMaskTimeLocation_, &timeSeconds,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(resources_.selectionMaskShader().get(),
                   selectionMaskWindLocation_, &NoWind,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(resources_.selectionMaskShader().get(),
                   selectionMaskColorLocation_, &WhiteMask,
                   SHADER_UNIFORM_VEC4);
    selectionMaskPassOpen_ = true;
    return true;
}

void Renderer::setSelectionOutlineBounds(
    BoundingBox worldBounds) {
    if (!selectionMaskPassOpen_ ||
        !resources_.selectionMaskValid() ||
        !world_transforms::finite(worldBounds)) {
        return;
    }
    const auto& target = resources_.selectionMask();
    const int width = std::max(target.texture.width, 1);
    const int height = std::max(target.texture.height, 1);
    const std::array<Vector3, 8> corners{{
        {worldBounds.min.x, worldBounds.min.y, worldBounds.min.z},
        {worldBounds.max.x, worldBounds.min.y, worldBounds.min.z},
        {worldBounds.min.x, worldBounds.max.y, worldBounds.min.z},
        {worldBounds.max.x, worldBounds.max.y, worldBounds.min.z},
        {worldBounds.min.x, worldBounds.min.y, worldBounds.max.z},
        {worldBounds.max.x, worldBounds.min.y, worldBounds.max.z},
        {worldBounds.min.x, worldBounds.max.y, worldBounds.max.z},
        {worldBounds.max.x, worldBounds.max.y, worldBounds.max.z},
    }};
    const Vector3 cameraForward = Vector3Normalize(Vector3Subtract(
        selectionMaskCamera_.target,
        selectionMaskCamera_.position));
    for (const Vector3 corner : corners) {
        if (Vector3DotProduct(
                Vector3Subtract(
                    corner, selectionMaskCamera_.position),
                cameraForward) <= 0.01F) {
            return;
        }
    }
    float minimumX = static_cast<float>(width);
    float minimumY = static_cast<float>(height);
    float maximumX = 0.0F;
    float maximumY = 0.0F;
    for (const Vector3 corner : corners) {
        const Vector2 screen = GetWorldToScreenEx(
            corner, selectionMaskCamera_, width, height);
        minimumX = std::min(minimumX, screen.x);
        minimumY = std::min(minimumY, screen.y);
        maximumX = std::max(maximumX, screen.x);
        maximumY = std::max(maximumY, screen.y);
    }
    constexpr float Padding = 16.0F;
    minimumX = std::clamp(minimumX - Padding, 0.0F,
                          static_cast<float>(width));
    minimumY = std::clamp(minimumY - Padding, 0.0F,
                          static_cast<float>(height));
    maximumX = std::clamp(maximumX + Padding, 0.0F,
                          static_cast<float>(width));
    maximumY = std::clamp(maximumY + Padding, 0.0F,
                          static_cast<float>(height));
    if (maximumX > minimumX && maximumY > minimumY) {
        const Rectangle nextBounds{
            minimumX, minimumY,
            maximumX - minimumX,
            maximumY - minimumY,
        };
        if (!selectionOutlineBounds_) {
            selectionOutlineBounds_ = nextBounds;
        } else {
            const Rectangle current = *selectionOutlineBounds_;
            const float unionMinimumX =
                std::min(current.x, nextBounds.x);
            const float unionMinimumY =
                std::min(current.y, nextBounds.y);
            const float unionMaximumX = std::max(
                current.x + current.width,
                nextBounds.x + nextBounds.width);
            const float unionMaximumY = std::max(
                current.y + current.height,
                nextBounds.y + nextBounds.height);
            selectionOutlineBounds_ = {
                unionMinimumX,
                unionMinimumY,
                unionMaximumX - unionMinimumX,
                unionMaximumY - unionMinimumY,
            };
        }
    }
}

void Renderer::setSelectionMaskWind(float amount) {
    if (!selectionMaskPassOpen_ ||
        !resources_.selectionMaskShader().valid()) {
        return;
    }
    rlDrawRenderBatchActive();
    SetShaderValue(resources_.selectionMaskShader().get(),
                   selectionMaskWindLocation_, &amount,
                   SHADER_UNIFORM_FLOAT);
}

void Renderer::setSelectionMaskColor(Color color) {
    if (!selectionMaskPassOpen_ ||
        !resources_.selectionMaskShader().valid()) {
        return;
    }
    rlDrawRenderBatchActive();
    const Vector4 normalized = ColorNormalize(color);
    SetShaderValue(resources_.selectionMaskShader().get(),
                   selectionMaskColorLocation_, &normalized,
                   SHADER_UNIFORM_VEC4);
}

void Renderer::setSelectionOutlineTint(Color tint) {
    selectionOutlineTint_ = tint;
}

void Renderer::endSelectionMaskPass() {
    if (!selectionMaskPassOpen_) {
        return;
    }
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
    EndMode3D();
    EndTextureMode();
    selectionMaskPassOpen_ = false;
    selectionMaskReady_ = true;
}

void Renderer::clearSelectionOutline() {
    selectionMaskReady_ = false;
    selectionOutlineBounds_.reset();
    selectionOutlineTint_ = WHITE;
}

void Renderer::drawSelectionOutline() {
    if (!selectionMaskReady_ ||
        !resources_.selectionMaskValid() ||
        !resources_.selectionOutlineShader().valid()) {
        return;
    }

    const auto& target = resources_.selectionMask();
    const Vector2 texelSize{
        1.0F / static_cast<float>(std::max(target.texture.width, 1)),
        1.0F / static_cast<float>(std::max(target.texture.height, 1)),
    };
    auto& shader = resources_.selectionOutlineShader().get();
    SetShaderValue(shader, selectionOutlineTexelSizeLocation_,
                   &texelSize, SHADER_UNIFORM_VEC2);
    const float outputScaleX =
        static_cast<float>(GetScreenWidth()) /
        static_cast<float>(std::max(target.texture.width, 1));
    const float outputScaleY =
        static_cast<float>(GetScreenHeight()) /
        static_cast<float>(std::max(target.texture.height, 1));
    const float outputScale =
        std::max(std::min(outputScaleX, outputScaleY), 0.001F);
    const float pulse =
        0.5F +
        0.5F *
            std::sin(
                static_cast<float>(GetTime()) * 3.2F);
    const int outlineRadius = std::clamp(
        static_cast<int>(std::ceil(12.0F / outputScale)), 1, 12);
    SetShaderValue(shader, selectionOutlineRadiusLocation_,
                   &outlineRadius, SHADER_UNIFORM_INT);
    const Rectangle source{
        0.0F, 0.0F,
        static_cast<float>(target.texture.width),
        -static_cast<float>(target.texture.height),
    };
    const Rectangle destination{
        0.0F, 0.0F,
        usingOffscreenTarget_
            ? static_cast<float>(resources_.sceneWidth())
            : static_cast<float>(GetScreenWidth()),
        usingOffscreenTarget_
            ? static_cast<float>(resources_.sceneHeight())
            : static_cast<float>(GetScreenHeight()),
    };
    BeginBlendMode(BLEND_ALPHA);
    if (selectionOutlineBounds_) {
        const Rectangle bounds = *selectionOutlineBounds_;
        BeginScissorMode(
            static_cast<int>(std::floor(bounds.x)),
            static_cast<int>(std::floor(bounds.y)),
            std::max(1, static_cast<int>(std::ceil(bounds.width))),
            std::max(1, static_cast<int>(std::ceil(bounds.height))));
    }
    BeginShaderMode(shader);
    const auto brightness =
        static_cast<unsigned char>(
            std::lround(244.0F + pulse * 11.0F));
    const auto alpha =
        static_cast<unsigned char>(
            std::lround(238.0F + pulse * 17.0F));
    DrawTexturePro(
        target.texture, source, destination,
        {0.0F, 0.0F}, 0.0F,
        {static_cast<unsigned char>(
             static_cast<unsigned int>(selectionOutlineTint_.r) *
             brightness / 255U),
         static_cast<unsigned char>(
             static_cast<unsigned int>(selectionOutlineTint_.g) *
             brightness / 255U),
         static_cast<unsigned char>(
             static_cast<unsigned int>(selectionOutlineTint_.b) *
             brightness / 255U),
         static_cast<unsigned char>(
             static_cast<unsigned int>(selectionOutlineTint_.a) *
             alpha / 255U)});
    EndShaderMode();
    if (selectionOutlineBounds_) {
        EndScissorMode();
    }
    EndBlendMode();
}

bool Renderer::shadowCasterVisible(Vector3 position, float radius) const {
    const float offsetX = position.x - shadowFocus_.x;
    const float offsetZ = position.z - shadowFocus_.z;
    const float maximumDistance = settings_.shadowDistance + radius;
    return offsetX * offsetX + offsetZ * offsetZ <=
           maximumDistance * maximumDistance;
}

void Renderer::beginWorldShader(const WorldLighting& lighting) {
    if (!settings_.worldShader || !resources_.worldShader().valid()) {
        worldShaderActive_ = false;
        return;
    }

    uploadWorldLighting(lighting);
    worldMaterial_ = {};
    uploadWorldMaterial(worldMaterial_);
    BeginShaderMode(resources_.worldShader().get());
    worldShaderActive_ = true;
    bindTerrainTexture();
    bindShadowMap();
}

void Renderer::setWorldMaterial(const WorldMaterialState& material) {
    if (!worldShaderActive_) {
        return;
    }
    if (material.baseColor.x == worldMaterial_.baseColor.x &&
        material.baseColor.y == worldMaterial_.baseColor.y &&
        material.baseColor.z == worldMaterial_.baseColor.z &&
        material.baseColor.w == worldMaterial_.baseColor.w &&
        material.bakedAo == worldMaterial_.bakedAo &&
        material.vertexAoAmount == worldMaterial_.vertexAoAmount &&
        material.terrainAmount == worldMaterial_.terrainAmount &&
        material.terrainGrassTint.x == worldMaterial_.terrainGrassTint.x &&
        material.terrainGrassTint.y == worldMaterial_.terrainGrassTint.y &&
        material.terrainGrassTint.z == worldMaterial_.terrainGrassTint.z &&
        material.terrainDirtTint.x == worldMaterial_.terrainDirtTint.x &&
        material.terrainDirtTint.y == worldMaterial_.terrainDirtTint.y &&
        material.terrainDirtTint.z == worldMaterial_.terrainDirtTint.z &&
        material.windAmount == worldMaterial_.windAmount &&
        material.localWindHeight == worldMaterial_.localWindHeight &&
        material.distantFadeAmount == worldMaterial_.distantFadeAmount &&
        material.hitFlashAmount == worldMaterial_.hitFlashAmount &&
        material.selectionAmount == worldMaterial_.selectionAmount &&
        material.selectionTint.x == worldMaterial_.selectionTint.x &&
        material.selectionTint.y == worldMaterial_.selectionTint.y &&
        material.selectionTint.z == worldMaterial_.selectionTint.z &&
        material.inkOutlineEligible == worldMaterial_.inkOutlineEligible) {
        return;
    }

    rlDrawRenderBatchActive();
    worldMaterial_ = material;
    uploadWorldMaterial(worldMaterial_);
}

void Renderer::beginGhostPreviewMaterial() {
    if (!worldShaderActive_ ||
        ghostPreviewRestoreMaterial_) {
        return;
    }
    ghostPreviewRestoreMaterial_ = worldMaterial_;
    WorldMaterialState ghost = worldMaterial_;
    ghost.bakedAo = std::max(ghost.bakedAo, 0.88F);
    ghost.selectionAmount = 0.16F;
    ghost.selectionTint = {1.0F, 1.0F, 1.0F};
    setWorldMaterial(ghost);
}

void Renderer::endGhostPreviewMaterial() {
    if (!ghostPreviewRestoreMaterial_) {
        return;
    }
    const WorldMaterialState restore =
        *ghostPreviewRestoreMaterial_;
    ghostPreviewRestoreMaterial_.reset();
    setWorldMaterial(restore);
}

void Renderer::endWorldShader() {
    if (!worldShaderActive_) {
        return;
    }
    EndShaderMode();
    constexpr int TerrainTextureSlot = 9;
    rlActiveTextureSlot(TerrainTextureSlot);
    rlDisableTexture();
    constexpr int ShadowTextureSlot = 10;
    rlActiveTextureSlot(ShadowTextureSlot);
    rlDisableTexture();
    rlActiveTextureSlot(0);
    worldShaderActive_ = false;
    ghostPreviewRestoreMaterial_.reset();
}

} // namespace ian
