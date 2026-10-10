#include "app/App.hpp"
#include "app/AppEffectDrawing.hpp"
#include "app/AppFreezeDrawing.hpp"
#include "app/AppRenderSupport.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <ranges>
#include <string>

namespace ian {

using namespace app_detail;
using namespace app_effect_drawing;

namespace {

float effectUnit(int index, int channel) {
    return atmosphereUnit(
        0x9e3779b9U * static_cast<std::uint32_t>(index + 1) +
        0x85ebca6bU * static_cast<std::uint32_t>(channel + 3));
}

void drawAtlasBillboard(
    Texture2D texture, int columns, int rows, int frame,
    Vector3 center, float worldSize, const Camera3D& camera,
    Vector3 cameraUp, Color tint, float rotation = 0.0F) {
    if (!IsTextureValid(texture) || columns <= 0 || rows <= 0) {
        return;
    }
    const int frameCount = columns * rows;
    const int clampedFrame = std::clamp(frame, 0, frameCount - 1);
    const float cellWidth = static_cast<float>(texture.width) /
        static_cast<float>(columns);
    const float cellHeight = static_cast<float>(texture.height) /
        static_cast<float>(rows);
    const Rectangle source{
        static_cast<float>(clampedFrame % columns) * cellWidth,
        static_cast<float>(clampedFrame / columns) * cellHeight,
        cellWidth, cellHeight,
    };
    DrawBillboardPro(
        camera, texture, source, center, cameraUp,
        {worldSize, worldSize},
        {worldSize * 0.5F, worldSize * 0.5F},
        rotation, tint);
}

int animationFrame(float progress, int frameCount) {
    return std::min(
        frameCount - 1,
        static_cast<int>(std::clamp(progress, 0.0F, 1.0F) *
                         static_cast<float>(frameCount)));
}

void emitImpactStreak(
    Vector3 center, Vector3 right, Vector3 up,
    float angle, float length, float halfWidth, Color color) {
    const Vector3 direction = Vector3Add(
        Vector3Scale(right, std::cos(angle)),
        Vector3Scale(up, std::sin(angle)));
    const Vector3 perpendicular = Vector3Add(
        Vector3Scale(right, -std::sin(angle)),
        Vector3Scale(up, std::cos(angle)));
    const Vector3 tip = Vector3Add(center, Vector3Scale(direction, length));
    const Vector3 sideA = Vector3Add(
        center, Vector3Scale(perpendicular, halfWidth));
    const Vector3 sideB = Vector3Subtract(
        center, Vector3Scale(perpendicular, halfWidth));
    Color transparent = color;
    transparent.a = 0;
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(sideA.x, sideA.y, sideA.z);
    rlColor4ub(transparent.r, transparent.g, transparent.b, transparent.a);
    rlVertex3f(tip.x, tip.y, tip.z);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(sideB.x, sideB.y, sideB.z);
}

Vector3 lightningOffsetDirection(
    Vector3 direction, Vector3 axis) {
    Vector3 result = Vector3CrossProduct(direction, axis);
    if (Vector3LengthSqr(result) < 0.0001F) {
        result = Vector3CrossProduct(
            direction, Vector3{1.0F, 0.0F, 0.0F});
    }
    return Vector3Normalize(result);
}

void drawLightningArc(
    Vector3 start, Vector3 end, float progress,
    int seed, float amplitudeScale, float alphaScale) {
    const Vector3 delta = Vector3Subtract(end, start);
    const float length = Vector3Length(delta);
    if (length < 0.01F) {
        return;
    }
    const Vector3 direction = Vector3Scale(delta, 1.0F / length);
    const Vector3 side = lightningOffsetDirection(
        direction, Vector3{0.0F, 1.0F, 0.0F});
    const Vector3 lift = Vector3Normalize(
        Vector3CrossProduct(side, direction));
    const int segmentCount = std::clamp(
        static_cast<int>(std::ceil(length * 2.2F)), 7, 20);
    const int flickerFrame =
        static_cast<int>(std::floor(progress * 34.0F));
    const float baseAmplitude =
        std::min(0.42F, 0.075F * length) * amplitudeScale;
    Vector3 previous = start;
    for (int segment = 1; segment <= segmentCount; ++segment) {
        const float t = static_cast<float>(segment) /
            static_cast<float>(segmentCount);
        Vector3 point = Vector3Lerp(start, end, t);
        if (segment < segmentCount) {
            const float envelope = std::sin(t * PI);
            const int sample = segment + flickerFrame * 29 + seed * 101;
            const float sideNoise =
                effectUnit(sample, 201 + seed) * 2.0F - 1.0F;
            const float liftNoise =
                effectUnit(sample, 263 + seed) * 2.0F - 1.0F;
            const float wave = std::sin(
                t * (10.0F + static_cast<float>(seed % 5)) +
                progress * 22.0F + static_cast<float>(seed));
            point = Vector3Add(
                point,
                Vector3Scale(
                    side,
                    (sideNoise * 0.78F + wave * 0.22F) *
                        baseAmplitude * envelope));
            point = Vector3Add(
                point,
                Vector3Scale(
                    lift, liftNoise * baseAmplitude * 0.62F *
                        envelope));
        }

        DrawCylinderEx(
            previous, point, 0.120F * amplitudeScale,
            0.120F * amplitudeScale, 6,
            {35, 104, 255,
             atmosphereAlpha(0.24F * alphaScale)});
        DrawCylinderEx(
            previous, point, 0.052F * amplitudeScale,
            0.052F * amplitudeScale, 6,
            {63, 187, 255,
             atmosphereAlpha(0.82F * alphaScale)});
        DrawCylinderEx(
            previous, point, 0.016F * amplitudeScale,
            0.016F * amplitudeScale, 5,
            {238, 253, 255,
             atmosphereAlpha(0.98F * alphaScale)});
        previous = point;
    }
}

struct ExplosionSphereVertex {
    Vector3 unit;
    int shade;
};

const auto& explosionSphereVertices() {
    // One shared low-poly sphere, reused by every cloud and ember. No mesh
    // allocation, per-puff draw call, or per-vertex trigonometry during combat.
    static const auto vertices = [] {
        constexpr int Slices = 12;
        constexpr int Rings = 6;
        std::array<ExplosionSphereVertex, Slices * (Rings - 1) * 6> result{};
        std::size_t count = 0;
        const auto point = [](int ring, int slice) {
            const float latitude = -PI * 0.5F + PI * static_cast<float>(ring) / Rings;
            const float longitude = PI * 2.0F * static_cast<float>(slice) / Slices;
            return Vector3{std::cos(latitude) * std::cos(longitude),
                           std::sin(latitude),
                           std::cos(latitude) * std::sin(longitude)};
        };
        const auto append = [&](Vector3 unit) {
            const float light = Vector3DotProduct(unit, {-0.42F, 0.82F, 0.38F});
            result[count++] = {unit, light > 0.58F ? 2 : light > -0.12F ? 1 : 0};
        };
        for (int ring = 0; ring < Rings; ++ring) {
            for (int slice = 0; slice < Slices; ++slice) {
                const Vector3 a = point(ring, slice);
                const Vector3 b = point(ring + 1, slice);
                const Vector3 c = point(ring + 1, slice + 1);
                const Vector3 d = point(ring, slice + 1);
                if (ring < Rings - 1) { append(a); append(b); append(c); }
                if (ring > 0) { append(a); append(c); append(d); }
            }
        }
        return result;
    }();
    return vertices;
}

struct ExplosionPuff {
    Vector3 position;
    Vector3 size;
    std::array<Color, 3> palette;
    float cameraDepth;
};

void emitExplosionPuff(const ExplosionPuff& puff) {
    for (const auto& vertex : explosionSphereVertices()) {
        const Color color = puff.palette[static_cast<std::size_t>(vertex.shade)];
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlNormal3f(vertex.unit.x, vertex.unit.y, vertex.unit.z);
        rlVertex3f(puff.position.x + vertex.unit.x * puff.size.x,
                   puff.position.y + vertex.unit.y * puff.size.y,
                   puff.position.z + vertex.unit.z * puff.size.z);
    }
}

void emitCartoonExplosion(
    Vector3 origin, float groundHeight, float progress, float scale,
    std::uint32_t seed, bool detailed, Vector3 cameraPosition,
    Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp) {
    progress = std::clamp(progress, 0.0F, 1.0F);
    if (progress >= 1.0F || scale <= 0.0F) return;
    const auto random = [seed](int index, int channel) {
        return atmosphereUnit(seed + static_cast<std::uint32_t>(index) * 0x9e3779b9U +
                              static_cast<std::uint32_t>(channel) * 0x85ebca6bU);
    };
    const auto vertex = [](Vector3 point, Color color) {
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(point.x, point.y, point.z);
    };

    // A broad, short pressure ring, rather than several thin wire circles.
    const float ringFade = 1.0F - smoothstep(0.12F, 0.38F, progress);
    if (ringFade > 0.01F && origin.y - groundHeight < scale * 2.2F) {
        constexpr int Segments = 32;
        const float travel = 1.0F - std::pow(1.0F - std::min(progress / 0.38F, 1.0F), 3.0F);
        const float radius = scale * (0.18F + travel * 2.3F);
        const float width = scale * (0.04F + 0.13F * ringFade);
        const Vector3 center{origin.x, groundHeight + 0.045F, origin.z};
        const Color color{255, 226, 146, atmosphereAlpha(ringFade * 0.72F)};
        rlNormal3f(0, 1, 0);
        for (int index = 0; index < Segments; ++index) {
            const float a = static_cast<float>(index) * 2.0F * PI / Segments;
            const float b = static_cast<float>(index + 1) * 2.0F * PI / Segments;
            const auto point = [center](float angle, float distance) {
                return Vector3{center.x + std::cos(angle) * distance, center.y,
                               center.z + std::sin(angle) * distance};
            };
            const Vector3 innerA = point(a, std::max(0.0F, radius - width));
            const Vector3 innerB = point(b, std::max(0.0F, radius - width));
            const Vector3 outerA = point(a, radius);
            const Vector3 outerB = point(b, radius);
            vertex(innerA, color); vertex(innerB, color); vertex(outerA, color);
            vertex(outerA, color); vertex(innerB, color); vertex(outerB, color);
        }
    }

    std::array<ExplosionPuff, 19> puffs{};
    std::size_t puffCount = 0;
    const auto addPuff = [&](Vector3 position, Vector3 size,
                             std::array<Color, 3> palette, float opacity) {
        if (opacity < 0.015F || size.x < 0.005F) return;
        for (Color& color : palette) color.a = atmosphereAlpha(opacity);
        puffs[puffCount++] = {position, size, palette,
            Vector3DotProduct(Vector3Subtract(position, cameraPosition), cameraForward)};
    };
    constexpr std::array<Color, 3> Fire{{{236, 79, 28, 255},
        {255, 159, 40, 255}, {255, 224, 117, 255}}};
    constexpr std::array<Color, 3> Smoke{{{115, 104, 128, 255},
        {174, 162, 181, 255}, {232, 221, 213, 255}}};
    const int cloudCount = detailed ? 9 : 5;
    for (int index = 0; index < cloudCount; ++index) {
        const float delay = index == 0 ? 0.0F : random(index, 1) * 0.08F;
        const float local = std::clamp((progress - delay) / (1.0F - delay), 0.0F, 1.0F);
        const float pop = smoothstep(0.0F, 0.12F, local);
        const float disappear = 1.0F - smoothstep(0.55F, 1.0F, local);
        const float heatLoss = smoothstep(0.20F, 0.54F, local);
        const float travel = 1.0F - std::pow(1.0F - local, 3.0F);
        const float angle = static_cast<float>(index) * 2.399963F + random(index, 2) * 0.5F;
        const float distance = index == 0 ? 0.0F : scale * travel *
            (0.58F + random(index, 3) * 1.05F);
        const float size = scale * (index == 0 ? 0.86F : 0.42F + random(index, 4) * 0.30F) *
            (0.16F + pop * 0.84F) * (1.0F + local * 0.22F) * disappear;
        std::array<Color, 3> palette{};
        for (std::size_t shade = 0; shade < palette.size(); ++shade)
            palette[shade] = ColorLerp(Fire[shade], Smoke[shade], heatLoss);
        addPuff({origin.x + std::cos(angle) * distance,
                 origin.y + scale * (0.26F + pop * 0.22F + local *
                     (index == 0 ? 1.7F : 0.75F + random(index, 5) * 1.2F)),
                 origin.z + std::sin(angle) * distance},
                {size, size * (0.90F + local * 0.18F), size}, palette,
                pop * (1.0F - smoothstep(0.70F, 1.0F, local)) * 0.98F);
    }
    if (detailed) {
        for (int index = 0; index < 4; ++index) {
            const float travel = 1.0F - std::pow(1.0F - progress, 4.0F);
            const float angle = random(index, 12) * 2.0F * PI;
            const float distance = scale * travel * (1.1F + random(index, 13) * 1.1F);
            const float fade = 1.0F - smoothstep(0.30F, 0.78F, progress);
            const float size = scale * (0.20F + random(index, 14) * 0.14F) *
                smoothstep(0.0F, 0.09F, progress) * fade;
            addPuff({origin.x + std::cos(angle) * distance,
                     groundHeight + size * 0.50F + scale * progress * 0.30F,
                     origin.z + std::sin(angle) * distance},
                    {size * 1.25F, size * 0.7F, size}, Smoke, fade * 0.85F);
        }
        constexpr std::array<Color, 3> Ember{{{248, 127, 29, 255},
            {255, 212, 70, 255}, {255, 245, 183, 255}}};
        for (int index = 0; index < 6; ++index) {
            const float angle = random(index, 20) * 2.0F * PI;
            const float distance = scale * progress * (2.4F + random(index, 21) * 1.8F);
            const float fade = 1.0F - smoothstep(0.25F, 0.62F, progress);
            const float size = scale * (0.055F + random(index, 22) * 0.04F) * fade;
            addPuff({origin.x + std::cos(angle) * distance,
                     origin.y + scale * (0.18F + progress *
                         (2.2F + random(index, 23) * 1.7F) - progress * progress * 3.1F),
                     origin.z + std::sin(angle) * distance},
                    {size, size * 1.5F, size}, Ember, fade);
        }
    }
    // Far-to-near alpha clouds, with depth testing against the world but no
    // depth writes: an almost vanished puff must never cut holes in another.
    std::sort(puffs.begin(), puffs.begin() + static_cast<std::ptrdiff_t>(puffCount),
        [](const ExplosionPuff& a, const ExplosionPuff& b) { return a.cameraDepth > b.cameraDepth; });
    for (std::size_t index = 0; index < puffCount; ++index) emitExplosionPuff(puffs[index]);

    const float flash = 1.0F - smoothstep(0.06F, 0.22F, progress);
    if (flash > 0.01F) {
        const float radius = scale * (0.22F + smoothstep(0.0F, 0.07F, progress) * 1.04F) * flash;
        const Vector3 center = Vector3Subtract(
            Vector3Add(origin, {0, scale * 0.45F, 0}), Vector3Scale(cameraForward, scale * 0.9F));
        rlNormal3f(-cameraForward.x, -cameraForward.y, -cameraForward.z);
        for (int layer = 0; layer < 2; ++layer) {
            const Color color = layer == 0 ? Color{255, 139, 37, atmosphereAlpha(flash)}
                                          : Color{255, 248, 206, atmosphereAlpha(flash)};
            const Vector3 layerCenter = Vector3Subtract(center,
                Vector3Scale(cameraForward, static_cast<float>(layer) * 0.01F));
            for (int index = 0; index < 24; ++index) {
                const auto point = [&](int corner) {
                    const float angle = static_cast<float>(corner) * 2.0F * PI / 24.0F;
                    const float length = radius * (layer == 0 ? 1.0F : 0.72F) *
                        (corner % 2 == 0 ? 0.82F + random(corner % 24, 30) * 0.18F : 0.40F);
                    return Vector3Add(layerCenter, Vector3Add(
                        Vector3Scale(cameraRight, std::cos(angle) * length),
                        Vector3Scale(cameraUp, std::sin(angle) * length)));
                };
                vertex(layerCenter, color); vertex(point(index), color); vertex(point(index + 1), color);
            }
        }
    }
}


} // namespace

void App::drawPresentationEffects(
    const Camera3D& camera, const WorldLighting& lighting) {
    Vector3 cameraForward = Vector3Subtract(camera.target, camera.position);
    cameraForward = Vector3LengthSqr(cameraForward) > 0.000001F
        ? Vector3Normalize(cameraForward)
        : Vector3{0.0F, 0.0F, -1.0F};
    Vector3 cameraRight = Vector3CrossProduct(cameraForward, camera.up);
    cameraRight = Vector3LengthSqr(cameraRight) > 0.000001F
        ? Vector3Normalize(cameraRight)
        : Vector3{1.0F, 0.0F, 0.0F};
    const Vector3 billboardUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, cameraForward));

    const bool hasResourceFragments = std::ranges::any_of(
        effects_, [](const PresentationEffect& effect) {
            return effect.startDelayRemaining <= 0.0 &&
                effect.type ==
                    PresentationEffectType::ResourceFragment;
        });
    if (hasResourceFragments) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState chipMaterial{};
        chipMaterial.bakedAo = 0.86F;
        chipMaterial.screenAoAmount = 0.0F;
        chipMaterial.inkOutlineEligible = true;
        chipMaterial.outlineMaterialClassOverride = 4.0F;
        renderer_->setWorldMaterial(chipMaterial);
        for (const PresentationEffect& effect : effects_) {
            if (effect.startDelayRemaining > 0.0 ||
                effect.type !=
                    PresentationEffectType::ResourceFragment ||
                !effect.targetPosition) {
                continue;
            }
            const float elapsed = static_cast<float>(
                effect.duration - effect.remaining);
            const float progress = std::clamp(
                elapsed / static_cast<float>(effect.duration),
                0.0F, 1.0F);
            const Vector3 velocity{
                static_cast<float>(effect.targetPosition->x),
                static_cast<float>(effect.targetPosition->y),
                static_cast<float>(effect.targetPosition->z),
            };
            Vector3 position{
                static_cast<float>(effect.position.x) +
                    velocity.x * elapsed,
                static_cast<float>(effect.position.y) +
                    velocity.y * elapsed - 3.9F * elapsed * elapsed,
                static_cast<float>(effect.position.z) +
                    velocity.z * elapsed,
            };
            const float groundHeight = static_cast<float>(
                simulation_.terrain().getHeight(
                    position.x, position.z));
            const float groundPenetration = std::max(
                0.0F, groundHeight + 0.025F - position.y);
            position.y = std::max(position.y, groundHeight + 0.025F);
            const std::uint32_t seed =
                static_cast<std::uint32_t>(effect.variant);
            const Vector3 rotationAxis =
                Vector3LengthSqr(velocity) > 0.000001F
                    ? Vector3Normalize(velocity)
                    : Vector3{0.0F, 1.0F, 0.0F};
            // Wood and crystal shards should tumble across their flight path.
            // Spinning around the flight vector makes long crystals roll like
            // a drill bit, especially when launched toward the camera.
            Vector3 tumbleAxis = Vector3CrossProduct(
                rotationAxis, {0.0F, 1.0F, 0.0F});
            if (Vector3LengthSqr(tumbleAxis) <= 0.000001F) {
                tumbleAxis = Vector3CrossProduct(
                    rotationAxis, {1.0F, 0.0F, 0.0F});
            }
            tumbleAxis = Vector3Normalize(tumbleAxis);
            const Vector3 secondaryTumbleAxis = Vector3Normalize(
                Vector3CrossProduct(rotationAxis, tumbleAxis));
            tumbleAxis = Vector3Normalize(Vector3Add(
                tumbleAxis,
                Vector3Scale(
                    secondaryTumbleAxis,
                    (effectUnit(static_cast<int>(seed), 734) - 0.5F) *
                        0.5F)));
            const float disappearScale =
                (1.0F - smoothstep(0.76F, 1.0F, progress)) *
                (1.0F - smoothstep(
                    0.0F, 0.18F, groundPenetration));
            const float rotationDegrees =
                progress *
                (480.0F + effectUnit(
                    static_cast<int>(seed), 733) * 420.0F);
            const float tumbleRadians =
                rotationDegrees * 0.60F * DEG2RAD;
            const float rollDirection = (seed & 1U) == 0U ? 1.0F : -1.0F;
            const Quaternion fragmentRotation = QuaternionMultiply(
                QuaternionFromAxisAngle(tumbleAxis, tumbleRadians),
                QuaternionFromAxisAngle(
                    rotationAxis,
                    tumbleRadians * 0.14F * rollDirection));
            Vector3 fragmentAxis{};
            float fragmentAngleRadians = 0.0F;
            QuaternionToAxisAngle(
                fragmentRotation, &fragmentAxis, &fragmentAngleRadians);
            if (effect.resourceType == ResourceType::Wood) {
                static_cast<void>(renderer_->drawWoodFragment(
                    position, fragmentAxis,
                    fragmentAngleRadians * RAD2DEG,
                    effect.scale * disappearScale, WHITE));
            } else if (effect.resourceType == ResourceType::Crystal) {
                static_cast<void>(renderer_->drawCrystalFragment(
                    position, fragmentAxis,
                    fragmentAngleRadians * RAD2DEG,
                    effect.scale * disappearScale, WHITE));
            } else {
                static_cast<void>(renderer_->drawDecorativeRockFragment(
                    position, rotationAxis, rotationDegrees,
                    effect.scale * disappearScale,
                    seed % 4U, WHITE));
            }
        }
        renderer_->endWorldShader();
    }

    // Hits are the highest-volume effect during tower-heavy nights. Submit
    // every flash and spark as one camera-facing triangle batch rather than
    // issuing spheres and lines per hit.
    beginAdditiveEffectPass(vfxImpactShader_.get(), vfxImpactUseTextureLocation_);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
    for (const PresentationEffect& effect : effects_) {
        if (effect.startDelayRemaining > 0.0 ||
            (effect.type != PresentationEffectType::Hit &&
             effect.type != PresentationEffectType::EnemyHitImpact)) {
            continue;
        }
        const float progress = std::clamp(static_cast<float>(
            1.0 - effect.remaining / effect.duration), 0.0F, 1.0F);
        const Vector3 origin{
            static_cast<float>(effect.position.x),
            static_cast<float>(effect.position.y),
            static_cast<float>(effect.position.z),
        };
        const bool enemyImpact =
            effect.type == PresentationEffectType::EnemyHitImpact;
        const bool critical = enemyImpact && (effect.variant & 8) != 0;
        const int style = enemyImpact ? effect.variant & 7 : 2;
        Color core{255, 244, 207, 255};
        Color edge{203, 176, 119, 255};
        if (style == 1) {
            core = {215, 250, 255, 255};
            edge = {86, 199, 255, 255};
        } else if (style == 2) {
            core = {255, 239, 154, 255};
            edge = {255, 92, 35, 255};
        } else if (style == 3) {
            core = {242, 224, 255, 255};
            edge = {151, 102, 255, 255};
        }
        const float fade = enemyImpact
            ? 1.0F - smoothstep(0.36F, 1.0F, progress)
            : 1.0F - progress;
        const float flash = enemyImpact
            ? 1.0F - smoothstep(0.0F, 0.30F, progress)
            : fade;
        const float criticalScale = critical ? 1.24F : 1.0F;
        Color outer = edge;
        outer.a = atmosphereAlpha(fade * 0.62F);
        Color inner = core;
        inner.a = atmosphereAlpha(flash * 0.94F);
        emitSoftBillboardDisc(
            origin, cameraRight, billboardUp,
            effect.scale * criticalScale *
                (0.22F + progress * 0.24F),
            outer);
        emitSoftBillboardDisc(
            origin, cameraRight, billboardUp,
            effect.scale * criticalScale *
                (0.095F + progress * 0.055F),
            inner, 6);

        const bool authoredImpactAvailable = enemyImpact ||
            vfxImpactTexture_.valid();
        if (renderer_->settings().particles &&
            !authoredImpactAvailable) {
            const int streakCount = critical ? 5 : 3;
            const int seed = effect.entityId
                ? static_cast<int>(effect.entityId->index) : style * 17;
            for (int index = 0; index < streakCount; ++index) {
                const float angle = effectUnit(
                    seed + index * 13, 401) * 2.0F * PI;
                Color streak = core;
                streak.a = atmosphereAlpha(fade * 0.78F);
                emitImpactStreak(
                    origin, cameraRight, billboardUp, angle,
                    effect.scale * criticalScale *
                        (0.24F + progress * 0.34F),
                    effect.scale * 0.025F, streak);
            }
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    endAdditiveEffectPass(vfxImpactShader_.get());

    // Authored flipbooks add a readable silhouette without multiplying the
    // sphere/line count. Draw in two texture-coherent passes so dense tower
    // combat remains batch friendly; the second pass is reserved for the
    // much rarer ricochet impacts.
    if (renderer_->settings().particles &&
        (vfxImpactTexture_.valid() ||
         vfxElectricRingTexture_.valid())) {
        beginAdditiveEffectPass(vfxImpactShader_.get(), vfxImpactUseTextureLocation_,
            EffectSurface::Texture);
        rlDisableDepthMask();
        for (int electricPass = 0; electricPass < 2; ++electricPass) {
            const TextureResource& resource = electricPass != 0
                ? vfxElectricRingTexture_ : vfxImpactTexture_;
            if (!resource.valid()) continue;
            for (const PresentationEffect& effect : effects_) {
                if (effect.startDelayRemaining > 0.0 ||
                    (effect.type != PresentationEffectType::Hit &&
                     effect.type !=
                         PresentationEffectType::EnemyHitImpact)) {
                    continue;
                }
                const bool enemyImpact = effect.type ==
                    PresentationEffectType::EnemyHitImpact;
                const int style = enemyImpact
                    ? effect.variant & 7 : 2;
                const bool electric = style == 5;
                if (electric != (electricPass != 0)) continue;
                if (enemyImpact && !electric) {
                    continue;
                }
                const float progress = std::clamp(static_cast<float>(
                    1.0 - effect.remaining / effect.duration),
                    0.0F, 1.0F);
                const bool critical = enemyImpact &&
                    (effect.variant & 8) != 0;
                Color tint{255, 238, 182, 255};
                if (style == 1) {
                    tint = {116, 218, 255, 255};
                } else if (style == 2) {
                    tint = {255, 139, 64, 255};
                } else if (style == 3) {
                    tint = {188, 128, 255, 255};
                } else if (electric) {
                    tint = {176, 235, 255, 255};
                }
                const float fade = 1.0F - smoothstep(
                    electric ? 0.62F : 0.48F, 1.0F, progress);
                tint.a = atmosphereAlpha(fade *
                    (electric ? 0.92F : 0.76F));
                const Vector3 origin{
                    static_cast<float>(effect.position.x),
                    static_cast<float>(effect.position.y),
                    static_cast<float>(effect.position.z),
                };
                const float size = effect.scale *
                    (electric ? 1.72F : 1.38F) *
                    (critical ? 1.26F : 1.0F);
                const int seed = effect.entityId
                    ? static_cast<int>(effect.entityId->index) : style * 19;
                drawAtlasBillboard(
                    resource.get(), 6, 5,
                    animationFrame(progress, 30), origin, size,
                    camera, billboardUp, tint,
                    static_cast<float>(seed % 7 - 3) * 4.0F);
            }
        }
        rlEnableDepthMask();
        endAdditiveEffectPass(vfxImpactShader_.get());
    }

    struct ExplosionDraw {
        const PresentationEffect* effect{};
        float distanceSquared{};
        bool detailed{};
    };
    std::array<ExplosionDraw, 128> explosions{};
    std::size_t explosionCount = 0;
    const float maximumExplosionDistance = lighting.fogEnd + 6.0F;
    for (auto it = effects_.rbegin(); it != effects_.rend(); ++it) {
        if (it->startDelayRemaining > 0.0 ||
            (it->type != PresentationEffectType::Explosion &&
             it->type != PresentationEffectType::FireImpact)) continue;
        const Vector3 delta{static_cast<float>(it->position.x) - camera.position.x,
                            static_cast<float>(it->position.y) - camera.position.y,
                            static_cast<float>(it->position.z) - camera.position.z};
        const float distanceSquared = Vector3LengthSqr(delta);
        if (distanceSquared > maximumExplosionDistance * maximumExplosionDistance ||
            Vector3DotProduct(delta, cameraForward) < -it->scale * 3.0F) continue;
        if (explosionCount == explosions.size()) break;
        // Only the four newest visible blasts get the extra dust and embers.
        explosions[explosionCount] = {&*it, distanceSquared,
            renderer_->settings().particles && explosionCount < 4U};
        ++explosionCount;
    }
    if (explosionCount > 0) {
        std::sort(explosions.begin(), explosions.begin() + static_cast<std::ptrdiff_t>(explosionCount),
            [](const ExplosionDraw& a, const ExplosionDraw& b) {
                return a.distanceSquared > b.distanceSquared;
            });
        renderer_->beginWorldShader(lighting);
        WorldMaterialState cloudMaterial{};
        cloudMaterial.unlitAmount = 1.0F;
        cloudMaterial.screenAoAmount = 0.0F;
        cloudMaterial.inkOutlineEligible = false;
        cloudMaterial.inkOccluder = true;
        renderer_->setWorldMaterial(cloudMaterial);
        renderer_->beginColorOnlyWorldEffects();
        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthMask();
        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);
        for (std::size_t index = 0; index < explosionCount; ++index) {
            const auto& draw = explosions[index];
            const PresentationEffect& effect = *draw.effect;
            const Vector3 origin{static_cast<float>(effect.position.x),
                                 static_cast<float>(effect.position.y),
                                 static_cast<float>(effect.position.z)};
            const float progress = static_cast<float>(1.0 - effect.remaining / effect.duration);
            const auto seed = effect.entityId ? effect.entityId->index :
                static_cast<std::uint32_t>(effect.variant);
            emitCartoonExplosion(origin, static_cast<float>(simulation_.terrain().getHeight(
                origin.x, origin.z)), progress, effect.scale, seed, draw.detailed,
                camera.position, cameraForward, cameraRight, billboardUp);
        }
        rlEnd();
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
        EndBlendMode();
        renderer_->endColorOnlyWorldEffects();
        renderer_->endWorldShader();
    }
    struct IceImpactDraw {
        const PresentationEffect* effect{};
        float distanceSquared{};
        bool detailed{};
    };
    std::array<IceImpactDraw, 48> iceImpacts{};
    std::size_t iceCount = 0;
    for (auto it = effects_.rbegin(); it != effects_.rend(); ++it) {
        if (it->startDelayRemaining > 0 || it->duration <= 0 ||
            (it->type != PresentationEffectType::IceImpact &&
             it->type != PresentationEffectType::IceCrack)) continue;
        const Vector3 origin{static_cast<float>(it->position.x),
                             static_cast<float>(it->position.y),
                             static_cast<float>(it->position.z)};
        const Vector3 delta = Vector3Subtract(origin, camera.position);
        const float distance = Vector3LengthSqr(delta);
        if (distance > 1600 || Vector3DotProduct(delta, cameraForward) < -it->scale * 3) continue;
        if (iceCount == iceImpacts.size()) break;
        iceImpacts[iceCount] = {&*it, distance, renderer_->settings().particles && iceCount < 16U};
        ++iceCount;
    }
    if (iceCount > 0) {
        std::sort(iceImpacts.begin(), iceImpacts.begin() + static_cast<std::ptrdiff_t>(iceCount),
            [](const IceImpactDraw& a, const IceImpactDraw& b) {
                return a.distanceSquared > b.distanceSquared;
            });
        renderer_->beginWorldShader(lighting);
        WorldMaterialState iceMaterial{};
        iceMaterial.unlitAmount = 1;
        iceMaterial.screenAoAmount = 0;
        iceMaterial.inkOutlineEligible = false;
        iceMaterial.inkOccluder = false;
        renderer_->setWorldMaterial(iceMaterial);
        renderer_->beginColorOnlyWorldEffects();
        for (const auto pass : {app_freeze_drawing::Pass::Ice,
                                app_freeze_drawing::Pass::Atmosphere}) {
            app_freeze_drawing::beginBatch();
            for (std::size_t index = 0; index < iceCount; ++index) {
                const auto& draw = iceImpacts[index];
                const auto& effect = *draw.effect;
                const Vector3 origin{static_cast<float>(effect.position.x),
                    static_cast<float>(effect.position.y), static_cast<float>(effect.position.z)};
                const auto seed = effect.entityId ? effect.entityId->index :
                    static_cast<std::uint32_t>(effect.variant);
                app_freeze_drawing::emitImpact(origin, cameraRight, billboardUp,
                    static_cast<float>(1 - effect.remaining / effect.duration), effect.scale,
                    seed, effect.type == PresentationEffectType::IceCrack, draw.detailed, pass);
            }
            app_freeze_drawing::endBatch();
        }
        renderer_->endColorOnlyWorldEffects();
        renderer_->endWorldShader();
    }
    for (const auto& effect : effects_) {
        // Frozen impacts and burning use their own batches, not generic debris.
        if (effect.startDelayRemaining > 0.0 ||
            effect.type == PresentationEffectType::EnemyBurn ||
            effect.type == PresentationEffectType::IceImpact ||
            effect.type == PresentationEffectType::IceCrack ||
            effect.type == PresentationEffectType::FireImpact) {
            continue;
        }
        const float progress =
            static_cast<float>(
                1.0 - effect.remaining / effect.duration);
        const Vector3 origin{
            static_cast<float>(effect.position.x),
            static_cast<float>(effect.position.y),
            static_cast<float>(effect.position.z),
        };
        if (effect.type == PresentationEffectType::Hit ||
            effect.type == PresentationEffectType::EnemyHitImpact ||
            effect.type ==
                PresentationEffectType::ResourceImpactDecal ||
            effect.type ==
                PresentationEffectType::ResourceFragment ||
            effect.type == PresentationEffectType::ResourceHit) {
            continue;
        }
        if (effect.type == PresentationEffectType::RepairShockwave) {
            renderer_->drawRepairShockwave(
                origin, camera.position, progress, effect.scale);
            continue;
        }
        if (effect.type == PresentationEffectType::EliteSpawn ||
            effect.type == PresentationEffectType::VolatileCharge) {
            const bool volatileCharge = effect.type ==
                PresentationEffectType::VolatileCharge;
            const float fade = volatileCharge
                ? smoothstep(0.0F, 0.16F, progress)
                : 1.0F - smoothstep(0.48F, 1.0F, progress);
            const float acceleratingPulse = 0.5F + 0.5F * std::sin(
                progress * progress *
                    (volatileCharge ? 58.0F : 24.0F));
            const Color accent = volatileCharge
                ? Color{255, 120, 38, 255}
                : Color{255, 207, 82, 255};
            rlDrawRenderBatchActive();
            BeginBlendMode(BLEND_ADDITIVE);
            rlDisableDepthMask();
            if (volatileCharge) {
                const float radius =
                    0.42F + progress * 0.72F +
                    acceleratingPulse * 0.08F;
                DrawCircle3D(
                    {origin.x, origin.y + 0.04F, origin.z},
                    radius, {1.0F, 0.0F, 0.0F}, 90.0F,
                    {accent.r, accent.g, accent.b,
                     atmosphereAlpha(fade * 0.72F)});
                DrawCircle3D(
                    {origin.x, origin.y + 0.055F, origin.z},
                    radius * 0.68F,
                    {1.0F, 0.0F, 0.0F}, 90.0F,
                    {255, 220, 116,
                     atmosphereAlpha(fade * 0.55F)});
                DrawSphere(
                    {origin.x, origin.y + 0.38F, origin.z},
                    0.10F + progress * 0.20F +
                        acceleratingPulse * 0.055F,
                    {255, 174, 70,
                     atmosphereAlpha(fade * 0.58F)});
                constexpr int WarningRays = 8;
                for (int ray = 0; ray < WarningRays; ++ray) {
                    const float angle =
                        static_cast<float>(ray) * 2.0F * PI /
                            static_cast<float>(WarningRays) +
                        progress * 1.7F;
                    const float inner = radius * 0.76F;
                    const float outer = radius *
                        (0.98F + acceleratingPulse * 0.16F);
                    DrawLine3D(
                        {origin.x + std::cos(angle) * inner,
                         origin.y + 0.05F,
                         origin.z + std::sin(angle) * inner},
                        {origin.x + std::cos(angle) * outer,
                         origin.y + 0.05F,
                         origin.z + std::sin(angle) * outer},
                        {255, 211, 107,
                         atmosphereAlpha(fade * 0.82F)});
                }
            } else {
                const float shock = 1.0F -
                    smoothstep(0.0F, 0.72F, progress);
                DrawCircle3D(
                    {origin.x, origin.y + 0.04F, origin.z},
                    0.20F + progress * 1.45F,
                    {1.0F, 0.0F, 0.0F}, 90.0F,
                    {accent.r, accent.g, accent.b,
                     atmosphereAlpha(fade * shock)});
                DrawSphere(
                    {origin.x, origin.y + 0.72F, origin.z},
                    0.10F + shock * 0.24F,
                    {255, 239, 177,
                     atmosphereAlpha(fade * 0.62F)});
                constexpr int ShardCount = 10;
                for (int shard = 0; shard < ShardCount; ++shard) {
                    const float angle = effectUnit(shard, 311) *
                        2.0F * PI;
                    const float distance = progress *
                        (0.45F + effectUnit(shard, 312) * 0.95F);
                    DrawSphereEx(
                        {origin.x + std::cos(angle) * distance,
                         origin.y + 0.18F +
                             effectUnit(shard, 313) * 1.15F,
                         origin.z + std::sin(angle) * distance},
                        0.025F + effectUnit(shard, 314) * 0.045F,
                        4, 4,
                        {accent.r, accent.g, accent.b,
                         atmosphereAlpha(fade * 0.78F)});
                }
            }
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
            EndBlendMode();
            continue;
        }
        if (effect.type ==
                PresentationEffectType::ChainLightning &&
            effect.targetPosition) {
            const Vector3 target{
                static_cast<float>(effect.targetPosition->x),
                static_cast<float>(effect.targetPosition->y),
                static_cast<float>(effect.targetPosition->z),
            };
            const float appear = smoothstep(0.0F, 0.08F, progress);
            const float fade = 1.0F -
                smoothstep(0.48F, 1.0F, progress);
            const float pulse = appear * fade;
            const int seed = effect.variant * 37 +
                (effect.entityId
                     ? static_cast<int>(effect.entityId->index % 997U)
                     : 0);
            rlDrawRenderBatchActive();
            BeginBlendMode(BLEND_ADDITIVE);
            rlDisableDepthMask();
            // The arc often runs through a dense pack. It is a very short
            // gameplay cue, so render its emissive layers over silhouettes
            // instead of letting several enemy meshes hide it completely.
            rlDisableDepthTest();
            drawLightningArc(
                origin, target, progress, seed, 1.0F, pulse);
            drawLightningArc(
                origin, target, progress, seed + 17,
                0.52F, pulse * 0.48F);
            drawLightningArc(
                origin, target, progress, seed + 41,
                0.34F, pulse * 0.30F);

            const float flash = pulse *
                (0.72F + 0.28F * std::sin(progress * 48.0F));
            DrawSphere(
                origin, 0.26F + flash * 0.12F,
                {45, 132, 255,
                 atmosphereAlpha(flash * 0.22F)});
            DrawSphere(
                origin, 0.075F + flash * 0.035F,
                {231, 252, 255,
                 atmosphereAlpha(flash * 0.92F)});
            DrawSphere(
                target, 0.52F + flash * 0.22F,
                {45, 132, 255,
                 atmosphereAlpha(flash * 0.26F)});
            DrawSphere(
                target, 0.135F + flash * 0.065F,
                {231, 252, 255,
                 atmosphereAlpha(flash * 0.96F)});
            for (int spark = 0; spark < 7; ++spark) {
                const float angle = effectUnit(
                    seed + spark * 13, 509) * 2.0F * PI;
                const float lift =
                    effectUnit(seed + spark * 17, 521) * 2.0F - 0.65F;
                Vector3 sparkDirection{
                    std::cos(angle), lift, std::sin(angle)};
                sparkDirection = Vector3Normalize(sparkDirection);
                const float sparkLength =
                    0.34F + effectUnit(seed + spark * 19, 523) * 0.48F;
                const Vector3 sparkEnd = Vector3Add(
                    target, Vector3Scale(
                        sparkDirection, sparkLength * flash));
                DrawCylinderEx(
                    target, sparkEnd, 0.018F, 0.004F, 4,
                    {139, 224, 255,
                     atmosphereAlpha(flash * 0.78F)});
                DrawSphere(
                    sparkEnd, 0.025F,
                    {230, 253, 255,
                     atmosphereAlpha(flash * 0.72F)});
            }
            const Vector3 ringCenter{
                target.x, target.y + 0.015F, target.z};
            DrawCircle3D(
                ringCenter, 0.18F + progress * 0.75F,
                {1.0F, 0.0F, 0.0F}, 90.0F,
                {92, 207, 255,
                 atmosphereAlpha(pulse * 0.72F)});
            rlDrawRenderBatchActive();
            rlEnableDepthTest();
            rlEnableDepthMask();
            EndBlendMode();
            continue;
        }
        if (effect.type ==
            PresentationEffectType::LootCollected) {
            const LootRarity rarity = effect.lootRarity.value_or(
                LootRarity::Common);
            const Color rarityColor = lootRarityColor(rarity);
            if (!renderer_->settings().particles) {
                continue;
            }

            const float burstFade = 1.0F -
                smoothstep(0.42F, 1.0F, progress);
            BeginBlendMode(BLEND_ADDITIVE);

            constexpr int ParticleCount = 24;
            const int seedOffset = effect.entityId
                ? static_cast<int>(effect.entityId->index % 97U)
                : 0;
            for (int index = 0; index < ParticleCount; ++index) {
                const int seed = index + seedOffset * 19;
                const float phase = effectUnit(seed, 41) * 2.0F * PI;
                const float angle = phase +
                    progress * (1.7F + effectUnit(seed, 42) * 1.8F);
                // Start every shard at the item center and send it through a
                // spherical burst. Elevation is signed, so particles spread
                // sideways, upward and downward instead of forming a rising
                // column above the chest.
                const float elevation =
                    (effectUnit(seed, 44) * 2.0F - 1.0F) * 0.85F;
                const float travel = effect.scale * progress *
                    (0.72F + effectUnit(seed, 43) * 1.05F);
                const float horizontal =
                    travel * std::cos(elevation);
                const Vector3 direction{
                    std::cos(angle) * std::cos(elevation),
                    std::sin(elevation),
                    std::sin(angle) * std::cos(elevation),
                };
                const Vector3 particle{
                    origin.x + std::cos(angle) * horizontal,
                    origin.y + travel * std::sin(elevation),
                    origin.z + std::sin(angle) * horizontal,
                };
                const float size = effect.scale * (
                    0.025F + effectUnit(seed, 45) * 0.045F) *
                    (0.68F + burstFade * 0.72F);
                const float alpha = burstFade *
                    (0.58F + effectUnit(seed, 46) * 0.42F);
                const Color particleColor = index % 5 == 0
                    ? Fade(WHITE, alpha)
                    : Fade(rarityColor, alpha);
                DrawSphereEx(particle, size, 4, 4, particleColor);
                const Vector3 tail{
                    particle.x - direction.x * size * 3.8F,
                    particle.y - direction.y * size * 3.8F,
                    particle.z - direction.z * size * 3.8F,
                };
                DrawLine3D(tail, particle,
                           Fade(particleColor, alpha * 0.72F));
            }
            DrawSphereEx(
                origin,
                effect.scale * (0.10F +
                    (1.0F - smoothstep(0.05F, 0.38F, progress)) * 0.16F),
                6, 6, Fade(WHITE, burstFade * 0.78F));
            EndBlendMode();
            continue;
        }
        if (effect.type == PresentationEffectType::SawSplinter &&
            effect.targetPosition) {
            const Vector3 target{
                static_cast<float>(effect.targetPosition->x),
                static_cast<float>(effect.targetPosition->y),
                static_cast<float>(effect.targetPosition->z),
            };
            const float eased = progress * progress *
                (3.0F - 2.0F * progress);
            Vector3 center = Vector3Lerp(origin, target, eased);
            center.y += std::sin(progress * PI) * 0.95F;
            const Vector3 direction = Vector3Normalize(
                Vector3Subtract(target, origin));
            const float fade = 1.0F -
                smoothstep(0.86F, 1.0F, progress);
            const float spinPhase = effectUnit(
                effect.variant * 7, 417) * 360.0F;
            renderer_->drawSawBladeProjectile(
                center, direction,
                spinPhase + progress * 1440.0F,
                1.0F, Fade(WHITE, fade));
            if (renderer_->settings().particles) {
                rlDrawRenderBatchActive();
                BeginBlendMode(BLEND_ADDITIVE);
                rlDisableDepthMask();
                for (int mote = 0; mote < 5; ++mote) {
                    const float delay = static_cast<float>(mote) * 0.035F;
                    const float trailProgress = std::clamp(
                        eased - delay, 0.0F, 1.0F);
                    Vector3 motePosition = Vector3Lerp(
                        origin, target, trailProgress);
                    motePosition.y +=
                        std::sin(trailProgress * PI) * 0.95F;
                    DrawSphere(
                        motePosition, 0.025F,
                        {205, 226, 238,
                         atmosphereAlpha(fade * 0.55F)});
                }
                rlDrawRenderBatchActive();
                rlEnableDepthMask();
                EndBlendMode();
            }
            continue;
        }
        if (effect.type ==
            PresentationEffectType::BuildingUpgrade) {
            renderer_->drawUpgradeEffect(
                origin, progress, effect.scale);
            continue;
        }
        if (effect.type ==
            PresentationEffectType::BuildingPlaced) {
            const float fade =
                1.0F -
                smoothstep(0.18F, 0.78F, progress);
            const auto alpha = static_cast<unsigned char>(
                std::lround(fade * 235.0F));
            const float ringRadius =
                effect.scale *
                (0.24F + progress * 1.85F);
            constexpr float RingHeight = 0.05F;
            DrawCircle3D(
                {origin.x, origin.y + RingHeight, origin.z},
                ringRadius, {1.0F, 0.0F, 0.0F}, 90.0F,
                {255, 188, 62, alpha});
            DrawCircle3D(
                {origin.x,
                 origin.y + RingHeight + 0.012F,
                 origin.z},
                ringRadius * 0.72F,
                {1.0F, 0.0F, 0.0F}, 90.0F,
                {255, 236, 155, alpha});

            constexpr int SparkCount = 10;
            for (int index = 0; index < SparkCount; ++index) {
                const float baseAngle =
                    static_cast<float>(index) *
                    (2.0F * PI /
                     static_cast<float>(SparkCount));
                const float distance =
                    effect.scale *
                    (0.18F + progress * 1.45F);
                const Vector3 center{
                    origin.x +
                        std::cos(baseAngle) * distance,
                    origin.y +
                        0.22F +
                        std::sin(progress * PI) *
                            0.72F * effect.scale,
                    origin.z +
                        std::sin(baseAngle) * distance,
                };
                const float starSize =
                    effect.scale *
                    (0.055F + fade * 0.075F);
                DrawSphere(
                    center, starSize * 0.48F,
                    {255, 224, 126, alpha});
                DrawLine3D(
                    {center.x - starSize, center.y, center.z},
                    {center.x + starSize, center.y, center.z},
                    {255, 244, 205, alpha});
                DrawLine3D(
                    {center.x, center.y - starSize, center.z},
                    {center.x, center.y + starSize, center.z},
                    {255, 244, 205, alpha});
            }
            continue;
        }
        if (effect.type ==
            PresentationEffectType::BuildingRepaired) {
            const float fade =
                1.0F -
                smoothstep(0.48F, 1.0F, progress);
            const auto alpha = static_cast<unsigned char>(
                std::lround(fade * 225.0F));
            const float ringRadius =
                effect.scale *
                (0.28F + progress * 1.28F);
            DrawCircle3D(
                {origin.x, 0.08F + progress * 1.65F,
                 origin.z},
                ringRadius, {1.0F, 0.0F, 0.0F}, 90.0F,
                {96, 255, 142, alpha});
            BeginBlendMode(BLEND_ADDITIVE);
            constexpr int ParticleCount = 14;
            for (int index = 0;
                 index < ParticleCount; ++index) {
                const float angle =
                    static_cast<float>(index) *
                        (2.0F * PI /
                         static_cast<float>(ParticleCount)) +
                    progress * 1.8F;
                const float distance =
                    effect.scale *
                    (1.35F - progress * 1.12F);
                const Vector3 particle{
                    origin.x + std::cos(angle) * distance,
                    origin.y + 0.18F +
                        progress * 1.5F +
                        std::sin(
                            angle * 2.0F +
                            progress * PI) *
                            0.16F,
                    origin.z + std::sin(angle) * distance,
                };
                DrawSphere(
                    particle,
                    effect.scale *
                        (0.035F + fade * 0.045F),
                    {106, 255, 158, alpha});
            }
            EndBlendMode();
            continue;
        }
        if (!renderer_->settings().particles) {
            continue;
        }
        if (effect.type == PresentationEffectType::EnemyHitImpact) {
            const float fade =
                1.0F - smoothstep(0.42F, 1.0F, progress);
            const float flash =
                1.0F - smoothstep(0.0F, 0.24F, progress);
            const bool critical = (effect.variant & 8) != 0;
            const int impactStyle = effect.variant & 7;
            Color core{255, 226, 142, 255};
            Color edge{235, 143, 62, 255};
            if (impactStyle == 1) {
                core = {215, 250, 255, 255};
                edge = {86, 199, 255, 255};
            } else if (impactStyle == 2) {
                core = {255, 239, 154, 255};
                edge = {255, 92, 35, 255};
            } else if (impactStyle == 3) {
                core = {242, 224, 255, 255};
                edge = {151, 102, 255, 255};
            } else if (impactStyle == 4) {
                core = {255, 244, 207, 255};
                edge = {203, 176, 119, 255};
            }

            Vector3 source{
                origin.x, origin.y - 0.1F, origin.z - 1.0F};
            if (effect.targetPosition) {
                source = {
                    static_cast<float>(effect.targetPosition->x),
                    static_cast<float>(effect.targetPosition->y),
                    static_cast<float>(effect.targetPosition->z),
                };
            }
            Vector3 forward = Vector3Subtract(origin, source);
            forward.y *= 0.28F;
            if (Vector3LengthSqr(forward) < 0.0001F) {
                forward = {0.0F, 0.12F, -1.0F};
            }
            forward = Vector3Normalize(forward);
            Vector3 side = Vector3CrossProduct(
                {0.0F, 1.0F, 0.0F}, forward);
            if (Vector3LengthSqr(side) < 0.0001F) {
                side = {1.0F, 0.0F, 0.0F};
            } else {
                side = Vector3Normalize(side);
            }

            const Vector3 visibleOrigin = Vector3Add(
                origin, Vector3Scale(
                    forward, -0.06F));
            const int shardCount = critical ? 14 : 10;
            for (int index = 0; index < shardCount; ++index) {
                const float lateral =
                    effectUnit(index, 91) * 2.0F - 1.0F;
                Vector3 shardDirection = Vector3Add(
                    Vector3Scale(side, lateral * 0.92F),
                    Vector3Scale(forward,
                        -0.04F - effectUnit(index, 96) * 0.06F));
                shardDirection.y =
                    0.24F + effectUnit(index, 93) * 0.68F;
                shardDirection = Vector3Normalize(shardDirection);
                const float speed = effect.scale *
                    (0.34F + effectUnit(index, 92) * 0.62F);
                Vector3 position = Vector3Add(
                    visibleOrigin,
                    Vector3Scale(shardDirection, progress * speed));
                position.y += std::sin(progress * PI) *
                        (0.08F + effectUnit(index, 94) * 0.18F) -
                    progress * progress * 0.16F;
                const float size = effect.scale *
                    (0.052F + effectUnit(index, 95) * 0.072F) *
                    (1.0F - progress * 0.42F);
                Color shardColor = index % 3 == 0 ? core : edge;
                shardColor.a = atmosphereAlpha(fade * 0.94F);
                DrawSphereEx(
                    position, size, 4, 3, shardColor);
            }

            BeginBlendMode(BLEND_ADDITIVE);
            const int streakCount = critical ? 5 : 3;
            for (int index = 0; index < streakCount; ++index) {
                const float lateral =
                    (effectUnit(index, 101) * 2.0F - 1.0F) * 0.82F;
                Vector3 streakDirection = Vector3Add(
                    Vector3Scale(forward, -0.05F),
                    Vector3Scale(side, lateral));
                streakDirection.y =
                    0.18F + effectUnit(index, 102) * 0.42F;
                streakDirection = Vector3Normalize(streakDirection);
                const float length = effect.scale *
                    (0.24F + effectUnit(index, 103) * 0.34F) *
                    (0.45F + progress);
                const Vector3 start = Vector3Add(
                    visibleOrigin,
                    Vector3Scale(streakDirection, progress * 0.28F));
                const Vector3 end = Vector3Add(
                    start, Vector3Scale(streakDirection, length));
                DrawLine3D(
                    start, end,
                    {core.r, core.g, core.b,
                     atmosphereAlpha(flash * 0.9F)});
            }
            DrawSphereEx(
                visibleOrigin, effect.scale * 0.24F * flash,
                6, 4,
                {core.r, core.g, core.b,
                 atmosphereAlpha(flash * 0.82F)});
            EndBlendMode();
        } else if (effect.type == PresentationEffectType::Hit) {
            const float flash = 1.0F - progress;
            BeginBlendMode(BLEND_ADDITIVE);
            DrawSphereEx(origin, effect.scale * 0.30F * flash,
                         7, 5,
                         {255, 133, 30,
                          atmosphereAlpha(flash * 0.72F)});
            DrawSphereEx(origin, effect.scale * 0.13F * flash,
                         6, 4,
                         {255, 244, 179,
                          atmosphereAlpha(flash)});
            constexpr int ImpactRayCount = 4;
            for (int index = 0; index < ImpactRayCount; ++index) {
                const float angle =
                    effectUnit(index, 211) * 2.0F * PI;
                Vector3 direction{
                    std::cos(angle),
                    0.20F + effectUnit(index, 212) * 0.55F,
                    std::sin(angle),
                };
                direction = Vector3Normalize(direction);
                const Vector3 end = Vector3Add(
                    origin, Vector3Scale(direction,
                        effect.scale * (0.18F + progress * 0.38F)));
                DrawLine3D(origin, end,
                           {255, 208, 106,
                            atmosphereAlpha(flash * 0.85F)});
            }
            EndBlendMode();
        } else if (effect.type == PresentationEffectType::PlayerArrivalDust) {
            // Rounded cloud clusters, with no ring or small debris. Their
            // alpha suppresses rear-world ink exactly like repair symbols.
            struct Puff {
                Vector3 center;
                float radius;
                float opacity;
                float distanceSquared;
            };
            constexpr int PuffCount = 14;
            std::array<Puff, PuffCount> puffs{};
            std::size_t count = 0;
            for (int index = 0; index < PuffCount; ++index) {
                const float age = std::clamp(
                    (progress - effectUnit(index, 301) * 0.06F) / 0.94F,
                    0.0F, 1.0F);
                if (age <= 0.0F || age >= 1.0F) continue;
                const float pop = smoothstep(0.0F, 0.12F, age);
                const float fade = 1.0F - smoothstep(0.30F, 1.0F, age);
                const float travel = 1.0F - std::pow(1.0F - age, 2.0F);
                const float angle = static_cast<float>(index) * 2.0F * PI / static_cast<float>(PuffCount) +
                    (effectUnit(index, 302) - 0.5F) * 0.35F;
                const float distance = effect.scale *
                    (0.48F + travel * (1.45F + effectUnit(index, 303) * 0.65F));
                const float radius = effect.scale *
                    (0.14F + effectUnit(index, 304) * 0.10F) *
                    (0.55F + pop * 0.45F + travel * 0.48F);
                Vector3 center{
                    origin.x + std::cos(angle) * distance,
                    origin.y,
                    origin.z + std::sin(angle) * distance,
                };
                center.y = static_cast<float>(simulation_.terrain().getHeight(
                    center.x, center.z)) + radius * 0.72F +
                    travel * (0.20F + effectUnit(index, 305) * 0.30F);
                puffs[count++] = {center, radius, pop * fade * 0.86F,
                    Vector3DistanceSqr(center, camera.position)};
            }
            std::sort(puffs.begin(), puffs.begin() + static_cast<std::ptrdiff_t>(count),
                [](const Puff& left, const Puff& right) {
                    return left.distanceSquared > right.distanceSquared;
                });
            renderer_->beginColorOnlyWorldEffects();
            rlDrawRenderBatchActive();
            rlSetBlendFactorsSeparate(
                RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
                RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            rlDisableDepthMask();
            for (std::size_t index = 0; index < count; ++index) {
                const Puff& puff = puffs[index];
                const auto alpha = atmosphereAlpha(puff.opacity);
                DrawSphereEx(puff.center, puff.radius, 6, 6,
                             {218, 206, 174, alpha});
                DrawSphereEx(Vector3Add(puff.center,
                    Vector3Scale(cameraRight, -puff.radius * 0.60F)),
                    puff.radius * 0.68F, 6, 6, {230, 219, 190, alpha});
                DrawSphereEx(Vector3Add(puff.center,
                    Vector3Scale(billboardUp, puff.radius * 0.48F)),
                    puff.radius * 0.70F, 6, 6, {240, 232, 206, alpha});
            }
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
            EndBlendMode();
            renderer_->endColorOnlyWorldEffects();
        } else if (effect.type == PresentationEffectType::LandingDust) {
            const float fade =
                1.0F - smoothstep(0.38F, 1.0F, progress);
            const float ringRadius =
                effect.scale * (0.18F + progress * 1.65F);
            DrawCircle3D(
                {origin.x, origin.y + 0.018F, origin.z},
                ringRadius, {1.0F, 0.0F, 0.0F}, 90.0F,
                {174, 147, 101,
                 atmosphereAlpha(fade * 0.48F)});
            constexpr int ParticleCount = 14;
            for (int index = 0; index < ParticleCount; ++index) {
                const float angle =
                    effectUnit(index, 31) * 2.0F * PI;
                const float speed = effect.scale *
                    (0.65F + effectUnit(index, 32) * 1.25F);
                const float distance =
                    0.12F + progress * speed;
                const float lift =
                    std::sin(progress * PI) *
                    (0.06F + effectUnit(index, 33) * 0.28F);
                const float size = effect.scale *
                    (0.035F + effectUnit(index, 34) * 0.075F +
                     progress * 0.055F);
                DrawSphereEx(
                    {origin.x + std::cos(angle) * distance,
                     origin.y + lift,
                     origin.z + std::sin(angle) * distance},
                    size, 4, 4,
                    {151, 125, 84,
                     atmosphereAlpha(fade * 0.72F)});
            }
        } else if (effect.type == PresentationEffectType::SplitBurst) {
            const float fade =
                1.0F - smoothstep(0.48F, 1.0F, progress);
            const float flash =
                1.0F - smoothstep(0.0F, 0.22F, progress);
            const float ringRadius = effect.scale *
                (0.22F + progress * 2.15F);
            BeginBlendMode(BLEND_ADDITIVE);
            for (int ring = 0; ring < 3; ++ring) {
                DrawCircle3D(
                    {origin.x,
                     origin.y + 0.08F +
                         static_cast<float>(ring) * 0.035F,
                     origin.z},
                    ringRadius - static_cast<float>(ring) * 0.07F,
                    {1.0F, 0.0F, 0.0F}, 90.0F,
                    {121, 255, 116,
                     atmosphereAlpha(
                         fade * (0.72F -
                             static_cast<float>(ring) * 0.16F))});
            }
            DrawSphereEx(
                {origin.x, origin.y + 0.62F, origin.z},
                effect.scale * (0.18F + flash * 0.42F),
                8, 8,
                {178, 255, 142,
                 atmosphereAlpha(flash * 0.78F)});
            EndBlendMode();

            constexpr int GlobCount = 18;
            for (int index = 0; index < GlobCount; ++index) {
                const float angle =
                    effectUnit(index, 91) * 2.0F * PI;
                const float speed = effect.scale *
                    (0.8F + effectUnit(index, 92) * 2.35F);
                const float distance =
                    0.12F + progress * speed;
                const float arc =
                    std::sin(progress * PI) *
                    (0.35F + effectUnit(index, 93) * 1.25F);
                const float size = effect.scale *
                    (0.035F + effectUnit(index, 94) * 0.095F) *
                    (1.0F - progress * 0.48F);
                DrawSphereEx(
                    {origin.x + std::cos(angle) * distance,
                     origin.y + 0.22F + arc,
                     origin.z + std::sin(angle) * distance},
                    size, 5, 5,
                    {static_cast<unsigned char>(72 +
                         effectUnit(index, 95) * 54.0F),
                     static_cast<unsigned char>(174 +
                         effectUnit(index, 96) * 62.0F),
                     73,
                     atmosphereAlpha(fade * 0.92F)});
            }
        } else if (effect.type == PresentationEffectType::RamImpact) {
            const float fade = 1.0F - smoothstep(0.35F, 1.0F, progress);
            const float impactScale = std::max(0.1F, effect.scale);
            const Vector3 ground{
                origin.x, origin.y + 0.035F, origin.z};
            DrawCircle3D(
                ground, (0.35F + progress * 3.0F) * impactScale,
                {1.0F, 0.0F, 0.0F}, 90.0F,
                {255, 72, 45, atmosphereAlpha(fade * 0.88F)});
            DrawCircle3D(
                {ground.x, ground.y + 0.008F, ground.z},
                (0.18F + progress * 1.85F) * impactScale,
                {1.0F, 0.0F, 0.0F}, 90.0F,
                {255, 177, 56, atmosphereAlpha(fade)});
            constexpr int RayCount = 10;
            for (int ray = 0; ray < RayCount; ++ray) {
                const float angle =
                    static_cast<float>(ray) * 2.0F * PI /
                    static_cast<float>(RayCount);
                const float inner =
                    (0.28F + progress * 0.55F) * impactScale;
                const float outer =
                    (0.55F + progress *
                        (1.65F + effectUnit(ray, 141) * 1.1F)) *
                    impactScale;
                DrawLine3D(
                    {ground.x + std::cos(angle) * inner,
                     ground.y,
                     ground.z + std::sin(angle) * inner},
                    {ground.x + std::cos(angle) * outer,
                     ground.y + progress * 0.12F,
                     ground.z + std::sin(angle) * outer},
                    {255, 117, 42,
                     atmosphereAlpha(fade * 0.82F)});
            }
        } else {
            const Color color{125, 112, 101, 255};
            for (int particle = 0; particle < 6; ++particle) {
                const float angle =
                    static_cast<float>(particle) * 1.04719755F;
                const float distance = progress * 1.4F;
                const Vector3 particlePosition{
                    origin.x + std::cos(angle) * distance,
                    origin.y + 0.25F +
                        progress * (1.0F - progress) * 2.0F,
                    origin.z + std::sin(angle) * distance,
                };
                DrawCube(particlePosition, 0.12F, 0.12F, 0.12F,
                         color);
            }
        }
    }
}

} // namespace ian
