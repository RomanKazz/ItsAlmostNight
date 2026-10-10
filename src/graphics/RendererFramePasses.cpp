#include "graphics/Renderer.hpp"
#include "graphics/CameraCulling.hpp"
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
namespace {

} // namespace

void Renderer::beginWorldPass(
    Color clearColor, const Camera3D& camera) {
    performanceStats_.instancedEnemyCount = 0U;
    performanceStats_.enemyBatchCount = 0U;
    performanceStats_.lowDetailEnemyCount = 0U;
    performanceStats_.blobShadowCount = 0U;
    performanceStats_.blobShadowTriangles = 0U;
    instancedEnemyMillisecondsThisFrame_ = 0.0;
    resources_.updateFramebuffer(settings_);
    const float aspectRatio = static_cast<float>(
        std::max(resources_.sceneWidth(), 1)) /
        static_cast<float>(std::max(resources_.sceneHeight(), 1));
    ssaoProjection_ = MatrixPerspective(
        static_cast<double>(camera.fovy*DEG2RAD),
        static_cast<double>(aspectRatio),
        rlGetCullDistanceNear(), rlGetCullDistanceFar());
    ssaoInverseProjection_ = MatrixInvert(ssaoProjection_);
    ssaoViewMatrix_ = GetCameraMatrix(camera);
    ssaoFrameReady_ = false;
    usingOffscreenTarget_ = resources_.sceneTargetValid();
    if (usingOffscreenTarget_) {
        BeginTextureMode(resources_.sceneTarget());
        rlActiveDrawBuffers(
            resources_.sceneScreenSpaceBuffers() ? 2 : 1);
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
    const bool skyboxesReady =
        resources_.skyboxTexture(0).valid() &&
        resources_.skyboxTexture(1).valid() &&
        resources_.skyboxTexture(2).valid();
    const float skyboxEnabled = skyboxesReady ? 1.0F : 0.0F;
    SetShaderValue(shader, skyShaderLocations_.skyboxEnabled,
                   &skyboxEnabled, SHADER_UNIFORM_FLOAT);
    constexpr int DaySkyboxSlot = 6;
    constexpr int MorningSkyboxSlot = 7;
    constexpr int NightSkyboxSlot = 8;
    if (skyboxesReady) {
        SetShaderValue(shader, skyShaderLocations_.daySkybox,
                       &DaySkyboxSlot, SHADER_UNIFORM_INT);
        SetShaderValue(shader, skyShaderLocations_.morningSkybox,
                       &MorningSkyboxSlot, SHADER_UNIFORM_INT);
        SetShaderValue(shader, skyShaderLocations_.nightSkybox,
                       &NightSkyboxSlot, SHADER_UNIFORM_INT);
        rlActiveTextureSlot(DaySkyboxSlot);
        rlEnableTexture(resources_.skyboxTexture(0).get().id);
        rlActiveTextureSlot(MorningSkyboxSlot);
        rlEnableTexture(resources_.skyboxTexture(1).get().id);
        rlActiveTextureSlot(NightSkyboxSlot);
        rlEnableTexture(resources_.skyboxTexture(2).get().id);
        rlActiveTextureSlot(0);
    }
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
    if (skyboxesReady) {
        rlActiveTextureSlot(DaySkyboxSlot);
        rlDisableTexture();
        rlActiveTextureSlot(MorningSkyboxSlot);
        rlDisableTexture();
        rlActiveTextureSlot(NightSkyboxSlot);
        rlDisableTexture();
        rlActiveTextureSlot(0);
    }
}

void Renderer::drawClouds(
    const Camera3D& camera, float nightAmount,
    const WorldLighting& lighting) {
    if (!settings_.sky || !resources_.cloudShader().valid()) {
        return;
    }

    const float night = std::clamp(nightAmount, 0.0F, 1.0F);
    const float cloudVisibility = 0.94F - night * 0.08F;

    Shader& shader = resources_.cloudShader().get();
    const Vector3 cameraPosition = camera.position;
    const auto cameraView =
        camera_culling::horizontalView(camera);
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
    SetShaderValue(
        shader, cloudNightAmountLocation_, &night,
        SHADER_UNIFORM_FLOAT);

    struct CloudInstance {
        Vector3 position;
        Vector3 scale;
        float rotationDegrees{};
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
        // Deterministic gaps break up the previous grid-like coverage while
        // retaining the same wrapping architecture and fixed upper bound.
        if (unitFloat(seed + 1U) < 0.30F) {
            continue;
        }
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
        const float speedVariation =
            0.88F + unitFloat(seed + 11U) * 0.24F;
        const float lateralSpeed =
            0.075F +
            (unitFloat(seed + 12U) - 0.5F) * 0.08F;
        const float x = wrapAround(
            baseX + timeSeconds * 0.42F * speedVariation,
            cameraPosition.x);
        const float z = wrapAround(
            baseZ + timeSeconds * lateralSpeed,
            cameraPosition.z);
        const float offsetX = x - cameraPosition.x;
        const float offsetZ = z - cameraPosition.z;
        const float distanceSquared =
            offsetX * offsetX + offsetZ * offsetZ;
        const float distance = std::sqrt(distanceSquared);
        const float facing =
            offsetX * cameraView.forward.x +
            offsetZ * cameraView.forward.y;
        if (distanceSquared > 2500.0F) {
            const float sideDistance = std::abs(
                offsetX * cameraView.forward.y -
                offsetZ * cameraView.forward.x);
            if (facing < -30.0F ||
                sideDistance >
                    std::max(facing, 0.0F) *
                        cameraView.fovTangent + 35.0F) {
                continue;
            }
        }
        const float distanceFade = 1.0F -
            smoothstep(FadeStart, FadeEnd, distance);
        const float visibility =
            cloudVisibility * distanceFade;
        if (visibility <= 0.002F) {
            continue;
        }

        const std::size_t variant =
            unitFloat(seed + 4U) < 0.46F ? 0U : 1U;
        const float scaleRoll = unitFloat(seed + 5U);
        const float scaleShape =
            scaleRoll * scaleRoll * (3.0F - 2.0F * scaleRoll);
        const float baseScale =
            (variant == 0U ? 4.8F : 5.4F) +
            scaleShape * (variant == 0U ? 14.2F : 15.8F);
        clouds[visibleCount++] = {
            .position = {
                x,
                54.0F + unitFloat(seed + 6U) * 34.0F,
                z,
            },
            .scale = {
                baseScale *
                    (0.72F + unitFloat(seed + 7U) * 0.76F),
                baseScale *
                    (0.42F + unitFloat(seed + 8U) * 0.30F),
                baseScale *
                    (0.68F + unitFloat(seed + 9U) * 0.68F),
            },
            .rotationDegrees =
                (unitFloat(seed + 10U) * 2.0F - 1.0F) * 8.0F,
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

    std::array<std::array<Matrix, CloudCount>, 2> transforms{};
    std::array<int, 2> transformCounts{};
    for (std::size_t index = 0; index < visibleCount; ++index) {
        const CloudInstance& cloud = clouds[index];
        ModelResource& resource = resources_.cloudModel(cloud.variant);
        if (!resource.valid()) {
            continue;
        }
        const std::size_t variant = cloud.variant % transforms.size();
        transforms[variant][static_cast<std::size_t>(
            transformCounts[variant]++)] = MatrixMultiply(
            resource.get().transform,
            MatrixMultiply(
                MatrixScale(
                    cloud.scale.x, cloud.scale.y, cloud.scale.z),
                MatrixMultiply(
                    MatrixRotateY(cloud.rotationDegrees * DEG2RAD),
                    MatrixTranslate(
                        cloud.position.x,
                        cloud.position.y,
                        cloud.position.z))));
    }
    SetShaderValue(
        shader, cloudVisibilityLocation_, &cloudVisibility,
        SHADER_UNIFORM_FLOAT);
    for (std::size_t variant = 0; variant < transforms.size(); ++variant) {
        const int transformCount = transformCounts[variant];
        ModelResource& resource = resources_.cloudModel(variant);
        if (transformCount <= 0 || !resource.valid()) {
            continue;
        }
        Model& model = resource.get();
        for (int materialIndex = 0;
             materialIndex < model.materialCount; ++materialIndex) {
            model.materials[materialIndex].shader = shader;
        }
        static_cast<void>(instanceBuffers_.drawModel(
            model, shader,
            std::span<const Matrix>(
                transforms[variant].data(),
                static_cast<std::size_t>(transformCount))));
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
        drawSsaoPass();
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
        const bool useFxaa = settings_.postProcessing &&
            resources_.postProcessShader().valid() &&
            resources_.fxaaShader().valid() &&
            resources_.postProcessTargetValid();
        if (useFxaa) {
            const RenderTexture2D& postTarget =
                resources_.postProcessTarget();
            BeginTextureMode(postTarget);
            ClearBackground(BLACK);
            BeginShaderMode(
                resources_.postProcessShader().get());
            // Additional sampler textures must be registered after entering
            // shader mode. BeginShaderMode flushes raylib's batch and clears
            // registrations made by SetShaderValueTexture beforehand.
            uploadPostProcessSettings();
            DrawTexturePro(
                target.texture, source,
                {0.0F, 0.0F,
                 static_cast<float>(postTarget.texture.width),
                 static_cast<float>(postTarget.texture.height)},
                {0.0F, 0.0F}, 0.0F, WHITE);
            EndShaderMode();
            EndTextureMode();

            BeginDrawing();
            ClearBackground(BLACK);
            BeginShaderMode(resources_.fxaaShader().get());
            DrawTexturePro(
                postTarget.texture,
                {0.0F, 0.0F,
                 static_cast<float>(postTarget.texture.width),
                 -static_cast<float>(postTarget.texture.height)},
                destination, {0.0F, 0.0F}, 0.0F, WHITE);
            EndShaderMode();
        } else {
            BeginDrawing();
            ClearBackground(BLACK);
            if (settings_.postProcessing &&
                resources_.postProcessShader().valid()) {
                BeginShaderMode(
                    resources_.postProcessShader().get());
                uploadPostProcessSettings();
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
    }
    worldPassOpen_ = false;
}

void Renderer::drawSsaoPass() {
    ssaoFrameReady_ = false;
    if (!settings_.postProcessing || !settings_.ssao ||
        !resources_.ssaoShader().valid() ||
        !resources_.ssaoTargetValid() ||
        !resources_.sceneTargetValid()) {
        return;
    }

    const RenderTexture2D& scene = resources_.sceneTarget();
    const RenderTexture2D& target = resources_.ssaoTarget();
    Shader& shader = resources_.ssaoShader().get();
    const Vector2 texelSize{
        1.0F / static_cast<float>(std::max(scene.texture.width, 1)),
        1.0F / static_cast<float>(std::max(scene.texture.height, 1)),
    };
    float radius = 0.9F;
    float bias = 0.045F;
    float fadeStart = 30.0F;
    float fadeEnd = 50.0F;
    int sampleCount = 8;
    if (settings_.quality == GraphicsQuality::Low) {
        radius = 0.65F;
        bias = 0.055F;
        fadeStart = 18.0F;
        fadeEnd = 30.0F;
        sampleCount = 4;
    } else if (settings_.quality == GraphicsQuality::Medium) {
        radius = 0.78F;
        bias = 0.05F;
        fadeStart = 26.0F;
        fadeEnd = 42.0F;
        sampleCount = 6;
    }
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginShaderMode(shader);
    // SetShaderValueTexture must happen inside shader mode; otherwise the
    // batch flush in BeginShaderMode silently drops the auxiliary textures.
    SetShaderValueTexture(
        shader, ssaoLocations_.sceneDepth, scene.depth);
    SetShaderValueTexture(
        shader, ssaoLocations_.sceneNormal,
        resources_.sceneNormalTexture());
    SetShaderValueMatrix(
        shader, ssaoLocations_.projection, ssaoProjection_);
    SetShaderValueMatrix(
        shader, ssaoLocations_.inverseProjection,
        ssaoInverseProjection_);
    SetShaderValueMatrix(
        shader, ssaoLocations_.viewMatrix, ssaoViewMatrix_);
    SetShaderValue(shader, ssaoLocations_.texelSize,
                   &texelSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, ssaoLocations_.radius,
                   &radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, ssaoLocations_.bias,
                   &bias, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, ssaoLocations_.fadeStart,
                   &fadeStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, ssaoLocations_.fadeEnd,
                   &fadeEnd, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, ssaoLocations_.sampleCount,
                   &sampleCount, SHADER_UNIFORM_INT);
    DrawTexturePro(
        scene.texture,
        {0.0F, 0.0F,
         static_cast<float>(scene.texture.width),
         -static_cast<float>(scene.texture.height)},
        {0.0F, 0.0F,
         static_cast<float>(target.texture.width),
         static_cast<float>(target.texture.height)},
        {}, 0.0F, WHITE);
    EndShaderMode();
    EndTextureMode();
    ssaoFrameReady_ = true;
}

bool Renderer::beginFirstPersonToolPass(
    const WorldLighting* lighting,
    const Camera3D* worldCamera) {
    resources_.updateViewModelTarget(settings_);
    if (!resources_.viewModelTargetValid()) {
        return false;
    }
    BeginTextureMode(resources_.viewModelTarget());
    ClearBackground(BLANK);
    if (resources_.viewModelShader().valid()) {
        Shader& shader = resources_.viewModelShader().get();
        const WorldLighting fallbackLighting{};
        const WorldLighting& source = lighting != nullptr
            ? *lighting
            : fallbackLighting;
        Vector3 viewSunDirection = source.sunDirection;
        if (worldCamera != nullptr) {
            const Vector3 forward = Vector3Normalize(Vector3Subtract(
                worldCamera->target, worldCamera->position));
            const Vector3 right = Vector3Normalize(
                Vector3CrossProduct(forward, worldCamera->up));
            const Vector3 up = Vector3Normalize(
                Vector3CrossProduct(right, forward));
            viewSunDirection = {
                Vector3DotProduct(source.sunDirection, right),
                Vector3DotProduct(source.sunDirection, up),
                -Vector3DotProduct(source.sunDirection, forward),
            };
        }
        viewSunDirection = Vector3Normalize(viewSunDirection);
        SetShaderValue(
            shader, viewModelSunDirectionLocation_,
            &viewSunDirection, SHADER_UNIFORM_VEC3);
        SetShaderValue(
            shader, viewModelSunColorLocation_,
            &source.sunColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(
            shader, viewModelSunIntensityLocation_,
            &source.sunIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(
            shader, viewModelSkyAmbientColorLocation_,
            &source.skyAmbientColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(
            shader, viewModelGroundAmbientColorLocation_,
            &source.groundAmbientColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(
            shader, viewModelAmbientIntensityLocation_,
            &source.ambientIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(
            shader, viewModelDayNightTintLocation_,
            &source.dayNightTint, SHADER_UNIFORM_VEC3);
        SetShaderValue(
            shader, viewModelExposureLocation_,
            &source.exposure, SHADER_UNIFORM_FLOAT);
    }
    return true;
}

void Renderer::endFirstPersonToolPass(
    const FirstPersonToolTuning& tuning,
    const WorldLighting*) {
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
        const float targetScale =
            static_cast<float>(target.texture.width) /
            static_cast<float>(std::max(GetRenderWidth(), 1));
        const float scaledOutlineWidth =
            tuning.outlineWidth * targetScale;
        SetShaderValue(shader, viewModelTexelSizeLocation_,
                       &texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, viewModelOutlineEnabledLocation_,
                       &outlineEnabled, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, viewModelOutlineWidthLocation_,
                       &scaledOutlineWidth, SHADER_UNIFORM_FLOAT);
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
        BeginShaderMode(
            resources_.postProcessShader().get());
        uploadPostProcessSettings();
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
    ssaoFrameReady_ = false;
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
    // Submit the final UI batch before waiting so the GPU can finish it while
    // the CPU approaches the presentation deadline.
    rlDrawRenderBatchActive();
    // Pace immediately before presenting. Waiting after EndDrawing() keeps
    // frame starts regular but still exposes render-time variance as visible
    // presentation jitter.
    paceFrame();
    const auto swapStart = PerformanceClock::now();
    EndDrawing();
    frameSwapMilliseconds_ = performanceMilliseconds(swapStart);
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
    const Vector3 worldUp{0.0F, 1.0F, 0.0F};
    const Vector3 fallbackUp{0.0F, 0.0F, 1.0F};
    const Vector3 up =
        std::abs(Vector3DotProduct(sunDirection, worldUp)) > 0.95F
            ? fallbackUp
            : worldUp;
    // Lock the orthographic projection to shadow-map texels. A sharper PCF
    // edge would otherwise crawl over static geometry as the player moves.
    const Vector3 lightRight = Vector3Normalize(
        Vector3CrossProduct(sunDirection, up));
    const Vector3 lightUp = Vector3Normalize(
        Vector3CrossProduct(lightRight, sunDirection));
    const float worldUnitsPerTexel =
        2.0F * shadowDistance / static_cast<float>(shadowMapSize);
    const float rightOffset =
        std::round(Vector3DotProduct(focus, lightRight) /
                   worldUnitsPerTexel) * worldUnitsPerTexel -
        Vector3DotProduct(focus, lightRight);
    const float upOffset =
        std::round(Vector3DotProduct(focus, lightUp) /
                   worldUnitsPerTexel) * worldUnitsPerTexel -
        Vector3DotProduct(focus, lightUp);
    const Vector3 snappedFocus = Vector3Add(
        focus,
        Vector3Add(Vector3Scale(lightRight, rightOffset),
                   Vector3Scale(lightUp, upOffset)));
    const Vector3 lightPosition = Vector3Subtract(
        snappedFocus,
        Vector3Scale(sunDirection, shadowDistance));
    const Camera3D lightCamera{
        .position = lightPosition,
        .target = snappedFocus,
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

void Renderer::setSelectionOutlineRadius(float radiusPixels) {
    selectionOutlineRadiusPixels_ = std::clamp(
        radiusPixels, 1.0F, 12.0F);
}

void Renderer::setSelectionOutlineDepthOcclusion(bool enabled) {
    selectionOutlineDepthOcclusion_ = enabled;
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
    selectionOutlineRadiusPixels_ = 6.0F;
    selectionOutlineDepthOcclusion_ = true;
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
        static_cast<int>(std::ceil(
            selectionOutlineRadiusPixels_ / outputScale)),
        1, 12);
    SetShaderValue(shader, selectionOutlineRadiusLocation_,
                   &outlineRadius, SHADER_UNIFORM_INT);
    const float depthOcclusionEnabled =
        selectionOutlineDepthOcclusion_ && usingOffscreenTarget_ &&
                resources_.sceneTargetValid()
            ? 1.0F : 0.0F;
    SetShaderValue(
        shader, selectionOutlineDepthOcclusionEnabledLocation_,
        &depthOcclusionEnabled, SHADER_UNIFORM_FLOAT);
    SetShaderValueMatrix(
        shader, selectionOutlineInverseProjectionLocation_,
        ssaoInverseProjection_);
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
    BeginBlendMode(BLEND_ALPHA);
    if (selectionOutlineBounds_) {
        const Rectangle bounds = *selectionOutlineBounds_;
        const float boundsScaleX = destination.width /
            static_cast<float>(std::max(target.texture.width, 1));
        const float boundsScaleY = destination.height /
            static_cast<float>(std::max(target.texture.height, 1));
        BeginScissorMode(
            static_cast<int>(std::floor(bounds.x * boundsScaleX)),
            static_cast<int>(std::floor(bounds.y * boundsScaleY)),
            std::max(1, static_cast<int>(
                std::ceil(bounds.width * boundsScaleX))),
            std::max(1, static_cast<int>(
                std::ceil(bounds.height * boundsScaleY))));
    }
    BeginShaderMode(shader);
    if (depthOcclusionEnabled > 0.5F) {
        SetShaderValueTexture(
            shader, selectionOutlineSceneDepthLocation_,
            resources_.sceneTarget().depth);
    }
    const auto brightness =
        static_cast<unsigned char>(
            std::lround(242.0F + pulse * 7.0F));
    const auto alpha =
        static_cast<unsigned char>(
            std::lround(225.0F + pulse * 12.0F));
    DrawTexturePro(
        target.texture, source, destination,
        {0.0F, 0.0F}, 0.0F,
        {static_cast<unsigned char>(
             static_cast<unsigned int>(selectionOutlineTint_.r) *
             brightness / 255U),
         static_cast<unsigned char>(
             static_cast<unsigned int>(selectionOutlineTint_.g) *
             static_cast<unsigned int>(brightness * 0.98F) / 255U),
         static_cast<unsigned char>(
             static_cast<unsigned int>(selectionOutlineTint_.b) *
             static_cast<unsigned int>(brightness * 0.92F) / 255U),
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

void Renderer::drawTacticalGroundSector(
    Vector3 center, float radius, float yawRadians,
    float arcDegrees, Color color, float innerRadius) {
    if (radius <= 0.0F || arcDegrees <= 0.0F ||
        !resources_.tacticalSectorShader().valid()) return;
    arcDegrees = std::clamp(arcDegrees, 2.0F, 360.0F);
    innerRadius = std::clamp(innerRadius, 0.0F, std::max(0.0F, radius - 0.05F));
    constexpr float EdgeWidth = 0.90F;
    const Vector4 shape{radius, innerRadius, arcDegrees * DEG2RAD * 0.5F, EdgeWidth};
    const Shader& shader = resources_.tacticalSectorShader().get();

    // Per-fragment distances keep the white rim smooth at every range and
    // angle. A vertex gradient stretched across a fan produces white bands.
    // This overlay affects color only, leaving world normals/AO and depth intact.
    endWorldShader();
    beginColorOnlyWorldEffects();
    BeginShaderMode(shader);
    SetShaderValue(shader, tacticalSectorShapeLocation_, &shape, SHADER_UNIFORM_VEC4);
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    const float sine = std::sin(yawRadians);
    const float cosine = std::cos(yawRadians);
    const auto emit = [&](float sideways, float forward) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(sideways, forward);
        rlVertex3f(center.x + cosine * sideways - sine * forward,
                   center.y, center.z - sine * sideways - cosine * forward);
    };
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    emit(-radius, -radius); emit(radius, -radius); emit(radius, radius);
    emit(-radius, -radius); emit(radius, radius); emit(-radius, radius);
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
    EndShaderMode();
    endColorOnlyWorldEffects();
}

void Renderer::beginColorOnlyWorldEffects() {
    if (!usingOffscreenTarget_ ||
        !resources_.sceneScreenSpaceBuffers()) return;
    rlDrawRenderBatchActive();
    rlActiveDrawBuffers(1);
}

void Renderer::endColorOnlyWorldEffects() {
    if (!usingOffscreenTarget_ ||
        !resources_.sceneScreenSpaceBuffers()) return;
    rlDrawRenderBatchActive();
    rlActiveDrawBuffers(2);
}

void Renderer::setWorldMaterial(const WorldMaterialState& material) {
    if (!worldShaderActive_) {
        return;
    }
    const auto sameVector3 = [](Vector3 left, Vector3 right) {
        return left.x == right.x && left.y == right.y &&
            left.z == right.z;
    };
    if (material.baseColor.x == worldMaterial_.baseColor.x &&
        material.baseColor.y == worldMaterial_.baseColor.y &&
        material.baseColor.z == worldMaterial_.baseColor.z &&
        material.baseColor.w == worldMaterial_.baseColor.w &&
        material.unlitAmount == worldMaterial_.unlitAmount &&
        material.bakedAo == worldMaterial_.bakedAo &&
        material.vertexAoAmount == worldMaterial_.vertexAoAmount &&
        material.screenAoAmount == worldMaterial_.screenAoAmount &&
        material.terrainAmount == worldMaterial_.terrainAmount &&
        material.terrainGrassTint.x == worldMaterial_.terrainGrassTint.x &&
        material.terrainGrassTint.y == worldMaterial_.terrainGrassTint.y &&
        material.terrainGrassTint.z == worldMaterial_.terrainGrassTint.z &&
        material.terrainDirtTint.x == worldMaterial_.terrainDirtTint.x &&
        material.terrainDirtTint.y == worldMaterial_.terrainDirtTint.y &&
        material.terrainDirtTint.z == worldMaterial_.terrainDirtTint.z &&
        material.groundTextureScale == worldMaterial_.groundTextureScale &&
        material.groundTextureStrength ==
            worldMaterial_.groundTextureStrength &&
        material.groundSaturation == worldMaterial_.groundSaturation &&
        material.groundBrightness == worldMaterial_.groundBrightness &&
        sameVector3(material.groundTextureTint,
                    worldMaterial_.groundTextureTint) &&
        material.groundTextureContrast ==
            worldMaterial_.groundTextureContrast &&
        material.groundHighlightCompression ==
            worldMaterial_.groundHighlightCompression &&
        material.groundSecondaryBlend ==
            worldMaterial_.groundSecondaryBlend &&
        material.groundMacroVariation ==
            worldMaterial_.groundMacroVariation &&
        material.windAmount == worldMaterial_.windAmount &&
        material.localWindHeight == worldMaterial_.localWindHeight &&
        material.distantFadeAmount == worldMaterial_.distantFadeAmount &&
        material.vegetationAmount == worldMaterial_.vegetationAmount &&
        material.treeAmount == worldMaterial_.treeAmount &&
        sameVector3(material.treeLeafShadowColor,
                    worldMaterial_.treeLeafShadowColor) &&
        sameVector3(material.treeLeafMidColor,
                    worldMaterial_.treeLeafMidColor) &&
        sameVector3(material.treeLeafLightColor,
                    worldMaterial_.treeLeafLightColor) &&
        sameVector3(material.treeTrunkShadowColor,
                    worldMaterial_.treeTrunkShadowColor) &&
        sameVector3(material.treeTrunkMidColor,
                    worldMaterial_.treeTrunkMidColor) &&
        sameVector3(material.treeTrunkLightColor,
                    worldMaterial_.treeTrunkLightColor) &&
        material.treeShadowThreshold ==
            worldMaterial_.treeShadowThreshold &&
        material.treeHighlightThreshold ==
            worldMaterial_.treeHighlightThreshold &&
        material.treeRampSmoothness == worldMaterial_.treeRampSmoothness &&
        material.treeHighlightStrength ==
            worldMaterial_.treeHighlightStrength &&
        material.treeTextureVariation == worldMaterial_.treeTextureVariation &&
        material.treeLeafFlatness == worldMaterial_.treeLeafFlatness &&
        material.bushAmount == worldMaterial_.bushAmount &&
        sameVector3(material.bushShadowColor,
                    worldMaterial_.bushShadowColor) &&
        sameVector3(material.bushMidColor,
                    worldMaterial_.bushMidColor) &&
        sameVector3(material.bushLightColor,
                    worldMaterial_.bushLightColor) &&
        material.bushShadowThreshold ==
            worldMaterial_.bushShadowThreshold &&
        material.bushHighlightThreshold ==
            worldMaterial_.bushHighlightThreshold &&
        material.bushRampSmoothness ==
            worldMaterial_.bushRampSmoothness &&
        material.bushHighlightStrength ==
            worldMaterial_.bushHighlightStrength &&
        material.coniferAmount == worldMaterial_.coniferAmount &&
        sameVector3(material.coniferShadowColor,
                    worldMaterial_.coniferShadowColor) &&
        sameVector3(material.coniferMidColor,
                    worldMaterial_.coniferMidColor) &&
        sameVector3(material.coniferLightColor,
                    worldMaterial_.coniferLightColor) &&
        material.coniferShadowThreshold ==
            worldMaterial_.coniferShadowThreshold &&
        material.coniferHighlightThreshold ==
            worldMaterial_.coniferHighlightThreshold &&
        material.coniferRampSmoothness ==
            worldMaterial_.coniferRampSmoothness &&
        material.coniferHighlightStrength ==
            worldMaterial_.coniferHighlightStrength &&
        material.coniferFacetBlend ==
            worldMaterial_.coniferFacetBlend &&
        material.hitFlashAmount == worldMaterial_.hitFlashAmount &&
        material.enemyWhiteFlashAmount ==
            worldMaterial_.enemyWhiteFlashAmount &&
        sameVector3(material.hitFlashPosition,
                    worldMaterial_.hitFlashPosition) &&
        material.hitFlashRadius == worldMaterial_.hitFlashRadius &&
        material.selectionAmount == worldMaterial_.selectionAmount &&
        material.selectionTint.x == worldMaterial_.selectionTint.x &&
        material.selectionTint.y == worldMaterial_.selectionTint.y &&
        material.selectionTint.z == worldMaterial_.selectionTint.z &&
        material.ghostAmount == worldMaterial_.ghostAmount &&
        material.ghostTint.x == worldMaterial_.ghostTint.x &&
        material.ghostTint.y == worldMaterial_.ghostTint.y &&
        material.ghostTint.z == worldMaterial_.ghostTint.z &&
        material.ghostOpacity == worldMaterial_.ghostOpacity &&
        material.outlineMaterialClassOverride ==
            worldMaterial_.outlineMaterialClassOverride &&
        material.inkOutlineEligible == worldMaterial_.inkOutlineEligible &&
        material.inkOccluder == worldMaterial_.inkOccluder) {
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
    ghost.screenAoAmount = 0.0F;
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
