#include "graphics/Renderer.hpp"
#include "graphics/RendererModelSupport.hpp"


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

} // namespace

bool Renderer::hasAlternateHarvestSwingAnimation() const {
    return resources_.pickaxeAlternateAnimation().valid();
}

bool Renderer::drawFirstPersonTool(
    FirstPersonToolVisual visual, float swingProgress,
    float movementPhase, float movementAmount,
    const FirstPersonToolTuning& tuning,
    float iceChargeProgress, float iceRecoilProgress,
    bool alternateHarvestSwing) {
    ModelResource* resource = nullptr;
    const bool iceWand = visual == FirstPersonToolVisual::IceWand;
    const bool fireWand = visual == FirstPersonToolVisual::FireWand;
    const bool wand = iceWand || fireWand;
    const bool bomb = visual == FirstPersonToolVisual::Bomb;
    const bool axe = visual == FirstPersonToolVisual::Axe;
    const bool pickaxe = visual == FirstPersonToolVisual::Pickaxe;
    const bool club = visual == FirstPersonToolVisual::Club;
    const bool axeLike = axe || club;
    const bool authoredHarvestSwing = axeLike || pickaxe;
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
    case FirstPersonToolVisual::FireWand:
        resource = &resources_.iceWandModel();
        break;
    case FirstPersonToolVisual::Hammer:
        resource = &resources_.hammerModel();
        break;
    case FirstPersonToolVisual::Bomb:
        break;
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
    if (!authoredHarvestSwing && progress > 0.0F && progress < 0.22F) {
        const float phase = smoothStep(progress / 0.22F);
        swingPitch = tuning.windupDegrees * phase;
    } else if (!authoredHarvestSwing && progress >= 0.22F &&
               progress < 0.53F) {
        const float phase = smoothStep((progress - 0.22F) / 0.31F);
        swingPitch = tuning.windupDegrees +
                     (tuning.strikeDegrees - tuning.windupDegrees) * phase;
        swingPush = std::sin(phase * PI) * tuning.depthPush;
    } else if (!authoredHarvestSwing && progress >= 0.53F) {
        const float phase = smoothStep((progress - 0.53F) / 0.47F);
        swingPitch = tuning.strikeDegrees * (1.0F - phase);
    }

    const float bob = std::clamp(movementAmount, 0.0F, 1.0F) *
                      std::max(tuning.movementBob, 0.0F);
    const float bobX = std::sin(movementPhase) * 0.012F * bob;
    const float bobY =
        std::abs(std::cos(movementPhase)) * 0.014F * bob;
    const float charge = smoothStep(std::clamp(iceChargeProgress, 0.0F, 1.0F));
    const float recoil = std::sin(
        std::clamp(iceRecoilProgress, 0.0F, 1.0F) * PI);
    rlPushMatrix();
    constexpr Vector3 LegacyPickaxePosition{
        0.55401313F, -0.349843F, -0.7427701F};
    constexpr Vector3 LegacyAxePosition{
        0.39473686F, -0.349843F, -0.7427701F};
    constexpr Vector3 LegacyPickaxeRotation{22.0F, -60.0F, 7.0F};
    constexpr float LegacyPickaxeScale = 0.74F;
    const Vector3 authoredPosition = axeLike
        ? LegacyAxePosition : LegacyPickaxePosition;
    const float wandX = authoredHarvestSwing
        ? tuning.position.x - authoredPosition.x + bobX
        : (wand ? tuning.position.x + 0.035F + bobX
                : tuning.position.x + bobX);
    const float wandY = authoredHarvestSwing
        ? tuning.position.y - authoredPosition.y - bobY
        : (wand ? tuning.position.y - bobY + charge * 0.045F
                : tuning.position.y - bobY);
    const float wandZ = authoredHarvestSwing
        ? tuning.position.z - authoredPosition.z
        : (wand ? tuning.position.z + recoil * 0.075F
                : tuning.position.z + swingPush);
    rlTranslatef(wandX, wandY, wandZ);
    // Swing in camera space. Applying this before the model's local pose
    // keeps the strike aimed forward even when the tool is yawed in-hand.
    if (!authoredHarvestSwing) {
        rlRotatef(swingPitch, 1.0F, 0.0F, 0.0F);
    }
    // Blender exports the handle along local +Y. With the origin at the
    // grip, these rotations move the head without making the hand slide.
    rlRotatef(authoredHarvestSwing
                  ? tuning.rotation.x - LegacyPickaxeRotation.x
                  : tuning.rotation.x,
              1.0F, 0.0F, 0.0F);
    rlRotatef(authoredHarvestSwing
                  ? tuning.rotation.y - LegacyPickaxeRotation.y
                  : tuning.rotation.y,
              0.0F, 1.0F, 0.0F);
    rlRotatef(authoredHarvestSwing
                  ? tuning.rotation.z - LegacyPickaxeRotation.z
                  : tuning.rotation.z,
              0.0F, 0.0F, 1.0F);
    const float modelScale = bomb
        ? tuning.scale / 1.5F
        : (authoredHarvestSwing
               ? tuning.scale / LegacyPickaxeScale
               : tuning.scale);
    rlScalef(modelScale, modelScale, modelScale);
    Shader* viewModelShader = resources_.viewModelShader().valid()
        ? &resources_.viewModelShader().get()
        : nullptr;
    if (bomb) {
        if (viewModelShader != nullptr) {
            BeginShaderMode(*viewModelShader);
        }
        DrawSphereEx({}, 0.27F, 12, 12, {49, 55, 63, 255});
        DrawCylinder({0.0F, 0.29F, 0.0F}, 0.055F, 0.04F,
                     0.16F, 8, {124, 80, 39, 255});
        DrawSphere({0.0F, 0.39F, 0.0F}, 0.035F,
                   {255, 152, 38, 255});
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
                    animation.baseTransform());
                model.transform = MatrixMultiply(
                    MatrixInvert(base), animated);
            } else {
                // The pickaxe mesh has this Blender child-object scale baked
                // into its vertices. Apply it to the standalone axe mesh so
                // both tools share the authored pivot and camera space.
                constexpr float AuthoredChildScale = 5.307597F;
                model.transform = MatrixMultiply(
                    MatrixScale(AuthoredChildScale,
                                AuthoredChildScale,
                                AuthoredChildScale),
                    animated);
            }
        }
        DrawModel(model, {}, 1.0F,
                  fireWand ? Color{255, 193, 150, 255} : WHITE);
        model.transform = originalTransform;
    }
    if (wand) {
        constexpr float CrystalHeight = 0.365F;
        const float crystalPulse = 0.42F + charge * 0.28F +
            0.035F * std::sin(static_cast<float>(GetTime()) * 5.0F);
        drawIceMagicSphere(
            {0.0F, CrystalHeight, 0.0F},
            0.052F + charge * 0.012F,
            static_cast<float>(GetTime()), crystalPulse,
            fireWand ? Color{255, 104, 24, 165}
                     : Color{142, 229, 255, 145});
        if (charge > 0.01F && settings_.particles) {
            BeginBlendMode(BLEND_ADDITIVE);
            for (int particle = 0; particle < 5; ++particle) {
                const float phase = static_cast<float>(GetTime()) *
                    (2.6F + static_cast<float>(particle) * 0.37F) +
                    static_cast<float>(particle) * 1.2566F;
                const float orbit = 0.18F * charge;
                DrawSphereEx(
                    {std::cos(phase) * orbit,
                     CrystalHeight + std::sin(phase * 1.3F) * orbit,
                     std::sin(phase) * orbit},
                    0.008F + charge * 0.005F, 4, 4,
                    fireWand ? Color{255, 128, 32, 155}
                             : Color{142, 229, 255, 135});
            }
            EndBlendMode();
        }
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
    const bool axe = visual == FirstPersonToolVisual::Axe;
    const bool club = visual == FirstPersonToolVisual::Club;
    const bool pickaxe = visual == FirstPersonToolVisual::Pickaxe;
    const bool axeLike = axe || club;
    if (!axeLike && !pickaxe) return;
    const bool useAlternateAnimation =
        alternateHarvestSwing &&
        resources_.pickaxeAlternateAnimation().valid();
    const GlbNodeAnimation& animation = useAlternateAnimation
        ? resources_.pickaxeAlternateAnimation()
        : resources_.pickaxeAnimation();
    if (!animation.valid()) return;
    const GlbNodeAnimation& trailAnimation = useAlternateAnimation
        ? resources_.pickaxeAnimation()
        : animation;
    if (!trailAnimation.valid()) return;

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

    // Match the outer transform used by drawFirstPersonTool. The sampled
    // pivot moves the blade; its recent positions form the translucent arc.
    const Vector3 legacyPosition = axeLike
        ? Vector3{0.39473686F, -0.349843F, -0.7427701F}
        : Vector3{0.55401313F, -0.349843F, -0.7427701F};
    constexpr Vector3 LegacyRotation{22.0F, -60.0F, 7.0F};
    constexpr float ChildScale = 5.307597F;
    constexpr float LegacyScale = 0.74F;
    const float bob = std::clamp(movementAmount, 0.0F, 1.0F) *
        std::max(tuning.movementBob, 0.0F);
    const Vector3 offset{
        tuning.position.x - legacyPosition.x +
            std::sin(movementPhase) * 0.012F * bob,
        tuning.position.y - legacyPosition.y -
            std::abs(std::cos(movementPhase)) * 0.014F * bob,
        tuning.position.z - legacyPosition.z,
    };
    const float scale = tuning.scale / LegacyScale;
    Matrix outer = MatrixScale(scale, scale, scale);
    outer = MatrixMultiply(outer, MatrixRotateZ(
        (tuning.rotation.z - LegacyRotation.z) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixRotateY(
        (tuning.rotation.y - LegacyRotation.y) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixRotateX(
        (tuning.rotation.x - LegacyRotation.x) * DEG2RAD));
    outer = MatrixMultiply(outer, MatrixTranslate(
        offset.x, offset.y, offset.z));

    const Vector3 bladeEdge = axeLike
        ? Vector3{-0.21F, 0.35F, 0.0F}
        : Vector3{-0.36F, 0.52F, 0.0F};
    const Vector3 bladeInsideFullWidth = axeLike
        ? Vector3{-0.02F, 0.24F, 0.0F}
        : Vector3{-0.13F, 0.37F, 0.0F};
    const Vector3 bladeInside = Vector3Lerp(
        bladeEdge, bladeInsideFullWidth, 0.5F);
    const auto colorFor = [visibility, useAlternateAnimation](float age) {
        const float fade = std::pow(
            std::clamp(1.0F - age, 0.0F, 1.0F), 1.25F);
        const float shade = 1.0F - std::clamp(age, 0.0F, 1.0F);
        const float headAlpha = useAlternateAnimation ? 138.0F : 92.0F;
        return Color{
            static_cast<unsigned char>(std::lround(246.0F - 80.0F * shade)),
            static_cast<unsigned char>(std::lround(237.0F - 78.0F * shade)),
            static_cast<unsigned char>(std::lround(221.0F - 72.0F * shade)),
            static_cast<unsigned char>(std::lround(
                std::clamp(visibility * fade * headAlpha, 0.0F, 255.0F))),
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
            Vector3Scale(localPoint, ChildScale), pivot);
        Vector2 projected = GetWorldToScreen(
            Vector3Transform(point, outer), viewModelCamera);
        if (useAlternateAnimation) {
            projected.x = static_cast<float>(GetScreenWidth()) - projected.x;
        }
        return projected;
    };
    const float shapeProgress = useAlternateAnimation
        ? progress - 7.0F / 60.0F
        : progress;
    int pointCount = 0;
    for (int pointIndex = 0; pointIndex <= Segments; ++pointIndex) {
        const float age =
            static_cast<float>(pointIndex) / Segments;
        const float sampleProgress =
            shapeProgress - HeadLag - age * History;
        if (sampleProgress < 0.0F) break;
        const Vector2 edgePoint = projectedPoint(
            trailAnimation, sampleProgress, bladeEdge);
        const Vector2 insidePoint = projectedPoint(
            trailAnimation, sampleProgress, bladeInside);
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
    if (!projectile.active) {
        return;
    }
    const float alpha = std::clamp(interpolationAlpha, 0.0F, 1.0F);
    const Vector3 previous{
        static_cast<float>(projectile.previousPosition.x),
        static_cast<float>(projectile.previousPosition.y),
        static_cast<float>(projectile.previousPosition.z)};
    const Vector3 current{
        static_cast<float>(projectile.position.x),
        static_cast<float>(projectile.position.y),
        static_cast<float>(projectile.position.z)};
    const Vector3 position = Vector3Lerp(previous, current, alpha);
    const float radius = static_cast<float>(projectile.radius);
    const bool fire = projectile.element == WandElement::Fire;
    const Color trailOuterHead = fire
        ? Color{255, 92, 12, 170}
        : Color{52, 175, 255, 145};
    const Color trailOuterTail = fire
        ? Color{188, 28, 4, 125}
        : Color{24, 105, 235, 120};
    const Color trailInnerHead = fire
        ? Color{255, 244, 174, 245}
        : Color{223, 248, 255, 235};
    const Color trailInnerTail = fire
        ? Color{255, 146, 34, 215}
        : Color{142, 229, 255, 205};
    const auto fadedColor = [](Color color, float amount) {
        color.a = static_cast<unsigned char>(std::lround(
            static_cast<float>(color.a) *
            std::clamp(amount, 0.0F, 1.0F)));
        return color;
    };

    if (projectile.trailCount > 1U) {
        rlDrawRenderBatchActive();
        BeginBlendMode(BLEND_ADDITIVE);
        rlDisableDepthMask();
        rlDisableBackfaceCulling();
        rlBegin(RL_TRIANGLES);
        const std::size_t count = std::min(
            std::min(projectile.trailCount, IceWandTrailPointCount),
            std::size_t{12});
        const auto trailPoint = [&projectile, position](std::size_t index) {
            if (index == 0U) {
                return position;
            }
            const Vec3& value = projectile.trail[index];
            return Vector3{
                static_cast<float>(value.x),
                static_cast<float>(value.y),
                static_cast<float>(value.z)};
        };
        const auto emitVertex = [](Vector3 vertex, Color color) {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex3f(vertex.x, vertex.y, vertex.z);
        };
        for (std::size_t index = 0; index + 1 < count; ++index) {
            const Vector3 point = trailPoint(index);
            const Vector3 next = trailPoint(index + 1U);
            Vector3 tangent = Vector3Normalize(Vector3Subtract(point, next));
            Vector3 toCamera = Vector3Normalize(Vector3Subtract(cameraPosition, point));
            Vector3 side = Vector3Normalize(Vector3CrossProduct(tangent, toCamera));
            if (Vector3LengthSqr(side) < 0.0001F) {
                side = {1.0F, 0.0F, 0.0F};
            }
            const float fade = std::pow(
                1.0F - static_cast<float>(index) /
                    static_cast<float>(std::max<std::size_t>(count - 1, 1U)),
                1.15F);
            const float nextFade = std::pow(
                1.0F - static_cast<float>(index + 1U) /
                    static_cast<float>(std::max<std::size_t>(count - 1, 1U)),
                1.15F);
            const auto emitRibbon = [&](float width, float nextWidth,
                                        Color headColor, Color tailColor) {
                const Vector3 left = Vector3Subtract(
                    point, Vector3Scale(side, width));
                const Vector3 right = Vector3Add(
                    point, Vector3Scale(side, width));
                const Vector3 nextLeft = Vector3Subtract(
                    next, Vector3Scale(side, nextWidth));
                const Vector3 nextRight = Vector3Add(
                    next, Vector3Scale(side, nextWidth));
                emitVertex(left, headColor);
                emitVertex(right, headColor);
                emitVertex(nextRight, tailColor);
                emitVertex(left, headColor);
                emitVertex(nextRight, tailColor);
                emitVertex(nextLeft, tailColor);
            };
            emitRibbon(
                radius * (0.92F * fade + 0.05F),
                radius * (0.92F * nextFade + 0.025F),
                fadedColor(trailOuterHead, fade),
                fadedColor(trailOuterTail, nextFade));
            emitRibbon(
                radius * (0.28F * fade + 0.018F),
                radius * (0.28F * nextFade + 0.008F),
                fadedColor(trailInnerHead, fade),
                fadedColor(trailInnerTail, nextFade));
        }
        rlEnd();
        rlDrawRenderBatchActive();
        rlEnableBackfaceCulling();
        rlEnableDepthMask();
        EndBlendMode();
    }

    const float pulse = 0.94F + 0.12F * std::sin(
        timeSeconds * 4.6F + static_cast<float>(projectile.id.index) * 0.41F);
    BeginBlendMode(BLEND_ALPHA);
    DrawSphereEx(position, radius * (0.94F + pulse * 0.025F), 12, 12,
                 fire ? Color{232, 61, 12, 255}
                      : Color{48, 145, 232, 255});
    EndBlendMode();
    drawIceMagicSphere(position, radius * (1.48F + pulse * 0.04F),
                       timeSeconds, 0.96F,
                       fire ? Color{255, 126, 28, 235}
                            : Color{142, 229, 255, 225});
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(timeSeconds * 72.0F, 0.0F, 1.0F, 0.0F);
    DrawCircle3D({}, radius * 1.42F,
                 {1.0F, 0.0F, 0.0F}, 63.0F,
                 fire ? Color{255, 212, 91, 165}
                      : Color{191, 246, 255, 145});
    rlRotatef(117.0F, 0.45F, 0.72F, 0.53F);
    DrawCircle3D({}, radius * 1.27F,
                 {0.0F, 0.0F, 1.0F}, 47.0F,
                 fire ? Color{255, 76, 12, 135}
                      : Color{85, 207, 255, 112});
    rlPopMatrix();
    EndBlendMode();
    if (settings_.particles) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int spark = 0; spark < 4; ++spark) {
            const float phase = timeSeconds *
                (2.1F + static_cast<float>(spark) * 0.31F) +
                static_cast<float>(projectile.id.index % 17U) * 0.13F +
                static_cast<float>(spark) * 0.897F;
            const float distance = radius * (1.30F + 0.38F *
                std::sin(timeSeconds * 3.0F + static_cast<float>(spark) * 1.4F));
            const float sparkSize = radius *
                (0.075F + 0.035F * (0.5F + 0.5F * std::sin(
                    timeSeconds * 7.0F + static_cast<float>(spark))));
            DrawSphereEx(
                {position.x + std::cos(phase) * distance,
                 position.y + std::sin(phase * 1.7F) * distance,
                 position.z + std::sin(phase) * distance},
                sparkSize, 4, 4,
                spark % 3 == 0
                    ? (fire ? Color{255, 245, 181, 225}
                            : Color{223, 248, 255, 215})
                    : (fire ? Color{255, 113, 24, 185}
                            : Color{142, 229, 255, 172}));
            if (projectile.trailCount > 2U) {
                const std::size_t trailIndex = std::min<std::size_t>(
                    projectile.trailCount - 1U,
                    1U + static_cast<std::size_t>(spark) * 2U);
                const Vec3& trailPoint = projectile.trail[trailIndex];
                const Vector3 trailPosition{
                    static_cast<float>(trailPoint.x),
                    static_cast<float>(trailPoint.y),
                    static_cast<float>(trailPoint.z)};
                const Vector3 streakEnd{
                    trailPosition.x + std::cos(phase) * radius * 0.28F,
                    trailPosition.y + std::sin(phase * 1.3F) * radius * 0.18F,
                    trailPosition.z + std::sin(phase) * radius * 0.28F};
                DrawLine3D(
                    trailPosition, streakEnd,
                    fire
                        ? Color{255, 112, 18,
                              static_cast<unsigned char>(105.0F * pulse)}
                        : Color{142, 229, 255,
                              static_cast<unsigned char>(95.0F * pulse)});
            }
        }
        EndBlendMode();
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

} // namespace ian
