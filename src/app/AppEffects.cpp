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
        if (effect.type == PresentationEffectType::Hit) {
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
            DrawSphereWires(origin, 0.4F + progress * 2.8F, 8, 8,
                            {255, 72, 45, 255});
            DrawSphereWires(origin, 0.2F + progress * 1.7F, 8, 8,
                            ORANGE);
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

void App::drawFloatingDamageNumbers(
    const Camera3D& camera) const {
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    for (const FloatingDamageNumber& number :
         floatingDamageNumbers_) {
        const Vector3 toNumber = Vector3Subtract(
            {static_cast<float>(number.position.x),
             static_cast<float>(number.position.y),
             static_cast<float>(number.position.z)},
            camera.position);
        if (Vector3DotProduct(toNumber, cameraForward) <= 0.0F) {
            continue;
        }

        const float progress = static_cast<float>(
            1.0 - number.remaining / number.duration);
        const Vector3 animatedPosition{
            static_cast<float>(number.position.x),
            static_cast<float>(number.position.y) +
                progress * 0.9F,
            static_cast<float>(number.position.z),
        };
        Vector2 screenPosition =
            GetWorldToScreen(animatedPosition, camera);
        screenPosition.x += number.horizontalDrift * progress;
        const float fade = std::clamp(
            static_cast<float>(number.remaining /
                               (number.duration * 0.4)),
            0.0F, 1.0F);
        const auto alpha = static_cast<unsigned char>(
            std::lround(fade * 255.0F));
        const int fontSize =
            (number.critical ? 50 : 40) +
            static_cast<int>(
                std::lround(std::sin(progress * PI) * 7.0F));
        const char* text = TextFormat("-%.1f", number.damage);
        const float x =
            screenPosition.x -
            measureUiText(text, static_cast<float>(fontSize)).x * 0.5F;
        const float y = screenPosition.y;
        drawUiText(text, {x + 2.0F, y + 2.0F},
                   static_cast<float>(fontSize),
                   {20, 16, 12, alpha});
        drawUiText(text, {x, y}, static_cast<float>(fontSize),
                   number.critical
                       ? Color{255, 214, 62, alpha}
                       : Color{255, 255, 255, alpha});
    }
}

void App::drawResourceGainVisuals(
    const Camera3D& camera) const {
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    for (const ResourceGainVisual& gain : resourceGainVisuals_) {
        const Vector3 worldPosition{
            static_cast<float>(gain.position.x),
            static_cast<float>(gain.position.y),
            static_cast<float>(gain.position.z),
        };
        if (Vector3DotProduct(
                Vector3Subtract(worldPosition, camera.position),
                cameraForward) <= 0.0F) {
            continue;
        }

        const float progress = std::clamp(
            static_cast<float>(
                1.0 - gain.remaining / gain.duration),
            0.0F, 1.0F);
        const Vector2 start =
            GetWorldToScreen(worldPosition, camera);
        const Vector2 target =
            gain.type == ResourceType::Wood
                ? Vector2{70.0F, 66.0F}
                : Vector2{260.0F, 66.0F};
        constexpr float RiseEnd = 0.3F;
        const float riseProgress =
            std::clamp(progress / RiseEnd, 0.0F, 1.0F);
        const float riseEased =
            riseProgress * riseProgress *
            (3.0F - 2.0F * riseProgress);
        const Vector2 liftedStart{
            start.x, start.y - 58.0F,
        };
        const float flightProgress = std::clamp(
            (progress - RiseEnd) / (1.0F - RiseEnd),
            0.0F, 1.0F);
        float eased = 0.0F;
        if (flightProgress < 0.8F) {
            const float early = flightProgress / 0.8F;
            const float smoothEarly =
                early * early * (3.0F - 2.0F * early);
            eased = smoothEarly * 0.72F;
        } else {
            const float magnet =
                (flightProgress - 0.8F) / 0.2F;
            eased =
                0.72F + 0.28F * magnet * magnet;
        }
        Vector2 position{
            start.x +
                (liftedStart.x - start.x) * riseEased,
            start.y +
                (liftedStart.y - start.y) * riseEased,
        };
        if (progress >= RiseEnd) {
            position.x =
                liftedStart.x +
                (target.x - liftedStart.x) * eased;
            position.y =
                liftedStart.y +
                (target.y - liftedStart.y) * eased -
                std::sin(flightProgress * PI) * 54.0F;
        }
        const float iconSize = 104.0F - eased * 36.0F;
        ui_.drawResourceIcon(
            {position.x - iconSize * 0.5F,
             position.y - iconSize * 0.5F, iconSize, iconSize},
            gain.type == ResourceType::Wood
                ? UiResourceIcon::Wood
                : UiResourceIcon::Stone);

        const float textFade = std::clamp(
            (0.7F - progress) / 0.25F, 0.0F, 1.0F);
        if (textFade > 0.0F) {
            const auto alpha = static_cast<unsigned char>(
                std::lround(textFade * 255.0F));
            drawUiText(
                "+" + std::to_string(gain.amount),
                {position.x + iconSize * 0.62F,
                 position.y - 36.0F},
                48.0F, {255, 245, 204, alpha});
        }
    }
}

void App::drawProductionVisuals(
    const Camera3D& camera) const {
    if (productionVisuals_.empty()) {
        return;
    }
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(cameraForward, camera.up));
    const Vector3 cameraUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, cameraForward));
    const Vector3 towardCamera =
        Vector3Negate(cameraForward);
    constexpr float FullyVisibleDistance = 15.0F;
    constexpr float MaximumVisibleDistance = 22.0F;

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    for (const ProductionVisual& production :
         productionVisuals_) {
        Vector3 worldPosition{
            static_cast<float>(production.position.x),
            static_cast<float>(production.position.y),
            static_cast<float>(production.position.z),
        };
        const Vector3 fromCamera =
            Vector3Subtract(worldPosition, camera.position);
        if (Vector3DotProduct(
                fromCamera, cameraForward) <= 0.0F) {
            continue;
        }
        const float distance =
            Vector3Length(fromCamera);
        if (distance >= MaximumVisibleDistance) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 -
                production.remaining /
                    production.duration),
            0.0F, 1.0F);
        const float rise =
            1.0F -
            std::pow(1.0F - progress, 3.0F);
        worldPosition = Vector3Add(
            worldPosition,
            Vector3Scale(cameraUp, rise * 0.95F));
        const float lifetimeFade = std::clamp(
            (1.0F - progress) / 0.32F,
            0.0F, 1.0F);
        const float distanceFade = 1.0F - std::clamp(
            (distance - FullyVisibleDistance) /
                (MaximumVisibleDistance -
                 FullyVisibleDistance),
            0.0F, 1.0F);
        const float fade = lifetimeFade * distanceFade;
        const auto alpha = static_cast<unsigned char>(
            std::lround(fade * 255.0F));
        constexpr float IconSize = 0.62F;
        constexpr float TextSize = 0.48F;
        constexpr float Gap = 0.1F;
        const std::string text =
            "+" + std::to_string(production.amount);
        const float textWidth =
            measureWorldBillboardText(text, TextSize);
        const float totalWidth =
            textWidth + Gap + IconSize;
        const Vector3 textCenter = Vector3Add(
            worldPosition,
            Vector3Scale(
                cameraRight,
                -totalWidth * 0.5F +
                    textWidth * 0.5F));
        const Vector3 iconCenter = Vector3Add(
            Vector3Add(
                worldPosition,
                Vector3Scale(
                    cameraRight,
                    totalWidth * 0.5F -
                        IconSize * 0.5F)),
            Vector3Scale(towardCamera, 0.006F));
        drawWorldBillboardText(
            text, textCenter, TextSize, camera,
            cameraRight, cameraUp,
            {224, 244, 255, alpha},
            {17, 18, 34, alpha});
        drawWorldBillboardTexture(
            ui_.resourceTexture(production.icon), iconCenter,
            {IconSize, IconSize}, camera, cameraUp,
            {255, 255, 255, alpha});
    }
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

Vec3 App::buildingImpactOffsetAt(EntityId id) const {
    Vec3 offset{};
    for (const BuildingImpactVisual& impact :
         buildingImpactVisuals_) {
        if (impact.id != id) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 -
                impact.remaining / impact.duration),
            0.0F, 1.0F);
        const double displacement =
            static_cast<double>(
                std::sin(progress * 2.0F * PI) *
                std::exp(-3.2F * progress) * 0.15F);
        offset.x += impact.direction.x * displacement;
        offset.z += impact.direction.z * displacement;
    }
    return offset;
}

Vec3 App::buildingShotRecoilOffsetAt(
    EntityId id, float yaw) const {
    Vec3 offset{};
    for (const BuildingShotRecoilVisual& recoil :
         buildingShotRecoilVisuals_) {
        if (recoil.id != id) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 -
                recoil.remaining / recoil.duration),
            0.0F, 1.0F);
        const float displacement =
            std::sin(std::sqrt(progress) * PI) *
            (1.0F - progress) * recoil.strength;
        offset.x +=
            static_cast<double>(
                std::sin(yaw) * displacement);
        offset.z +=
            static_cast<double>(
                std::cos(yaw) * displacement);
    }
    return offset;
}

void App::addBuildingShotRecoil(
    EntityId id, double duration, float strength) {
    const auto existing = std::find_if(
        buildingShotRecoilVisuals_.begin(),
        buildingShotRecoilVisuals_.end(),
        [id](const BuildingShotRecoilVisual& recoil) {
            return recoil.id == id;
        });
    if (existing != buildingShotRecoilVisuals_.end()) {
        existing->remaining = duration;
        existing->duration = duration;
        existing->strength =
            std::max(existing->strength, strength);
        return;
    }
    buildingShotRecoilVisuals_.push_back({
        .id = id,
        .remaining = duration,
        .duration = duration,
        .strength = strength,
    });
}

float App::buildingAnimationScaleAt(
    BuildingType type, GridPosition position) const {
    return buildingAnimationScaleAt(
        buildingWorldPosition(type, position));
}

float App::buildingAnimationScaleAt(
    Vec3 center,
    std::optional<EntityId> entityId) const {
    float scale = 1.0F;
    for (const PresentationEffect& effect : effects_) {
        if (effect.startDelayRemaining > 0.0) {
            continue;
        }
        if (effect.type !=
                PresentationEffectType::BuildingPlaced &&
            effect.type !=
                PresentationEffectType::BuildingUpgrade &&
            effect.type !=
                PresentationEffectType::BuildingDamaged) {
            continue;
        }
        const bool matchedEntity =
            effect.entityId && entityId &&
            effect.entityId == entityId;
        if (effect.entityId && entityId &&
            !matchedEntity) {
            continue;
        }
        if (!matchedEntity &&
            (std::abs(
                 effect.position.x - center.x) > 0.01 ||
             std::abs(
                 effect.position.y - center.y) > 0.01 ||
             std::abs(
                 effect.position.z - center.z) > 0.01)) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 - effect.remaining / effect.duration),
            0.0F, 1.0F);
        if (effect.type ==
            PresentationEffectType::BuildingPlaced) {
            const float spring =
                1.0F -
                std::exp(-6.0F * progress) *
                    std::cos(4.5F * PI * progress);
            scale = std::clamp(spring, 0.06F, 1.18F);
        } else if (
            effect.type ==
            PresentationEffectType::BuildingUpgrade) {
            const float bounce =
                1.0F +
                std::sin(progress * 4.0F * PI) *
                    std::exp(-4.5F * progress) * 0.09F;
            scale = std::clamp(bounce, 0.94F, 1.09F);
        } else {
            // A fast, readable squash mirrors enemy hit feedback. It returns
            // exactly to 1.0, so repeated attacks cannot leave scale drift.
            const float squash =
                1.0F - std::sin(progress * PI) * 0.14F;
            scale = std::min(scale, squash);
        }
    }
    return scale;
}

std::vector<ModularAnimationScale>
App::modularAnimationScales(
    const SimulationSnapshot& snapshot) const {
    const double cellSize =
        simulation_.terrain().config().cellSize;
    std::vector<ModularAnimationScale> scales;
    scales.reserve(
        snapshot.platformFrames.size() +
        snapshot.modularWalls.size() +
        snapshot.ramps.size());
    const auto addScale =
        [this, &scales](EntityId id, Vec3 center) {
            const float scale =
                buildingAnimationScaleAt(center, id);
            if (std::abs(scale - 1.0F) > 1e-4F) {
                scales.push_back({
                    .id = id,
                    .scale = scale,
                });
            }
        };
    for (const PlatformFrameInstance& frame :
         snapshot.platformFrames) {
        addScale(
            frame.id,
            {
                (frame.anchor.x + 1.0) * cellSize,
                frame.floorHeight,
                (frame.anchor.z + 1.0) * cellSize,
            });
    }
    for (const WallInstance& wall :
         snapshot.modularWalls) {
        addScale(
            wall.id,
            {
                (wall.anchor.x + 0.5) * cellSize,
                wall.bottomHeight,
                (wall.anchor.z + 0.5) * cellSize,
            });
    }
    for (const RampInstance& ramp : snapshot.ramps) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const int widthCells =
            alongZ ? ModularRampWidthCells
                   : ModularRampRunCells;
        const int depthCells =
            alongZ ? ModularRampRunCells
                   : ModularRampWidthCells;
        addScale(
            ramp.id,
            {
                (ramp.anchor.x +
                 widthCells * 0.5) *
                    cellSize,
                ramp.bottomHeight,
                (ramp.anchor.z +
                 depthCells * 0.5) *
                    cellSize,
            });
    }
    return scales;
}

float App::productionScaleAt(
    EntityId id) const {
    for (auto visual =
             productionVisuals_.rbegin();
         visual != productionVisuals_.rend();
         ++visual) {
        if (visual->buildingId != id) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 -
                visual->remaining / visual->duration),
            0.0F, 1.0F);
        constexpr float CompressionEnd = 0.18F;
        constexpr float MinimumScale = 0.84F;
        if (progress < CompressionEnd) {
            const float t = progress / CompressionEnd;
            const float eased =
                1.0F - std::pow(1.0F - t, 3.0F);
            return 1.0F -
                   (1.0F - MinimumScale) * eased;
        }
        const float t =
            (progress - CompressionEnd) /
            (1.0F - CompressionEnd);
        return 1.0F -
               0.16F * std::cos(t * PI * 4.0F) *
                   std::exp(-4.2F * t);
    }
    return 1.0F;
}
} // namespace ian
