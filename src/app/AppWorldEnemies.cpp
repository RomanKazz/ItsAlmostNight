#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "graphics/CameraCulling.hpp"
#include "presentation/PresentationEffectQueries.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

using namespace app_detail;

namespace {

void emitFlameVertex(Vector3 position, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(position.x, position.y, position.z);
}

void emitFlameTriangle(
    Vector3 first, Color firstColor,
    Vector3 second, Color secondColor,
    Vector3 third, Color thirdColor) {
    emitFlameVertex(first, firstColor);
    emitFlameVertex(second, secondColor);
    emitFlameVertex(third, thirdColor);
}

void emitFlameLobe(
    Vector3 base, Vector3 cameraRight, float width,
    float height, float sway, float amount) {
    const Vector3 left = Vector3Add(
        base, Vector3Scale(cameraRight, -width));
    const Vector3 right = Vector3Add(
        base, Vector3Scale(cameraRight, width));
    Vector3 middle = base;
    middle.y += height * 0.52F;
    middle = Vector3Add(
        middle, Vector3Scale(cameraRight, sway * 0.34F));
    const Vector3 middleLeft = Vector3Add(
        middle, Vector3Scale(cameraRight, -width * 0.58F));
    const Vector3 middleRight = Vector3Add(
        middle, Vector3Scale(cameraRight, width * 0.58F));
    Vector3 tip = base;
    tip.y += height;
    tip = Vector3Add(tip, Vector3Scale(cameraRight, sway));
    const auto alpha = [amount](float multiplier) {
        return static_cast<unsigned char>(std::lround(
            255.0F * std::clamp(amount * multiplier, 0.0F, 1.0F)));
    };
    const Color hot{255, 250, 194, alpha(1.0F)};
    const Color orange{255, 116, 12, alpha(1.0F)};
    const Color ember{255, 42, 3, alpha(0.72F)};
    const Color clear{170, 20, 4, 0};
    emitFlameTriangle(left, hot, right, hot, middleRight, orange);
    emitFlameTriangle(left, hot, middleRight, orange, middleLeft, orange);
    emitFlameTriangle(
        middleLeft, orange, middleRight, ember, tip, clear);
    const Vector3 innerLeft = Vector3Add(
        base, Vector3Scale(cameraRight, -width * 0.43F));
    const Vector3 innerRight = Vector3Add(
        base, Vector3Scale(cameraRight, width * 0.43F));
    Vector3 innerTip = base;
    innerTip.y += height * 0.72F;
    innerTip = Vector3Add(
        innerTip, Vector3Scale(cameraRight, sway * 0.48F));
    emitFlameTriangle(
        innerLeft, {255, 255, 226, alpha(1.0F)},
        innerRight, {255, 242, 128, alpha(1.0F)},
        innerTip, {255, 142, 18, alpha(0.18F)});
}

void emitEmber(
    Vector3 center, Vector3 cameraRight,
    float size, float amount) {
    const Vector3 left = Vector3Add(
        center, Vector3Scale(cameraRight, -size));
    const Vector3 right = Vector3Add(
        center, Vector3Scale(cameraRight, size));
    Vector3 bottom = center;
    bottom.y -= size * 1.55F;
    Vector3 top = center;
    top.y += size * 1.55F;
    const auto alpha = static_cast<unsigned char>(std::lround(
        255.0F * std::clamp(amount, 0.0F, 1.0F)));
    const Color core{255, 246, 183, alpha};
    const Color edge{255, 94, 12,
                     static_cast<unsigned char>(alpha * 0.72F)};
    const Color clear{190, 24, 2, 0};
    emitFlameTriangle(bottom, clear, right, edge, top, core);
    emitFlameTriangle(bottom, clear, top, core, left, edge);
}

float enemyFlameScale(EnemyType type) {
    switch (type) {
    case EnemyType::Boss: return 1.65F;
    case EnemyType::Heavy: return 1.25F;
    case EnemyType::Splitter: return 1.35F;
    case EnemyType::Splitling: return 0.62F;
    case EnemyType::Fast: return 0.82F;
    case EnemyType::Flying: return 0.78F;
    case EnemyType::Basic:
    case EnemyType::Ranged:
    case EnemyType::Sapper:
        return 1.0F;
    }
    return 1.0F;
}

} // namespace

void App::drawWorldEnemies(
    const SimulationSnapshot& snapshot,
    const Camera3D& camera) {
    const auto enemyRenderStart = PerformanceClock::now();
    performanceStats_.visibleEnemies = 0U;
    enemyDrawInstances_.clear();
    enemyDrawInstances_.reserve(snapshot.enemies.size());
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const auto horizontalView =
        camera_culling::horizontalView(camera);
    const float maximumDrawDistance = static_cast<float>(
        simulation_.terrain().config().terrainRenderDistance + 24.0);
    const auto enemyVisible =
        [camera, horizontalView, maximumDrawDistance](
            Vector3 position, float radius) {
            const float offsetX = position.x - camera.position.x;
            const float offsetZ = position.z - camera.position.z;
            return camera_culling::visibleInHorizontalRange(
                offsetX, offsetZ, horizontalView,
                maximumDrawDistance + radius, radius);
        };
    constexpr float EnemyFullDetailDistance = 20.0F;
    constexpr float EnemyFullDetailDistanceSquared =
        EnemyFullDetailDistance * EnemyFullDetailDistance;
    enemyHitFlashById_.clear();
    enemyHitFlashById_.reserve(effects_.size());
    enemyBurnAmountById_.clear();
    enemyBurnAmountById_.reserve(effects_.size());
    const auto effectKey = [](EntityId id) {
        return
            (static_cast<std::uint64_t>(id.generation) << 32U) |
            static_cast<std::uint64_t>(id.index);
    };
    for (const PresentationEffect& effect : effects_) {
        if (!effect.entityId || effect.duration <= 0.0 ||
            effect.startDelayRemaining > 0.0) {
            continue;
        }
        const std::uint64_t key = effectKey(*effect.entityId);
        const float remainingFraction = std::clamp(
            static_cast<float>(effect.remaining / effect.duration),
            0.0F, 1.0F);
        if (effect.type == PresentationEffectType::EnemyHitImpact) {
            const float flashStrength = std::clamp(
                std::pow(remainingFraction, 0.42F) * effect.scale * 0.48F,
                0.0F, 1.0F);
            auto [entry, inserted] =
                enemyHitFlashById_.try_emplace(
                    key, flashStrength);
            if (!inserted) {
                entry->second = std::max(
                    entry->second, flashStrength);
            }
        } else if (
            effect.type == PresentationEffectType::EnemyBurn) {
            const float fade = std::clamp(
                remainingFraction / 0.28F, 0.0F, 1.0F);
            auto [entry, inserted] =
                enemyBurnAmountById_.try_emplace(key, fade);
            if (!inserted) {
                entry->second = std::max(entry->second, fade);
            }
        }
    }
    for (const auto& enemy : snapshot.enemies) {
        const bool splitting =
            enemy.type == EnemyType::Splitter &&
            enemy.splitAnimationRemaining > 0.0;
        if (!enemy.active && !splitting) {
            continue;
        }
        const Vector3 cullPosition = enemyRenderPosition(enemy);
        const float cullRadius =
            enemy.type == EnemyType::Boss ? 6.0F : 3.5F;
        if (!enemyVisible(cullPosition, cullRadius)) {
            continue;
        }
        Vector3 enemyPosition = cullPosition;
        enemyPosition.y += static_cast<float>(
            simulation_.terrain().getHeight(
                enemy.position.x,
                enemy.position.z));
        const Vector3 toEnemy =
            Vector3Subtract(enemyPosition, camera.position);
        const float enemyDistanceSquared =
            Vector3LengthSqr(toEnemy);
        ++performanceStats_.visibleEnemies;
        const bool aimed =
            snapshot.aimedEnemy &&
            *snapshot.aimedEnemy == enemy.id;
        const bool lowDetail =
            !aimed && enemy.type != EnemyType::Boss &&
            enemy.eliteAffixes == 0U &&
            enemyDistanceSquared > EnemyFullDetailDistanceSquared;
        const auto flash = enemyHitFlashById_.find(
            effectKey(enemy.id));
        const float hitFlash =
            flash != enemyHitFlashById_.end()
                ? flash->second
                : 0.0F;
        const float enemyScale =
            enemyVisualScale(enemy.type) * enemyHitScale(enemy) *
            (enemy.eliteAffixes != 0U ? 1.08F : 1.0F);
        const EnemyStatusEffect& freezeStatus =
            enemyStatusEffect(enemy, StatusEffectType::Freeze);
        const bool frozen = freezeStatus.remaining > 0.0;
        const bool burning = enemyBurnAmountById_.contains(
            effectKey(enemy.id));
        Color modelTint = WHITE;
        if (splitting) {
            const float progress = std::clamp(
                static_cast<float>(
                    1.0 - enemy.splitAnimationRemaining / 0.38),
                0.0F, 1.0F);
            modelTint = {
                255,
                static_cast<unsigned char>(
                    std::lround(238.0F - progress * 72.0F)),
                static_cast<unsigned char>(
                    std::lround(184.0F - progress * 86.0F)),
                255,
            };
        } else if (frozen) {
            modelTint = {151, 224, 255, 255};
        } else if (burning) {
            modelTint = {255, 118, 42, 255};
        } else if (enemy.slowRemaining > 0.0) {
            modelTint = {184, 222, 255, 255};
        } else if (
            enemy.state == EnemyState::BossRamWindup) {
            modelTint = {255, 178, 150, 255};
        } else if (enemy.state == EnemyState::BossSlamWindup) {
            modelTint = {255, 112, 64, 255};
        } else if (enemy.state == EnemyState::BossWarCryWindup) {
            modelTint = {197, 103, 255, 255};
        } else if (enemy.state == EnemyState::BossPhaseTransition) {
            modelTint = {255, 220, 116, 255};
        } else if (hasEliteAffix(
                       enemy.eliteAffixes,
                       EliteAffix::Berserker)) {
            const bool enraged = enemy.maxHealth > 0.0 &&
                enemy.health / enemy.maxHealth <= 0.5;
            modelTint = enraged
                ? Color{255, 82, 70, 255}
                : Color{238, 150, 140, 255};
        } else if (hasEliteAffix(
                       enemy.eliteAffixes,
                       EliteAffix::Warden)) {
            modelTint = {124, 195, 255, 255};
        } else if (hasEliteAffix(
                       enemy.eliteAffixes,
                       EliteAffix::Volatile)) {
            modelTint = {255, 181, 78, 255};
        }
        if (!aimed) {
            if (hitFlash > 0.001F) {
                const float smoothFlash =
                    std::clamp(hitFlash, 0.0F, 1.0F);
                const float batchedFlash =
                    std::ceil(smoothFlash * 4.0F) / 4.0F;
                modelTint = {
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.r) +
                            (255.0F -
                             static_cast<float>(
                                 modelTint.r)) *
                                batchedFlash)),
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.g) +
                            (244.0F -
                             static_cast<float>(
                                 modelTint.g)) *
                                batchedFlash)),
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.b) +
                            (205.0F -
                             static_cast<float>(
                                 modelTint.b)) *
                                batchedFlash)),
                    255,
                };
            }
            float animationTime = frozen
                ? 0.0F
                : enemyAnimationSeconds(enemy, snapshot.elapsedSeconds);
            if (enemyDistanceSquared > 625.0F &&
                enemy.hitAnimationRemaining <= 0.0 &&
                enemy.state !=
                    EnemyState::BossRamWindup &&
                enemy.state != EnemyState::BossSlamWindup &&
                enemy.state != EnemyState::BossWarCryWindup &&
                enemy.state != EnemyState::BossPhaseTransition) {
                animationTime = static_cast<float>(
                    snapshot.elapsedSeconds);
            }
            enemyDrawInstances_.push_back({
                .modelVisual = enemyModelVisual(enemy.type),
                .animationVisual =
                    frozen || splitting
                        ? EnemyAnimationVisual::Idle
                           : enemyAnimationVisual(enemy),
                .animationSeconds = animationTime,
                .position = enemyPosition,
                .yawRadians =
                    static_cast<float>(enemy.yaw),
                .tint = modelTint,
                .scale = enemyScale,
                .lowDetail = lowDetail,
            });
            continue;
        }
        WorldMaterialState material{};
        material.bakedAo = 0.82F;
        material.hitFlashAmount = hitFlash;
        material.selectionAmount = aimed ? 0.32F : 0.0F;
        material.selectionTint = {1.0F, 0.38F, 0.12F};
        renderer_->setWorldMaterial(material);
        float width = 0.8F;
        float height = 1.6F;
        Color body = {150, 55, 52, 255};
        if (enemy.type == EnemyType::Fast) {
            width = 0.65F;
            height = 1.35F;
            body = {191, 104, 52, 255};
        } else if (enemy.type == EnemyType::Heavy) {
            width = 1.15F;
            height = 2.0F;
            body = {93, 60, 105, 255};
            } else if (enemy.type == EnemyType::Boss) {
                width = 2.0F;
                height = 3.2F;
                body = {74, 35, 45, 255};
            } else if (enemy.type == EnemyType::Ranged) {
                width = 0.75F;
                height = 1.55F;
                body = {55, 118, 154, 255};
            } else if (enemy.type == EnemyType::Sapper) {
                width = 0.86F;
                height = 1.5F;
                body = {170, 118, 43, 255};
            } else if (enemy.type == EnemyType::Flying) {
                width = 0.72F;
                height = 1.0F;
                body = {102, 71, 167, 255};
            } else if (enemy.type == EnemyType::Splitter) {
                width = 1.45F;
                height = 2.35F;
                body = {67, 154, 73, 255};
            } else if (enemy.type == EnemyType::Splitling) {
                width = 0.65F;
                height = 1.05F;
                body = {82, 190, 91, 255};
        }
        if (aimed) {
            body = {242, 118, 76, 255};
        } else if (frozen) {
            body = {91, 183, 225, 255};
        } else if (burning) {
            body = {246, 76, 16, 255};
        } else if (enemy.slowRemaining > 0.0) {
            body = {70, 128, 170, 255};
        } else if (enemy.state == EnemyState::BossRamWindup) {
            body = {235, 64, 45, 255};
        } else if (enemy.state == EnemyState::BossSlamWindup) {
            body = {245, 91, 38, 255};
        } else if (enemy.state == EnemyState::BossWarCryWindup) {
            body = {154, 62, 211, 255};
        } else if (enemy.state == EnemyState::BossPhaseTransition) {
            body = {238, 174, 55, 255};
        }
        if (!renderer_->drawEnemy(
                enemyModelVisual(enemy.type),
                frozen || splitting
                    ? EnemyAnimationVisual::Idle
                       : enemyAnimationVisual(enemy),
                frozen ? 0.0F : enemyAnimationSeconds(
                    enemy, snapshot.elapsedSeconds),
                enemyPosition, static_cast<float>(enemy.yaw),
                modelTint, enemyScale)) {
            const float hitScale = enemyHitScale(enemy);
            DrawCube(enemyPosition, width * hitScale,
                     height * hitScale, width * hitScale,
                     body);
            DrawSphere(
                {enemyPosition.x,
                 enemyPosition.y + height * hitScale * 0.62F,
                 enemyPosition.z},
                width * hitScale * 0.52F,
                aimed ? ORANGE : MAROON);
        }
    }
    if (!enemyDrawInstances_.empty()) {
        WorldMaterialState material{};
        material.bakedAo = 0.82F;
        renderer_->setWorldMaterial(material);
        if (!renderer_->drawEnemiesInstanced(
                enemyDrawInstances_)) {
            for (const EnemyDrawInstance& instance :
                 enemyDrawInstances_) {
                static_cast<void>(renderer_->drawEnemy(
                    instance.modelVisual,
                    instance.animationVisual,
                    instance.animationSeconds,
                    instance.position,
                    instance.yawRadians,
                    instance.tint, instance.scale,
                    instance.loop,
                    instance.inkOutlineEligible));
            }
        }
    }
    BeginBlendMode(BLEND_ADDITIVE);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const EnemyInstance& elite : snapshot.enemies) {
        if (!elite.active || elite.eliteAffixes == 0U) {
            continue;
        }
        Vector3 center = enemyRenderPosition(elite);
        if (!enemyVisible(center, 4.0F)) {
            continue;
        }
        center.y += static_cast<float>(
            simulation_.terrain().getHeight(
                elite.position.x, elite.position.z));
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(snapshot.elapsedSeconds) * 5.5F +
            static_cast<float>(elite.id.index % 31U));
        Color aura{255, 128, 68, 125};
        if (hasEliteAffix(
                elite.eliteAffixes, EliteAffix::Warden)) {
            aura = {74, 174, 255, 130};
        } else if (hasEliteAffix(
                       elite.eliteAffixes,
                       EliteAffix::Berserker)) {
            aura = elite.maxHealth > 0.0 &&
                    elite.health / elite.maxHealth <= 0.5
                ? Color{255, 54, 42, 170}
                : Color{255, 112, 82, 110};
        }
        DrawCircle3D(
            {center.x, center.y + 0.025F, center.z},
            0.68F + pulse * 0.11F,
            {1.0F, 0.0F, 0.0F}, 90.0F, aura);
        DrawCircle3D(
            {center.x, center.y + 0.035F, center.z},
            0.42F + pulse * 0.07F,
            {1.0F, 0.0F, 0.0F}, 90.0F,
            {aura.r, aura.g, aura.b,
             static_cast<unsigned char>(aura.a / 2U)});

        if (!hasEliteAffix(
                elite.eliteAffixes, EliteAffix::Warden)) {
            continue;
        }
        int linked = 0;
        for (const EnemyInstance& protectedEnemy :
             snapshot.enemies) {
            if (!protectedEnemy.active ||
                protectedEnemy.id == elite.id || linked >= 8) {
                continue;
            }
            const double x =
                protectedEnemy.position.x - elite.position.x;
            const double z =
                protectedEnemy.position.z - elite.position.z;
            if (x * x + z * z > 5.5 * 5.5) {
                continue;
            }
            Vector3 target = enemyRenderPosition(protectedEnemy);
            target.y += static_cast<float>(
                simulation_.terrain().getHeight(
                    protectedEnemy.position.x,
                    protectedEnemy.position.z)) + 0.55F;
            const Vector3 source{
                center.x, center.y + 0.72F, center.z};
            DrawCylinderEx(
                source, target, 0.025F, 0.012F, 5,
                {63, 162, 255, 48});
            DrawLine3D(
                source, target, {176, 226, 255, 155});
            ++linked;
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    for (const EnemyProjectile& projectile : snapshot.enemyProjectiles) {
        if (!projectile.active) continue;
        const Vector3 head{
            static_cast<float>(projectile.position.x),
            static_cast<float>(projectile.position.y),
            static_cast<float>(projectile.position.z)};
        if (!enemyVisible(head, 1.0F)) {
            continue;
        }
        Vector3 velocity{
            static_cast<float>(projectile.velocity.x),
            static_cast<float>(projectile.velocity.y),
            static_cast<float>(projectile.velocity.z)};
        if (Vector3LengthSqr(velocity) > 0.001F) {
            velocity = Vector3Normalize(velocity);
        }
        const Vector3 tail = Vector3Subtract(
            head, Vector3Scale(velocity, 0.72F));
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(snapshot.elapsedSeconds) * 13.0F +
            static_cast<float>(projectile.id.index) * 0.73F);
        DrawCylinderEx(tail, head, 0.035F, 0.12F, 8,
                       {84, 58, 220, 150});
        DrawSphere(head, 0.20F + pulse * 0.035F,
                   {166, 105, 255, 235});
        DrawSphere(head, 0.09F, {238, 221, 255, 255});
    }
    EndBlendMode();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlBegin(RL_TRIANGLES);
    Vector3 flameRight = Vector3CrossProduct(
        cameraForward, {0.0F, 1.0F, 0.0F});
    if (Vector3LengthSqr(flameRight) < 0.001F) {
        flameRight = {1.0F, 0.0F, 0.0F};
    } else {
        flameRight = Vector3Normalize(flameRight);
    }
    const bool detailedFlames = renderer_->settings().particles;
    for (const EnemyInstance& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        const auto burn = enemyBurnAmountById_.find(
            effectKey(enemy.id));
        if (burn == enemyBurnAmountById_.end() ||
            burn->second <= 0.01F) {
            continue;
        }
        Vector3 position = enemyRenderPosition(enemy);
        if (!enemyVisible(position, 4.0F)) {
            continue;
        }
        position.y += static_cast<float>(
            simulation_.terrain().getHeight(
                enemy.position.x, enemy.position.z));
        const float distanceSquared = Vector3DistanceSqr(
            position, camera.position);
        if (distanceSquared > 1600.0F) {
            continue;
        }
        const float scale = enemyFlameScale(enemy.type);
        Vector3 toCamera = Vector3Subtract(camera.position, position);
        toCamera.y = 0.0F;
        if (Vector3LengthSqr(toCamera) < 0.001F) {
            toCamera = Vector3Negate(cameraForward);
            toCamera.y = 0.0F;
        }
        toCamera = Vector3Normalize(toCamera);
        const Vector3 visibleFront = Vector3Add(
            position, Vector3Scale(toCamera, scale * 0.42F));
        const int lobeCount =
            detailedFlames && distanceSquared < 324.0F ? 5 : 3;
        for (int lobe = 0; lobe < lobeCount; ++lobe) {
            const float lobeIndex = static_cast<float>(lobe);
            const float phase = static_cast<float>(
                snapshot.elapsedSeconds * (8.2 + lobe * 1.35)) +
                static_cast<float>(enemy.id.index % 97U) * 0.37F +
                lobeIndex * 2.1F;
            Vector3 base = visibleFront;
            base.y += scale * (0.12F + lobeIndex * 0.16F);
            base = Vector3Add(
                base,
                Vector3Scale(
                    flameRight,
                    scale * (lobeIndex -
                        static_cast<float>(lobeCount - 1) * 0.5F) *
                        0.22F));
            const float height = scale *
                (1.02F + lobeIndex * 0.18F +
                 (0.5F + 0.5F * std::sin(phase * 1.31F)) * 0.24F);
            const float width = scale *
                (0.21F + 0.03F * lobeIndex);
            const float sway = scale * 0.20F * std::sin(phase);
            emitFlameLobe(
                base, flameRight, width, height, sway,
                burn->second * (1.0F - lobeIndex * 0.08F));
        }
        const int emberCount = detailedFlames &&
                distanceSquared < 324.0F
            ? 6
            : 2;
        for (int ember = 0; ember < emberCount; ++ember) {
            const float emberIndex = static_cast<float>(ember);
            const float seed = static_cast<float>(
                (enemy.id.index * 17U +
                 static_cast<std::uint32_t>(ember) * 29U) % 101U) /
                101.0F;
            const float rise = std::fmod(
                static_cast<float>(snapshot.elapsedSeconds) *
                    (0.62F + emberIndex * 0.035F) + seed,
                1.0F);
            const float orbit =
                static_cast<float>(snapshot.elapsedSeconds) *
                    (2.2F + emberIndex * 0.18F) +
                seed * 2.0F * PI;
            Vector3 emberPosition = visibleFront;
            emberPosition = Vector3Add(
                emberPosition,
                Vector3Scale(
                    flameRight,
                    std::sin(orbit) * scale *
                        (0.24F + emberIndex * 0.035F)));
            emberPosition = Vector3Add(
                emberPosition,
                Vector3Scale(toCamera, std::cos(orbit) * scale * 0.08F));
            emberPosition.y += scale * (0.48F + rise * 1.42F);
            const float emberFade =
                (1.0F - rise) * std::min(1.0F, rise * 7.0F);
            emitEmber(
                emberPosition, flameRight,
                scale * (0.035F + 0.008F *
                    (0.5F + 0.5F * std::sin(orbit * 1.7F))),
                burn->second * emberFade * 0.95F);
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
    if (detailedFlames) {
        for (const EnemyInstance& enemy : snapshot.enemies) {
            if (!enemy.active) continue;
            const auto burn = enemyBurnAmountById_.find(effectKey(enemy.id));
            if (burn == enemyBurnAmountById_.end() || burn->second <= 0.01F) {
                continue;
            }
            Vector3 position = enemyRenderPosition(enemy);
            if (!enemyVisible(position, 4.0F)) {
                continue;
            }
            position.y += static_cast<float>(simulation_.terrain().getHeight(
                enemy.position.x, enemy.position.z));
            if (Vector3DistanceSqr(position, camera.position) > 324.0F) {
                continue;
            }
            const float scale = enemyFlameScale(enemy.type);
            for (int puff = 0; puff < 2; ++puff) {
                const float phase = std::fmod(
                    static_cast<float>(snapshot.elapsedSeconds) *
                        (0.34F + static_cast<float>(puff) * 0.07F) +
                    static_cast<float>(enemy.id.index % 43U) * 0.11F +
                    static_cast<float>(puff) * 0.47F,
                    1.0F);
                const float angle = phase * 7.2F +
                    static_cast<float>(puff) * PI;
                const float fade = (1.0F - phase) *
                    std::min(1.0F, phase * 6.0F) * burn->second;
                DrawSphereEx(
                    {position.x + std::cos(angle) * scale * 0.15F,
                     position.y + scale * (1.0F + phase * 1.25F),
                     position.z + std::sin(angle) * scale * 0.15F},
                    scale * (0.07F + phase * 0.13F), 5, 5,
                    {53, 45, 42, static_cast<unsigned char>(
                        std::lround(78.0F * fade))});
            }
        }
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        const EnemyStatusEffect& freezeStatus =
            enemyStatusEffect(enemy, StatusEffectType::Freeze);
        if (freezeStatus.visualParameter <= 0.01) {
            continue;
        }
        Vector3 position = enemyRenderPosition(enemy);
        if (!enemyVisible(position, 4.0F)) {
            continue;
        }
        position.y += static_cast<float>(simulation_.terrain().getHeight(
            enemy.position.x, enemy.position.z));
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(snapshot.elapsedSeconds) * 6.0F +
            static_cast<float>(enemy.id.index) * 0.07F);
        const bool thawing = freezeStatus.remaining > 0.0 &&
            freezeStatus.remaining < 0.28;
        const float thawPulse = thawing
            ? 0.58F + 0.42F * (0.5F + 0.5F * std::sin(
                static_cast<float>(snapshot.elapsedSeconds) * 24.0F +
                static_cast<float>(enemy.id.index) * 0.31F))
            : 1.0F;
        const float effectAmount = static_cast<float>(
            std::clamp(freezeStatus.visualParameter, 0.0, 1.0)) *
            thawPulse;
        DrawCircle3D(
            {position.x, position.y + 0.035F, position.z},
            0.48F + pulse * 0.09F,
            {1.0F, 0.0F, 0.0F}, 90.0F,
            {142, 229, 255, static_cast<unsigned char>(
                150.0F * effectAmount)});
        if (freezeStatus.remaining > 0.0 && renderer_->settings().particles) {
            for (int mistIndex = 0; mistIndex < 1; ++mistIndex) {
                const float mistPhase =
                    static_cast<float>(snapshot.elapsedSeconds) *
                        (1.4F + static_cast<float>(mistIndex) * 0.35F) +
                    static_cast<float>(enemy.id.index) * 0.23F +
                    static_cast<float>(mistIndex) * 3.1F;
                DrawSphereEx(
                    {position.x + std::cos(mistPhase) * 0.24F,
                     position.y + 0.28F +
                         std::sin(mistPhase * 1.17F) * 0.11F,
                     position.z + std::sin(mistPhase) * 0.24F},
                    0.075F + 0.018F *
                        (0.5F + 0.5F * std::sin(mistPhase * 1.6F)),
                    5, 5,
                    {142, 229, 255,
                     static_cast<unsigned char>(42.0F * effectAmount)});
            }
            for (int crystal = 0; crystal < 4; ++crystal) {
                const float crystalIndex = static_cast<float>(crystal);
                const float angle = crystalIndex * 1.5708F +
                    static_cast<float>(enemy.id.index % 11U) * 0.19F;
                const float height = 0.22F +
                    (0.13F + crystalIndex * 0.025F) * effectAmount;
                const Vector3 base{
                    position.x + std::cos(angle) * 0.34F,
                    position.y + 0.04F,
                    position.z + std::sin(angle) * 0.34F};
                const Vector3 tip{
                    base.x + std::cos(angle) * 0.06F,
                    base.y + height,
                    base.z + std::sin(angle) * 0.06F};
                DrawLine3D(base, tip, {191, 246, 255,
                                       static_cast<unsigned char>(
                                           185.0F * effectAmount)});
                if (crystal % 2 == 0) {
                    DrawSphereEx(tip, 0.045F, 5, 5,
                                 {191, 246, 255,
                                  static_cast<unsigned char>(
                                      150.0F * effectAmount)});
                }
            }
        }
        if ((freezeStatus.remaining <= 0.0 || thawing) &&
            effectAmount > 0.05F) {
            const float crack = effectAmount * (0.5F + 0.5F * pulse);
            for (int branch = 0; branch < 3; ++branch) {
                const float branchIndex = static_cast<float>(branch);
                const float angle = branchIndex * 2.0944F +
                    static_cast<float>(enemy.id.index % 5U) * 0.21F;
                DrawLine3D(
                    {position.x, position.y + 0.55F, position.z},
                    {position.x + std::cos(angle) * 0.42F * crack,
                     position.y + 0.72F + branchIndex * 0.09F * crack,
                     position.z + std::sin(angle) * 0.42F * crack},
                    {223, 248, 255,
                     static_cast<unsigned char>(190.0F * crack)});
            }
        }
    }
    EndBlendMode();
    performanceStats_.enemyRender.sample(
        performanceMilliseconds(enemyRenderStart));
    destroyedEnemyDrawInstances_.clear();
    destroyedEnemyDrawInstances_.reserve(
        destroyedEnemyVisuals_.size());
    for (const DestroyedEnemyVisual& visual :
         destroyedEnemyVisuals_) {
        const float progress = static_cast<float>(
            1.0 - visual.remaining / visual.duration);
        const float fade = 1.0F - smoothstep(
            0.68F, 1.0F, progress);
        const float quantizedFade =
            std::ceil(
                std::clamp(fade, 0.0F, 1.0F) * 4.0F) /
            4.0F;
        const auto alpha = static_cast<unsigned char>(
            std::lround(quantizedFade * 255.0F));
        const EnemyInstance visualEnemy{
            .type = visual.type,
            .position = visual.position,
            .eliteAffixes = visual.eliteAffixes,
            .surfaceHeightOffset =
                visual.surfaceHeightOffset,
        };
        Vector3 position =
            enemyRenderPosition(visualEnemy);
        position.y += static_cast<float>(
            simulation_.terrain().getHeight(
                visual.position.x,
                visual.position.z));
        const float cullRadius =
            visual.type == EnemyType::Boss ? 6.0F : 3.5F;
        if (!enemyVisible(position, cullRadius)) {
            continue;
        }
        const Vector3 toEnemy =
            Vector3Subtract(position, camera.position);
        const float distanceSquared =
            Vector3LengthSqr(toEnemy);
        destroyedEnemyDrawInstances_.push_back({
            .modelVisual =
                enemyModelVisual(visual.type),
            .animationVisual =
                EnemyAnimationVisual::Death,
            .animationSeconds =
                progress *
                static_cast<float>(visual.duration),
            .position = position,
            .yawRadians =
                static_cast<float>(visual.yaw),
            .tint = visual.eliteAffixes != 0U
                ? Color{255, 176, 122, alpha}
                : Color{255, 255, 255, alpha},
            .scale = enemyVisualScale(visual.type) *
                (visual.eliteAffixes != 0U ? 1.08F : 1.0F),
            .loop = false,
            .lowDetail = distanceSquared >
                EnemyFullDetailDistanceSquared &&
                visual.type != EnemyType::Boss,
        });
    }
    if (!destroyedEnemyDrawInstances_.empty()) {
        WorldMaterialState material{};
        material.bakedAo = 0.82F;
        renderer_->setWorldMaterial(material);
        if (!renderer_->drawEnemiesInstanced(
                destroyedEnemyDrawInstances_)) {
            for (const EnemyDrawInstance& instance :
                 destroyedEnemyDrawInstances_) {
                static_cast<void>(renderer_->drawEnemy(
                    instance.modelVisual,
                    instance.animationVisual,
                    instance.animationSeconds,
                    instance.position,
                    instance.yawRadians,
                    instance.tint, instance.scale,
                    instance.loop,
                    instance.inkOutlineEligible));
            }
        }
    }
}

} // namespace ian
