#include "graphics/Renderer.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/FireWandEffect.hpp"
#include "graphics/WandHeadEffect.hpp"
#include "graphics/BombDrawing.hpp"
#include "graphics/IceWandEffect.hpp"
#include "graphics/ViewModelCompositeBounds.hpp"


#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

namespace ian {
namespace {

Matrix transformMatrix(const Transform& transform) {
    return MatrixMultiply(
        MatrixMultiply(
            MatrixScale(transform.scale.x, transform.scale.y,
                        transform.scale.z),
            QuaternionToMatrix(transform.rotation)),
        MatrixTranslate(transform.translation.x,
                        transform.translation.y,
                        transform.translation.z));
}

constexpr float AuthoredChildScale = 5.307597F;
constexpr float LegacySwingScale = .74F;
constexpr Vector3 LegacySwingRotation{22.0F, -60.0F, 7.0F};

Matrix authoredSwingOuterTransform(
    FirstPersonToolVisual visual, const FirstPersonToolTuning& tuning,
    float movementPhase, float movementAmount) {
    const Vector3 legacyPosition = visual == FirstPersonToolVisual::Pickaxe
        ? Vector3{.55401313F, -.349843F, -.7427701F}
        : Vector3{.39473686F, -.349843F, -.7427701F};
    const float bob = std::clamp(movementAmount, 0.0F, 1.0F) *
        std::max(tuning.movementBob, 0.0F);
    const Vector3 offset{
        tuning.position.x - legacyPosition.x + std::sin(movementPhase) * .012F * bob,
        tuning.position.y - legacyPosition.y - std::abs(std::cos(movementPhase)) * .014F * bob,
        tuning.position.z - legacyPosition.z,
    };
    const float scale = tuning.scale / LegacySwingScale;
    Matrix outer = MatrixScale(scale, scale, scale);
    outer = MatrixMultiply(outer, MatrixRotateZ(
        (tuning.rotation.z - LegacySwingRotation.z) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixRotateY(
        (tuning.rotation.y - LegacySwingRotation.y) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixRotateX(
        (tuning.rotation.x - LegacySwingRotation.x) * DEG2RAD));
    return MatrixMultiply(outer, MatrixTranslate(offset.x, offset.y, offset.z));
}

} // namespace

bool Renderer::hasAlternateHarvestSwingAnimation() const {
    return resources_.pickaxeAlternateAnimation().valid();
}

bool Renderer::drawFirstPersonTool(
    FirstPersonToolVisual visual, float swingProgress,
    float movementPhase, float movementAmount,
    const FirstPersonToolTuning& tuning,
    float iceChargeProgress, float iceRecoilProgress,
    bool alternateHarvestSwing, int toolLevel) {
    ModelResource* resource = nullptr;
    const bool iceWand = visual == FirstPersonToolVisual::IceWand;
    const bool fireWand = visual == FirstPersonToolVisual::FireWand;
    const bool wand = iceWand || fireWand;
    const bool bomb = visual == FirstPersonToolVisual::Bomb;
    const bool pickaxe = visual == FirstPersonToolVisual::Pickaxe;
    const bool authoredHarvestSwing = usesAuthoredToolSwing(visual);
    switch (visual) {
    case FirstPersonToolVisual::None:
        break;
    case FirstPersonToolVisual::Axe:
        resource = &resources_.axeModel();
        break;
    case FirstPersonToolVisual::Pickaxe:
        resource = &resources_.pickaxeModel();
        break;
    case FirstPersonToolVisual::Club:
        resource = &resources_.clubModel();
        break;
    case FirstPersonToolVisual::IceWand:
        resource = &resources_.iceWandModel();
        break;
    case FirstPersonToolVisual::FireWand:
        resource = &resources_.fireWandModel();
        break;
    case FirstPersonToolVisual::Bomb:
        break;
    }
    const bool leveledTool = visual == FirstPersonToolVisual::Axe || pickaxe;
    toolLevel = std::clamp(toolLevel, 1, 8);
    if (leveledTool && toolLevel > 1) {
        ModelResource& levels = pickaxe
            ? resources_.pickaxeLevelModel() : resources_.axeLevelModel();
        if (levels.valid() && levels.get().meshCount >= toolLevel) {
            resource = &levels;
        } else {
            toolLevel = 1;
        }
    }
    if (!bomb && (resource == nullptr || !resource->valid())) {
        return false;
    }

    const float progress = std::clamp(swingProgress, 0.0F, 1.0F);
    const auto smoothStep = [](float value) {
        value = std::clamp(value, 0.0F, 1.0F);
        return value * value * (3.0F - 2.0F * value);
    };
    float swingPitch = 0.0F;
    float swingPush = 0.0F;
    if (!authoredHarvestSwing && !wand && progress > 0.0F && progress < 0.22F) {
        const float phase = smoothStep(progress / 0.22F);
        swingPitch = tuning.windupDegrees * phase;
    } else if (!authoredHarvestSwing && !wand && progress >= 0.22F &&
               progress < 0.53F) {
        const float phase = smoothStep((progress - 0.22F) / 0.31F);
        swingPitch = tuning.windupDegrees +
                     (tuning.strikeDegrees - tuning.windupDegrees) * phase;
        swingPush = std::sin(phase * PI) * tuning.depthPush;
    } else if (!authoredHarvestSwing && !wand && progress >= 0.53F) {
        const float phase = smoothStep((progress - 0.53F) / 0.47F);
        swingPitch = tuning.strikeDegrees * (1.0F - phase);
    }

    const float bob = std::clamp(movementAmount, 0.0F, 1.0F) *
                      std::max(tuning.movementBob, 0.0F);
    const float bobX = std::sin(movementPhase) * 0.012F * bob;
    const float bobY =
        std::abs(std::cos(movementPhase)) * 0.014F * bob;
    const auto cast = wand_presentation::pose(fireWand, iceChargeProgress, iceRecoilProgress);
    rlPushMatrix();
    if (authoredHarvestSwing) {
        const Matrix outer = authoredSwingOuterTransform(
            visual, tuning, movementPhase, movementAmount);
        rlMultMatrixf(MatrixToFloatV(outer).v);
    } else {
        rlTranslatef(
            tuning.position.x + bobX + (wand ? -.12F + cast.offset.x : 0.0F),
            tuning.position.y - bobY + (wand ? cast.offset.y : 0.0F),
            tuning.position.z + (wand ? cast.offset.z : swingPush));
        if (wand) {
            rlRotatef(cast.rotation.x, 1, 0, 0);
            rlRotatef(cast.rotation.y, 0, 1, 0);
            rlRotatef(cast.rotation.z, 0, 0, 1);
        }
        rlRotatef(swingPitch, 1, 0, 0);
        rlRotatef(tuning.rotation.x, 1, 0, 0);
        rlRotatef(tuning.rotation.y, 0, 1, 0);
        rlRotatef(tuning.rotation.z, 0, 0, 1);
        const float scale = bomb ? tuning.scale / 1.5F : tuning.scale;
        rlScalef(scale, scale, scale);
    }
    Shader* viewModelShader = resources_.viewModelShader().valid()
        ? &resources_.viewModelShader().get()
        : nullptr;
    if (bomb) {
        if (viewModelShader != nullptr) {
            BeginShaderMode(*viewModelShader);
        }
        const float time = static_cast<float>(GetTime());
        const Vector3 fuse = bomb_drawing::drawBody(time);
        const Matrix inverseTool = MatrixInvert(rlGetMatrixTransform());
        const Vector3 localCamera = Vector3Transform({}, inverseTool);
        const Vector3 localUp = Vector3Normalize(Vector3Subtract(
            Vector3Transform({0, 1, 0}, inverseTool), localCamera));
        bomb_drawing::drawFuse(fuse, localCamera, localUp, time, 0,
                              settings_.particles);
        if (viewModelShader != nullptr) {
            EndShaderMode();
        }
    } else {
        Model& model = resource->get();
        if (viewModelShader != nullptr) {
            for (int index = 0; index < model.materialCount; ++index) {
                model.materials[index].shader = *viewModelShader;
            }
        }
        const Matrix originalTransform = model.transform;
        const GlbNodeAnimation& animation =
            alternateHarvestSwing &&
                    resources_.pickaxeAlternateAnimation().valid()
                ? resources_.pickaxeAlternateAnimation()
                : resources_.pickaxeAnimation();
        if (authoredHarvestSwing && animation.valid()) {
            const Matrix animated = transformMatrix(
                animation.sample(progress));
            if (pickaxe) {
                const Matrix base = transformMatrix(
                    resources_.pickaxeAnimation().baseTransform());
                model.transform = MatrixMultiply(
                    MatrixInvert(base), animated);
            } else {
                // The pickaxe mesh has this Blender child-object scale baked
                // into its vertices. Apply it to the standalone axe mesh so
                // both tools share the authored pivot and camera space.
                model.transform = MatrixMultiply(
                    MatrixScale(AuthoredChildScale,
                                AuthoredChildScale,
                                AuthoredChildScale),
                    animated);
            }
        }
        // Animated rigid tools use their current authored transform, not a
        // static right-hand region: the crop follows the complete swing.
        // Wand particles extend beyond the mesh and retain the full pass.
        if (!wand) {
            const Matrix modelViewProjection = MatrixMultiply(
                MatrixMultiply(model.transform, rlGetMatrixTransform()),
                MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()));
            const float padding = 4.0F + std::max(tuning.outlineWidth, 0.0F) *
                static_cast<float>(GetScreenWidth()) /
                static_cast<float>(std::max(GetRenderWidth(), 1));
            BoundingBox bounds = leveledTool && toolLevel > 1
                ? resource->meshBounds()[0] : resource->visualBounds();
            if (leveledTool && toolLevel > 1) {
                const BoundingBox overlay = resource->meshBounds()[
                    static_cast<std::size_t>(toolLevel - 1)];
                bounds.min = {std::min(bounds.min.x, overlay.min.x),
                              std::min(bounds.min.y, overlay.min.y),
                              std::min(bounds.min.z, overlay.min.z)};
                bounds.max = {std::max(bounds.max.x, overlay.max.x),
                              std::max(bounds.max.y, overlay.max.y),
                              std::max(bounds.max.z, overlay.max.z)};
            }
            firstPersonCompositeBounds_ = viewModelCompositeBounds(
                bounds, modelViewProjection,
                GetScreenWidth(), GetScreenHeight(), padding);
        }
        if (leveledTool && toolLevel > 1) {
            // Only the selected cumulative overlay. All parts share the
            // authored rigid swing, including the alternate harvesting clip.
            for (const int mesh : {0, toolLevel - 1}) {
                DrawMesh(model.meshes[mesh],
                         model.materials[model.meshMaterial[mesh]],
                         model.transform);
            }
        } else {
            DrawModel(model, {}, 1.0F, WHITE);
        }
        model.transform = originalTransform;
    }
    if (wand) {
        const Vector3 tip{0, .35F, 0};
        const Matrix inverseTool = MatrixInvert(rlGetMatrixTransform());
        const Vector3 localCamera = Vector3Transform({}, inverseTool);
        const Vector3 right = fire_wand_effect::normalized(Vector3Subtract(
            Vector3Transform({1, 0, 0}, inverseTool), localCamera), {1, 0, 0});
        const Vector3 up = fire_wand_effect::normalized(Vector3Subtract(
            Vector3Transform({0, 1, 0}, inverseTool), localCamera), {0, 1, 0});
        const Vector3 front = fire_wand_effect::normalized(
            Vector3Subtract(localCamera, tip), {0, 0, 1});
        wand_presentation::drawHead(fireWand, static_cast<float>(GetTime()),
            iceChargeProgress, iceRecoilProgress, settings_.particles, right, up, front);
    }
    rlPopMatrix();
    return true;
}

void Renderer::drawFirstPersonSwingTrail(
    FirstPersonToolVisual visual, float swingProgress,
    float movementPhase, float movementAmount,
    const FirstPersonToolTuning& tuning,
    const Camera3D& viewModelCamera,
    bool alternateHarvestSwing) {
    const bool pickaxe = visual == FirstPersonToolVisual::Pickaxe;
    if (!usesAuthoredToolSwing(visual)) return;
    const bool useAlternateAnimation =
        alternateHarvestSwing &&
        resources_.pickaxeAlternateAnimation().valid();
    const GlbNodeAnimation& animation = useAlternateAnimation
        ? resources_.pickaxeAlternateAnimation()
        : resources_.pickaxeAnimation();
    if (!animation.valid()) return;

    const float progress = std::clamp(swingProgress, 0.0F, 1.0F);
    // The primary clip contacts on frame 20; the alternate on frame 27 and
    // completes its readable left-to-right sweep sooner after contact.
    const float trailStart = useAlternateAnimation ? 0.42F : 0.14F;
    const float trailFadeInEnd = useAlternateAnimation ? 0.49F : 0.23F;
    const float trailFadeOutStart = useAlternateAnimation ? 0.61F : 0.39F;
    const float trailEnd = useAlternateAnimation ? 0.70F : 0.60F;
    if (progress < trailStart || progress > trailEnd) return;
    const auto smoothStep = [](float value) {
        value = std::clamp(value, 0.0F, 1.0F);
        return value * value * (3.0F - 2.0F * value);
    };
    const float visibility =
        smoothStep((progress - trailStart) /
                   (trailFadeInEnd - trailStart)) *
        (1.0F - smoothStep((progress - trailFadeOutStart) /
                           (trailEnd - trailFadeOutStart)));
    if (visibility <= 0.001F) return;
    // Start with a faint ribbon and build its opacity through the stroke;
    // retain the final fade so the trail disappears without a hard cut.
    const float strokeOpacity = 0.45F + 0.55F * smoothStep(
        (progress - trailStart) / (trailFadeOutStart - trailStart));

    // Match the outer transform used by drawFirstPersonTool. The sampled
    // pivot moves the blade; its recent positions form the translucent arc.
    const Matrix outer = authoredSwingOuterTransform(
        visual, tuning, movementPhase, movementAmount);
    const Vector3 bladeEdge = pickaxe
        ? Vector3{-.36F, .52F, 0}
        : Vector3{-.21F, .35F, 0};
    const Vector3 bladeInsideFullWidth = pickaxe
        ? Vector3{-.13F, .37F, 0}
        : Vector3{-.02F, .24F, 0};
    const Vector3 bladeInside = Vector3Lerp(
        bladeEdge, bladeInsideFullWidth, 0.5F);
    const auto colorFor = [visibility, strokeOpacity, useAlternateAnimation](float age) {
        const float fade = std::pow(
            std::clamp(1.0F - age, 0.0F, 1.0F), 1.25F);
        const float shade = 1.0F - std::clamp(age, 0.0F, 1.0F);
        const float headAlpha = (useAlternateAnimation ? 138.0F : 92.0F) * 0.8F;
        return Color{
            static_cast<unsigned char>(std::lround(246.0F - 80.0F * shade)),
            static_cast<unsigned char>(std::lround(237.0F - 78.0F * shade)),
            static_cast<unsigned char>(std::lround(221.0F - 72.0F * shade)),
            static_cast<unsigned char>(std::lround(
                std::clamp(visibility * strokeOpacity * fade * headAlpha,
                           0.0F, 255.0F))),
        };
    };
    const auto emit = [](Vector2 point, Color color) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(point.x, point.y);
    };

    constexpr int Segments = 18;
    constexpr float History = 0.22F;
    // The first cross-section belongs to the tool's current pose. Keeping a
    // time offset here detaches the ribbon from the blade and makes it look as
    // though the trail begins in empty space.
    constexpr float HeadLag = 0.0F;
    std::array<Vector2, Segments + 1> edgePoints{};
    std::array<Vector2, Segments + 1> insidePoints{};
    std::array<Color, Segments + 1> colors{};
    const auto projectedPoint = [&](const GlbNodeAnimation& clip,
                                    float sampleProgress,
                                    Vector3 localPoint) {
        const Matrix pivot = transformMatrix(
            clip.sample(sampleProgress));
        const Vector3 point = Vector3Transform(
            Vector3Scale(localPoint, AuthoredChildScale), pivot);
        return GetWorldToScreen(
            Vector3Transform(point, outer), viewModelCamera);
    };
    int pointCount = 0;
    for (int pointIndex = 0; pointIndex <= Segments; ++pointIndex) {
        const float age =
            static_cast<float>(pointIndex) / Segments;
        const float sampleProgress =
            progress - HeadLag - age * History;
        if (sampleProgress < 0.0F) break;
        const Vector2 edgePoint = projectedPoint(
            animation, sampleProgress, bladeEdge);
        const Vector2 insidePoint = projectedPoint(
            animation, sampleProgress, bladeInside);
        edgePoints[static_cast<std::size_t>(pointIndex)] = edgePoint;
        insidePoints[static_cast<std::size_t>(pointIndex)] = insidePoint;
        colors[static_cast<std::size_t>(pointIndex)] = colorFor(age);
        ++pointCount;
    }
    if (pointCount < 2) return;

    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int segment = 0; segment + 1 < pointCount; ++segment) {
        const std::size_t first = static_cast<std::size_t>(segment);
        const std::size_t second = first + 1U;
        const Vector2 edge0 = edgePoints[first];
        const Vector2 inside0 = insidePoints[first];
        const Vector2 edge1 = edgePoints[second];
        const Vector2 inside1 = insidePoints[second];
        const Color color0 = colors[first];
        const Color color1 = colors[second];
        emit(edge0, color0);
        emit(inside0, color0);
        emit(inside1, color1);
        emit(edge0, color0);
        emit(inside1, color1);
        emit(edge1, color1);
    }
    rlEnd();
    // The reverse swing reverses ribbon winding. Submit both sides while
    // culling is disabled; rlEnd only queues vertices for the next batch.
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    EndBlendMode();
}

void Renderer::drawIceMagicSphere(
    Vector3 position, float radius, float timeSeconds,
    float intensity, Color tint) {
    if (!resources_.iceMagicShader().valid()) {
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphereEx(position, radius, 10, 10, tint);
        EndBlendMode();
        rlEnableDepthMask();
        return;
    }
    rlDrawRenderBatchActive();
    Shader& shader = resources_.iceMagicShader().get();
    const Vector4 shaderTint{
        tint.r / 255.0F, tint.g / 255.0F,
        tint.b / 255.0F, tint.a / 255.0F};
    SetShaderValue(shader, iceMagicTimeLocation_, &timeSeconds,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, iceMagicTintLocation_, &shaderTint,
                   SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, iceMagicIntensityLocation_, &intensity,
                   SHADER_UNIFORM_FLOAT);
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(shader);
    DrawSphereEx(position, radius, 10, 10, WHITE);
    EndShaderMode();
    EndBlendMode();
    rlEnableDepthMask();
    if (worldShaderActive_) BeginShaderMode(resources_.worldShader().get());
}

void Renderer::drawIceWandProjectile(
    const IceWandProjectile& projectile, Vector3 cameraPosition,
    float timeSeconds, float interpolationAlpha) {
    if (!projectile.active) return;
    const WorldMaterialState previousMaterial = worldMaterial_;
    WorldMaterialState material{};
    material.unlitAmount = 1;
    material.screenAoAmount = 0;
    material.inkOutlineEligible = false;
    material.inkOccluder = true;
    setWorldMaterial(material);
    fire_wand_effect::beginBatch(true);
    if (projectile.element == WandElement::Fire) {
        fire_wand_effect::emitProjectile(projectile, cameraPosition, timeSeconds, interpolationAlpha);
    } else {
        ice_wand_effect::emitProjectile(projectile, cameraPosition, timeSeconds, interpolationAlpha);
    }
    fire_wand_effect::endBatch();
    setWorldMaterial(previousMaterial);
}

} // namespace ian
