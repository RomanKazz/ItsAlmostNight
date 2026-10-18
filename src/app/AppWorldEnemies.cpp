#include "graphics/RoyalBossEffect.hpp"
#include "graphics/PortalArrivalEffect.hpp"
#include "presentation/PortalArrivalVisibility.hpp"
#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppBurnDrawing.hpp"
#include "app/AppFreezeDrawing.hpp"
#include "app/AppPoisonDrawing.hpp"
#include "graphics/CameraCulling.hpp"
#include "graphics/EnemyMagicEffect.hpp"
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
    case EnemyType::AirTank: return 1.35F;
    case EnemyType::Heavy: return 1.25F;
    case EnemyType::Splitter: return 1.35F;
    case EnemyType::Splitling: return 0.62F;
    case EnemyType::Fast: return 0.82F;
    case EnemyType::Bee:
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
    const double splitSubTickSeconds = enemySplitSubTickSeconds(snapshot, fixedStep_.interpolationAlpha());
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
        if ((!enemy.active && !splitting) || !presentation::portalArrivalVisible(enemy,
            {camera.position.x,camera.position.y,camera.position.z})) {
            continue;
        }
        const auto splitPose = enemySplitPose(enemy, splitSubTickSeconds);
        const Vector3 cullPosition = enemyRenderPosition(enemy, splitSubTickSeconds);
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
            !aimed && !splitting && enemy.spawnAnimationRemaining <= 0.0 && enemy.portalArrivalRemaining<=0 &&
            enemy.type != EnemyType::Boss &&
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
            // Keep the authored slime pigment; a brief mint flash marks release.
            hitFlash = std::max(hitFlash, .38F * smoothstep(.72F, 1.0F,
                static_cast<float>(1.0 - (enemy.splitAnimationRemaining - splitSubTickSeconds) / .38)));
        } else if (freezeStatus.visualParameter > 0.01) {
            const float cold = static_cast<float>(std::clamp(freezeStatus.visualParameter, 0.0, 1.0));
            modelTint = {static_cast<unsigned char>(255 - 104 * cold),
                         static_cast<unsigned char>(255 - 31 * cold), 255, 255};
        } else if (burning) {
            modelTint = {255, 198, 132, 255};
        } else if (enemy.slowRemaining > 0.0) {
            modelTint = {184, 222, 255, 255};
        } else if (
            enemy.state == EnemyState::BossRamWindup) {
            modelTint = {255, 178, 150, 255};
        } else if (enemy.type == EnemyType::AirTank && enemy.airTankWindupRemaining > 0) {
            const float pulse = .5F + .5F * std::sin(static_cast<float>(snapshot.elapsedSeconds) * 16);
            modelTint = {255, static_cast<unsigned char>(220 - 30 * pulse), 160, 255};
        } else if (enemy.state == EnemyState::BossSlamWindup) {
            modelTint = {255, 216, 162, 255};
        } else if (enemy.state == EnemyState::BossWarCryWindup) {
            modelTint = {244, 248, 169, 255};
        } else if (enemy.state == EnemyState::BossVolleyWindup || enemy.state == EnemyState::BossStormWindup) {
            modelTint = {255, 239, 175, 255};
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
                .modelVisual = enemyModelVisual(enemy),
                .animationVisual =
                    frozen || splitting
                        ? EnemyAnimationVisual::Idle
                           : enemyAnimationVisual(enemy),
                .animationSeconds = animationTime,
                .position = enemyPosition,
                .yawRadians =
                    static_cast<float>(enemy.yaw),
                .tint = modelTint,
                .whiteFlashAmount = splitting ? hitFlash : batchedHitFlash,
                .scale = enemyScale,
                .lowDetail = lowDetail,
                .eliteAffixes = enemy.eliteAffixes,
                .bodyStretch = splitPose.stretch, .bodyRoll = splitPose.roll,
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
                enemyModelVisual(enemy),
                frozen || splitting
                    ? EnemyAnimationVisual::Idle
                       : enemyAnimationVisual(enemy),
                frozen ? 0.0F : enemyAnimationSeconds(
                    enemy, snapshot.elapsedSeconds),
                enemyPosition, static_cast<float>(enemy.yaw),
                modelTint, enemyScale, true, true, false, enemy.eliteAffixes,
                splitPose.stretch, splitPose.roll)) {
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
                    instance.inkOutlineEligible, false, instance.eliteAffixes, instance.bodyStretch, instance.bodyRoll));
            }
        }
    }
    BeginBlendMode(BLEND_ADDITIVE);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const EnemyInstance& elite : snapshot.enemies) {
        if (!elite.active || elite.eliteAffixes == 0U || !presentation::portalArrivalVisible(elite,
            {camera.position.x,camera.position.y,camera.position.z})) {
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
                protectedEnemy.id == elite.id || linked >= 8 || !presentation::portalArrivalVisible(protectedEnemy,
                {camera.position.x,camera.position.y,camera.position.z})) {
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
    EndBlendMode();
    const bool beeCharging=std::ranges::any_of(snapshot.enemies,[](const auto& enemy){
        return enemy.active && enemy.type==EnemyType::Bee && enemy.beeWindupRemaining>0;});
    if (!snapshot.enemyProjectiles.empty() || beeCharging) {
        WorldMaterialState magicMaterial{};
        magicMaterial.unlitAmount = 1;
        magicMaterial.screenAoAmount = 0;
        magicMaterial.inkOutlineEligible = false;
        const float interpolation = static_cast<float>(fixedStep_.interpolationAlpha());
        for (const bool headPass : {false, true}) {
            magicMaterial.inkOccluder = headPass;
            renderer_->setWorldMaterial(magicMaterial);
            renderer_->beginColorOnlyWorldEffects();
            fire_wand_effect::beginBatch(headPass);
            if(headPass && beeCharging) for(const auto& enemy:snapshot.enemies) {
                if(!enemy.active || enemy.type!=EnemyType::Bee || enemy.beeWindupRemaining<=0 ||
                    !presentation::portalArrivalVisible(enemy,{camera.position.x,camera.position.y,camera.position.z})) continue;
                const float progress=static_cast<float>(1-enemy.beeWindupRemaining/(enemy.eliteAffixes?1.0:.75));
                const Vector3 center{static_cast<float>(enemy.position.x+std::sin(enemy.yaw)*.43),
                    static_cast<float>(enemy.worldSurfaceHeight+2.10),static_cast<float>(enemy.position.z+std::cos(enemy.yaw)*.43)};
                if(!enemyVisible(center,1))continue;
                const auto basis=enemy_magic_effect::basis(center,camera.position);
                enemy_magic_effect::star(center,basis,.06F+.15F*progress,static_cast<float>(snapshot.elapsedSeconds)*7,{255,231,91,255});
            }
            for (const EnemyProjectile& projectile : snapshot.enemyProjectiles) {
                if (!projectile.active || !enemyVisible(enemy_magic_effect::point(projectile.position), 2)) continue;
                enemy_magic_effect::projectile(projectile, camera.position, snapshot.elapsedSeconds,
                    interpolation, headPass, renderer_->settings().particles);
            }
            fire_wand_effect::endBatch();
            renderer_->endColorOnlyWorldEffects();
        }
        renderer_->setWorldMaterial({});
    }
    if(std::ranges::any_of(snapshot.enemies,[](const EnemyInstance& enemy){return enemy.active && enemy.portalArrivalRemaining>0;})) {
        WorldMaterialState energy{};energy.unlitAmount=1;energy.screenAoAmount=0;
        energy.inkOutlineEligible=false;energy.inkOccluder=false;
        renderer_->setWorldMaterial(energy);renderer_->beginColorOnlyWorldEffects();
        fire_wand_effect::beginBatch(false);
        for(const auto& enemy:snapshot.enemies) if(enemy.active && enemy.portalArrivalRemaining>0 &&
            presentation::portalArrivalVisible(enemy,{camera.position.x,camera.position.y,camera.position.z})) {
            Vector3 root=enemyRenderPosition(enemy,subTickSeconds);
            root.y+=static_cast<float>(simulation_.terrain().getHeight(enemy.position.x,enemy.position.z));
            if(!enemyVisible(root,3)) continue;
            portal_arrival::exit(root,camera.position,static_cast<float>(1-
                std::max(0.0,enemy.portalArrivalRemaining-subTickSeconds)/EnemyPortalArrivalDuration),
                enemy.type==EnemyType::Boss?1.7F:enemy.type==EnemyType::Heavy?1.3F:.85F,
                renderer_->settings().particles && !userSettings_.accessibility.reduceFlashes,enemy.id.index);
        }
        fire_wand_effect::endBatch();renderer_->endColorOnlyWorldEffects();renderer_->setWorldMaterial({});
    }
    if(std::ranges::any_of(snapshot.enemies,[](const EnemyInstance& enemy){return enemy.active && enemy.type==EnemyType::Boss;})) {
        WorldMaterialState royalMaterial{};
        royalMaterial.unlitAmount=1;royalMaterial.screenAoAmount=0;
        royalMaterial.inkOutlineEligible=false;royalMaterial.inkOccluder=false;
        renderer_->setWorldMaterial(royalMaterial);renderer_->beginColorOnlyWorldEffects();
        fire_wand_effect::beginBatch();
        const bool detailed=renderer_->settings().particles && !userSettings_.accessibility.reduceFlashes;
        for(const auto& enemy:snapshot.enemies) if(enemy.active && enemy.type==EnemyType::Boss &&
            presentation::portalArrivalVisible(enemy,{camera.position.x,camera.position.y,camera.position.z})) {
            const Vector3 center{static_cast<float>(enemy.position.x),
                static_cast<float>(enemy.worldSurfaceHeight+enemy.position.y),static_cast<float>(enemy.position.z)};
            if(!enemyVisible(center,8)) continue;
            const float charge=enemy.bossAbilityWindupRemaining>0 && enemy.bossWindupDuration>0 ? static_cast<float>(
                std::clamp(1-enemy.bossAbilityWindupRemaining/enemy.bossWindupDuration,0.0,1.0)) : 0;
            royal_boss_effect::aura(center,camera.position,snapshot.elapsedSeconds,enemy.bossPhase,charge,
                enemy.state==EnemyState::BossPhaseTransition,detailed);
        }
        // Telegraphs follow authoritative attack timers. Freeze/phase changes
        // cannot leave an invisible delayed hit or a stale warning on the floor.
        for(const auto& enemy:snapshot.enemies) if(enemy.active && enemy.type==EnemyType::Boss) {
            const auto drawWarning=[&](Vec3 position,float radius,double eta,double duration) {
                const Vector3 center=enemy_magic_effect::point(position);
                if(enemyVisible(center,radius+1)) royal_boss_effect::marker(center,radius,
                    static_cast<float>(std::clamp(1-eta/std::max(.01,duration),0.0,.98)),
                    snapshot.elapsedSeconds,detailed);
            };
            if(enemy.state==EnemyState::BossSlamWindup)
                drawWarning(enemy.bossAttackTarget,static_cast<float>(enemy.bossAttackRadius),enemy.bossAbilityWindupRemaining,enemy.bossWindupDuration);
            if(enemy.state==EnemyState::BossStormWindup || enemy.bossStrikesRemaining>0) {
                const int first=enemy.bossStrikesRemaining>0 ? 3-enemy.bossStrikesRemaining : 0;
                for(int index=first;index<3;++index) {
                    const double eta=enemy.bossStrikesRemaining>0
                        ? enemy.bossStrikeTimer+(index-first)*.3 : enemy.bossAbilityWindupRemaining+index*.3;
                    drawWarning(enemy.bossStrikeTargets[static_cast<std::size_t>(index)],
                        static_cast<float>(enemy.bossAttackRadius),eta,enemy.bossWindupDuration+index*.3);
                }
            }
        }
        fire_wand_effect::endBatch();renderer_->endColorOnlyWorldEffects();renderer_->setWorldMaterial({});
    }
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
        if (!enemy.active || !presentation::portalArrivalVisible(enemy,
            {camera.position.x,camera.position.y,camera.position.z})) {
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
            enemyModelVisual(enemy), candidate.position,
            static_cast<float>(enemy.yaw), enemyVisualScale(enemy.type) * appearance);
        const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), .5F);
        // Flying rigs hover above their root. Start fire at the body, not in
        // the empty space between the rig origin and its authored silhouette.
        if (enemyIsFlying(enemy.type) || enemy.type == EnemyType::Boss)
            candidate.position.y = bounds.min.y;
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
    // Keep nearest frozen bodies in a bounded heap: no per-particle allocation,
    // and no geometry burst when an entire horde is frozen at once.
    struct FrozenDraw {
        std::size_t enemyIndex{};
        Vector3 position{};
        Vector3 front{};
        float distanceSquared{}, radius{}, height{}, amount{};
        bool thawing{};
    };
    constexpr std::size_t FrozenBudget = 72U;
    constexpr std::size_t DetailedFrozenBudget = 24U;
    std::array<FrozenDraw, FrozenBudget> frozenDraws{};
    std::size_t frozenCount = 0;
    const auto nearer = [](const FrozenDraw& a, const FrozenDraw& b) {
        return a.distanceSquared < b.distanceSquared ||
            (a.distanceSquared == b.distanceSquared && a.enemyIndex < b.enemyIndex);
    };
    for (std::size_t index = 0; index < snapshot.enemies.size(); ++index) {
        const auto& enemy = snapshot.enemies[index];
        const auto& status = enemyStatusEffect(enemy, StatusEffectType::Freeze);
        if (!enemy.active || status.visualParameter <= .01 || !presentation::portalArrivalVisible(enemy,
            {camera.position.x,camera.position.y,camera.position.z})) continue;
        const Vector3 position = enemyRenderPosition(enemy);
        const float distance = Vector3DistanceSqr(position, camera.position);
        if (distance > 1600 || !enemyVisible(position, 4)) continue;
        FrozenDraw candidate{};
        candidate.enemyIndex = index;
        candidate.position = position;
        candidate.distanceSquared = distance;
        candidate.amount = static_cast<float>(std::clamp(status.visualParameter, 0.0, 1.0));
        candidate.thawing = status.remaining <= 0;
        if (frozenCount < FrozenBudget) {
            frozenDraws[frozenCount++] = candidate;
            std::push_heap(frozenDraws.begin(), frozenDraws.begin() +
                static_cast<std::ptrdiff_t>(frozenCount), nearer);
        } else if (nearer(candidate, frozenDraws.front())) {
            std::pop_heap(frozenDraws.begin(), frozenDraws.end(), nearer);
            frozenDraws.back() = candidate;
            std::push_heap(frozenDraws.begin(), frozenDraws.end(), nearer);
        }
    }
    std::sort(frozenDraws.begin(), frozenDraws.begin() +
        static_cast<std::ptrdiff_t>(frozenCount), nearer);
    for (std::size_t rank = 0; rank < frozenCount; ++rank) {
        auto& draw = frozenDraws[rank];
        const auto& enemy = snapshot.enemies[draw.enemyIndex];
        draw.position.y += static_cast<float>(simulation_.terrain().getHeight(
            enemy.position.x, enemy.position.z));
        draw.front = Vector3Subtract(camera.position, draw.position);
        draw.front.y = 0;
        draw.front = Vector3LengthSqr(draw.front) < .001F
            ? Vector3{0, 0, 1} : Vector3Normalize(draw.front);
        const float appearance = enemyHitScale(enemy) *
            (enemy.eliteAffixes != 0U ? 1.08F : 1.0F);
        const float scale = enemyFlameScale(enemy.type) * appearance;
        const auto bounds = renderer_->enemyWorldBounds(enemyModelVisual(enemy),
            draw.position, static_cast<float>(enemy.yaw), enemyVisualScale(enemy.type) * appearance);
        const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), .5F);
        draw.radius = std::clamp(std::max(extent.x, extent.z), scale * .35F, scale * .95F);
        draw.height = std::clamp(bounds.max.y - bounds.min.y, scale * .8F, scale * 2.5F);
    }
    if (frozenCount > 0) {
        renderer_->setWorldMaterial(burnMaterial);
        renderer_->beginColorOnlyWorldEffects();
        for (const auto pass : {app_freeze_drawing::Pass::Ice,
                                app_freeze_drawing::Pass::Atmosphere}) {
            app_freeze_drawing::beginBatch();
            for (std::size_t reverse = frozenCount; reverse > 0; --reverse) {
                const std::size_t rank = reverse - 1;
                const auto& draw = frozenDraws[rank];
                app_freeze_drawing::emitFrozen(draw.position, flameRight, draw.front,
                    draw.radius, draw.height, snapshot.enemies[draw.enemyIndex].id.index,
                    snapshot.elapsedSeconds, draw.amount, draw.thawing,
                    renderer_->settings().particles && rank < DetailedFrozenBudget, pass);
            }
            app_freeze_drawing::endBatch();
        }
        renderer_->endColorOnlyWorldEffects();
    }
    struct PoisonDraw { std::size_t index{}; Vector3 position{}; float distance{}, amount{}; };
    constexpr std::size_t PoisonBudget = 64;
    std::array<PoisonDraw, PoisonBudget> poisonDraws{};
    std::size_t poisonCount = 0;
    const auto poisonNearer = [](const PoisonDraw& a, const PoisonDraw& b) {
        return a.distance < b.distance || (a.distance == b.distance && a.index < b.index);
    };
    for (std::size_t i = 0; i < snapshot.enemies.size(); ++i) {
        const auto& enemy = snapshot.enemies[i];
        const auto& status = enemyStatusEffect(enemy, StatusEffectType::Poison);
        if (!enemy.active || status.visualParameter <= .01 || !presentation::portalArrivalVisible(enemy,
            {camera.position.x,camera.position.y,camera.position.z})) continue;
        const Vector3 position = enemyRenderPosition(enemy);
        const float distance = Vector3DistanceSqr(position, camera.position);
        if (distance > 1600 || !enemyVisible(position, 4)) continue;
        PoisonDraw candidate{i, position, distance, static_cast<float>(status.visualParameter)};
        if (poisonCount < PoisonBudget) {
            poisonDraws[poisonCount++] = candidate;
            std::push_heap(poisonDraws.begin(), poisonDraws.begin() +
                static_cast<std::ptrdiff_t>(poisonCount), poisonNearer);
        } else if (poisonNearer(candidate, poisonDraws.front())) {
            std::pop_heap(poisonDraws.begin(), poisonDraws.end(), poisonNearer);
            poisonDraws.back() = candidate;
            std::push_heap(poisonDraws.begin(), poisonDraws.end(), poisonNearer);
        }
    }
    std::sort(poisonDraws.begin(), poisonDraws.begin() +
        static_cast<std::ptrdiff_t>(poisonCount), poisonNearer);
    if (poisonCount > 0) {
        renderer_->setWorldMaterial(burnMaterial);
        renderer_->beginColorOnlyWorldEffects();
        app_freeze_drawing::beginBatch();
        const Vector3 up = Vector3Normalize(Vector3CrossProduct(flameRight, cameraForward));
        for (std::size_t reverse = poisonCount; reverse > 0; --reverse) {
            const std::size_t rank = reverse - 1;
            auto draw = poisonDraws[rank];
            const auto& enemy = snapshot.enemies[draw.index];
            draw.position.y += static_cast<float>(simulation_.terrain().getHeight(enemy.position.x, enemy.position.z));
            const float appearance = enemyAppearanceScale(enemy) * (enemy.eliteAffixes != 0 ? 1.08F : 1.0F);
            const float scale = enemyFlameScale(enemy.type) * appearance;
            const auto bounds = renderer_->enemyWorldBounds(enemyModelVisual(enemy),
                draw.position, static_cast<float>(enemy.yaw), enemyVisualScale(enemy.type) * appearance);
            const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), .5F);
            const float radius = std::clamp(std::max(extent.x, extent.z), scale * .35F, scale * .95F);
            const float height = std::clamp(bounds.max.y - bounds.min.y, scale * .8F, scale * 2.5F);
            draw.position.y = bounds.min.y;
            app_poison_drawing::emitPoison(draw.position, flameRight, up, Vector3Negate(cameraForward),
                radius, height, enemy.id.index, snapshot.elapsedSeconds, draw.amount,
                renderer_->settings().particles && rank < 24);
        }
        app_freeze_drawing::endBatch();
        renderer_->endColorOnlyWorldEffects();
    }
    WorldMaterialState statusMaterial{};
    statusMaterial.bakedAo = .82F;
    renderer_->setWorldMaterial(statusMaterial);
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
            .summonerId = visual.summonerId,
            .bossHoverOffset = visual.bossHoverOffset,
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
        const float popProgress = std::clamp(elapsed / 0.17F, 0.0F, 1.0F);
        const float popScale = visual.flashOnDeath
            ? (1.0F + std::sin(popProgress * PI) * 0.15F) *
                (0.75F + opacity * 0.25F)
            : 1.0F;
        destroyedEnemyDrawInstances_.push_back({
            .modelVisual =
                enemyModelVisual(visualEnemy),
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
            .scale = enemyVisualScale(visual.type) * popScale *
                (visual.eliteAffixes != 0U ? 1.08F : 1.0F),
            .loop = false,
            .lowDetail = distanceSquared >
                EnemyFullDetailDistanceSquared &&
                visual.type != EnemyType::Boss,
            .eliteAffixes = visual.eliteAffixes,
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
                    instance.inkOutlineEligible, false, instance.eliteAffixes));
            }
        }
    }
}

} // namespace ian
