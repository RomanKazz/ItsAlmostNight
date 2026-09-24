#include "app/App.hpp"
#include "app/AppEffectDrawing.hpp"
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

float wrapAtmosphere(float value, float center, float halfExtent) {
    const float extent = halfExtent * 2.0F;
    float relative = std::fmod(
        value - center + halfExtent, extent);
    if (relative < 0.0F) {
        relative += extent;
    }
    return center + relative - halfExtent;
}

std::optional<std::array<Vector2, 4>> projectedSurfaceQuad(
    Vector3 center, Vector3 right, Vector3 up, float worldSize,
    float rotationRadians, const Camera3D& camera) {
    if (worldSize <= 0.0F) {
        return std::nullopt;
    }
    const float cosine = std::cos(rotationRadians);
    const float sine = std::sin(rotationRadians);
    const Vector3 rotatedRight = Vector3Add(
        Vector3Scale(right, cosine), Vector3Scale(up, sine));
    const Vector3 rotatedUp = Vector3Add(
        Vector3Scale(up, cosine), Vector3Scale(right, -sine));
    const Vector3 halfRight = Vector3Scale(
        rotatedRight, worldSize * 0.5F);
    const Vector3 halfUp = Vector3Scale(
        rotatedUp, worldSize * 0.5F);
    const Vector3 topLeft = Vector3Add(
        Vector3Subtract(center, halfRight), halfUp);
    const Vector3 bottomLeft = Vector3Subtract(
        Vector3Subtract(center, halfRight), halfUp);
    const Vector3 bottomRight = Vector3Subtract(
        Vector3Add(center, halfRight), halfUp);
    const Vector3 topRight = Vector3Add(
        Vector3Add(center, halfRight), halfUp);
    return std::array<Vector2, 4>{
        GetWorldToScreen(topLeft, camera),
        GetWorldToScreen(bottomLeft, camera),
        GetWorldToScreen(bottomRight, camera),
        GetWorldToScreen(topRight, camera),
    };
}

void drawScreenTextureQuad(
    Texture2D texture, const std::array<Vector2, 4>& corners,
    Color tint) {
    if (!IsTextureValid(texture)) {
        return;
    }
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlTexCoord2f(0.0F, 0.0F);
    rlVertex2f(corners[0].x, corners[0].y);
    rlTexCoord2f(0.0F, 1.0F);
    rlVertex2f(corners[1].x, corners[1].y);
    rlTexCoord2f(1.0F, 1.0F);
    rlVertex2f(corners[2].x, corners[2].y);
    rlTexCoord2f(1.0F, 0.0F);
    rlVertex2f(corners[3].x, corners[3].y);
    rlEnd();
    rlSetTexture(0);
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
    Vector3 cameraForward = Vector3Subtract(
        camera.target, camera.position);
    cameraForward = Vector3LengthSqr(cameraForward) > 0.000001F
        ? Vector3Normalize(cameraForward)
        : Vector3{0.0F, 0.0F, -1.0F};
    Vector3 cameraRight = Vector3CrossProduct(
        cameraForward, camera.up);
    cameraRight = Vector3LengthSqr(cameraRight) > 0.000001F
        ? Vector3Normalize(cameraRight)
        : Vector3{1.0F, 0.0F, 0.0F};
    const Vector3 billboardUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, cameraForward));
    Vector3 horizontalView = cameraForward;
    horizontalView.y = 0.0F;
    horizontalView = Vector3LengthSqr(horizontalView) > 0.000001F
        ? Vector3Normalize(horizontalView)
        : Vector3{0.0F, 0.0F, -1.0F};

    // Additive particles do not naturally change scene alpha, so the ink
    // pass would otherwise composite the outlined surface behind them over
    // their glow. The impact shader preserves the same premultiplied additive
    // RGB while writing the post-process-only foreground tag to alpha.
    const bool tagAdditiveParticles = vfxImpactShader_.valid();
    const auto beginAdditiveParticlePass = [&]() {
        if (!tagAdditiveParticles) {
            BeginBlendMode(BLEND_ADDITIVE);
            return;
        }
        rlDrawRenderBatchActive();
        rlSetBlendFactorsSeparate(
            RL_ONE, RL_ONE,
            RL_ONE, RL_ZERO,
            RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        constexpr float UseTexture = 0.0F;
        SetShaderValue(
            vfxImpactShader_.get(), vfxImpactUseTextureLocation_,
            &UseTexture, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(vfxImpactShader_.get());
    };
    const auto endAdditiveParticlePass = [&]() {
        if (tagAdditiveParticles) {
            EndShaderMode();
        }
        EndBlendMode();
    };

    // Rare low-poly leaves. Positions loop around the camera, so the effect
    // has fixed cost and never allocates or fills the world with entities.
    constexpr int LeafCount = 11;
    constexpr float LeafHalfExtent = 25.0F;
    rlDrawRenderBatchActive();
    // Preserve regular alpha-blended RGB, but clear scene alpha wherever a
    // leaf is visible. This tags it as a foreground overlay for the ink pass
    // without making the leaf additive or changing its opacity.
    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
        RL_ZERO, RL_ZERO,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlDisableDepthMask();
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

        const float viewOffsetX = x - camera.position.x;
        const float viewOffsetZ = z - camera.position.z;
        const float viewDistanceSquared =
            viewOffsetX * viewOffsetX + viewOffsetZ * viewOffsetZ;
        if (viewDistanceSquared > 16.0F &&
            viewOffsetX * horizontalView.x +
                    viewOffsetZ * horizontalView.z <
                -1.0F) {
            continue;
        }

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
        rlScalef(size, size, size);
        const Color leafColor =
            palette[static_cast<std::size_t>(index) % 4U];
        const std::array<Vector3, 4> leafEdge{{
            {-0.10F, 0.0F, 0.0F},
            {0.0F, 0.0F, 0.045F},
            {0.11F, 0.0F, 0.0F},
            {0.0F, 0.0F, -0.045F},
        }};
        rlBegin(RL_TRIANGLES);
        rlColor4ub(
            leafColor.r, leafColor.g, leafColor.b, leafColor.a);
        for (std::size_t edge = 0; edge < leafEdge.size(); ++edge) {
            const Vector3& a = leafEdge[edge];
            const Vector3& b = leafEdge[(edge + 1U) % leafEdge.size()];
            rlVertex3f(0.0F, 0.018F, 0.0F);
            rlVertex3f(a.x, a.y, a.z);
            rlVertex3f(b.x, b.y, b.z);
            rlVertex3f(0.0F, -0.018F, 0.0F);
            rlVertex3f(b.x, b.y, b.z);
            rlVertex3f(a.x, a.y, a.z);
        }
        rlEnd();
        rlPopMatrix();
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();

    // Small pollen motes are additive and deliberately sparse. A soft
    // camera-facing octagon avoids the square silhouette of a solid quad.
    constexpr int PollenCount = 24;
    constexpr float PollenHalfExtent = 19.0F;
    beginAdditiveParticlePass();
    rlBegin(RL_TRIANGLES);
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
        const float viewOffsetX = x - camera.position.x;
        const float viewOffsetZ = z - camera.position.z;
        const float viewDistanceSquared =
            viewOffsetX * viewOffsetX + viewOffsetZ * viewOffsetZ;
        if (viewDistanceSquared > 9.0F &&
            viewOffsetX * horizontalView.x +
                    viewOffsetZ * horizontalView.z <
                -0.5F) {
            continue;
        }
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
        emitSoftBillboardDisc(
            {x, y, z}, cameraRight, billboardUp, radius,
            {255, 239, 170, atmosphereAlpha(alpha)}, 8);
    }
    rlEnd();
    endAdditiveParticlePass();

    // Fireflies live in small world-anchored colonies around planted pond
    // shores. They fade in during dusk instead of following the player as a
    // uniform particle field.
    const float fireflyVisibility = smoothstep(
        0.28F, 0.70F, std::clamp(nightAmount, 0.0F, 1.0F));
    if (fireflyVisibility <= 0.01F) {
        return;
    }
    beginAdditiveParticlePass();
    rlBegin(RL_TRIANGLES);
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
            const float colonyOffsetX = center.x - camera.position.x;
            const float colonyOffsetZ = center.y - camera.position.z;
            if (colonyDistance > 4.0F &&
                colonyOffsetX * horizontalView.x +
                        colonyOffsetZ * horizontalView.z <
                    -2.0F) {
                continue;
            }
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
                emitSoftBillboardDisc(
                    {x, y, z}, cameraRight, billboardUp, 0.11F,
                    glow, 8);
                emitSoftBillboardDisc(
                    {x, y, z}, cameraRight, billboardUp, 0.025F,
                    core, 6);
            }
        }
        ++pondIndex;
    }
    rlEnd();
    endAdditiveParticlePass();
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
    Vector3 horizontalView = Vector3Subtract(
        camera.target, camera.position);
    horizontalView.y = 0.0F;
    horizontalView = Vector3LengthSqr(horizontalView) > 1e-6F
        ? Vector3Normalize(horizontalView)
        : Vector3{0.0F, 0.0F, -1.0F};
    const float maximumDistance = static_cast<float>(
        simulation_.terrain().config().terrainRenderDistance + 22.0);
    const float maximumDistanceSquared =
        maximumDistance * maximumDistance;
    const float time = static_cast<float>(snapshot.elapsedSeconds);

    rlDrawRenderBatchActive();
    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA, RL_ONE, RL_ZERO, RL_ZERO,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    rlDisableDepthMask();
    rlBegin(RL_TRIANGLES);
    for (const LootChestInstance& chest : snapshot.lootChests) {
        const float offsetX =
            static_cast<float>(chest.position.x) - camera.position.x;
        const float offsetZ =
            static_cast<float>(chest.position.z) - camera.position.z;
        const float distanceSquared =
            offsetX * offsetX + offsetZ * offsetZ;
        if (distanceSquared > maximumDistanceSquared ||
            (distanceSquared > 16.0F &&
             offsetX * horizontalView.x +
                     offsetZ * horizontalView.z <
                 -2.0F)) {
            continue;
        }
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

void App::drawResourceImpactOverlays(const Camera3D& camera) const {
    Vector3 cameraForward = Vector3Subtract(
        camera.target, camera.position);
    cameraForward = Vector3LengthSqr(cameraForward) > 0.000001F
        ? Vector3Normalize(cameraForward)
        : Vector3{0.0F, 0.0F, -1.0F};

    const auto drawPass = [&](bool glowPass) {
        BeginBlendMode(glowPass ? BLEND_ADDITIVE : BLEND_ALPHA);
        for (const PresentationEffect& effect : effects_) {
            if (effect.startDelayRemaining > 0.0 ||
                effect.type !=
                    PresentationEffectType::ResourceImpactDecal) {
                continue;
            }
            if (!resourceImpactTexture_.valid()) {
                continue;
            }
            const Vector3 hitPosition{
                static_cast<float>(effect.position.x),
                static_cast<float>(effect.position.y),
                static_cast<float>(effect.position.z),
            };
            if (Vector3DotProduct(
                    Vector3Subtract(hitPosition, camera.position),
                    cameraForward) <= 0.0F) {
                continue;
            }
            // Impact art is a readable screen-facing cue, not a decal glued
            // to the mesh. Object normals made it squash and tilt heavily on
            // low-poly facets, especially on tree canopies and crystals.
            const Vector3 surfaceNormal = Vector3Negate(cameraForward);
            Vector3 surfaceRight = Vector3CrossProduct(
                camera.up, surfaceNormal);
            if (Vector3LengthSqr(surfaceRight) <= 0.0001F) {
                continue;
            }
            surfaceRight = Vector3Normalize(surfaceRight);
            const Vector3 surfaceUp = Vector3Normalize(
                Vector3CrossProduct(surfaceNormal, surfaceRight));
            const float progress = std::clamp(static_cast<float>(
                1.0 - effect.remaining / effect.duration),
                0.0F, 1.0F);
            float scaleEnvelope = 1.0F;
            if (progress < 0.28F) {
                const float t = progress / 0.28F;
                const float easeOut =
                    1.0F - std::pow(1.0F - t, 3.0F);
                scaleEnvelope = 1.12F * easeOut;
            } else if (progress < 0.44F) {
                const float t = smoothstep(
                    0.28F, 0.44F, progress);
                scaleEnvelope = 1.12F + (1.0F - 1.12F) * t;
            } else if (progress > 0.70F) {
                scaleEnvelope = 1.0F - smoothstep(
                    0.70F, 1.0F, progress);
            }
            const float fade =
                1.0F - smoothstep(0.80F, 1.0F, progress);
            const float glow =
                (1.0F - smoothstep(0.18F, 1.0F, progress)) * 0.14F;
            const float size = glowPass
                ? effect.scale * 0.78F * scaleEnvelope
                : effect.scale * 0.70F * scaleEnvelope;
            const auto quad = projectedSurfaceQuad(
                Vector3Add(
                    hitPosition,
                    Vector3Scale(surfaceNormal, 0.028F)),
                surfaceRight, surfaceUp, size,
                0.0F,
                camera);
            if (!quad) {
                continue;
            }
            drawScreenTextureQuad(
                resourceImpactTexture_.get(), *quad,
                glowPass
                    ? Color{255, 202, 72, atmosphereAlpha(glow)}
                    : Color{255, 248, 220,
                            atmosphereAlpha(fade * 0.92F)});
        }
        EndBlendMode();
    };

    // Render after world ink: first a restrained glow, then the authored
    // alpha layer. Neither pass participates in world outline detection.
    drawPass(true);
    drawPass(false);

}

} // namespace ian
