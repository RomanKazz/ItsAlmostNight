#include "graphics/Renderer.hpp"

#include "buildings/BuildingSystem.hpp"
#include "ui/UiText.hpp"

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

void Renderer::initialize() {
    resources_.initialize(settings_);
    resolveWorldShaderLocations();
    resolveSkyShaderLocations();
    resolvePostProcessLocations();
    if (resources_.selectionOutlineShader().valid()) {
        selectionOutlineTexelSizeLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(), "texelSize");
        selectionOutlineRadiusLocation_ = GetShaderLocation(
            resources_.selectionOutlineShader().get(),
            "outlineRadius");
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
        selectionMaskSkinningEnabledLocation_ =
            GetShaderLocation(shader, "skinningEnabled");
    }
    if (resources_.shadowShader().valid()) {
        Shader& shader = resources_.shadowShader().get();
        configureSkinningLocations(shader);
        shadowSkinningEnabledLocation_ =
            GetShaderLocation(shader, "skinningEnabled");
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
    if (resources_.upgradeEffectShader().valid()) {
        Shader& shader = resources_.upgradeEffectShader().get();
        shader.locs[SHADER_LOC_MATRIX_MODEL] =
            GetShaderLocation(shader, "matModel");
        shader.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(shader, "matNormal");
        upgradeEffectOriginLocation_ =
            GetShaderLocation(shader, "effectOrigin");
        upgradeEffectHeightLocation_ =
            GetShaderLocation(shader, "effectHeight");
        upgradeEffectProgressLocation_ =
            GetShaderLocation(shader, "progress");
        upgradeEffectTimeLocation_ =
            GetShaderLocation(shader, "timeSeconds");
    }
}

void Renderer::shutdown() {
    terrainRenderer_.shutdown();
    resources_.shutdown();
}

void Renderer::processInput() {
    const bool shiftDown =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool controlDown =
        IsKeyDown(KEY_LEFT_CONTROL) ||
        IsKeyDown(KEY_RIGHT_CONTROL);
    if (IsKeyPressed(KEY_F2)) {
        showDebugPanel_ = !showDebugPanel_;
    }
    if (IsKeyPressed(KEY_F1)) {
        showShadowMap_ = !showShadowMap_;
    }
    if (IsKeyPressed(KEY_F3)) {
        settings_.shadows = !settings_.shadows;
    }
    if (IsKeyPressed(KEY_F4)) {
        settings_.fog = !settings_.fog;
    }
    if (IsKeyPressed(KEY_F5)) {
        settings_.postProcessing = !settings_.postProcessing;
    }
    if (!controlDown && IsKeyPressed(KEY_F6)) {
        settings_.particles = !settings_.particles;
    }
    if (controlDown && IsKeyPressed(KEY_F7)) {
        // Reserved for terrain regeneration.
    } else if (shiftDown && IsKeyPressed(KEY_F7)) {
        cycleAoStrength();
    } else if (IsKeyPressed(KEY_F7)) {
        settings_.blobShadows = !settings_.blobShadows;
    }
    if (!controlDown && IsKeyPressed(KEY_F8)) {
        settings_.bloom = !settings_.bloom;
    }
    if (!controlDown && IsKeyPressed(KEY_F9)) {
        settings_.ssao = !settings_.ssao;
    }
    if (!controlDown && IsKeyPressed(KEY_F10)) {
        cycleQuality();
    }
    if (!controlDown && IsKeyPressed(KEY_F11)) {
        adjustPixelSize(shiftDown ? -1 : 1);
    }
    if (shiftDown && IsKeyPressed(KEY_F12)) {
        settings_.sky = !settings_.sky;
    } else if (IsKeyPressed(KEY_F12)) {
        settings_.worldShader = !settings_.worldShader;
    }
}

bool Renderer::graphicsPanelVisible() const {
    return showDebugPanel_;
}

void Renderer::setGraphicsPanelVisible(bool visible) {
    showDebugPanel_ = visible;
}

bool Renderer::shadowMapVisible() const {
    return showShadowMap_;
}

void Renderer::setShadowMapVisible(bool visible) {
    showShadowMap_ = visible;
}

void Renderer::beginWorldPass(Color clearColor) {
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
    DrawRectangle(0, 0, std::max(width, 1), std::max(height, 1), WHITE);
    EndShaderMode();
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
    EndDrawing();
    frameOpen_ = false;
}

bool Renderer::beginShadowPass(const WorldLighting& lighting, Vector3 focus) {
    resources_.updateShadowMap(settings_);
    shadowFrameValid_ = false;
    if (!settings_.shadows || !settings_.worldShader ||
        !resources_.worldShader().valid() ||
        !resources_.shadowMap().valid() ||
        !resources_.shadowShader().valid()) {
        return false;
    }

    const float shadowDistance = std::max(settings_.shadowDistance, 10.0F);
    const Vector3 sunDirection = Vector3Normalize(lighting.sunDirection);
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
    BeginShaderMode(resources_.selectionMaskShader().get());
    const float timeSeconds = static_cast<float>(GetTime());
    constexpr float NoWind = 0.0F;
    SetShaderValue(resources_.selectionMaskShader().get(),
                   selectionMaskTimeLocation_, &timeSeconds,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(resources_.selectionMaskShader().get(),
                   selectionMaskWindLocation_, &NoWind,
                   SHADER_UNIFORM_FLOAT);
    selectionMaskPassOpen_ = true;
    return true;
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

void Renderer::endSelectionMaskPass() {
    if (!selectionMaskPassOpen_) {
        return;
    }
    EndShaderMode();
    EndMode3D();
    EndTextureMode();
    selectionMaskPassOpen_ = false;
    selectionMaskReady_ = true;
}

void Renderer::clearSelectionOutline() {
    selectionMaskReady_ = false;
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
        static_cast<int>(std::ceil(6.0F / outputScale)), 1, 6);
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
    BeginShaderMode(shader);
    const auto brightness =
        static_cast<unsigned char>(
            std::lround(244.0F + pulse * 11.0F));
    const auto alpha =
        static_cast<unsigned char>(
            std::lround(238.0F + pulse * 17.0F));
    DrawTexturePro(target.texture, source, destination,
                   {0.0F, 0.0F}, 0.0F,
                   {brightness, brightness, brightness, alpha});
    EndShaderMode();
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
        material.terrainPrimaryTint.x == worldMaterial_.terrainPrimaryTint.x &&
        material.terrainPrimaryTint.y == worldMaterial_.terrainPrimaryTint.y &&
        material.terrainPrimaryTint.z == worldMaterial_.terrainPrimaryTint.z &&
        material.terrainSecondaryTint.x ==
            worldMaterial_.terrainSecondaryTint.x &&
        material.terrainSecondaryTint.y ==
            worldMaterial_.terrainSecondaryTint.y &&
        material.terrainSecondaryTint.z ==
            worldMaterial_.terrainSecondaryTint.z &&
        material.terrainPatchTint.x == worldMaterial_.terrainPatchTint.x &&
        material.terrainPatchTint.y == worldMaterial_.terrainPatchTint.y &&
        material.terrainPatchTint.z == worldMaterial_.terrainPatchTint.z &&
        material.windAmount == worldMaterial_.windAmount &&
        material.hitFlashAmount == worldMaterial_.hitFlashAmount &&
        material.selectionAmount == worldMaterial_.selectionAmount &&
        material.selectionTint.x == worldMaterial_.selectionTint.x &&
        material.selectionTint.y == worldMaterial_.selectionTint.y &&
        material.selectionTint.z == worldMaterial_.selectionTint.z) {
        return;
    }

    rlDrawRenderBatchActive();
    worldMaterial_ = material;
    uploadWorldMaterial(worldMaterial_);
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
}

void Renderer::rebuildTerrain(
    const TerrainHeightfield& terrain) {
    terrainHeightfield_ = &terrain;
    terrainRenderer_.rebuild(terrain);
}

void Renderer::drawTerrain(
    Color tint, bool wireframe) {
    Shader shader{};
    if (shadowPassOpen_ &&
        resources_.shadowShader().valid()) {
        shader = resources_.shadowShader().get();
    } else if (
        worldShaderActive_ &&
        resources_.worldShader().valid()) {
        shader = resources_.worldShader().get();
    }
    terrainRenderer_.draw(shader, tint);
    if (wireframe) {
        terrainRenderer_.drawWireframe(
            {245, 224, 154, 150});
    }
}

void Renderer::drawGrassInstances(Vector3 cameraPosition,
                                  float worldLimit,
                                  float nightAmount,
                                  const WorldLighting& lighting,
                                  std::span<const GrassClearArea>
                                      clearAreas) {
    if (!settings_.grass) {
        return;
    }

    constexpr std::size_t VariantCount = 3;
    constexpr std::size_t MaximumInstancesPerVariant = 512;
    constexpr float Spacing = 1.8F;
    constexpr float DrawRadius = 27.0F;
    std::array<std::array<Matrix, MaximumInstancesPerVariant>,
               VariantCount>
        transforms{};
    std::array<std::size_t, VariantCount> counts{};

    const int minimumX = static_cast<int>(
        std::floor((cameraPosition.x - DrawRadius) / Spacing));
    const int maximumX = static_cast<int>(
        std::ceil((cameraPosition.x + DrawRadius) / Spacing));
    const int minimumZ = static_cast<int>(
        std::floor((cameraPosition.z - DrawRadius) / Spacing));
    const int maximumZ = static_cast<int>(
        std::ceil((cameraPosition.z + DrawRadius) / Spacing));

    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U;
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        return value ^ (value >> 16U);
    };
    const auto unitFloat = [](std::uint32_t value) {
        return static_cast<float>(value & 0xffffU) /
               65535.0F;
    };

    for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
        for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
            const std::uint32_t hash = hashCell(cellX, cellZ);
            const float jitterX =
                (unitFloat(hash) - 0.5F) * Spacing * 0.72F;
            const float jitterZ =
                (unitFloat(hash >> 8U) - 0.5F) *
                Spacing * 0.72F;
            const float x =
                (static_cast<float>(cellX) + 0.5F) * Spacing +
                jitterX;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * Spacing +
                jitterZ;
            if (std::abs(x) > worldLimit - 0.5F ||
                std::abs(z) > worldLimit - 0.5F) {
                continue;
            }
            const float offsetX = x - cameraPosition.x;
            const float offsetZ = z - cameraPosition.z;
            const float distanceSquared =
                offsetX * offsetX + offsetZ * offsetZ;
            if (distanceSquared > DrawRadius * DrawRadius) {
                continue;
            }

            const std::size_t variant =
                static_cast<std::size_t>(hash % VariantCount);
            if (counts[variant] >= MaximumInstancesPerVariant) {
                continue;
            }
            const float baseScale =
                0.58F + unitFloat(hash >> 16U) * 0.34F;
            float visibility = 1.0F;
            for (const GrassClearArea& area : clearAreas) {
                const float deltaX = x - area.center.x;
                const float deltaZ = z - area.center.y;
                const float distance =
                    std::sqrt(
                        deltaX * deltaX + deltaZ * deltaZ);
                constexpr float Feather = 0.85F;
                const float proximity =
                    1.0F -
                    std::clamp(
                        (distance - area.innerRadius) /
                            Feather,
                        0.0F, 1.0F);
                const float clearing =
                    std::clamp(area.amount, 0.0F, 1.0F) *
                    proximity * proximity *
                    (3.0F - 2.0F * proximity);
                visibility *= 1.0F - clearing;
            }
            const float clearing = 1.0F - visibility;
            if (clearing >= 0.68F) {
                continue;
            }
            const float scale = baseScale * visibility;
            const float sink = clearing * 0.4F;
            const float terrainHeight =
                terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->getHeight(
                              x, z))
                    : 0.0F;
            const float rotation =
                unitFloat(hash ^ 0xa511e9b3U) * PI * 2.0F;
            transforms[variant][counts[variant]++] =
                MatrixMultiply(
                    MatrixScale(scale, scale, scale),
                    MatrixMultiply(
                        MatrixRotateY(rotation),
                        MatrixTranslate(
                            x,
                            terrainHeight + 0.02F - sink,
                            z)));
        }
    }

    std::array<ModelResource*, VariantCount> variants{
        &resources_.grassModelB(),
        &resources_.grassModelC(),
        &resources_.grassModelD(),
    };
    const float darkness =
        1.0F - std::clamp(nightAmount, 0.0F, 1.0F) * 0.12F;
    const Color tint{
        static_cast<unsigned char>(255.0F * darkness),
        static_cast<unsigned char>(255.0F * darkness),
        static_cast<unsigned char>(
            255.0F * std::min(darkness * 1.08F, 1.0F)),
        255,
    };
    const Vector4 shaderTint = ColorNormalize(tint);
    if (resources_.grassShader().valid()) {
        Shader& shader = resources_.grassShader().get();
        const float timeSeconds = static_cast<float>(GetTime());
        const float fogStart =
            settings_.fog ? lighting.fogStart : 1000000.0F;
        const float fogEnd =
            settings_.fog ? lighting.fogEnd : 1000001.0F;
        SetShaderValue(shader, grassTintLocation_, &shaderTint,
                       SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, grassTimeLocation_, &timeSeconds,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassCameraPositionLocation_,
                       &cameraPosition, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassSunDirectionLocation_,
                       &lighting.sunDirection, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassSunColorLocation_,
                       &lighting.sunColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassSunIntensityLocation_,
                       &lighting.sunIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassSkyAmbientColorLocation_,
                       &lighting.skyAmbientColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassGroundAmbientColorLocation_,
                       &lighting.groundAmbientColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassAmbientIntensityLocation_,
                       &lighting.ambientIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassFogColorLocation_,
                       &lighting.fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassFogStartLocation_,
                       &fogStart, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassFogEndLocation_,
                       &fogEnd, SHADER_UNIFORM_FLOAT);
        const float fogBandsEnabled =
            settings_.fogBands ? 1.0F : 0.0F;
        SetShaderValue(shader, grassFogBandsEnabledLocation_,
                       &fogBandsEnabled, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassFogBandCountLocation_,
                       &settings_.fogBandCount,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassDayNightTintLocation_,
                       &lighting.dayNightTint, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassExposureLocation_,
                       &lighting.exposure, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassSaturationLocation_,
                       &lighting.saturation, SHADER_UNIFORM_FLOAT);
    }

    for (std::size_t variant = 0; variant < VariantCount;
         ++variant) {
        ModelResource& resource = *variants[variant];
        if (!resource.valid() || counts[variant] == 0U) {
            continue;
        }
        Model& model = resource.get();
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex =
                model.meshMaterial[meshIndex];
            Material material = model.materials[materialIndex];
            if (resources_.grassShader().valid()) {
                material.shader = resources_.grassShader().get();
                material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            } else {
                material.maps[MATERIAL_MAP_DIFFUSE].color = tint;
            }
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                transforms[variant].data(),
                static_cast<int>(counts[variant]));
        }
    }
}

void Renderer::drawUpgradeEffect(Vector3 position, float progress,
                                 float scale) {
    progress = std::clamp(progress, 0.0F, 1.0F);
    scale = std::max(scale, 0.1F);
    const float radius = 0.82F * scale;
    const float height = 3.2F * scale;
    const Vector3 top{
        position.x,
        position.y + height,
        position.z,
    };
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    if (resources_.upgradeEffectShader().valid()) {
        Shader& shader = resources_.upgradeEffectShader().get();
        const float timeSeconds =
            static_cast<float>(GetTime());
        SetShaderValue(
            shader, upgradeEffectOriginLocation_, &position,
            SHADER_UNIFORM_VEC3);
        SetShaderValue(
            shader, upgradeEffectHeightLocation_, &height,
            SHADER_UNIFORM_FLOAT);
        SetShaderValue(
            shader, upgradeEffectProgressLocation_, &progress,
            SHADER_UNIFORM_FLOAT);
        SetShaderValue(
            shader, upgradeEffectTimeLocation_, &timeSeconds,
            SHADER_UNIFORM_FLOAT);
        BeginShaderMode(shader);
        DrawCylinderEx(position, top, radius, radius, 48,
                       WHITE);
        EndShaderMode();
    } else {
        const float cylinderEnvelope =
            std::sin(progress * PI);
        const auto alpha = static_cast<unsigned char>(
            std::lround(
                std::max(cylinderEnvelope, 0.0F) * 70.0F));
        DrawCylinderEx(
            position, top, radius, radius, 48,
            {255, 172, 48, alpha});
    }

    const float appear =
        std::clamp(progress / 0.12F, 0.0F, 1.0F);
    const float disappear =
        1.0F -
        std::clamp((progress - 0.62F) / 0.38F, 0.0F, 1.0F);
    const float envelope =
        appear * appear * (3.0F - 2.0F * appear) *
        disappear * disappear *
        (3.0F - 2.0F * disappear);
    constexpr int RingCount = 7;
    for (int index = 0; index < RingCount; ++index) {
        const float phase = std::fmod(
            progress * 1.65F +
                static_cast<float>(index) /
                    static_cast<float>(RingCount),
            1.0F);
        const float ringFade =
            std::sin(phase * PI) * envelope;
        const auto alpha = static_cast<unsigned char>(
            std::lround(ringFade * 185.0F));
        constexpr int RingLayers = 7;
        for (int layer = 0; layer < RingLayers; ++layer) {
            const float centeredLayer =
                static_cast<float>(layer) -
                static_cast<float>(RingLayers - 1) * 0.5F;
            const float layerFade =
                1.0F -
                std::abs(centeredLayer) /
                    (static_cast<float>(RingLayers) * 0.5F);
            const auto layerAlpha =
                static_cast<unsigned char>(
                    std::lround(
                        static_cast<float>(alpha) *
                        layerFade));
            DrawCircle3D(
                {position.x,
                 position.y + phase * height +
                     centeredLayer * 0.012F * scale,
                 position.z},
                radius *
                    (1.015F + ringFade * 0.045F +
                     centeredLayer * 0.012F),
                {1.0F, 0.0F, 0.0F}, 90.0F,
                {255, 205, 82, layerAlpha});
        }
    }

    constexpr int StarCount = 14;
    for (int index = 0; index < StarCount; ++index) {
        const float seed =
            static_cast<float>(index) /
            static_cast<float>(StarCount);
        const float angle =
            seed * 2.0F * PI + progress * 2.4F;
        const float phase = std::fmod(
            seed * 3.731F + progress * 1.28F, 1.0F);
        const float pulse =
            (0.55F +
             0.45F *
                 std::sin(progress * 31.0F +
                          static_cast<float>(index) * 2.17F)) *
            envelope * std::sin(phase * PI);
        const auto alpha = static_cast<unsigned char>(
            std::lround(std::max(pulse, 0.0F) * 235.0F));
        const Vector3 center{
            position.x + std::cos(angle) * radius * 1.04F,
            position.y + phase * height,
            position.z + std::sin(angle) * radius * 1.04F,
        };
        const float starSize =
            scale * (0.045F + pulse * 0.075F);
        const Vector3 tangent{
            -std::sin(angle), 0.0F, std::cos(angle)};
        DrawLine3D(
            {center.x - tangent.x * starSize, center.y,
             center.z - tangent.z * starSize},
            {center.x + tangent.x * starSize, center.y,
             center.z + tangent.z * starSize},
            {255, 242, 190, alpha});
        DrawLine3D(
            {center.x, center.y - starSize, center.z},
            {center.x, center.y + starSize, center.z},
            {255, 242, 190, alpha});
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
}

bool Renderer::beginBlobShadowBatch(Vector3 cameraPosition) {
    if (!settings_.blobShadows || blobShadowBatchOpen_) {
        return false;
    }

    blobShadowCamera_ = cameraPosition;
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    blobShadowBatchOpen_ = true;
    return true;
}

void Renderer::drawBlobShadow(Vector3 groundPosition, float radiusX,
                              float radiusZ, float opacity) {
    if (!blobShadowBatchOpen_ || radiusX <= 0.0F || radiusZ <= 0.0F ||
        opacity <= 0.0F) {
        return;
    }

    const float offsetX = groundPosition.x - blobShadowCamera_.x;
    const float offsetZ = groundPosition.z - blobShadowCamera_.z;
    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    const float maximumDistance = settings_.shadowDistance + 24.0F;
    const float fadeStart = maximumDistance * 0.58F;
    const float fade =
        1.0F - std::clamp((distance - fadeStart) /
                              std::max(maximumDistance - fadeStart, 0.001F),
                          0.0F, 1.0F);
    if (fade <= 0.0F) {
        return;
    }

    int segmentCount = 24;
    float qualityOpacity = 1.0F;
    if (settings_.quality == GraphicsQuality::Low) {
        segmentCount = 12;
        qualityOpacity = 0.78F;
    } else if (settings_.quality == GraphicsQuality::Medium) {
        segmentCount = 18;
        qualityOpacity = 0.9F;
    }
    const float finalOpacity =
        std::clamp(opacity * fade * qualityOpacity, 0.0F, 1.0F);
    const auto centerAlpha =
        static_cast<unsigned char>(finalOpacity * 255.0F);
    constexpr float Tau = 6.28318530718F;
    constexpr unsigned char ShadowRed = 14;
    constexpr unsigned char ShadowGreen = 18;
    constexpr unsigned char ShadowBlue = 24;

    for (int segment = 0; segment < segmentCount; ++segment) {
        const float angle0 =
            Tau * static_cast<float>(segment) / static_cast<float>(segmentCount);
        const float angle1 =
            Tau * static_cast<float>(segment + 1) /
            static_cast<float>(segmentCount);
        rlColor4ub(ShadowRed, ShadowGreen, ShadowBlue, centerAlpha);
        rlVertex3f(groundPosition.x, groundPosition.y, groundPosition.z);
        rlColor4ub(ShadowRed, ShadowGreen, ShadowBlue, 0);
        rlVertex3f(groundPosition.x + std::cos(angle0) * radiusX,
                   groundPosition.y,
                   groundPosition.z + std::sin(angle0) * radiusZ);
        rlVertex3f(groundPosition.x + std::cos(angle1) * radiusX,
                   groundPosition.y,
                   groundPosition.z + std::sin(angle1) * radiusZ);
    }
}

void Renderer::endBlobShadowBatch() {
    if (!blobShadowBatchOpen_) {
        return;
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
    blobShadowBatchOpen_ = false;
}

GraphicsSettings& Renderer::settings() {
    return settings_;
}

const GraphicsSettings& Renderer::settings() const {
    return settings_;
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
        .bakedAo = GetShaderLocation(shader, "bakedAo"),
        .vertexAoAmount = GetShaderLocation(shader, "vertexAoAmount"),
        .aoStrength = GetShaderLocation(shader, "aoStrength"),
        .terrainAmount = GetShaderLocation(shader, "terrainAmount"),
        .terrainPrimaryTint =
            GetShaderLocation(shader, "terrainPrimaryTint"),
        .terrainSecondaryTint =
            GetShaderLocation(shader, "terrainSecondaryTint"),
        .terrainPatchTint =
            GetShaderLocation(shader, "terrainPatchTint"),
        .terrainTexture =
            GetShaderLocation(shader, "terrainTexture"),
        .terrainTextureEnabled =
            GetShaderLocation(shader, "terrainTextureEnabled"),
        .timeSeconds = GetShaderLocation(shader, "timeSeconds"),
        .windAmount = GetShaderLocation(shader, "windAmount"),
        .hitFlashAmount = GetShaderLocation(shader, "hitFlashAmount"),
        .selectionAmount = GetShaderLocation(shader, "selectionAmount"),
        .selectionTint = GetShaderLocation(shader, "selectionTint"),
        .shadowMap = GetShaderLocation(shader, "shadowMap"),
        .lightViewProjection = GetShaderLocation(shader, "lightViewProjection"),
        .shadowsEnabled = GetShaderLocation(shader, "shadowsEnabled"),
        .constantBias = GetShaderLocation(shader, "constantBias"),
        .slopeBias = GetShaderLocation(shader, "slopeBias"),
        .shadowStrength = GetShaderLocation(shader, "shadowStrength"),
        .shadowMapTexelSize =
            GetShaderLocation(shader, "shadowMapTexelSize"),
        .instancingEnabled = worldInstancingEnabledLocation_,
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
        .paletteEnabled =
            GetShaderLocation(shader, "paletteEnabled"),
        .paletteLevels =
            GetShaderLocation(shader, "paletteLevels"),
        .ditherEnabled =
            GetShaderLocation(shader, "ditherEnabled"),
        .ditherStrength =
            GetShaderLocation(shader, "ditherStrength"),
        .posterizedLightingEnabled = GetShaderLocation(
            shader, "posterizedLightingEnabled"),
        .lightingSteps =
            GetShaderLocation(shader, "lightingSteps"),
        .bloomEnabled =
            GetShaderLocation(shader, "bloomEnabled"),
        .bloomStrength =
            GetShaderLocation(shader, "bloomStrength"),
        .inkOutlinesEnabled = GetShaderLocation(
            shader, "inkOutlinesEnabled"),
        .outlineStrength =
            GetShaderLocation(shader, "outlineStrength"),
        .paperGrainEnabled =
            GetShaderLocation(shader, "paperGrainEnabled"),
        .paperGrainStrength = GetShaderLocation(
            shader, "paperGrainStrength"),
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
    upload(postProcessLocations_.paletteEnabled,
           settings_.paletteQuantization ? 1.0F : 0.0F);
    upload(postProcessLocations_.paletteLevels,
           settings_.paletteLevels);
    upload(postProcessLocations_.ditherEnabled,
           settings_.dithering ? 1.0F : 0.0F);
    upload(postProcessLocations_.ditherStrength,
           settings_.ditherStrength);
    upload(postProcessLocations_.posterizedLightingEnabled,
           settings_.posterizedLighting ? 1.0F : 0.0F);
    upload(postProcessLocations_.lightingSteps,
           settings_.lightingSteps);
    upload(postProcessLocations_.bloomEnabled,
           settings_.bloom ? 1.0F : 0.0F);
    upload(postProcessLocations_.bloomStrength,
           settings_.bloomStrength);
    upload(postProcessLocations_.inkOutlinesEnabled,
           settings_.inkOutlines ? 1.0F : 0.0F);
    upload(postProcessLocations_.outlineStrength,
           settings_.outlineStrength);
    upload(postProcessLocations_.paperGrainEnabled,
           settings_.paperGrain ? 1.0F : 0.0F);
    upload(postProcessLocations_.paperGrainStrength,
           settings_.paperGrainStrength);
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
    SetShaderValue(shader, worldShaderLocations_.terrainAmount,
                   &material.terrainAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.terrainPrimaryTint,
                   &material.terrainPrimaryTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.terrainSecondaryTint,
                   &material.terrainSecondaryTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.terrainPatchTint,
                   &material.terrainPatchTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.windAmount,
                   &material.windAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.hitFlashAmount,
                   &material.hitFlashAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.selectionAmount,
                   &material.selectionAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.selectionTint,
                   &material.selectionTint, SHADER_UNIFORM_VEC3);
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

void Renderer::drawShadowMapDebug() const {
    if (!resources_.shadowMap().valid()) {
        return;
    }

    constexpr float PreviewSize = 280.0F;
    constexpr float Margin = 18.0F;
    const Rectangle source{
        0.0F,
        0.0F,
        static_cast<float>(resources_.shadowMap().size()),
        -static_cast<float>(resources_.shadowMap().size()),
    };
    const Rectangle destination{
        static_cast<float>(GetScreenWidth()) - PreviewSize - Margin,
        52.0F,
        PreviewSize,
        PreviewSize,
    };
    if (resources_.shadowDebugShader().valid()) {
        BeginShaderMode(resources_.shadowDebugShader().get());
    }
    DrawTexturePro(resources_.shadowMap().depthTexture(), source, destination,
                   {0.0F, 0.0F}, 0.0F, WHITE);
    if (resources_.shadowDebugShader().valid()) {
        EndShaderMode();
    }
    DrawRectangleLines(static_cast<int>(destination.x),
                       static_cast<int>(destination.y),
                       static_cast<int>(destination.width),
                       static_cast<int>(destination.height), YELLOW);
    drawUiText("SHADOW MAP [F1]",
               {destination.x, destination.y - 24.0F}, 18.0F,
               YELLOW);
}

void Renderer::cycleQuality() {
    switch (settings_.quality) {
    case GraphicsQuality::Low:
        settings_.quality = GraphicsQuality::Medium;
        settings_.shadowMapSize = 1024;
        settings_.shadowDistance = 60.0F;
        break;
    case GraphicsQuality::Medium:
        settings_.quality = GraphicsQuality::High;
        settings_.shadowMapSize = 2048;
        settings_.shadowDistance = 80.0F;
        break;
    case GraphicsQuality::High:
        settings_.quality = GraphicsQuality::Low;
        settings_.shadowMapSize = 512;
        settings_.shadowDistance = 40.0F;
        break;
    }
}

void Renderer::cycleAoStrength() {
    if (settings_.aoStrength < 0.1F) {
        settings_.aoStrength = 0.2F;
    } else if (settings_.aoStrength < 0.25F) {
        settings_.aoStrength = 0.3F;
    } else if (settings_.aoStrength < 0.34F) {
        settings_.aoStrength = 0.35F;
    } else {
        settings_.aoStrength = 0.0F;
    }
}

void Renderer::adjustPixelSize(int direction) {
    constexpr int PixelSizes[]{1, 2, 3, 4, 6, 8};
    const auto current =
        std::lower_bound(std::begin(PixelSizes), std::end(PixelSizes),
                         settings_.pixelSize);
    const auto currentIndex =
        current == std::end(PixelSizes)
            ? static_cast<int>(std::size(PixelSizes)) - 1
            : static_cast<int>(current - std::begin(PixelSizes));
    const int nextIndex =
        std::clamp(currentIndex + direction, 0,
                   static_cast<int>(std::size(PixelSizes)) - 1);
    settings_.pixelSize = PixelSizes[nextIndex];
}

} // namespace ian
