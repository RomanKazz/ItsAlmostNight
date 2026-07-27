#include "graphics/Renderer.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

const char* enabledText(bool enabled) {
    return enabled ? "ON" : "OFF";
}

const char* qualityText(GraphicsQuality quality) {
    switch (quality) {
    case GraphicsQuality::Low:
        return "LOW";
    case GraphicsQuality::Medium:
        return "MEDIUM";
    case GraphicsQuality::High:
        return "HIGH";
    }
    return "";
}

} // namespace

void Renderer::initialize() {
    resources_.initialize(settings_);
    resolveWorldShaderLocations();
    resolveSkyShaderLocations();
}

void Renderer::shutdown() {
    resources_.shutdown();
}

void Renderer::processInput() {
    const bool shiftDown =
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
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
    if (IsKeyPressed(KEY_F6)) {
        settings_.particles = !settings_.particles;
    }
    if (shiftDown && IsKeyPressed(KEY_F7)) {
        cycleAoStrength();
    } else if (IsKeyPressed(KEY_F7)) {
        settings_.blobShadows = !settings_.blobShadows;
    }
    if (IsKeyPressed(KEY_F8)) {
        settings_.bloom = !settings_.bloom;
    }
    if (IsKeyPressed(KEY_F9)) {
        settings_.ssao = !settings_.ssao;
    }
    if (IsKeyPressed(KEY_F10)) {
        cycleQuality();
    }
    if (IsKeyPressed(KEY_F11)) {
        cycleRenderScale();
    }
    if (shiftDown && IsKeyPressed(KEY_F12)) {
        settings_.sky = !settings_.sky;
    } else if (IsKeyPressed(KEY_F12)) {
        settings_.worldShader = !settings_.worldShader;
    }
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
        DrawTexturePro(target.texture, source, destination, {0.0F, 0.0F}, 0.0F, WHITE);
    }
    worldPassOpen_ = false;
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
    if (showDebugPanel_) {
        drawDebugPanel();
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
    constexpr int ShadowTextureSlot = 10;
    rlActiveTextureSlot(ShadowTextureSlot);
    rlDisableTexture();
    rlActiveTextureSlot(0);
    worldShaderActive_ = false;
}

bool Renderer::drawCannon(Vector3 position, float yawRadians,
                          float pitchRadians, Color tint) {
    auto& resource = resources_.cannonModel();
    if (!resource.valid()) {
        return false;
    }

    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }

    constexpr float ModelScale = 3.0F;
    constexpr float GroundOffset = 0.155F;
    constexpr float ModelForwardOffset = PI;
    position.y += GroundOffset;

    if (model.meshCount < 2) {
        DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                    (yawRadians + ModelForwardOffset) * RAD2DEG,
                    {ModelScale, ModelScale, ModelScale}, tint);
        return true;
    }

    const Matrix scale = MatrixScale(ModelScale, ModelScale, ModelScale);
    const Matrix yaw =
        MatrixRotateY(yawRadians + ModelForwardOffset);
    const Matrix pitch = MatrixRotateX(pitchRadians);
    const Matrix translation =
        MatrixTranslate(position.x, position.y, position.z);
    const Matrix baseTransform =
        MatrixMultiply(MatrixMultiply(scale, yaw), translation);
    const Matrix barrelTransform = MatrixMultiply(
        MatrixMultiply(MatrixMultiply(scale, pitch), yaw), translation);

    const auto drawMesh = [&model, tint](int meshIndex,
                                         Matrix transform) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };

    // glTF order: barrel.002 (0), weapon-cannon/base (1).
    drawMesh(1, baseTransform);
    drawMesh(0, barrelTransform);
    for (int meshIndex = 2; meshIndex < model.meshCount; ++meshIndex) {
        drawMesh(meshIndex, baseTransform);
    }
    return true;
}

bool Renderer::drawCannonball(Vector3 position, Color tint) {
    auto& resource = resources_.cannonballModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {1.5F, 1.5F, 1.5F}, tint);
    return true;
}

bool Renderer::drawArrow(Vector3 position, Vector3 direction, Color tint) {
    auto& resource = resources_.arrowModel();
    if (!resource.valid() ||
        Vector3LengthSqr(direction) <= 0.000001F) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }

    const Vector3 forward = Vector3Normalize(direction);
    const Vector3 referenceUp =
        std::abs(Vector3DotProduct(forward, {0.0F, 1.0F, 0.0F})) > 0.98F
            ? Vector3{1.0F, 0.0F, 0.0F}
            : Vector3{0.0F, 1.0F, 0.0F};
    const Vector3 right =
        Vector3Normalize(Vector3CrossProduct(referenceUp, forward));
    const Vector3 up = Vector3CrossProduct(forward, right);
    Matrix rotation = MatrixIdentity();
    rotation.m0 = right.x;
    rotation.m1 = right.y;
    rotation.m2 = right.z;
    rotation.m4 = up.x;
    rotation.m5 = up.y;
    rotation.m6 = up.z;
    rotation.m8 = forward.x;
    rotation.m9 = forward.y;
    rotation.m10 = forward.z;

    constexpr float ArrowScale = 1.2F;
    const Matrix transform = MatrixMultiply(
        MatrixMultiply(
            MatrixScale(ArrowScale, ArrowScale, ArrowScale), rotation),
        MatrixTranslate(position.x, position.y, position.z));
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

bool Renderer::drawCrossbow(Vector3 position, float yawRadians, Color tint) {
    auto& resource = resources_.crossbowModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    constexpr float ModelScale = 2.5F;
    constexpr float GroundOffset = 0.13F;
    constexpr float ModelForwardOffset = PI;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                (yawRadians + ModelForwardOffset) * RAD2DEG,
                {ModelScale, ModelScale, ModelScale}, tint);
    return true;
}

bool Renderer::drawCore(Vector3 position, float yawRadians, Color tint) {
    auto& resource = resources_.coreModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    constexpr float ModelScale = 2.0F;
    constexpr float GroundOffset = 0.005F;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                yawRadians * RAD2DEG,
                {ModelScale, ModelScale, ModelScale}, tint);
    return true;
}

bool Renderer::drawRock(Vector3 position, Color tint) {
    auto& resource = resources_.rockModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    constexpr float ModelScale = 2.0F;
    constexpr float GroundOffset = 0.204F;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {ModelScale, ModelScale, ModelScale}, tint);
    return true;
}

bool Renderer::drawTree(Vector3 position, Color tint) {
    auto& resource = resources_.treeModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    if (shader != nullptr) {
        for (int index = 0; index < model.materialCount; ++index) {
            model.materials[index].shader = *shader;
        }
    }
    constexpr float ModelScale = 2.7F;
    constexpr float GroundOffset = 0.144F;
    position.y += GroundOffset;
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {ModelScale, ModelScale, ModelScale}, tint);
    return true;
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

    int segmentCount = 12;
    float qualityOpacity = 1.0F;
    if (settings_.quality == GraphicsQuality::Low) {
        segmentCount = 8;
        qualityOpacity = 0.78F;
    } else if (settings_.quality == GraphicsQuality::Medium) {
        segmentCount = 10;
        qualityOpacity = 0.9F;
    }
    const float finalOpacity =
        std::clamp(opacity * fade * qualityOpacity, 0.0F, 1.0F);
    const auto centerAlpha =
        static_cast<unsigned char>(finalOpacity * 255.0F);
    constexpr float Tau = 6.28318530718F;
    constexpr unsigned char ShadowRed = 8;
    constexpr unsigned char ShadowGreen = 11;
    constexpr unsigned char ShadowBlue = 14;

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

const GraphicsSettings& Renderer::settings() const {
    return settings_;
}

void Renderer::resolveWorldShaderLocations() {
    if (!resources_.worldShader().valid()) {
        return;
    }

    auto& shader = resources_.worldShader().get();
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
    };
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
        .timeSeconds = GetShaderLocation(shader, "timeSeconds"),
        .exposure = GetShaderLocation(shader, "exposure"),
        .saturation = GetShaderLocation(shader, "saturation"),
    };
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
    SetShaderValue(shader, worldShaderLocations_.dayNightTint,
                   &lighting.dayNightTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.exposure,
                   &lighting.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.saturation,
                   &lighting.saturation, SHADER_UNIFORM_FLOAT);
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
    SetShaderValue(shader, worldShaderLocations_.hitFlashAmount,
                   &material.hitFlashAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.selectionAmount,
                   &material.selectionAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, worldShaderLocations_.selectionTint,
                   &material.selectionTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, worldShaderLocations_.aoStrength,
                   &settings_.aoStrength, SHADER_UNIFORM_FLOAT);
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

void Renderer::drawDebugPanel() const {
    constexpr int PanelX = 18;
    constexpr int PanelY = 18;
    constexpr int PanelWidth = 370;
    constexpr int PanelHeight = 358;
    constexpr int TextX = PanelX + 14;

    DrawRectangle(PanelX, PanelY, PanelWidth, PanelHeight, {10, 13, 20, 226});
    DrawRectangleLines(PanelX, PanelY, PanelWidth, PanelHeight, {110, 145, 180, 255});
    DrawText("GRAPHICS [F2]", TextX, PanelY + 12, 20, {245, 184, 76, 255});
    DrawText(TextFormat("Framebuffer: %d x %d", GetRenderWidth(), GetRenderHeight()),
             TextX, PanelY + 42, 17, RAYWHITE);
    DrawText(TextFormat("Scene target: %d x %d  %s",
                        resources_.sceneWidth(), resources_.sceneHeight(),
                        resources_.sceneTargetValid() ? "READY" : "FALLBACK"),
             TextX, PanelY + 64, 17, RAYWHITE);
    DrawText(TextFormat("F3 Shadows: %s   F4 Fog: %s",
                        enabledText(settings_.shadows), enabledText(settings_.fog)),
             TextX, PanelY + 90, 17, LIGHTGRAY);
    DrawText(TextFormat("F5 Pipeline: %s   F6 Particles: %s",
                        enabledText(settings_.postProcessing),
                        enabledText(settings_.particles)),
             TextX, PanelY + 112, 17, LIGHTGRAY);
    DrawText(TextFormat("F7 Blob: %s   F8 Bloom: %s",
                        enabledText(settings_.blobShadows), enabledText(settings_.bloom)),
             TextX, PanelY + 134, 17, LIGHTGRAY);
    DrawText(TextFormat("F9 SSAO: %s   Shift+F7 AO: %.2f",
                        enabledText(settings_.ssao), settings_.aoStrength),
             TextX, PanelY + 156, 17, LIGHTGRAY);
    DrawText(TextFormat("F10 Quality: %s   Shadow map: %d",
                        qualityText(settings_.quality), settings_.shadowMapSize),
             TextX, PanelY + 182, 17, LIGHTGRAY);
    DrawText(TextFormat("F11 Render scale: %.2f", settings_.renderScale),
             TextX, PanelY + 204, 17, LIGHTGRAY);
    DrawText(TextFormat("Shadow distance: %.0f", settings_.shadowDistance),
             TextX, PanelY + 226, 17, LIGHTGRAY);
    DrawText(TextFormat("F12 World shader: %s  %s",
                        enabledText(settings_.worldShader),
                        resources_.worldShader().valid() ? "READY" : "FALLBACK"),
             TextX, PanelY + 248, 17, LIGHTGRAY);
    DrawText(TextFormat("F1 Shadow map view   Shadow: %s",
                        resources_.shadowMap().valid() ? "READY" : "FALLBACK"),
             TextX, PanelY + 270, 17, LIGHTGRAY);
    DrawText(TextFormat("Bias: %.5f + slope %.5f",
                        settings_.constantBias, settings_.slopeBias),
             TextX, PanelY + 292, 17, LIGHTGRAY);
    DrawText(TextFormat("Shadow strength: %.2f", settings_.shadowStrength),
             TextX, PanelY + 314, 17, LIGHTGRAY);
    DrawText(TextFormat("Shift+F12 Sky: %s  %s",
                        enabledText(settings_.sky),
                        resources_.skyShader().valid() ? "READY" : "FALLBACK"),
             TextX, PanelY + 336, 17, LIGHTGRAY);
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
    DrawText("SHADOW MAP [F1]", static_cast<int>(destination.x),
             static_cast<int>(destination.y) - 24, 18, YELLOW);
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

void Renderer::cycleRenderScale() {
    if (settings_.renderScale > 0.9F) {
        settings_.renderScale = 0.75F;
    } else if (settings_.renderScale > 0.6F) {
        settings_.renderScale = 0.5F;
    } else {
        settings_.renderScale = 1.0F;
    }
}

} // namespace ian
