#include "graphics/ChainLightningEffect.hpp"
#include "graphics/LaserBeamEffect.hpp"
#include "combat/LaserAttack.hpp"
#include "combat/AttackCollision.hpp"
#include "buildings/TowerRig.hpp"
#include "graphics/RoyalBossEffect.hpp"
#include "graphics/EnemyMagicEffect.hpp"
#include "graphics/PropBreakEffect.hpp"
#include "graphics/BuildingPlacementEffect.hpp"
#include "graphics/BuildingDestructionEffect.hpp"
#include "app/App.hpp"
#include "graphics/StumpRootEffect.hpp"
#include "app/AppPoisonDrawing.hpp"
#include "app/AppEffectDrawing.hpp"
#include "app/AppFreezeDrawing.hpp"
#include "app/AppRenderSupport.hpp"
#include "graphics/BuildingFeedbackEffect.hpp"

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

void emitGunBulletImpactRing(
    Vector3 center, Vector3 right, Vector3 up,
    float radius, float width, Color color) {
    constexpr std::array<Vector2, 13> Circle{{
        {1.0F, 0.0F}, {0.8660254F, 0.5F},
        {0.5F, 0.8660254F}, {0.0F, 1.0F},
        {-0.5F, 0.8660254F}, {-0.8660254F, 0.5F},
        {-1.0F, 0.0F}, {-0.8660254F, -0.5F},
        {-0.5F, -0.8660254F}, {0.0F, -1.0F},
        {0.5F, -0.8660254F}, {0.8660254F, -0.5F},
        {1.0F, 0.0F},
    }};
    const auto ringPoint = [center, right, up](
                               Vector2 unit, float distance) {
        return Vector3Add(center, Vector3Add(
            Vector3Scale(right, unit.x * distance),
            Vector3Scale(up, unit.y * distance)));
    };
    Color transparent = color;
    transparent.a = 0;
    const float innerRadius = std::max(0.0F, radius - width);
    for (std::size_t index = 1; index < Circle.size(); ++index) {
        const Vector3 innerA = ringPoint(Circle[index - 1], innerRadius);
        const Vector3 outerA = ringPoint(Circle[index - 1], radius);
        const Vector3 innerB = ringPoint(Circle[index], innerRadius);
        const Vector3 outerB = ringPoint(Circle[index], radius);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(innerA.x, innerA.y, innerA.z);
        rlColor4ub(transparent.r, transparent.g,
                   transparent.b, transparent.a);
        rlVertex3f(outerA.x, outerA.y, outerA.z);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(innerB.x, innerB.y, innerB.z);
        rlVertex3f(innerB.x, innerB.y, innerB.z);
        rlColor4ub(transparent.r, transparent.g,
                   transparent.b, transparent.a);
        rlVertex3f(outerA.x, outerA.y, outerA.z);
        rlVertex3f(outerB.x, outerB.y, outerB.z);
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
    Vector3 origin, float groundHeight, const SurfaceEffectPlane& surface, float progress, float scale,
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
        const Vector3 center{origin.x, groundHeight, origin.z};
        const Color color{255, 226, 146, atmosphereAlpha(ringFade * 0.72F)};
        rlNormal3f(surface.normal.x, surface.normal.y, surface.normal.z);
        for (int index = 0; index < Segments; ++index) {
            const float a = static_cast<float>(index) * 2.0F * PI / Segments;
            const float b = static_cast<float>(index + 1) * 2.0F * PI / Segments;
            const auto point = [center,&surface](float angle, float distance) {
                return surface.point(center,std::cos(angle)*distance,std::sin(angle)*distance);
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

    if (std::ranges::any_of(effects_,[](const auto& e){return e.type==PresentationEffectType::StumpRootWave;})) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState rootMaterial{};rootMaterial.unlitAmount=.28F;rootMaterial.screenAoAmount=0;
        rootMaterial.inkOutlineEligible=false;rootMaterial.inkOccluder=true;
        renderer_->setWorldMaterial(rootMaterial);
        renderer_->beginColorOnlyWorldEffects();
        rlDrawRenderBatchActive();rlSetTexture(0);rlDisableBackfaceCulling();rlBegin(RL_TRIANGLES);
        for (const auto& effect:effects_) {
            if (effect.type!=PresentationEffectType::StumpRootWave || !effect.targetPosition || effect.remaining<=0) continue;
            const Vector3 origin{static_cast<float>(effect.position.x),static_cast<float>(effect.position.y),static_cast<float>(effect.position.z)};
            auto direction=Vector3Normalize(Vector3{static_cast<float>(effect.targetPosition->x-effect.position.x),0,
                static_cast<float>(effect.targetPosition->z-effect.position.z)});
            stump_root_effect::emitWave(origin,direction,effect.scale,static_cast<float>(effect.duration-effect.remaining),
                static_cast<unsigned int>(effect.variant),[this](float x,float z){return simulation_.terrain().getHeight(x,z);});
        }
        rlEnd();rlDrawRenderBatchActive();rlEnableBackfaceCulling();
        renderer_->endColorOnlyWorldEffects();renderer_->endWorldShader();
    }
    const auto& itemSnapshot = simulation_.snapshot();
    if (!itemSnapshot.itemPoisonClouds.empty() || std::ranges::any_of(itemSnapshot.enemies,
        [](const EnemyInstance& e) { return e.active && e.markRemaining > 0; })) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState material{};
        material.unlitAmount = 1; material.screenAoAmount = 0;
        material.inkOutlineEligible = false; material.inkOccluder = true;
        renderer_->setWorldMaterial(material);
        renderer_->beginColorOnlyWorldEffects();
        app_freeze_drawing::beginBatch();
        std::size_t drawn = 0;
        for (const auto& cloud : itemSnapshot.itemPoisonClouds) {
            Vector3 center{static_cast<float>(cloud.position.x), 0, static_cast<float>(cloud.position.z)};
            center.y = static_cast<float>(simulation_.terrain().getHeight(center.x, center.z)) + 0.15F;
            if (Vector3DistanceSqr(center, camera.position) > lighting.fogEnd * lighting.fogEnd || drawn++ >= 32) continue;
            const float age = static_cast<float>(cloud.duration - cloud.remaining);
            const float fade = std::min(1.0F, age * 5) * std::min(1.0F, static_cast<float>(cloud.remaining) * 3);
            app_poison_drawing::emitPoison(center, cameraRight, billboardUp, Vector3Negate(cameraForward),
                static_cast<float>(cloud.radius) * 0.7F, 1.0F,
                static_cast<unsigned int>(drawn * 179), itemSnapshot.elapsedSeconds, fade, false);
        }
        std::size_t marks = 0;
        for (const auto& enemy : itemSnapshot.enemies) {
            if (!enemy.active || enemy.markRemaining <= 0 || marks++ >= 32) continue;
            Vector3 base = enemyRenderPosition(enemy);
            base.y += static_cast<float>(simulation_.terrain().getHeight(enemy.position.x, enemy.position.z));
            if (Vector3DistanceSqr(base, camera.position) > lighting.fogEnd * lighting.fogEnd) continue;
            const auto bounds = renderer_->enemyWorldBounds(enemyModelVisual(enemy), base,
                static_cast<float>(enemy.yaw), enemyVisualScale(enemy.type) * (enemy.eliteAffixes ? 1.08F : 1.0F));
            Vector3 center{(bounds.min.x+bounds.max.x)*.5F, (bounds.min.y+bounds.max.y)*.5F,
                (bounds.min.z+bounds.max.z)*.5F};
            const float radius = std::max(bounds.max.x-bounds.min.x, bounds.max.z-bounds.min.z)*.55F;
            center = Vector3Add(center, Vector3Scale(Vector3Negate(cameraForward),radius+.04F));
            const float size = .12F + .015F * std::sin(static_cast<float>(itemSnapshot.elapsedSeconds)*6);
            const auto point = [&](float x,float y) { return Vector3Add(center,
                Vector3Add(Vector3Scale(cameraRight,x),Vector3Scale(billboardUp,y))); };
            const Color gold{255,210,65,230};
            app_freeze_drawing::triangle(point(-size*1.5F,size),point(-size*1.5F,-size),point(-size*.7F,0),gold);
            app_freeze_drawing::triangle(point(size*1.5F,-size),point(size*1.5F,size),point(size*.7F,0),gold);
        }
        app_freeze_drawing::endBatch();
        renderer_->endColorOnlyWorldEffects();
        renderer_->endWorldShader();
    }
    if (std::ranges::any_of(effects_, [](const PresentationEffect& effect) {
            return effect.type == PresentationEffectType::EnemyMagicCast ||
                   effect.type == PresentationEffectType::EnemyMagicImpact ||
                   effect.type == PresentationEffectType::RoyalImpact ||
                   effect.type == PresentationEffectType::Debris ||
                   (effect.type == PresentationEffectType::BuildingPlaced && effect.variant == 0) ||
                   effect.type == PresentationEffectType::PropBreak;
        })) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState material{};
        material.unlitAmount = 1;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        material.inkOccluder = true;
        renderer_->setWorldMaterial(material);
        renderer_->beginColorOnlyWorldEffects();
        fire_wand_effect::beginBatch();
        int constructionBursts = 0;
        int destructionBursts = 0;
        for (const auto& effect : effects_) {
            if (effect.startDelayRemaining > 0 || effect.duration <= 0 ||
                (effect.type != PresentationEffectType::EnemyMagicCast &&
                 effect.type != PresentationEffectType::EnemyMagicImpact &&
                 effect.type != PresentationEffectType::RoyalImpact &&
                 effect.type != PresentationEffectType::Debris &&
                 effect.type != PresentationEffectType::BuildingPlaced &&
                 effect.type != PresentationEffectType::PropBreak)) continue;
            const Vector3 center = enemy_magic_effect::point(effect.position);
            const Vector3 delta = Vector3Subtract(center, camera.position);
            if (Vector3LengthSqr(delta) > 1600 || Vector3DotProduct(delta, cameraForward) < -1) continue;
            if (effect.type == PresentationEffectType::Debris) {
                if (effect.remaining <= 0 || effect.remaining >= effect.duration || destructionBursts >= 12) continue;
                ++destructionBursts;
                building_destruction_effect::burst(center,camera.position,
                    static_cast<float>(1-effect.remaining/effect.duration),effect.scale,
                    effect.entityId ? effect.entityId->index :
                        static_cast<std::uint32_t>(std::abs(center.x*73+center.z*137)),
                    renderer_->settings().particles,effect.variant);
                continue;
            }
            if (effect.type == PresentationEffectType::BuildingPlaced) {
                if (effect.variant != 0 || effect.remaining <= 0 ||
                    effect.remaining >= effect.duration * .88 || constructionBursts >= 12) continue;
                ++constructionBursts;
                building_placement_effect::burst(center, camera.position,
                    static_cast<float>(1 - effect.remaining / effect.duration), effect.scale,
                    effect.entityId ? effect.entityId->index :
                        static_cast<std::uint32_t>(std::abs(center.x * 73 + center.z * 137)),
                    renderer_->settings().particles, userSettings_.accessibility.reduceFlashes);
                continue;
            }
            if(effect.type==PresentationEffectType::PropBreak) {
                prop_break_effect::burst(center,camera.position,
                    static_cast<float>(1-effect.remaining/effect.duration),effect.scale,
                    effect.entityId?effect.entityId->index:0,renderer_->settings().particles,
                    effect.resourceType==ResourceType::Barrel);
                continue;
            }
            if(effect.type==PresentationEffectType::RoyalImpact) {
                royal_boss_effect::impact(center,camera.position,
                    static_cast<float>(1-effect.remaining/effect.duration),effect.scale,renderer_->settings().particles);
                continue;
            }
            enemy_magic_effect::burst(center, camera.position,
                static_cast<float>(1 - effect.remaining / effect.duration), effect.scale,
                effect.type == PresentationEffectType::EnemyMagicImpact,
                renderer_->settings().particles,
                effect.entityId ? effect.entityId->index : 0, effect.variant==1, effect.variant==2);
        }
        fire_wand_effect::endBatch();
        renderer_->endColorOnlyWorldEffects();
        renderer_->endWorldShader();
    }
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
            // Keep fragments from filling the view when they pass close to
            // the first-person camera after leaving the impact point.
            const float cameraDistanceScale = std::clamp(
        Vector3Distance(position, camera.position) / 1.0F,
        0.8F, 1.0F);
            const float visibleScale =
                effect.scale * disappearScale * cameraDistanceScale;
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
                    visibleScale, WHITE));
            } else if (effect.resourceType == ResourceType::Crystal) {
                static_cast<void>(renderer_->drawCrystalFragment(
                    position, fragmentAxis,
                    fragmentAngleRadians * RAD2DEG,
                    visibleScale, WHITE));
            } else {
                static_cast<void>(renderer_->drawDecorativeRockFragment(
                    position, rotationAxis, rotationDegrees,
                    visibleScale,
                    seed % 4U, WHITE));
            }
        }
        renderer_->endWorldShader();
    }

    // Small authored-looking chips share one depth-tested color batch.
    // No per-chip models, render textures or gameplay particles are created.
    const bool buildingHits = renderer_->settings().particles && std::ranges::any_of(effects_,
        [](const PresentationEffect& effect) { return effect.type == PresentationEffectType::BuildingDamaged; });
    if (buildingHits) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState material{};
        material.unlitAmount = .35F;
        material.inkOutlineEligible = false;
        renderer_->setWorldMaterial(material);
        renderer_->beginColorOnlyWorldEffects();
        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthMask();
        rlDisableBackfaceCulling();
        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);
        for (const auto& effect : effects_) {
            if (effect.type != PresentationEffectType::BuildingDamaged || !effect.targetPosition) continue;
            const Vector3 origin{static_cast<float>(effect.targetPosition->x),
                static_cast<float>(effect.targetPosition->y), static_cast<float>(effect.targetPosition->z)};
            const Vector3 delta = Vector3Subtract(origin, camera.position);
            if (Vector3LengthSqr(delta) > 45 * 45 || Vector3DotProduct(delta, cameraForward) < 0) continue;
            const float age = static_cast<float>(effect.duration - effect.remaining);
            const float progress = std::clamp(age / static_cast<float>(effect.duration), 0.0F, 1.0F);
            const auto seed = effect.entityId ? effect.entityId->index : 0U;
            const int count = userSettings_.accessibility.reduceFlashes ? 4 : 8;
            for (int i = 0; i < count; ++i) {
                const float angle = (static_cast<float>(i) + effectUnit(static_cast<int>(seed), i + 61)) * 2.39996F;
                const float speed = (.9F + effectUnit(static_cast<int>(seed), i + 91) * 1.5F) * effect.scale;
                Vector3 position = Vector3Add(origin, Vector3Scale(cameraRight, std::cos(angle) * speed * age));
                position.y += (.7F + std::sin(angle) * .6F) * speed * age - 3.5F * age * age;
                position = Vector3Add(position, Vector3Scale(cameraForward, -.3F * age));
                const float radius = (.025F + effectUnit(static_cast<int>(seed), i + 11) * .035F) *
                    effect.scale * (1 - progress * progress);
                Color color = effect.variant == 1 ? Color{207, 135, 65, 255} : Color{176, 196, 207, 255};
                if (i % 3 == 0) color = {255, 216, 142, 255};
                color.a = atmosphereAlpha(1 - progress);
                rlColor4ub(color.r, color.g, color.b, color.a);
                for (int corner = 0; corner < 3; ++corner) {
                    const float rotation = angle + age * 11 + static_cast<float>(corner) * 2.094395F;
                    const Vector3 point = Vector3Add(position, Vector3Add(
                        Vector3Scale(cameraRight, std::cos(rotation) * radius),
                        Vector3Scale(billboardUp, std::sin(rotation) * radius * .65F)));
                    rlVertex3f(point.x, point.y, point.z);
                }
            }
        }
        rlEnd();
        rlEnableBackfaceCulling();
        rlEnableDepthMask();
        EndBlendMode();
        renderer_->endColorOnlyWorldEffects();
        renderer_->setWorldMaterial({});
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
             effect.type !=
                 PresentationEffectType::GunBulletImpact &&
             effect.type != PresentationEffectType::EnemyHitImpact &&
             effect.type != PresentationEffectType::BuildingDamaged)) {
            continue;
        }
        const float progress = std::clamp(static_cast<float>(
            1.0 - effect.remaining / effect.duration), 0.0F, 1.0F);
        const Vector3 origin{
            static_cast<float>(effect.position.x),
            static_cast<float>(effect.position.y),
            static_cast<float>(effect.position.z),
        };
        if (effect.type == PresentationEffectType::BuildingDamaged) {
            if (!effect.targetPosition) continue;
            const Vector3 contact{static_cast<float>(effect.targetPosition->x),
                static_cast<float>(effect.targetPosition->y), static_cast<float>(effect.targetPosition->z)};
            const Vector3 delta = Vector3Subtract(contact, camera.position);
            if (Vector3LengthSqr(delta) > 45 * 45 || Vector3DotProduct(delta, cameraForward) < 0) continue;
            const float fade = 1 - smoothstep(.05F, .48F, progress);
            const float reduced = userSettings_.accessibility.reduceFlashes ? .24F : 1.0F;
            emitSoftBillboardDisc(contact, cameraRight, billboardUp,
                (.07F + progress * .32F) * effect.scale,
                {255, 204, 113, atmosphereAlpha(fade * .55F * reduced)});
            if (!userSettings_.accessibility.reduceFlashes) {
                emitGunBulletImpactRing(contact, cameraRight, billboardUp,
                    (.035F + progress * .45F) * effect.scale, .016F * fade,
                    {255, 230, 171, atmosphereAlpha(fade * .65F)});
                for (int i = 0; i < 5; ++i) {
                    const float angle = static_cast<float>(i) * 2.39996F;
                    const Vector3 direction = Vector3Add(Vector3Scale(cameraRight, std::cos(angle)),
                        Vector3Scale(billboardUp, std::sin(angle)));
                    emitImpactStreak(Vector3Add(contact, Vector3Scale(direction, (.05F + progress * .24F) * effect.scale)),
                        cameraRight, billboardUp, angle,
                        .13F * fade * effect.scale, .018F * fade,
                        {255, 225, 157, atmosphereAlpha(fade)});
                }
            }
            continue;
        }
        if (effect.type == PresentationEffectType::GunBulletImpact) {
            // The shot target is inside the enemy's bounds. Move the small
            // billboard toward the camera along the view ray so the model
            // does not hide the impact without shifting its screen position.
            const Vector3 visibleOrigin = Vector3Subtract(
                origin, Vector3Scale(cameraForward, 0.75F));
            const float fade = 1.0F - smoothstep(0.16F, 1.0F, progress);
            const float flash = 1.0F - smoothstep(0.0F, 0.46F, progress);
            const Color tint{effect.bulletAppearance.color.r,
                             effect.bulletAppearance.color.g,
                             effect.bulletAppearance.color.b, 255};
            const auto impactColor = [tint](float brightness, unsigned char alpha) {
                Color color = ColorLerp(tint, WHITE, brightness);
                color.a = alpha;
                return color;
            };
            emitSoftBillboardDisc(
                visibleOrigin, cameraRight, billboardUp,
                effect.scale * (0.13F + progress * 0.11F),
                impactColor(0.16F, atmosphereAlpha(flash * 0.68F)));
            emitSoftBillboardDisc(
                visibleOrigin, cameraRight, billboardUp,
                effect.scale * (0.065F + progress * 0.025F),
                impactColor(0.55F, atmosphereAlpha(flash * 0.92F)), 6);
            emitGunBulletImpactRing(
                visibleOrigin, cameraRight, billboardUp,
                effect.scale * (0.10F + progress * 0.28F),
                effect.scale * 0.045F,
                impactColor(0.0F, atmosphereAlpha(fade * 0.72F)));

            Vector3 travel{1.0F, 0.0F, 0.0F};
            if (effect.targetPosition) {
                travel = Vector3Subtract(origin, {
                    static_cast<float>(effect.targetPosition->x),
                    static_cast<float>(effect.targetPosition->y),
                    static_cast<float>(effect.targetPosition->z),
                });
            }
            const float across = Vector3DotProduct(travel, cameraRight);
            const float upward = Vector3DotProduct(travel, billboardUp);
            const float baseAngle = across * across + upward * upward >
                    0.0001F
                ? std::atan2(upward, across) : 0.0F;
            const int sparkCount = effect.variant != 0 ? 2 : 4;
            for (int index = 0; index < sparkCount; ++index) {
                const float offset =
                    (static_cast<float>(index) -
                     static_cast<float>(sparkCount - 1) * 0.5F) * 0.34F;
                const float angle = baseAngle + offset;
                const Vector3 sparkDirection = Vector3Add(
                    Vector3Scale(cameraRight, std::cos(angle)),
                    Vector3Scale(billboardUp, std::sin(angle)));
                const Vector3 sparkOrigin = Vector3Add(
                    visibleOrigin, Vector3Scale(sparkDirection,
                        effect.scale * (0.06F + progress * 0.23F)));
                emitImpactStreak(
                    sparkOrigin, cameraRight, billboardUp, angle,
                    effect.scale * (0.16F - progress * 0.07F),
                    effect.scale * 0.018F,
                    impactColor(0.07F, atmosphereAlpha(fade * 0.83F)));
            }
            continue;
        }
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
                origin.x, origin.z)), SurfaceEffectPlane{
                    enemy_magic_effect::point(simulation_.terrain().getNormal(origin.x,origin.z)),
                    &simulation_.terrain()}, progress, effect.scale, seed, draw.detailed,
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
    if (std::ranges::any_of(effects_, [](const auto& effect) {
            return effect.type == PresentationEffectType::ChainLightning && effect.startDelayRemaining <= 0;
        })) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState material{}; material.unlitAmount=1;material.screenAoAmount=0;
        material.inkOutlineEligible=false;material.inkOccluder=true;renderer_->setWorldMaterial(material);
        renderer_->beginColorOnlyWorldEffects();
        fire_wand_effect::beginBatch();
        int remaining=chain_lightning_effect::MaximumVisibleArcs;
        for(auto it=effects_.rbegin();it!=effects_.rend() && remaining>0;++it) {
            const auto& effect=*it;
            if(effect.type!=PresentationEffectType::ChainLightning || effect.startDelayRemaining>0 || !effect.targetPosition || effect.duration<=0) continue;
            --remaining;
            const auto seed=static_cast<std::uint64_t>(effect.variant)*37U + (effect.entityId?effect.entityId->index:0U);
            chain_lightning_effect::emit(chain_lightning_effect::point(effect.position),
                chain_lightning_effect::point(*effect.targetPosition),camera.position,
                static_cast<float>(effect.duration-effect.remaining),static_cast<float>(effect.duration),seed,effect.scale,static_cast<float>(itemSnapshot.elapsedSeconds));
        }
        fire_wand_effect::endBatch();renderer_->endColorOnlyWorldEffects();renderer_->endWorldShader();
    }
    if (!itemSnapshot.laserBeams.empty()) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;
        material.inkOutlineEligible=false;material.inkOccluder=true;renderer_->setWorldMaterial(material);
        renderer_->beginColorOnlyWorldEffects();fire_wand_effect::beginBatch();
        int budget=laser_beam_effect::MaximumVisibleBeams;
        const float interpolation=worldInterpolationAlpha(itemSnapshot, fixedStep_.interpolationAlpha());
        const auto targetPoint=[&](EntityId id,Vec3 fallback) {
            const auto enemy=std::ranges::find(itemSnapshot.enemies,id,&EnemyInstance::id);
            return enemy!=itemSnapshot.enemies.end() && enemy->active ? attackTargetPosition(*enemy) : fallback;
        };
        // First show every active tower, then spend the remaining budget on
        // extra parallel rays and chains. A dense volley cannot hide other towers.
        for(const auto& tower:itemSnapshot.towers) {
            if(tower.type!=BuildingType::LaserTower || tower.laserRemaining<=0 || budget<=0)continue;
            const auto building=std::ranges::find(itemSnapshot.buildings,tower.buildingId,&BuildingInstance::id);
            if(building==itemSnapshot.buildings.end())continue;
            const auto start=laserMuzzleWorldPosition(buildingWorldPosition(*building),
                towerYaw(itemSnapshot,*building,interpolation),towerPitch(itemSnapshot,*building,interpolation));
            auto end=tower.targetId?targetPoint(*tower.targetId,tower.laserEnd):tower.laserEnd;
            auto origin=chain_lightning_effect::point(start);
            auto finish=chain_lightning_effect::point(end);
            if(Vector3DistanceSqr(origin,camera.position)>lighting.fogEnd*lighting.fogEnd)continue;
            if(effectiveAttackCount(tower.attack.stats.quantity,1)>1) {
                origin=laser_beam_effect::parallelPoint(origin,camera.position,-.5F,.16F);
                finish=laser_beam_effect::parallelPoint(finish,camera.position,-.5F,.36F);
            }
            --budget;
            laser_beam_effect::emit(origin,finish,
                camera.position,float(tower.laserFocusSeconds/LaserFocusRampSeconds),
                float(itemSnapshot.elapsedSeconds),float(std::min(1.0,tower.laserRemaining/.08)));
        }
        for(int priority=0;priority<2;++priority)for(const auto& beam:itemSnapshot.laserBeams) {
            if(!beam.fromTarget && beam.ray==0)continue;
            if((priority==0 && beam.fromTarget) || (priority==1 && !beam.fromTarget))continue;
            if(beam.remaining<=0 || budget<=0)continue;
            const auto building=std::ranges::find(itemSnapshot.buildings,beam.sourceId,&BuildingInstance::id);
            if(building==itemSnapshot.buildings.end()) continue;
            const auto start=beam.fromTarget ? targetPoint(*beam.fromTarget,beam.origin)
                : laserMuzzleWorldPosition(buildingWorldPosition(*building),
                    towerYaw(itemSnapshot,*building,interpolation),towerPitch(itemSnapshot,*building,interpolation));
            auto origin=chain_lightning_effect::point(start);
            auto end=chain_lightning_effect::point(targetPoint(beam.targetId,beam.end));
            if(Vector3DistanceSqr(origin,camera.position)>lighting.fogEnd*lighting.fogEnd) continue;
            --budget;
            const float lane=beam.rayCount<=1?0:float(beam.ray)/float(beam.rayCount-1)-.5F;
            origin=laser_beam_effect::parallelPoint(origin,camera.position,lane,beam.fromTarget?.36F:.16F);
            end=laser_beam_effect::parallelPoint(end,camera.position,lane,.36F);
            laser_beam_effect::emit(origin,end,camera.position,float(beam.focus),
                float(itemSnapshot.elapsedSeconds)+float(beam.ray)*.1F,float(std::min(1.0,beam.remaining/.08)));
        }
        fire_wand_effect::endBatch();renderer_->endColorOnlyWorldEffects();renderer_->endWorldShader();
    }
    for (const auto& effect : effects_) {
        // Placement has its own batched dust; burning is drawn on the enemy
        // in drawWorldEnemies. Neither should emit generic destruction cubes.
        if (effect.startDelayRemaining > 0.0 ||
            effect.type == PresentationEffectType::RoyalImpact ||
            effect.type == PresentationEffectType::PropBreak ||
            effect.type == PresentationEffectType::Debris ||
            effect.type == PresentationEffectType::EnemyMagicCast ||
            effect.type == PresentationEffectType::EnemyMagicImpact ||
            effect.type == PresentationEffectType::BuildingPlaced ||
            effect.type == PresentationEffectType::EnemyBurn ||
            effect.type == PresentationEffectType::IceImpact ||
            effect.type == PresentationEffectType::IceCrack ||
            effect.type == PresentationEffectType::ChainLightning ||
            effect.type == PresentationEffectType::Explosion ||
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
        if (effect.type == PresentationEffectType::StumpRootWave || effect.type == PresentationEffectType::Hit ||
            effect.type == PresentationEffectType::TurretMuzzleFlash ||
            effect.type == PresentationEffectType::GunBulletImpact ||
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
        if (effect.type == PresentationEffectType::ChestOpened) {
            if (!renderer_->settings().particles) continue;
            const float fade = 1.0F - smoothstep(0.35F, 1.0F, progress);
            const Vector3 center{origin.x, origin.y + 0.60F, origin.z};
            BeginBlendMode(BLEND_ADDITIVE);
            constexpr int SparkCount = 16;
            for (int index = 0; index < SparkCount; ++index) {
                const float angle = 2.0F * PI *
                    (static_cast<float>(index) / SparkCount +
                     effectUnit(index, 730) * 0.035F);
                const float distance = progress *
                    (0.55F + effectUnit(index, 731) * 0.75F);
                const float rise = progress *
                    (0.15F + effectUnit(index, 732) * 0.9F);
                DrawSphereEx({center.x + std::cos(angle) * distance,
                              center.y + rise,
                              center.z + std::sin(angle) * distance},
                             0.025F + effectUnit(index, 733) * 0.035F,
                             4, 4, Fade(index % 4 == 0 ? WHITE : GOLD,
                                        fade * 0.8F));
            }
            DrawSphere(center, 0.10F + 0.24F * (1.0F - progress),
                       Fade(GOLD, fade * 0.30F));
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
        if (effect.type == PresentationEffectType::BuildingUpgrade ||
            effect.type == PresentationEffectType::BuildingRepaired) {
            renderer_->beginColorOnlyWorldEffects();
            drawBuildingFeedbackEffect(
                origin, camera.position, progress, effect.scale,
                effect.type == PresentationEffectType::BuildingUpgrade
                    ? BuildingFeedbackKind::Upgrade : BuildingFeedbackKind::Repair,
                static_cast<std::uint32_t>(effect.variant));
            renderer_->endColorOnlyWorldEffects();
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
            // A compact, opaque slime pop: no ground rings, glowing orb or
            // additive wash. Shrinking fragments leave the babies readable.
            constexpr int GlobCount = 7;
            const float flight = progress * (2.0F - progress);
            const float shrink = 1.0F - smoothstep(.28F, 1.0F, progress);
            for (int index = 0; index < GlobCount; ++index) {
                const float angle = (static_cast<float>(index) + effectUnit(index, 91) * .4F) *
                    (2.0F * PI / static_cast<float>(GlobCount));
                const float distance = effect.scale * flight *
                    (.45F + effectUnit(index, 92) * .55F);
                const float arc = std::sin(progress * PI) *
                    (.22F + effectUnit(index, 93) * .38F);
                const float size = effect.scale * shrink *
                    (.055F + effectUnit(index, 94) * .07F);
                if (size < .002F) continue;
                const Vector3 center{origin.x + std::cos(angle) * distance,
                    origin.y + .32F + arc, origin.z + std::sin(angle) * distance};
                const Color slime{100, 202, 90, 255};
                rlPushMatrix();
                rlTranslatef(center.x, center.y, center.z);
                rlRotatef(angle * RAD2DEG, 0, 1, 0);
                rlRotatef(-30 + progress * 95, 0, 0, 1);
                rlScalef(.82F, 1.35F - progress * .4F, .82F);
                DrawSphereEx({}, size, 6, 8, slime);
                DrawSphereEx({-size * .24F, size * .30F, size * .72F},
                    size * .23F, 4, 6, {211, 246, 169, 255});
                rlPopMatrix();
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
