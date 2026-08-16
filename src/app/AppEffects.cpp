#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "ui/UiText.hpp"
#include "ui/WorldBillboard.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>

namespace ian {

using namespace app_detail;

namespace {

float atmosphereUnit(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return static_cast<float>(value & 0xffffU) / 65535.0F;
}

float wrapAtmosphere(float value, float center, float halfExtent) {
    const float extent = halfExtent * 2.0F;
    float relative = std::fmod(
        value - center + halfExtent, extent);
    if (relative < 0.0F) {
        relative += extent;
    }
    return center + relative - halfExtent;
}

unsigned char atmosphereAlpha(float amount) {
    return static_cast<unsigned char>(std::lround(
        std::clamp(amount, 0.0F, 1.0F) * 255.0F));
}

float effectUnit(int index, int channel) {
    return atmosphereUnit(
        0x9e3779b9U * static_cast<std::uint32_t>(index + 1) +
        0x85ebca6bU * static_cast<std::uint32_t>(channel + 3));
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

} // namespace

void App::drawAtmosphereParticles(
    const Camera3D& camera, float nightAmount) {
    if (!renderer_->settings().particles) {
        return;
    }

    const float time = static_cast<float>(GetTime());
    const float daylight =
        1.0F - std::clamp(nightAmount, 0.0F, 1.0F) * 0.72F;

    // Rare low-poly leaves. Positions loop around the camera, so the effect
    // has fixed cost and never allocates or fills the world with entities.
    constexpr int LeafCount = 11;
    constexpr float LeafHalfExtent = 25.0F;
    for (int index = 0; index < LeafCount; ++index) {
        const std::uint32_t seed =
            0x6d2b79f5U + static_cast<std::uint32_t>(index) *
                0x9e3779b9U;
        const float speed = 2.1F + atmosphereUnit(seed + 1U) * 1.4F;
        float x = wrapAtmosphere(
            (atmosphereUnit(seed + 2U) * 2.0F - 1.0F) *
                    LeafHalfExtent +
                time * speed,
            camera.position.x, LeafHalfExtent);
        float z = wrapAtmosphere(
            (atmosphereUnit(seed + 3U) * 2.0F - 1.0F) *
                    LeafHalfExtent +
                time * speed * 0.24F,
            camera.position.z, LeafHalfExtent);
        x += std::sin(time * 1.35F + atmosphereUnit(seed + 4U) * 17.0F) *
            0.55F;
        z += std::cos(time * 1.08F + atmosphereUnit(seed + 5U) * 19.0F) *
            0.35F;

        const float cycle = std::fmod(
            atmosphereUnit(seed + 6U) +
                time * (0.035F + atmosphereUnit(seed + 7U) * 0.022F),
            1.0F);
        const float ground = static_cast<float>(
            simulation_.terrain().getHeight(x, z));
        const float y = ground + 0.65F + (1.0F - cycle) * 5.2F +
            std::sin(time * 2.0F + atmosphereUnit(seed + 8U) * 13.0F) *
                0.22F;
        const float distance = Vector2Distance(
            {x, z}, {camera.position.x, camera.position.z});
        const float edgeFade = 1.0F -
            smoothstep(18.0F, LeafHalfExtent, distance);
        const float cycleFade =
            smoothstep(0.0F, 0.08F, cycle) *
            (1.0F - smoothstep(0.88F, 1.0F, cycle));
        const float alpha = edgeFade * cycleFade * daylight * 0.88F;
        if (alpha <= 0.01F) {
            continue;
        }

        const Color palette[4] = {
            {210, 143, 47, atmosphereAlpha(alpha)},
            {229, 184, 67, atmosphereAlpha(alpha)},
            {159, 174, 55, atmosphereAlpha(alpha)},
            {191, 104, 39, atmosphereAlpha(alpha)},
        };
        const float size = 0.72F + atmosphereUnit(seed + 9U) * 0.55F;
        const Vector3 axis = Vector3Normalize({
            0.35F + atmosphereUnit(seed + 10U),
            0.55F + atmosphereUnit(seed + 11U),
            0.25F + atmosphereUnit(seed + 12U),
        });
        const float rotation =
            time * (95.0F + atmosphereUnit(seed + 13U) * 120.0F) +
            atmosphereUnit(seed + 14U) * 360.0F;
        rlPushMatrix();
        rlTranslatef(x, y, z);
        rlRotatef(rotation, axis.x, axis.y, axis.z);
        rlScalef(0.18F * size, 0.025F * size, 0.10F * size);
        DrawCube(
            {}, 1.0F, 1.0F, 1.0F,
            palette[static_cast<std::size_t>(index) % 4U]);
        rlPopMatrix();
    }

    // Small pollen motes are additive and deliberately sparse. Low-segment
    // spheres keep them readable after pixelization without expensive meshes.
    constexpr int PollenCount = 24;
    constexpr float PollenHalfExtent = 19.0F;
    BeginBlendMode(BLEND_ADDITIVE);
    for (int index = 0; index < PollenCount; ++index) {
        const std::uint32_t seed =
            0xa511e9b3U + static_cast<std::uint32_t>(index) *
                0x85ebca6bU;
        const float x = wrapAtmosphere(
            (atmosphereUnit(seed + 1U) * 2.0F - 1.0F) *
                    PollenHalfExtent +
                time * (0.28F + atmosphereUnit(seed + 2U) * 0.22F),
            camera.position.x, PollenHalfExtent);
        const float z = wrapAtmosphere(
            (atmosphereUnit(seed + 3U) * 2.0F - 1.0F) *
                    PollenHalfExtent +
                time * 0.12F,
            camera.position.z, PollenHalfExtent);
        const float rise = std::fmod(
            atmosphereUnit(seed + 4U) +
                time * (0.022F + atmosphereUnit(seed + 5U) * 0.018F),
            1.0F);
        const float ground = static_cast<float>(
            simulation_.terrain().getHeight(x, z));
        const float y = ground + 0.35F + rise * 4.2F +
            std::sin(time * 0.8F + atmosphereUnit(seed + 6U) * 21.0F) *
                0.18F;
        const float distance = Vector2Distance(
            {x, z}, {camera.position.x, camera.position.z});
        const float fade =
            (1.0F - smoothstep(13.0F, PollenHalfExtent, distance)) *
            smoothstep(0.0F, 0.12F, rise) *
            (1.0F - smoothstep(0.82F, 1.0F, rise));
        const float alpha = fade * daylight *
            (0.32F + atmosphereUnit(seed + 7U) * 0.28F);
        if (alpha <= 0.01F) {
            continue;
        }
        const float radius =
            0.025F + atmosphereUnit(seed + 8U) * 0.025F;
        DrawSphereEx(
            {x, y, z}, radius, 4, 4,
            {255, 239, 170, atmosphereAlpha(alpha)});
    }
    EndBlendMode();

    // Fireflies live in small world-anchored colonies around planted pond
    // shores. They fade in during dusk instead of following the player as a
    // uniform particle field.
    const float fireflyVisibility = smoothstep(
        0.28F, 0.70F, std::clamp(nightAmount, 0.0F, 1.0F));
    if (fireflyVisibility <= 0.01F) {
        return;
    }
    BeginBlendMode(BLEND_ADDITIVE);
    std::size_t pondIndex = 0U;
    for (const PondDefinition& pond : simulation_.terrain().ponds()) {
        constexpr int ColoniesPerPond = 3;
        constexpr int FliesPerColony = 7;
        for (int colony = 0; colony < ColoniesPerPond; ++colony) {
            const std::uint32_t colonySeed =
                0x7f4a7c15U ^
                static_cast<std::uint32_t>(pondIndex + 1U) * 0x9e3779b9U ^
                static_cast<std::uint32_t>(colony + 5) * 0x85ebca6bU;
            const float angle =
                static_cast<float>(colony) * 2.0F * PI /
                    static_cast<float>(ColoniesPerPond) +
                (atmosphereUnit(colonySeed) - 0.5F) * 0.82F;
            float radial = 1.03F;
            Vector2 center{};
            double shoreDistance = -1.0;
            for (int attempt = 0; attempt < 8; ++attempt) {
                const float directionX = std::cos(angle);
                const float directionZ = std::sin(angle);
                const float localX = directionX *
                    static_cast<float>(pond.radiusX) * radial;
                const float localZ = directionZ *
                    static_cast<float>(pond.radiusZ) * radial;
                const float cosine =
                    std::cos(static_cast<float>(pond.rotation));
                const float sine =
                    std::sin(static_cast<float>(pond.rotation));
                center = {
                    static_cast<float>(pond.x) +
                        localX * cosine - localZ * sine,
                    static_cast<float>(pond.z) +
                        localX * sine + localZ * cosine,
                };
                shoreDistance = simulation_.terrain().waterSignedDistance(
                    center.x, center.y);
                if (shoreDistance >= 0.45) {
                    break;
                }
                radial += 0.035F;
            }
            if (shoreDistance < 0.10 || shoreDistance > 5.0) {
                continue;
            }
            const float colonyDistance = Vector2Distance(
                center, {camera.position.x, camera.position.z});
            const float distanceFade = 1.0F -
                smoothstep(24.0F, 38.0F, colonyDistance);
            if (distanceFade <= 0.01F) {
                continue;
            }
            for (int fly = 0; fly < FliesPerColony; ++fly) {
                const std::uint32_t seed = colonySeed +
                    static_cast<std::uint32_t>(fly + 1) * 0xc2b2ae35U;
                const float phase = atmosphereUnit(seed + 1U) * 2.0F * PI;
                const float orbit = atmosphereUnit(seed + 2U) * 2.0F * PI;
                const float radius = 0.28F +
                    atmosphereUnit(seed + 3U) * 1.05F;
                const float speed = 0.42F +
                    atmosphereUnit(seed + 4U) * 0.58F;
                const float x = center.x + std::cos(orbit) * radius +
                    std::sin(time * speed + phase) * 0.38F +
                    std::sin(time * 0.31F + phase * 1.7F) * 0.16F;
                const float z = center.y + std::sin(orbit) * radius +
                    std::cos(time * speed * 0.83F + phase) * 0.34F;
                const float ground = static_cast<float>(
                    simulation_.terrain().getHeight(x, z));
                const float y = ground + 0.48F +
                    atmosphereUnit(seed + 5U) * 1.48F +
                    std::sin(time * (0.72F + speed * 0.35F) + phase) *
                        0.22F;
                const float flickerWave = 0.5F + 0.5F *
                    std::sin(time *
                                 (2.2F + atmosphereUnit(seed + 6U) * 2.8F) +
                             phase);
                const float flicker = 0.22F +
                    flickerWave * flickerWave * 0.78F;
                const float alpha = fireflyVisibility * distanceFade *
                    flicker;
                const Color glow = fly % 4 == 0
                    ? Color{255, 226, 92, atmosphereAlpha(alpha * 0.22F)}
                    : Color{166, 255, 96, atmosphereAlpha(alpha * 0.20F)};
                const Color core = fly % 4 == 0
                    ? Color{255, 248, 177, atmosphereAlpha(alpha * 0.95F)}
                    : Color{225, 255, 157, atmosphereAlpha(alpha * 0.92F)};
                DrawSphereEx({x, y, z}, 0.11F, 5, 5, glow);
                DrawSphereEx({x, y, z}, 0.025F, 4, 4, core);
            }
        }
        ++pondIndex;
    }
    EndBlendMode();
}

void App::drawChestLootGlow(
    const SimulationSnapshot& snapshot,
    const Camera3D& camera) {
    if (!renderer_->settings().particles) {
        return;
    }
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(
            Vector3Normalize(Vector3Subtract(camera.target, camera.position)),
            camera.up));
    const Vector3 cameraUp = Vector3Normalize(camera.up);
    const float time = static_cast<float>(snapshot.elapsedSeconds);

    rlDrawRenderBatchActive();
    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA, RL_ONE, RL_ZERO, RL_ZERO,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlDisableDepthMask();
    rlBegin(RL_TRIANGLES);
    for (const LootChestInstance& chest : snapshot.lootChests) {
        const float progress = std::clamp(
            static_cast<float>(chest.openingProgress), 0.0F, 1.0F);
        if (chest.state == LootChestState::Closed ||
            chest.loot.collected || progress <= 0.0F) {
            continue;
        }
        const float appear = smoothstep(0.015F, 0.08F, progress);
        const float openingEnergy = 1.0F -
            smoothstep(0.18F, 0.92F, progress);
        const float pulse = 1.0F +
            std::sin(time * 2.15F +
                     static_cast<float>(chest.id.index) * 0.83F) * 0.08F;
        const float intensity =
            appear * (0.66F + openingEnergy * 0.50F) * pulse;
        if (intensity <= 0.005F) continue;

        constexpr float ChestGlowHeight = 0.58F;
        const Vector3 origin{
            static_cast<float>(
                chest.position.x +
                chest.surfaceNormal.x * ChestGlowHeight),
            static_cast<float>(
                chest.position.y +
                chest.surfaceNormal.y * ChestGlowHeight),
            static_cast<float>(
                chest.position.z +
                chest.surfaceNormal.z * ChestGlowHeight),
        };

        constexpr int GlowLayers = 9;
        constexpr int GlowSegments = 18;
        for (int layer = 0; layer < GlowLayers; ++layer) {
            const float amount = static_cast<float>(layer) /
                static_cast<float>(GlowLayers - 1);
            const float drift =
                std::sin(time * 1.55F + amount * 4.1F +
                         static_cast<float>(chest.id.index)) *
                0.045F * amount;
            Vector3 center = Vector3Add(
                origin, Vector3Scale(cameraRight, drift));
            center.y += amount * (0.76F + progress * 1.40F);
            const float breathing = 1.0F +
                std::sin(time * 2.65F + amount * 2.9F) * 0.055F;
            const float width =
                (0.62F + amount * 0.48F) * breathing;
            const float height = width * (0.72F + amount * 0.22F);
            const float verticalFade =
                std::pow(1.0F - amount * 0.80F, 1.22F);
            const unsigned char alpha = atmosphereAlpha(
                intensity * verticalFade * 0.30F);

            for (int segment = 0; segment < GlowSegments; ++segment) {
                const float angleA = 2.0F * PI *
                    static_cast<float>(segment) /
                    static_cast<float>(GlowSegments);
                const float angleB = 2.0F * PI *
                    static_cast<float>(segment + 1) /
                    static_cast<float>(GlowSegments);
                const auto edgePoint = [&](float angle) {
                    Vector3 point = Vector3Add(
                        center,
                        Vector3Scale(cameraRight, std::cos(angle) * width));
                    return Vector3Add(
                        point,
                        Vector3Scale(cameraUp, std::sin(angle) * height));
                };
                const Vector3 edgeA = edgePoint(angleA);
                const Vector3 edgeB = edgePoint(angleB);
                rlColor4ub(255, 226, 122, alpha);
                rlVertex3f(center.x, center.y, center.z);
                rlColor4ub(255, 190, 76, 0);
                rlVertex3f(edgeA.x, edgeA.y, edgeA.z);
                rlVertex3f(edgeB.x, edgeB.y, edgeB.z);
            }
        }
    }

    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
}

void App::drawPresentationEffects() {
    for (const auto& effect : effects_) {
        if (effect.startDelayRemaining > 0.0) {
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
        const bool resourceHit =
            effect.type ==
                PresentationEffectType::ResourceHitWood ||
            effect.type ==
                PresentationEffectType::ResourceHitStone;
        const bool resourceDestroyed =
            effect.type ==
                PresentationEffectType::ResourceDestroyedWood ||
            effect.type ==
                PresentationEffectType::ResourceDestroyedStone;
        if (resourceHit || resourceDestroyed) {
            const bool wood =
                effect.type ==
                    PresentationEffectType::ResourceHitWood ||
                effect.type ==
                    PresentationEffectType::ResourceDestroyedWood;
            const int particleCount =
                resourceDestroyed ? 26 : 11;
            const float fade =
                1.0F -
                smoothstep(resourceDestroyed ? 0.48F : 0.34F,
                           1.0F, progress);
            BeginBlendMode(BLEND_ADDITIVE);
            for (int index = 0; index < particleCount;
                 ++index) {
                const float seed = std::fmod(
                    std::sin(
                        static_cast<float>(index + 1) *
                        91.731F) *
                        43758.5453F,
                    1.0F);
                const float unit =
                    seed < 0.0F ? seed + 1.0F : seed;
                const float second = std::fmod(
                    unit * 17.137F + 0.319F, 1.0F);
                const float angle =
                    unit * 2.0F * PI +
                    progress * (second - 0.5F);
                const float speed =
                    resourceDestroyed
                        ? 1.2F + second * 1.9F
                        : 0.48F + second * 0.72F;
                const float distance =
                    progress * speed * effect.scale;
                const float lift =
                    progress *
                        (resourceDestroyed
                             ? 2.3F + unit * 2.1F
                             : 0.7F + unit * 0.9F) -
                    progress * progress *
                        (resourceDestroyed ? 2.1F : 0.72F);
                const Vector3 particlePosition{
                    origin.x + std::cos(angle) * distance,
                    origin.y +
                        (resourceDestroyed
                             ? (second - 0.5F) * 1.25F
                             : 0.0F) +
                        lift,
                    origin.z + std::sin(angle) * distance,
                };
                const float baseSize =
                    resourceDestroyed ? 0.065F : 0.035F;
                const float size =
                    (baseSize + unit *
                                    (resourceDestroyed
                                         ? 0.17F
                                         : 0.085F)) *
                    std::sqrt(std::max(0.0F, 1.0F - progress)) *
                    effect.scale;
                const auto particleAlpha =
                    static_cast<unsigned char>(
                        std::lround(fade * 235.0F));
                const Color color =
                    wood
                        ? (index % 3 == 0
                               ? Color{255, 211, 92,
                                       particleAlpha}
                               : Color{242, 111, 28,
                                       particleAlpha})
                        : (index % 3 == 0
                               ? Color{184, 225, 255,
                                       particleAlpha}
                               : Color{104, 145, 190,
                                       particleAlpha});
                DrawSphere(particlePosition, size, color);
            }
            if (resourceDestroyed) {
                const auto ringAlpha =
                    static_cast<unsigned char>(
                        std::lround(fade * 220.0F));
                DrawCircle3D(
                    {origin.x, 0.06F, origin.z},
                    0.35F + progress * 2.1F,
                    {1.0F, 0.0F, 0.0F}, 90.0F,
                    wood
                        ? Color{255, 158, 42, ringAlpha}
                        : Color{145, 196, 240, ringAlpha});
            }
            EndBlendMode();
            continue;
        }
        if (effect.type == PresentationEffectType::FireImpact) {
            const float fade = 1.0F - smoothstep(0.42F, 1.0F, progress);
            const float shock = 1.0F - smoothstep(0.05F, 0.62F, progress);
            const float travel = smoothstep(0.0F, 0.34F, progress);
            BeginBlendMode(BLEND_ADDITIVE);
            for (int layer = 0; layer < 3; ++layer) {
                const float layerIndex = static_cast<float>(layer);
                DrawCircle3D(
                    {origin.x, origin.y + 0.025F + layerIndex * 0.014F,
                     origin.z},
                    effect.scale * (0.18F + progress * 2.45F) -
                        layerIndex * 0.065F,
                    {1.0F, 0.0F, 0.0F}, 90.0F,
                    {255,
                     static_cast<unsigned char>(190 - layer * 42),
                     static_cast<unsigned char>(55 - layer * 14),
                     atmosphereAlpha(shock *
                         (0.88F - layerIndex * 0.2F))});
            }
            constexpr int FlameRayCount = 8;
            for (int index = 0; index < FlameRayCount; ++index) {
                const float angle = effectUnit(index, 150) * 2.0F * PI;
                const float length = effect.scale *
                    (0.2F + travel *
                        (0.65F + effectUnit(index, 151) * 1.5F));
                const Vector3 direction{
                    std::cos(angle),
                    0.18F + effectUnit(index, 152) * 0.72F,
                    std::sin(angle)};
                const Vector3 tip = Vector3Add(
                    origin, Vector3Scale(direction, length));
                DrawCylinderEx(
                    origin, tip, effect.scale * 0.026F * shock,
                    0.0F, 4,
                    index % 3 == 0
                        ? Color{255, 244, 172,
                                atmosphereAlpha(shock)}
                        : Color{255, 91, 15,
                                atmosphereAlpha(shock * 0.82F)});
            }
            if (renderer_->settings().particles) {
                constexpr int EmberCount = 6;
                for (int index = 0; index < EmberCount; ++index) {
                    const float angle = effectUnit(index, 160) * 2.0F * PI;
                    const float distance = effect.scale * progress *
                        (0.8F + effectUnit(index, 161) * 2.4F);
                    const Vector3 ember{
                        origin.x + std::cos(angle) * distance,
                        origin.y + 0.12F + progress *
                            (0.4F + effectUnit(index, 162) * 1.1F),
                        origin.z + std::sin(angle) * distance};
                    DrawSphereEx(
                        ember, effect.scale * 0.025F,
                        4, 4,
                        {255, 181, 52,
                         atmosphereAlpha(fade * 0.82F)});
                }
            }
            EndBlendMode();
            continue;
        }
        if (effect.type == PresentationEffectType::IceImpact ||
            effect.type == PresentationEffectType::IceCrack) {
            const bool crack = effect.type == PresentationEffectType::IceCrack;
            const float fade = 1.0F - smoothstep(0.36F, 1.0F, progress);
            const float flash = 1.0F - smoothstep(0.02F, 0.20F, progress);
            const float ring = smoothstep(0.02F, 0.70F, progress) * fade;
            const float shock = 1.0F - smoothstep(
                crack ? 0.06F : 0.04F,
                crack ? 0.42F : 0.62F, progress);
            BeginBlendMode(BLEND_ADDITIVE);
            const float burstTravel = smoothstep(
                0.0F, crack ? 0.32F : 0.24F, progress);
            const int rayCount = crack ? 5 : 9;
            for (int index = 0; index < rayCount; ++index) {
                const float angle = effectUnit(index, 120) * 2.0F * PI;
                const float vertical = crack
                    ? 0.08F + effectUnit(index, 121) * 0.32F
                    : -0.18F + effectUnit(index, 121) * 1.02F;
                Vector3 direction{
                    std::cos(angle), vertical, std::sin(angle)};
                direction = Vector3Normalize(direction);
                const float length = effect.scale *
                    (0.22F + burstTravel *
                        (0.72F + effectUnit(index, 122) * 1.18F));
                const Vector3 start = Vector3Add(
                    origin, Vector3Scale(direction, effect.scale * 0.05F));
                const Vector3 end = Vector3Add(
                    origin, Vector3Scale(direction, length));
                DrawCylinderEx(
                    start, end,
                    effect.scale * (crack ? 0.018F : 0.028F) * shock,
                    0.0F, 4,
                    index % 3 == 0
                        ? Color{223, 248, 255,
                                atmosphereAlpha(shock * flash)}
                        : Color{85, 207, 255,
                                atmosphereAlpha(shock * 0.82F)});
            }
            for (int layer = 0; layer < 3; ++layer) {
                const float layerProgress = std::clamp(
                    progress - static_cast<float>(layer) * 0.035F,
                    0.0F, 1.0F);
                const float radius = effect.scale *
                    (crack ? 0.10F + layerProgress * 0.72F
                           : 0.20F + layerProgress * 2.65F) -
                    static_cast<float>(layer) * (crack ? 0.025F : 0.07F);
                DrawCircle3D(
                    {origin.x, origin.y + 0.025F + static_cast<float>(layer) * 0.012F, origin.z},
                    radius, {1.0F, 0.0F, 0.0F}, 90.0F,
                    {142, 229, 255,
                     atmosphereAlpha(ring * (0.86F - static_cast<float>(layer) * 0.18F))});
            }
            const int shardCount = crack ? 5 : 10;
            for (int index = 0; index < shardCount; ++index) {
                const float angle = effectUnit(index, 70) * 2.0F * PI;
                const float speed = effect.scale *
                    (crack ? 0.28F + effectUnit(index, 71) * 0.72F
                           : 0.55F + effectUnit(index, 71) * 1.8F);
                const float distance = speed * smoothstep(0.05F, 0.58F, progress);
                const float height = effect.scale *
                    (0.12F + effectUnit(index, 72) * 0.55F) *
                    std::sin(progress * PI);
                const Vector3 shard{
                    origin.x + std::cos(angle) * distance,
                    origin.y + height + progress *
                        (0.22F + effectUnit(index, 73) * 0.42F),
                    origin.z + std::sin(angle) * distance};
                const float size = effect.scale *
                    (0.025F + effectUnit(index, 74) * 0.055F) *
                    (1.0F - smoothstep(0.62F, 1.0F, progress));
                DrawLine3D(
                    {shard.x - std::cos(angle) * size * 2.2F,
                     shard.y - size,
                     shard.z - std::sin(angle) * size * 2.2F},
                    {shard.x + std::cos(angle) * size * 1.2F,
                     shard.y + size * 2.5F,
                     shard.z + std::sin(angle) * size * 1.2F},
                    {191, 246, 255, atmosphereAlpha(fade * 0.78F)});
            }
            if (!crack) {
                constexpr int IceChunkCount = 8;
                for (int index = 0; index < IceChunkCount; ++index) {
                    const float angle = effectUnit(index, 100) * 2.0F * PI;
                    const float distance = effect.scale * progress *
                        (0.65F + effectUnit(index, 101) * 2.25F);
                    const float height = effect.scale *
                        (0.72F + effectUnit(index, 102) * 2.1F) * progress -
                        effect.scale * 2.8F * progress * progress;
                    const Vector3 chunk{
                        origin.x + std::cos(angle) * distance,
                        origin.y + 0.12F + height,
                        origin.z + std::sin(angle) * distance};
                    const float size = effect.scale *
                        (0.045F + effectUnit(index, 103) * 0.11F) *
                        (1.0F - smoothstep(0.72F, 1.0F, progress));
                    rlPushMatrix();
                    rlTranslatef(chunk.x, chunk.y, chunk.z);
                    rlRotatef(
                        progress * (210.0F + effectUnit(index, 104) * 420.0F),
                        0.35F, 0.82F, 0.24F);
                    DrawCube(
                        {}, size, size * 0.62F, size * 1.45F,
                        {191, 246, 255, atmosphereAlpha(shock * 0.84F)});
                    rlPopMatrix();
                }
                constexpr int IceSparkCount = 12;
                for (int index = 0; index < IceSparkCount; ++index) {
                    const float angle = effectUnit(index, 110) * 2.0F * PI;
                    const float speed = effect.scale *
                        (1.8F + effectUnit(index, 111) * 4.0F);
                    const float distance = speed * progress;
                    const float rise = effect.scale *
                        (0.3F + effectUnit(index, 112) * 1.4F) * progress;
                    const Vector3 tip{
                        origin.x + std::cos(angle) * distance,
                        origin.y + 0.2F + rise -
                            effect.scale * 1.4F * progress * progress,
                        origin.z + std::sin(angle) * distance};
                    const float tailLength = effect.scale *
                        (0.08F + effectUnit(index, 113) * 0.24F);
                    const Vector3 tail{
                        tip.x - std::cos(angle) * tailLength,
                        tip.y - tailLength * 0.28F,
                        tip.z - std::sin(angle) * tailLength};
                    DrawLine3D(
                        tail, tip,
                        {223, 248, 255,
                         atmosphereAlpha(shock * 0.92F)});
                }
            }
            if (renderer_->settings().particles) {
                constexpr int SnowCount = 12;
                for (int index = 0; index < SnowCount; ++index) {
                    const float angle = effectUnit(index, 80) * 2.0F * PI;
                    const float distance = effect.scale *
                        (0.2F + progress * (0.8F + effectUnit(index, 81) * 1.4F));
                    DrawSphereEx(
                        {origin.x + std::cos(angle) * distance,
                         origin.y + 0.16F + progress *
                             (0.35F + effectUnit(index, 82) * 0.8F),
                         origin.z + std::sin(angle) * distance},
                        effect.scale * 0.018F,
                        4, 4,
                        {191, 246, 255, atmosphereAlpha(fade * 0.75F)});
                }
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
            DrawSphere(origin, 0.18F * (1.0F - progress),
                       {255, 220, 120, 255});
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
        } else if (effect.type == PresentationEffectType::Explosion) {
            const float scale = effect.scale;
            const float flash = 1.0F - smoothstep(0.02F, 0.2F, progress);
            const float flame = 1.0F - smoothstep(0.12F, 0.58F, progress);
            const float smoke = smoothstep(0.14F, 0.34F, progress) *
                (1.0F - smoothstep(0.72F, 1.0F, progress));
            const float shock = 1.0F - smoothstep(0.16F, 0.62F, progress);

            // Ground-hugging pressure wave and dust front.
            const float shockRadius = scale * (0.35F + progress * 5.4F);
            for (int ring = 0; ring < 3; ++ring) {
                DrawCircle3D(
                    {origin.x, origin.y + 0.025F + static_cast<float>(ring) * 0.012F,
                     origin.z},
                    shockRadius - static_cast<float>(ring) * 0.09F * scale,
                    {1.0F, 0.0F, 0.0F}, 90.0F,
                    {255, static_cast<unsigned char>(172 - ring * 24), 72,
                     atmosphereAlpha(shock * (0.78F - static_cast<float>(ring) * 0.17F))});
            }

            constexpr int DustCount = 22;
            for (int index = 0; index < DustCount; ++index) {
                const float angle = effectUnit(index, 0) * 2.0F * PI;
                const float speed = scale * (2.2F + effectUnit(index, 1) * 3.0F);
                const float distance = progress * speed;
                const float lift = std::sin(progress * PI) *
                    (0.12F + effectUnit(index, 2) * 0.58F);
                const float size = scale *
                    (0.07F + effectUnit(index, 3) * 0.16F + progress * 0.13F);
                DrawSphereEx(
                    {origin.x + std::cos(angle) * distance,
                     origin.y + 0.04F + lift,
                     origin.z + std::sin(angle) * distance},
                    size, 4, 4,
                    {111, 83, 58, atmosphereAlpha(shock * 0.72F)});
            }

            // Dense dark smoke survives after the fireball disappears.
            constexpr int SmokeCount = 15;
            for (int index = 0; index < SmokeCount; ++index) {
                const float delay = effectUnit(index, 4) * 0.22F;
                const float local = std::clamp(
                    (progress - delay) / std::max(1.0F - delay, 0.01F), 0.0F, 1.0F);
                const float angle = effectUnit(index, 5) * 2.0F * PI;
                const float spread = scale * local *
                    (0.35F + effectUnit(index, 6) * 1.15F);
                const float size = scale *
                    (0.12F + effectUnit(index, 7) * 0.18F + local * 0.58F);
                const unsigned char shade = static_cast<unsigned char>(
                    43 + effectUnit(index, 8) * 32.0F);
                DrawSphereEx(
                    {origin.x + std::cos(angle) * spread,
                     origin.y + 0.2F + local *
                         (1.0F + effectUnit(index, 9) * 2.0F),
                     origin.z + std::sin(angle) * spread},
                    size, 5, 5,
                    {shade, static_cast<unsigned char>(shade - 4),
                     static_cast<unsigned char>(shade - 7),
                     atmosphereAlpha(smoke * (0.42F + effectUnit(index, 10) * 0.28F))});
            }

            // Chunks retain weight: quick launch, gravity, rotation.
            constexpr int DebrisCount = 13;
            for (int index = 0; index < DebrisCount; ++index) {
                const float angle = effectUnit(index, 11) * 2.0F * PI;
                const float speed = scale * (1.7F + effectUnit(index, 12) * 3.3F);
                const float lift = scale * (2.2F + effectUnit(index, 13) * 3.2F);
                const Vector3 position{
                    origin.x + std::cos(angle) * speed * progress,
                    origin.y + 0.14F + lift * progress -
                        4.6F * scale * progress * progress,
                    origin.z + std::sin(angle) * speed * progress,
                };
                const float size = scale * (0.06F + effectUnit(index, 14) * 0.12F);
                rlPushMatrix();
                rlTranslatef(position.x, position.y, position.z);
                rlRotatef(progress * (260.0F + effectUnit(index, 15) * 480.0F),
                          0.4F, 0.8F, 0.25F);
                DrawCube({}, size, size * 0.7F, size * 1.25F,
                         {72, 58, 46, atmosphereAlpha(shock)});
                rlPopMatrix();
            }

            BeginBlendMode(BLEND_ADDITIVE);
            // White-hot core followed by layered orange fire blobs.
            DrawSphereEx(
                {origin.x, origin.y + 0.24F, origin.z},
                scale * (0.22F + progress * 1.55F), 8, 8,
                {255, 246, 202, atmosphereAlpha(flash * 0.95F)});
            DrawSphereEx(
                {origin.x, origin.y + 0.28F, origin.z},
                scale * (0.38F + progress * 1.9F), 8, 8,
                {255, 91, 18, atmosphereAlpha(flame * 0.52F)});
            constexpr int FlameCount = 16;
            for (int index = 0; index < FlameCount; ++index) {
                const float angle = effectUnit(index, 16) * 2.0F * PI;
                const float radial = progress * scale *
                    (0.4F + effectUnit(index, 17) * 2.5F);
                const float rise = progress * scale *
                    (0.45F + effectUnit(index, 18) * 2.2F);
                const float size = scale *
                    (0.11F + effectUnit(index, 19) * 0.3F) *
                    std::sqrt(std::max(0.0F, flame));
                const Color color = index % 3 == 0
                    ? Color{255, 238, 142, atmosphereAlpha(flame * 0.9F)}
                    : Color{255, 78, 12, atmosphereAlpha(flame * 0.72F)};
                DrawSphereEx(
                    {origin.x + std::cos(angle) * radial,
                     origin.y + 0.2F + rise,
                     origin.z + std::sin(angle) * radial},
                    size, 5, 5, color);
            }

            // Long, bright fragments make the blast readable at distance.
            constexpr int SparkCount = 28;
            for (int index = 0; index < SparkCount; ++index) {
                const float angle = effectUnit(index, 20) * 2.0F * PI;
                const float speed = scale * (3.0F + effectUnit(index, 21) * 5.5F);
                const float height = scale * (1.2F + effectUnit(index, 22) * 4.0F);
                const Vector3 tip{
                    origin.x + std::cos(angle) * speed * progress,
                    origin.y + 0.2F + height * progress -
                        3.2F * scale * progress * progress,
                    origin.z + std::sin(angle) * speed * progress,
                };
                const float tailLength = scale * (0.12F + effectUnit(index, 23) * 0.35F);
                const Vector3 tail{
                    tip.x - std::cos(angle) * tailLength,
                    tip.y - tailLength * 0.35F,
                    tip.z - std::sin(angle) * tailLength,
                };
                DrawLine3D(tail, tip,
                           {255, 202, 65, atmosphereAlpha(shock * 0.94F)});
                DrawSphereEx(tip, 0.025F * scale, 4, 4,
                             {255, 244, 183, atmosphereAlpha(shock)});
            }
            EndBlendMode();
        } else if (effect.type == PresentationEffectType::RamImpact) {
            const float fade = 1.0F - smoothstep(0.35F, 1.0F, progress);
            const Vector3 ground{
                origin.x, origin.y + 0.035F, origin.z};
            DrawCircle3D(
                ground, 0.35F + progress * 3.0F,
                {1.0F, 0.0F, 0.0F}, 90.0F,
                {255, 72, 45, atmosphereAlpha(fade * 0.88F)});
            DrawCircle3D(
                {ground.x, ground.y + 0.008F, ground.z},
                0.18F + progress * 1.85F,
                {1.0F, 0.0F, 0.0F}, 90.0F,
                {255, 177, 56, atmosphereAlpha(fade)});
            constexpr int RayCount = 10;
            for (int ray = 0; ray < RayCount; ++ray) {
                const float angle =
                    static_cast<float>(ray) * 2.0F * PI /
                    static_cast<float>(RayCount);
                const float inner = 0.28F + progress * 0.55F;
                const float outer = 0.55F + progress *
                    (1.65F + effectUnit(ray, 141) * 1.1F);
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
            const Color color =
                effect.type == PresentationEffectType::ResourceBurst
                    ? Color{184, 145, 82, 255}
                    : Color{125, 112, 101, 255};
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
