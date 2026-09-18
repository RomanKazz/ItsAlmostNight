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

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif

namespace ian {
namespace {

PerformanceMetric& gpuMetricForStage(
    RendererPerformanceStats& stats, RendererGpuStage stage) {
    switch (stage) {
    case RendererGpuStage::World:
        return stats.gpuWorld;
    case RendererGpuStage::PostProcess:
        return stats.gpuPostProcess;
    case RendererGpuStage::ViewModelTarget:
        return stats.gpuViewModelTarget;
    case RendererGpuStage::ViewModelComposite:
        return stats.gpuViewModelComposite;
    case RendererGpuStage::Ui:
        return stats.gpuUi;
    case RendererGpuStage::Count:
        break;
    }
    return stats.gpuUi;
}

const std::array<std::array<Vector2, 25>, 3>& blobShadowUnitCircles() {
    static const auto circles = [] {
        std::array<std::array<Vector2, 25>, 3> result{};
        constexpr std::array<int, 3> SegmentCounts{12, 18, 24};
        constexpr float Tau = 6.28318530718F;
        for (std::size_t ringIndex = 0;
             ringIndex < SegmentCounts.size(); ++ringIndex) {
            const int segmentCount = SegmentCounts[ringIndex];
            for (int point = 0; point <= segmentCount; ++point) {
                const float angle =
                    Tau * static_cast<float>(point) /
                    static_cast<float>(segmentCount);
                result[ringIndex][static_cast<std::size_t>(point)] = {
                    std::cos(angle), std::sin(angle)};
            }
        }
        return result;
    }();
    return circles;
}

} // namespace

void Renderer::initialize() {
    shutdown();
    enemyCrowdLodModel_ = LoadModelFromMesh(
        GenMeshSphere(0.5F, 6, 4));
    Mesh rampSupportBeamMesh =
        GenMeshCube(0.16F, 4.0F, 0.16F);
    for (int vertex = 0;
         vertex < rampSupportBeamMesh.vertexCount;
         ++vertex) {
        const int coordinate = vertex * 3;
        float& y = rampSupportBeamMesh
                       .vertices[coordinate + 1];
        const float z = rampSupportBeamMesh
                            .vertices[coordinate + 2];
        if (y > 0.0F) {
            // Match the 1:1 ramp slope and leave a narrow seam below
            // the deck instead of letting a flat post pierce it.
            y = 3.82F + z;
            if (rampSupportBeamMesh
                    .normals[coordinate + 1] > 0.5F) {
                rampSupportBeamMesh.normals[coordinate] =
                    0.0F;
                rampSupportBeamMesh
                    .normals[coordinate + 1] =
                    0.70710678F;
                rampSupportBeamMesh
                    .normals[coordinate + 2] =
                    -0.70710678F;
            }
        } else {
            y += 2.0F;
        }
        // The ramp atlas is a palette strip. Sampling its middle wood
        // swatch makes the posts use exactly the same material as the deck.
        rampSupportBeamMesh.texcoords[vertex * 2] =
            0.59375F;
        rampSupportBeamMesh.texcoords[vertex * 2 + 1] =
            0.875F;
    }
    UpdateMeshBuffer(
        rampSupportBeamMesh,
        RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION,
        rampSupportBeamMesh.vertices,
        rampSupportBeamMesh.vertexCount * 3 *
            static_cast<int>(sizeof(float)),
        0);
    UpdateMeshBuffer(
        rampSupportBeamMesh,
        RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD,
        rampSupportBeamMesh.texcoords,
        rampSupportBeamMesh.vertexCount * 2 *
            static_cast<int>(sizeof(float)),
        0);
    UpdateMeshBuffer(
        rampSupportBeamMesh,
        RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL,
        rampSupportBeamMesh.normals,
        rampSupportBeamMesh.vertexCount * 3 *
            static_cast<int>(sizeof(float)),
        0);
    rampSupportBeamModel_ = LoadModelFromMesh(
        rampSupportBeamMesh);
    resources_.initialize(settings_);
    initializePlatformTopPartMeshes();
    resolveWorldShaderLocations();
    resolveSkyShaderLocations();
    resolvePostProcessLocations();
    resolveSsaoLocations();
    resolveAuxiliaryShaderLocations();
    initializeGpuTimers();
}

void Renderer::beginFrame() {
    pollGpuTimers();
    instanceBuffers_.beginFrame();
}

void Renderer::shutdown() {
    shutdownGpuTimers();
    frameOpen_ = false;
    worldPassOpen_ = false;
    worldShaderActive_ = false;
    shadowPassOpen_ = false;
    selectionMaskPassOpen_ = false;
    selectionMaskReady_ = false;
    blobShadowBatchOpen_ = false;
    usingOffscreenTarget_ = false;
    ssaoFrameReady_ = false;
    shadowFrameValid_ = false;
    shadowCacheInitialized_ = false;
    shadowLastUpdateTime_ = -1.0;
    shadowLastMapSize_ = -1;
    for (auto& modelMappings : enemyBoneMappings_) {
        for (auto& mapping : modelMappings) {
            mapping.clear();
        }
    }
    enemyBatches_.clear();
    activeEnemyBatches_.clear();
    enemyBatchFrame_ = 0U;
    enemyBonePoseCache_.clear();
    performanceStats_ = {};
    instancedEnemyMillisecondsThisFrame_ = 0.0;
    grassClearAreaCells_.clear();
    indexedGrassClearAreaData_ = nullptr;
    indexedGrassClearAreaCount_ = 0U;
    grassClearAreaContentHash_ = 0U;
    grassClearAreaDimension_ = 0;
    grassInstanceCacheValid_ = false;
    decorativeInstanceCacheValid_ = false;
    for (auto& candidates : grassInstanceCandidates_) {
        candidates.clear();
    }
    for (auto& transforms : grassInstanceTransforms_) {
        transforms.clear();
    }
    for (auto& candidates : decorativeRockCandidates_) {
        candidates.clear();
    }
    for (auto& candidates : decorativeBushCandidates_) {
        candidates.clear();
    }
    for (auto& transforms : boundaryForestTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : boundaryForestRevealTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : boundaryForestVisibleTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : resourceTreeTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : resourceRockTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : decorativeRockTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : decorativeBushTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : pondDecorTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : visiblePondDecorTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : pondShoreRockTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : visiblePondShoreRockTransforms_) {
        transforms.clear();
    }
    boundaryForestCached_ = false;
    terrainHeightfield_ = nullptr;
    decorationExclusionMap_ = {};
    selectionOutlineBounds_.reset();
    ghostPreviewRestoreMaterial_.reset();
    worldMaterial_ = {};
    if (IsModelValid(enemyCrowdLodModel_)) {
        UnloadModel(enemyCrowdLodModel_);
    }
    enemyCrowdLodModel_ = {};
    if (IsModelValid(rampSupportBeamModel_)) {
        UnloadModel(rampSupportBeamModel_);
    }
    rampSupportBeamModel_ = {};
    shutdownPlatformTopPartMeshes();
    instanceBuffers_.shutdown();
    terrainRenderer_.shutdown();
    resources_.shutdown();
}

void Renderer::initializeGpuTimers() {
    shutdownGpuTimers();
#if defined(__APPLE__)
    constexpr std::size_t StageCount =
        static_cast<std::size_t>(RendererGpuStage::Count);
    std::array<GLuint, StageCount * GpuQueryBufferCount> ids{};
    glGenQueries(static_cast<GLsizei>(ids.size()), ids.data());
    bool valid = true;
    for (std::size_t stage = 0; stage < StageCount; ++stage) {
        for (std::size_t slot = 0; slot < GpuQueryBufferCount; ++slot) {
            const GLuint id = ids[stage * GpuQueryBufferCount + slot];
            gpuQueries_[stage][slot].id = id;
            gpuQueries_[stage][slot].pending = false;
            valid = valid && id != 0U;
        }
    }
    if (!valid) {
        glDeleteQueries(static_cast<GLsizei>(ids.size()), ids.data());
        gpuQueries_ = {};
        return;
    }
    gpuTimersInitialized_ = true;
    performanceStats_.gpuTimersSupported = true;
#endif
}

void Renderer::shutdownGpuTimers() {
#if defined(__APPLE__)
    if (gpuTimersInitialized_) {
        constexpr std::size_t StageCount =
            static_cast<std::size_t>(RendererGpuStage::Count);
        std::array<GLuint, StageCount * GpuQueryBufferCount> ids{};
        for (std::size_t stage = 0; stage < StageCount; ++stage) {
            for (std::size_t slot = 0; slot < GpuQueryBufferCount; ++slot) {
                ids[stage * GpuQueryBufferCount + slot] =
                    gpuQueries_[stage][slot].id;
            }
        }
        glDeleteQueries(static_cast<GLsizei>(ids.size()), ids.data());
    }
#endif
    gpuQueries_ = {};
    gpuQueryWriteIndices_ = {};
    activeGpuStage_.reset();
    activeGpuQueryIndex_ = 0U;
    gpuProfilingEnabled_ = false;
    gpuTimersInitialized_ = false;
    performanceStats_.gpuTimersSupported = false;
}

void Renderer::setGpuProfilingEnabled(bool enabled) {
    gpuProfilingEnabled_ = enabled && gpuTimersInitialized_;
}

void Renderer::beginGpuTimer(RendererGpuStage stage) {
#if defined(__APPLE__)
    if (!gpuProfilingEnabled_ || activeGpuStage_ ||
        stage == RendererGpuStage::Count) {
        return;
    }
    const std::size_t stageIndex = static_cast<std::size_t>(stage);
    std::size_t slotIndex = gpuQueryWriteIndices_[stageIndex];
    bool found = false;
    for (std::size_t attempt = 0; attempt < GpuQueryBufferCount;
         ++attempt) {
        if (!gpuQueries_[stageIndex][slotIndex].pending) {
            found = true;
            break;
        }
        slotIndex = (slotIndex + 1U) % GpuQueryBufferCount;
    }
    if (!found) {
        return;
    }
    rlDrawRenderBatchActive();
    glBeginQuery(
        GL_TIME_ELAPSED, gpuQueries_[stageIndex][slotIndex].id);
    activeGpuStage_ = stage;
    activeGpuQueryIndex_ = slotIndex;
#else
    (void)stage;
#endif
}

void Renderer::endGpuTimer(RendererGpuStage stage) {
#if defined(__APPLE__)
    if (!activeGpuStage_ || *activeGpuStage_ != stage) {
        return;
    }
    rlDrawRenderBatchActive();
    glEndQuery(GL_TIME_ELAPSED);
    const std::size_t stageIndex = static_cast<std::size_t>(stage);
    gpuQueries_[stageIndex][activeGpuQueryIndex_].pending = true;
    gpuQueryWriteIndices_[stageIndex] =
        (activeGpuQueryIndex_ + 1U) % GpuQueryBufferCount;
    activeGpuStage_.reset();
#else
    (void)stage;
#endif
}

void Renderer::pollGpuTimers() {
#if defined(__APPLE__)
    if (!gpuTimersInitialized_) {
        return;
    }
    constexpr std::size_t StageCount =
        static_cast<std::size_t>(RendererGpuStage::Count);
    bool viewModelTimerUpdated = false;
    for (std::size_t stage = 0; stage < StageCount; ++stage) {
        for (GpuQuerySlot& slot : gpuQueries_[stage]) {
            if (!slot.pending) {
                continue;
            }
            GLint available = GL_FALSE;
            glGetQueryObjectiv(
                slot.id, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available == GL_FALSE) {
                continue;
            }
            GLuint64 nanoseconds = 0U;
            glGetQueryObjectui64v(
                slot.id, GL_QUERY_RESULT, &nanoseconds);
            gpuMetricForStage(
                performanceStats_,
                static_cast<RendererGpuStage>(stage))
                .sample(static_cast<double>(nanoseconds) / 1'000'000.0);
            viewModelTimerUpdated = viewModelTimerUpdated ||
                stage == static_cast<std::size_t>(
                    RendererGpuStage::ViewModelTarget) ||
                stage == static_cast<std::size_t>(
                    RendererGpuStage::ViewModelComposite);
            slot.pending = false;
        }
    }
    if (viewModelTimerUpdated) {
        performanceStats_.gpuViewModel.sample(
            performanceStats_.gpuViewModelTarget.lastMilliseconds +
            performanceStats_.gpuViewModelComposite.lastMilliseconds);
    }
#endif
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
    // Plain F5 belongs to the in-game item grant menu.
    if (shiftDown && IsKeyPressed(KEY_F5)) {
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
    if (!controlDown && !shiftDown && IsKeyPressed(KEY_F10)) {
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

void Renderer::drawRepairShockwave(
    Vector3 position, Vector3 cameraPosition,
    float progress, float radius) {
    progress = std::clamp(progress, 0.0F, 1.0F);
    radius = std::max(radius, 0.1F);
    const float eased = 1.0F - std::pow(1.0F - progress, 3.0F);
    const float sphereRadius = radius * (0.08F + 0.92F * eased);
    position.y += 0.7F;
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    if (resources_.shockwaveShader().valid()) {
        Shader& shader = resources_.shockwaveShader().get();
        const float timeSeconds = static_cast<float>(GetTime());
        SetShaderValue(shader, shockwaveCameraLocation_, &cameraPosition,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, shockwaveProgressLocation_, &progress,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, shockwaveTimeLocation_, &timeSeconds,
                       SHADER_UNIFORM_FLOAT);
        BeginShaderMode(shader);
        DrawSphereEx(position, sphereRadius, 28, 28, WHITE);
        EndShaderMode();
    } else {
        const float fade = 1.0F - progress;
        DrawSphereWires(
            position, sphereRadius, 20, 20,
            {158, 232, 255,
             static_cast<unsigned char>(fade * 190.0F)});
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
                              float radiusZ, float opacity,
                              int segmentCountOverride) {
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
    if (segmentCountOverride == 12 || segmentCountOverride == 18 ||
        segmentCountOverride == 24) {
        segmentCount = segmentCountOverride;
    }
    const float finalOpacity = std::clamp(
        opacity * 1.18F * fade * qualityOpacity, 0.0F, 1.0F);
    const auto centerAlpha =
        static_cast<unsigned char>(finalOpacity * 255.0F);
    ++performanceStats_.blobShadowCount;
    performanceStats_.blobShadowTriangles +=
        static_cast<std::size_t>(segmentCount);
    constexpr unsigned char ShadowRed = 24;
    constexpr unsigned char ShadowGreen = 36;
    constexpr unsigned char ShadowBlue = 34;
    const std::size_t ringIndex = segmentCount == 12
        ? 0U
        : segmentCount == 18 ? 1U : 2U;
    const auto& unitCircle = blobShadowUnitCircles()[ringIndex];

    Vector3 surfaceNormal{0.0F, 1.0F, 0.0F};
    if (terrainHeightfield_ != nullptr) {
        const float terrainY = static_cast<float>(
            terrainHeightfield_->getHeight(
                groundPosition.x, groundPosition.z));
        // Ground props follow terrain. Shadows belonging to elevated floors
        // or supports stay horizontal instead of tilting toward terrain far
        // below them.
        if (std::abs(groundPosition.y - terrainY) <= 0.75F) {
            surfaceNormal = terrainSurfaceNormal(
                groundPosition.x, groundPosition.z);
        }
    }
    Vector3 tangentX = Vector3Normalize({
        surfaceNormal.y, -surfaceNormal.x, 0.0F});
    if (Vector3LengthSqr(tangentX) <= 0.0001F) {
        tangentX = {1.0F, 0.0F, 0.0F};
    }
    Vector3 tangentZ = Vector3Normalize(
        Vector3CrossProduct(tangentX, surfaceNormal));
    const Vector3 center = Vector3Add(
        groundPosition, Vector3Scale(surfaceNormal, 0.008F));

    const auto surfacePoint = [&](Vector2 point) {
        return Vector3Add(
            center,
            Vector3Add(
                Vector3Scale(tangentX, point.x * radiusX),
                Vector3Scale(tangentZ, point.y * radiusZ)));
    };

    for (int segment = 0; segment < segmentCount; ++segment) {
        const Vector2 point0 =
            unitCircle[static_cast<std::size_t>(segment)];
        const Vector2 point1 =
            unitCircle[static_cast<std::size_t>(segment + 1)];
        const Vector3 vertex0 = surfacePoint(point0);
        const Vector3 vertex1 = surfacePoint(point1);
        rlColor4ub(ShadowRed, ShadowGreen, ShadowBlue, centerAlpha);
        rlVertex3f(center.x, center.y, center.z);
        rlColor4ub(ShadowRed, ShadowGreen, ShadowBlue, 0);
        // Counter-clockwise along the sampled surface normal.
        rlVertex3f(vertex1.x, vertex1.y, vertex1.z);
        rlVertex3f(vertex0.x, vertex0.y, vertex0.z);
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

} // namespace ian
