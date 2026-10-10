#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppBurnDrawing.hpp"
#include "graphics/CameraCulling.hpp"
#include "presentation/PresentationEffectQueries.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ian {

using namespace app_detail;

namespace {

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
    const double presentationSeconds = GetTime();
    for (auto flash = enemyHitFlashById_.begin();
         flash != enemyHitFlashById_.end();) {
        if (presentationSeconds >
            flash->second.startedAt + EnemyHitFlashDuration) {
            flash = enemyHitFlashById_.erase(flash);
        } else {
            ++flash;
        }
    }
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
        if (effect.type == PresentationEffectType::EnemyBurn) {
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
        float hitFlash = 0.0F;
        if (flash != enemyHitFlashById_.end()) {
            const float age = static_cast<float>(
                presentationSeconds - flash->second.startedAt);
            constexpr float FlashHoldSeconds = 0.012F;
            const float fade = std::clamp(
                (age - FlashHoldSeconds) /
                    static_cast<float>(
                        EnemyHitFlashDuration - FlashHoldSeconds),
                0.0F, 1.0F);
            hitFlash = EnemyHitFlashPeak *
                (1.0F - fade * fade * (3.0F - 2.0F * fade));
        }
        const float batchedHitFlash = hitFlash > 0.001F
            ? std::ceil(hitFlash * 8.0F) * 0.125F
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
            modelTint = {255, 198, 132, 255};
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
                .whiteFlashAmount = batchedHitFlash,
                .scale = enemyScale,
                .lowDetail = lowDetail,
            });
            continue;
        }
        WorldMaterialState material{};
        material.bakedAo = 0.82F;
        material.enemyWhiteFlashAmount = hitFlash;
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
                material.enemyWhiteFlashAmount = instance.whiteFlashAmount;
                renderer_->setWorldMaterial(material);
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
    // Enemy projectile glow/trails are transparent too. Keep depth testing,
    // but do not let their fading silhouette occlude subsequent objects.
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
    Vector3 flameRight = Vector3CrossProduct(
        cameraForward, {0.0F, 1.0F, 0.0F});
    if (Vector3LengthSqr(flameRight) < 0.001F) {
        flameRight = {1.0F, 0.0F, 0.0F};
    } else {
        flameRight = Vector3Normalize(flameRight);
    }
    const bool detailedFlames = renderer_->settings().particles;
    // Select once and reuse positions in both passes. Bound geometry and smoke
    // independently of how many enemies a chain explosion ignites.
    constexpr std::size_t BurnEnemyVisualBudget = 72U;
    constexpr std::size_t DetailedBurnEnemyBudget = 24U;
    auto& burnVisualCandidates = burnVisualCandidates_;
    burnVisualCandidates.clear();
    burnVisualCandidates.reserve(std::min(
        snapshot.enemies.size(), BurnEnemyVisualBudget * 2U));
    for (std::size_t index = 0; index < snapshot.enemies.size(); ++index) {
        const EnemyInstance& enemy = snapshot.enemies[index];
        if (!enemy.active) {
            continue;
        }
        const auto burn = enemyBurnAmountById_.find(effectKey(enemy.id));
        if (burn == enemyBurnAmountById_.end() || burn->second <= 0.01F) {
            continue;
        }
        const Vector3 position = enemyRenderPosition(enemy);
        if (!enemyVisible(position, 4.0F)) {
            continue;
        }
        const float distanceSquared = Vector3DistanceSqr(
            position, camera.position);
        if (distanceSquared > 1600.0F) {
            continue;
        }
        burnVisualCandidates.push_back({
            .enemyIndex = index,
            .position = position,
            .distanceSquared = distanceSquared,
            .burnAmount = burn->second,
        });
    }
    const std::size_t visualCount = std::min(
        burnVisualCandidates.size(), BurnEnemyVisualBudget);
    std::ranges::partial_sort(
        burnVisualCandidates, burnVisualCandidates.begin() +
            static_cast<std::ptrdiff_t>(visualCount),
        [](const BurnVisualCandidate& left, const BurnVisualCandidate& right) {
            return left.distanceSquared < right.distanceSquared ||
                (left.distanceSquared == right.distanceSquared &&
                 left.enemyIndex < right.enemyIndex);
        });
    burnVisualCandidates.resize(visualCount);
    std::array<std::size_t, BurnEnemyVisualBudget> drawOrder{};
    std::array<Vector3, BurnEnemyVisualBudget> burnDirections{};
    std::array<float, BurnEnemyVisualBudget> burnScales{};
    std::array<float, BurnEnemyVisualBudget> burnSurfaceRadii{};
    for (std::size_t rank = 0; rank < visualCount; ++rank) {
        auto& candidate = burnVisualCandidates[rank];
        const auto& enemy = snapshot.enemies[candidate.enemyIndex];
        candidate.position.y += static_cast<float>(
            simulation_.terrain().getHeight(enemy.position.x, enemy.position.z));
        Vector3 toCamera = Vector3Subtract(camera.position, candidate.position);
        toCamera.y = 0.0F;
        burnDirections[rank] = Vector3LengthSqr(toCamera) < 0.001F
            ? Vector3{0, 0, 1} : Vector3Normalize(toCamera);
        const float appearance = enemyHitScale(enemy) *
            (enemy.eliteAffixes != 0U ? 1.08F : 1.0F);
        burnScales[rank] = enemyFlameScale(enemy.type) * appearance;
        const BoundingBox bounds = renderer_->enemyWorldBounds(
            enemyModelVisual(enemy.type), candidate.position,
            static_cast<float>(enemy.yaw), enemyVisualScale(enemy.type) * appearance);
        const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), .5F);
        // Fit to the authored body instead of burying flames inside wide models.
        // Clamp weapon/wing extents so a protruding accessory cannot detach fire.
        burnSurfaceRadii[rank] = std::clamp(
            std::abs(burnDirections[rank].x) * extent.x +
            std::abs(burnDirections[rank].z) * extent.z,
            burnScales[rank] * .35F, burnScales[rank] * .95F);
        drawOrder[rank] = rank;
    }
    // Alpha silhouettes and smoke require back-to-front order. Distance rank
    // still determines detail, independent of the draw order.
    std::ranges::sort(
        std::span{drawOrder.data(), visualCount}, {},
        [&burnVisualCandidates](std::size_t rank) {
            return -burnVisualCandidates[rank].distanceSquared;
        });
    WorldMaterialState burnMaterial{};
    burnMaterial.unlitAmount = 1.0F;
    burnMaterial.screenAoAmount = 0.0F;
    burnMaterial.inkOutlineEligible = false;
    burnMaterial.inkOccluder = false;
    renderer_->setWorldMaterial(burnMaterial);
    // Transparent fire must preserve the world's normal/AO attachment. Scene
    // alpha already masks rear ink under the foreground flame silhouettes.
    renderer_->beginColorOnlyWorldEffects();
    for (const auto pass : {app_burn_drawing::Pass::Glow,
                            app_burn_drawing::Pass::Smoke,
                            app_burn_drawing::Pass::Flame}) {
        app_burn_drawing::beginBatch(pass);
        for (const std::size_t rank : std::span{drawOrder.data(), visualCount}) {
            const auto& candidate = burnVisualCandidates[rank];
            const auto& enemy = snapshot.enemies[candidate.enemyIndex];
            app_burn_drawing::emit(candidate.position, flameRight, burnDirections[rank],
                burnScales[rank], enemy.id.index, snapshot.elapsedSeconds,
                candidate.burnAmount,
                detailedFlames && rank < DetailedBurnEnemyBudget, pass,
                burnSurfaceRadii[rank]);
        }
        app_burn_drawing::endBatch();
    }
    renderer_->endColorOnlyWorldEffects();
    // The following freeze pass shares the transparent-effects depth policy.
    WorldMaterialState statusMaterial{};
    statusMaterial.bakedAo = 0.82F;
    renderer_->setWorldMaterial(statusMaterial);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
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
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
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
