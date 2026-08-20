#include "app/App.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "app/AppRenderSupport.hpp"
#include "graphics/WorldTransforms.hpp"

#include "ui/UiLabels.hpp"
#include "ui/TargetHealthBarAnchor.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <string>
#include <utility>

namespace ian {

using namespace app_detail;

void App::processPresentationEvents(
    std::span<const GameEvent> events,
    const SimulationSnapshot& eventSnapshot) {
    const auto setStatusMessage =
        [this](std::string message, double duration,
               bool lootDescription = false) {
            if (!lootDescription && lootDescriptionRemaining_ > 0.0) {
                return;
            }
            statusMessage_ = std::move(message);
            statusMessageRemaining_ = duration;
            if (lootDescription) {
                lootDescriptionRemaining_ = duration;
            }
        };
    const auto addProductionVisual =
        [this, &eventSnapshot](EntityId buildingId,
                               UiResourceIcon icon,
                               int amount) {
            const auto building = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [buildingId](const BuildingInstance& candidate) {
                    return candidate.id == buildingId;
                });
            Vec3 position{};
            if (building != eventSnapshot.buildings.end()) {
                position = buildingProductionVisualWorldAnchor(*building);
            } else {
                const auto landmark = std::find_if(
                    eventSnapshot.worldLandmarks.begin(),
                    eventSnapshot.worldLandmarks.end(),
                    [buildingId](const WorldLandmarkInstance& candidate) {
                        return candidate.id == buildingId;
                    });
                if (landmark == eventSnapshot.worldLandmarks.end()) return;
                position = landmark->position;
                position.y += 5.2;
            }
            constexpr std::size_t MaximumVisuals = 12;
            if (productionVisuals_.size() >= MaximumVisuals) {
                productionVisuals_.erase(
                    productionVisuals_.begin());
            }
            constexpr double Duration = 0.95;
            productionVisuals_.push_back({
                .buildingId = buildingId,
                .icon = icon,
                .position = position,
                .amount = amount,
                .remaining = Duration,
                .duration = Duration,
            });
        };
    const auto enemyBounds =
        [this, &eventSnapshot](EntityId id)
            -> std::optional<BoundingBox> {
            const auto enemy = std::find_if(
                eventSnapshot.enemies.begin(), eventSnapshot.enemies.end(),
                [id](const EnemyInstance& candidate) {
                    // The combat step can mark an enemy inactive before the
                    // presentation event is consumed. EntityId is still the
                    // authoritative match, so use its last snapshot bounds
                    // for damage numbers even on the death frame.
                    return candidate.id == id;
                });
            if (enemy == eventSnapshot.enemies.end()) {
                return std::nullopt;
            }
            Vector3 position = enemyRenderPosition(*enemy);
            position.y += static_cast<float>(simulation_.terrain().getHeight(
                enemy->position.x, enemy->position.z));
            const BoundingBox bounds = renderer_->enemyWorldBounds(
                enemyModelVisual(enemy->type), position,
                static_cast<float>(enemy->yaw),
                enemyVisualScale(enemy->type) *
                    (enemy->eliteAffixes != 0U
                        ? 1.08F : 1.0F));
            if (!world_transforms::finite(bounds)) {
                return std::nullopt;
            }
            return bounds;
        };
    const auto enemyDamageAnchor =
        [&enemyBounds](EntityId id, Vec3 fallback) {
            const auto bounds = enemyBounds(id);
            if (!bounds) {
                return fallback;
            }
            return Vec3{
                static_cast<double>((bounds->min.x + bounds->max.x) * 0.5F),
                static_cast<double>(bounds->max.y + 0.12F),
                static_cast<double>((bounds->min.z + bounds->max.z) * 0.5F)};
        };
    const auto enemyImpactAnchor =
        [&enemyBounds](EntityId id, Vec3 fallback) {
            const auto bounds = enemyBounds(id);
            if (!bounds) {
                return fallback;
            }
            const float height = bounds->max.y - bounds->min.y;
            return Vec3{
                fallback.x,
                static_cast<double>(bounds->min.y + height * 0.56F),
                fallback.z};
        };
    const auto enemySurfaceImpactAnchor =
        [&enemyBounds](EntityId id, Vec3 fallback, Vec3 source) {
            const auto bounds = enemyBounds(id);
            if (!bounds) {
                return fallback;
            }
            const double centerX =
                static_cast<double>((bounds->min.x + bounds->max.x) * 0.5F);
            const double centerZ =
                static_cast<double>((bounds->min.z + bounds->max.z) * 0.5F);
            double directionX = centerX - source.x;
            double directionZ = centerZ - source.z;
            const double directionLength =
                std::hypot(directionX, directionZ);
            if (directionLength <= 1e-6) {
                directionX = 0.0;
                directionZ = 1.0;
            } else {
                directionX /= directionLength;
                directionZ /= directionLength;
            }
            const double halfWidth =
                static_cast<double>(bounds->max.x - bounds->min.x) * 0.5;
            const double halfDepth =
                static_cast<double>(bounds->max.z - bounds->min.z) * 0.5;
            const double surfaceDistance =
                std::abs(directionX) * halfWidth +
                std::abs(directionZ) * halfDepth + 0.10;
            const double height =
                static_cast<double>(bounds->max.y - bounds->min.y);
            return Vec3{
                centerX - directionX * surfaceDistance,
                static_cast<double>(bounds->min.y) + height * 0.56,
                centerZ - directionZ * surfaceDistance,
            };
        };
    for (const auto& event : events) {
        const bool towerHit =
            event.type == GameEventType::ProjectileHit &&
            event.sourceId && event.buildingType &&
            (*event.buildingType == BuildingType::Turret ||
             *event.buildingType == BuildingType::GunTurret);
        const bool enemyHit =
            (event.type == GameEventType::ProjectileHit &&
             !event.sourceId) ||
            towerHit ||
            event.type == GameEventType::TrapHit ||
            event.type == GameEventType::CannonHit ||
            event.type == GameEventType::PickaxeHit ||
            event.type == GameEventType::IceWandHit ||
            event.type == GameEventType::FireWandHit ||
            event.type == GameEventType::ChainLightningHit ||
            event.type == GameEventType::OverkillHit;
        if (enemyHit && event.entityId) {
            targetHealthBar_.notifyEnemyHit(*event.entityId);
            Vec3 sourcePosition = eventSnapshot.playerPosition;
            if (event.type == GameEventType::ChainLightningHit) {
                sourcePosition = event.position;
            } else if (event.sourceId) {
                const auto source = std::find_if(
                    eventSnapshot.buildings.begin(),
                    eventSnapshot.buildings.end(),
                    [&event](const BuildingInstance& building) {
                        return building.id == *event.sourceId;
                    });
                if (source != eventSnapshot.buildings.end()) {
                    sourcePosition = buildingWorldPosition(*source);
                }
            }
            const Vec3 impactPosition = enemySurfaceImpactAnchor(
                *event.entityId,
                event.targetPosition.value_or(event.position),
                sourcePosition);
            const double distance = std::hypot(
                impactPosition.x - eventSnapshot.playerPosition.x,
                impactPosition.z - eventSnapshot.playerPosition.z);
            const float distanceScale = static_cast<float>(
                1.0 - std::clamp((distance - 10.0) / 28.0, 0.0, 1.0) *
                    0.38);
            int variant = 0;
            if (event.type == GameEventType::IceWandHit) {
                variant = 1;
            } else if (event.type == GameEventType::FireWandHit) {
                variant = 2;
            } else if (event.type == GameEventType::ChainLightningHit) {
                variant = 3;
            } else if (event.type == GameEventType::ProjectileHit ||
                       event.type == GameEventType::CannonHit) {
                variant = 4;
            }
            if (event.critical) {
                variant |= 8;
            }
            addEffect(
                PresentationEffectType::EnemyHitImpact,
                impactPosition, event.critical ? 0.46 : 0.38,
                distanceScale * (event.critical ? 1.38F : 1.0F),
                event.entityId);
            PresentationEffect& impact = effects_.back();
            impact.targetPosition = sourcePosition;
            impact.variant = variant;
        }
        if ((event.type == GameEventType::ResourceHit ||
             event.type == GameEventType::ResourceCollected) &&
            event.entityId) {
            targetHealthBar_.notifyResourceHit(*event.entityId);
        }
        if (event.type == GameEventType::RunStarted ||
            event.type == GameEventType::RunRestarted) {
            displayedInsight_ = 0.0;
            insightGainAmount_ = 0.0;
            insightGainRemaining_ = 0.0;
            insightPointSequenceRemaining_ = 0.0;
            pendingInsightPointNotification_ = 0;
            lootDescriptionRemaining_ = 0.0;
            objectiveProgressCache_.clear();
            objectivePulseId_.clear();
            objectivePulseRemaining_ = 0.0;
        }
        if (event.type == GameEventType::InsightGranted) {
            const InsightConfig& insightConfig = simulation_.insightSystem().config();
            if (insightGainRemaining_ <= 0.0) insightGainAmount_ = 0.0;
            insightGainAmount_ += event.insightAmount;
            insightGainDuration_ = std::max(0.01, insightConfig.hudAggregationWindowSeconds);
            insightGainRemaining_ = insightGainDuration_;
            insightPulseDuration_ = std::max(0.01,
                event.insightAmount >= insightConfig.hudLargeRewardThreshold
                    ? insightConfig.hudLargePulseSeconds
                    : insightConfig.hudSmallPulseSeconds);
            insightPulseRemaining_ = insightPulseDuration_;
            if (event.treePointsGranted > 0) {
                insightPointSequenceDuration_ = std::max(
                    0.05, insightConfig.hudPointSequenceSeconds);
                insightPointSequenceRemaining_ = insightPointSequenceDuration_;
                insightAnimationBefore_ = event.insightBefore;
                insightAnimationAfter_ = event.insightAfter;
                insightAnimationRequirement_ = event.insightRequirement;
                insightAnimationPoints_ = event.treePointsGranted;
                pendingInsightPointNotification_ +=
                    event.treePointsGranted;
            } else if (event.insightAmount >= insightConfig.hudLargeRewardThreshold &&
                       event.insightSource &&
                       *event.insightSource != InsightSource::Objective) {
                setStatusMessage("+" + std::to_string(
                    static_cast<int>(std::lround(event.insightAmount))) +
                    " INSIGHT — " + std::string(insightSourceName(*event.insightSource)),
                    1.8);
            }
        }
        if (event.type == GameEventType::ObjectiveCompleted && event.objectiveId) {
            const auto objective = std::ranges::find_if(
                eventSnapshot.objectives,
                [&event](const ObjectiveStatus& status) {
                    return status.definition.id == *event.objectiveId;
                });
            const std::string title = objective != eventSnapshot.objectives.end()
                ? objective->definition.title : *event.objectiveId;
            setStatusMessage("OBJECTIVE COMPLETE: " + title + "  +" +
                std::to_string(static_cast<int>(std::lround(event.intensity))) +
                " INSIGHT", 2.6);
        }
        if (event.type == GameEventType::RunStarted ||
            event.type == GameEventType::RunRestarted) {
            constexpr double SpawnDropHeight = 3.2;
            playerSpawnDropActive_ = true;
            playerSpawnDropHeight_ = SpawnDropHeight;
            playerSpawnDropVelocity_ = -0.35;
        }
        audio_.playEvent(event, eventSnapshot);
        if (event.type == GameEventType::PlayerDashed) {
            Vec3 dustPosition = event.position;
            dustPosition.y = simulation_.terrain().getHeight(
                dustPosition.x, dustPosition.z) + 0.04;
            addEffect(
                PresentationEffectType::LandingDust,
                dustPosition, 0.34, 0.72F);
            addCameraImpulse({0.0, 0.002, -0.032});
            addCameraShake(0.08, 0.009);
        } else if (event.type == GameEventType::CannonFired &&
            event.sourceId &&
            event.buildingType != BuildingType::Catapult) {
            addBuildingShotRecoil(
                *event.sourceId, 0.18, 0.13F);
        } else if (
            event.type == GameEventType::ProjectileHit &&
            event.sourceId) {
            addBuildingShotRecoil(
                *event.sourceId, 0.12, 0.075F);
        }
        const bool playerHit =
            event.type == GameEventType::ResourceHit ||
            event.type == GameEventType::ResourceCollected ||
            event.type == GameEventType::PickaxeHit ||
            event.type == GameEventType::ChainLightningHit ||
            (event.type == GameEventType::ProjectileHit &&
             !event.sourceId);
        if (playerHit) {
            crosshairHitRemaining_ = crosshairHitDuration_;
            crosshairHitCritical_ = event.critical;
        }
        if (event.type == GameEventType::IceWandChargeStarted ||
            event.type == GameEventType::FireWandChargeStarted) {
            // Use the same authored tool-swing timeline as the other
            // first-person tools; the wand's charge glow runs in parallel.
            toolSwingUsesAxe_ = false;
            toolSwingDuration_ = activeToolTuning().swingDuration;
            toolSwingRemaining_ = toolSwingDuration_;
            toolSwingAttackPending_ = false;
            toolSwingQueued_ = false;
            toolQueuedSwingHasAttack_ = false;
            toolQueuedResourceTarget_.reset();
            toolSwingQueueRemaining_ = 0.0;
        } else if (event.type == GameEventType::WeaponFired) {
            weaponRecoilDuration_ = 0.16;
            weaponRecoilRemaining_ = weaponRecoilDuration_;
            weaponRecoilStrength_ = 0.045F;
            addCameraImpulse({0.0, 0.008, -0.012});
        } else if (event.type == GameEventType::IceWandFired ||
                   event.type == GameEventType::FireWandFired) {
            iceWandRecoilDuration_ = 0.20;
            iceWandRecoilRemaining_ = iceWandRecoilDuration_;
            addCameraImpulse({0.0, 0.006, -0.018});
        } else if (event.type == GameEventType::IceWandImpact) {
            addEffect(PresentationEffectType::IceImpact,
                      event.position, 0.72, 1.0F,
                      event.entityId);
            const SimulationSnapshot& playerSnapshot = eventSnapshot;
            const double distance = std::hypot(
                event.position.x - playerSnapshot.playerPosition.x,
                event.position.z - playerSnapshot.playerPosition.z);
            if (distance < 9.0) {
                iceImpactFlashRemaining_ = std::max(
                    iceImpactFlashRemaining_,
                    0.10 * (1.0 - distance / 9.0));
                addCameraImpulse({0.0, 0.003, -0.006});
                addCameraShake(0.10, 0.018 *
                    (1.0 - std::min(distance / 9.0, 1.0)));
            }
        } else if (event.type == GameEventType::IceWandHit) {
            const Vec3 numberPosition = event.entityId
                ? enemyDamageAnchor(*event.entityId, event.position)
                : event.position;
            addFloatingDamageNumber(numberPosition, event.damage, false);
            if (event.critical) {
                addEffect(PresentationEffectType::IceCrack,
                          event.position, 0.38, 0.55F,
                          event.entityId);
            }
        } else if (event.type == GameEventType::FireWandImpact) {
            addEffect(
                PresentationEffectType::FireImpact,
                event.position, 0.62, 1.0F, event.entityId);
            const double distance = std::hypot(
                event.position.x - eventSnapshot.playerPosition.x,
                event.position.z - eventSnapshot.playerPosition.z);
            if (distance < 9.0) {
                addCameraImpulse({0.0, 0.004, -0.008});
                addCameraShake(
                    0.11, 0.022 *
                        (1.0 - std::min(distance / 9.0, 1.0)));
            }
        } else if (event.type == GameEventType::FireWandHit) {
            const Vec3 numberPosition = event.entityId
                ? enemyDamageAnchor(*event.entityId, event.position)
                : event.position;
            addFloatingDamageNumber(numberPosition, event.damage, false);
            if (event.entityId && event.intensity > 0.0) {
                const auto existing = std::find_if(
                    effects_.begin(), effects_.end(),
                    [&event](const PresentationEffect& effect) {
                        return effect.type ==
                                   PresentationEffectType::EnemyBurn &&
                            effect.entityId == event.entityId;
                    });
                if (existing != effects_.end()) {
                    existing->position = event.position;
                    existing->remaining = event.intensity;
                    existing->duration = event.intensity;
                } else {
                    addEffect(
                        PresentationEffectType::EnemyBurn,
                        event.position, event.intensity, 1.0F,
                        event.entityId);
                }
            }
        } else if (event.type == GameEventType::OverkillHit) {
            const Vec3 numberPosition = event.entityId
                ? enemyDamageAnchor(*event.entityId, event.position)
                : event.position;
            addFloatingDamageNumber(numberPosition, event.damage, false);
            addEffect(PresentationEffectType::Hit,
                      event.position, 0.24, 1.05F,
                      event.entityId);
        } else if (
            event.type == GameEventType::PickaxeHit ||
            (!event.sourceId &&
             (event.type == GameEventType::ResourceHit ||
              event.type == GameEventType::ResourceCollected))) {
            weaponRecoilDuration_ = 0.12;
            weaponRecoilRemaining_ = weaponRecoilDuration_;
            weaponRecoilStrength_ =
                event.critical ? 0.035F : 0.024F;
            addCameraImpulse({0.0, -0.006, 0.008});
        }
        if (event.type == GameEventType::SawSplinterLaunched &&
            event.targetPosition) {
            constexpr std::size_t MaxEffects = 128;
            if (effects_.size() >= MaxEffects) {
                effects_.erase(effects_.begin());
            }
            const double duration = event.intensity > 0.0
                ? event.intensity : 0.36;
            effects_.push_back({
                .type = PresentationEffectType::SawSplinter,
                .entityId = event.entityId,
                .position = event.position,
                .targetPosition = event.targetPosition,
                .remaining = duration,
                .duration = duration,
                .scale = 1.0F,
                .variant = event.amount,
            });
        } else if (event.type == GameEventType::EliteEnemySpawned) {
            addEffect(
                PresentationEffectType::EliteSpawn,
                event.position, 0.72, 1.0F,
                event.entityId);
        } else if (
            event.type == GameEventType::EliteVolatilePrimed) {
            addEffect(
                PresentationEffectType::VolatileCharge,
                event.position,
                event.intensity > 0.0 ? event.intensity : 1.15,
                1.0F, event.entityId);
        } else if (event.type == GameEventType::ChainLightningHit &&
            event.entityId && event.targetPosition) {
            constexpr std::size_t MaxEffects = 128;
            if (effects_.size() >= MaxEffects) {
                effects_.erase(effects_.begin());
            }
            // Lightning should connect through the enemies' torsos. The
            // damage-number anchor intentionally sits above the head and is
            // therefore unsuitable for a world-space electrical arc.
            const Vec3 target = enemyImpactAnchor(
                *event.entityId, *event.targetPosition);
            const Vec3 source = event.sourceId
                ? enemyImpactAnchor(
                      *event.sourceId, event.position)
                : event.position;
            effects_.push_back({
                .type = PresentationEffectType::ChainLightning,
                .entityId = event.entityId,
                .position = source,
                .targetPosition = target,
                .remaining = 0.42,
                .duration = 0.42,
                .startDelayRemaining =
                    static_cast<double>(event.amount) * 0.045,
                .scale = 1.0F,
                .variant = event.amount,
            });
            addFloatingDamageNumber(
                target, event.damage, false);
            addCameraShake(
                0.10, event.amount == 0 ? 0.025 : 0.012);
        } else if (event.type == GameEventType::ResourceHit &&
            !event.sourceId) {
            toolContactHoldRemaining_ = std::max(
                toolContactHoldRemaining_, 0.025);
        } else if (event.type == GameEventType::ResourceCollected &&
            !event.sourceId) {
            toolContactHoldRemaining_ = std::max(
                toolContactHoldRemaining_, 0.04);
        }
        if (event.type == GameEventType::BuildingRejected ||
            event.type ==
                GameEventType::BuildingUpgradeRejected ||
            event.type ==
                GameEventType::BuildingRepairRejected ||
            event.type ==
                GameEventType::BuildingSellRejected ||
            event.type ==
                GameEventType::WeaponUpgradeRejected ||
            event.type ==
                GameEventType::GateToggleRejected ||
            event.type ==
                GameEventType::EconomyPurchaseRejected ||
            event.type ==
                GameEventType::ChestOpenRejected) {
            invalidActionRemaining_ = 0.22;
        }
        if (event.type == GameEventType::ProjectileHit &&
            event.sourceId && !event.secondaryImpact) {
            const auto source = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [&event](const BuildingInstance& building) {
                    return building.id == *event.sourceId &&
                           (building.type == BuildingType::Turret ||
                            building.type == BuildingType::GunTurret);
                });
            if (source != eventSnapshot.buildings.end()) {
                Vec3 origin = buildingWorldPosition(*source);
                Vec3 projectileTarget = event.position;
                std::optional<Vec3> authoredDirection;
                const bool gunTurret =
                    source->type == BuildingType::GunTurret;
                if (gunTurret) {
                    const auto runtime = std::find_if(
                        eventSnapshot.towers.begin(),
                        eventSnapshot.towers.end(),
                        [&source](const TowerRuntime& tower) {
                            return tower.buildingId == source->id;
                        });
                    const float yaw = runtime != eventSnapshot.towers.end()
                        ? static_cast<float>(runtime->yaw)
                        : static_cast<float>(buildingRotationYaw(
                              source->type, source->rotation));
                    origin = renderer_->gunTurretMuzzlePosition(
                        origin, yaw,
                        static_cast<std::size_t>(std::max(0, event.amount)));
                    if (event.entityId) {
                        if (const auto bounds = enemyBounds(*event.entityId)) {
                            projectileTarget.y =
                                (static_cast<double>(bounds->min.y) +
                                 static_cast<double>(bounds->max.y)) * 0.5;
                        }
                    }
                } else {
                    const auto runtime = std::find_if(
                        eventSnapshot.towers.begin(),
                        eventSnapshot.towers.end(),
                        [&source](const TowerRuntime& tower) {
                            return tower.buildingId == source->id;
                        });
                    const float yaw = runtime != eventSnapshot.towers.end()
                        ? static_cast<float>(runtime->yaw)
                        : static_cast<float>(buildingRotationYaw(
                              source->type, source->rotation));
                    const float pitch = runtime != eventSnapshot.towers.end()
                        ? static_cast<float>(runtime->pitch) : 0.0F;
                    origin = renderer_->crossbowMuzzlePosition(
                        origin, yaw, pitch);
                    const double horizontal = std::cos(
                        static_cast<double>(pitch));
                    authoredDirection = Vec3{
                        -std::sin(static_cast<double>(yaw)) * horizontal,
                        std::sin(static_cast<double>(pitch)),
                        -std::cos(static_cast<double>(yaw)) * horizontal,
                    };
                }
                const double deltaX = projectileTarget.x - origin.x;
                const double deltaY = projectileTarget.y - origin.y;
                const double deltaZ = projectileTarget.z - origin.z;
                const double distance = std::sqrt(
                    deltaX * deltaX + deltaY * deltaY +
                    deltaZ * deltaZ);
                const double duration =
                    std::clamp(distance / (gunTurret ? 45.0 : 18.0),
                               gunTurret ? 0.03 : 0.08,
                               gunTurret ? 0.16 : 0.35);
                arrowVisuals_.push_back({
                    .origin = origin,
                    .target = projectileTarget,
                    .direction = authoredDirection.value_or(Vec3{
                        deltaX, deltaY, deltaZ}),
                    .remaining = duration,
                    .duration = duration,
                    .turretBullet = gunTurret,
                });
            }
        }
        if (event.type == GameEventType::ResourceHit) {
            addEffect(PresentationEffectType::Hit,
                      event.position, 0.34, 1.0F,
                      event.entityId);
            if (event.resourceType) {
                addEffect(
                    *event.resourceType == ResourceType::Wood
                        ? PresentationEffectType::ResourceHitWood
                        : PresentationEffectType::ResourceHitStone,
                    event.position, 0.46,
                    event.critical ? 1.3F : 1.0F,
                    event.entityId);
            }
        } else if (
            event.type == GameEventType::ProjectileHit ||
            event.type == GameEventType::TrapHit ||
            event.type == GameEventType::PickaxeHit ||
            event.type == GameEventType::IceWandHit ||
            event.type == GameEventType::FireWandHit) {
            const Vec3 impactPosition = event.entityId
                ? enemyImpactAnchor(*event.entityId, event.position)
                : event.position;
            if (event.type == GameEventType::TrapHit ||
                (event.type == GameEventType::ProjectileHit &&
                 event.sourceId)) {
                addEffect(PresentationEffectType::Hit,
                          impactPosition, 0.18, 0.8F,
                          event.entityId);
            }
            if (event.type == GameEventType::TrapHit &&
                event.entityId) {
                addFloatingDamageNumber(
                    enemyDamageAnchor(
                        *event.entityId, event.position),
                    event.damage, false);
            }
        } else if (event.type == GameEventType::ResourceCollected) {
            if (event.resourceType) {
                const auto node = std::find_if(
                    eventSnapshot.resourceNodes.begin(),
                    eventSnapshot.resourceNodes.end(),
                    [&event](const ResourceNode& candidate) {
                        return event.entityId &&
                               candidate.id == *event.entityId;
                    });
                const Vec3 center =
                    node != eventSnapshot.resourceNodes.end()
                        ? node->position
                        : event.position;
                const auto type =
                    *event.resourceType == ResourceType::Wood
                        ? PresentationEffectType::ResourceDestroyedWood
                        : PresentationEffectType::ResourceDestroyedStone;
                addEffect(type, center, 0.92, 1.0F);
                destroyedResourceVisuals_.push_back({
                    .type = *event.resourceType,
                    .visualVariant =
                        node != eventSnapshot.resourceNodes.end()
                            ? node->visualVariant
                            : 0U,
                    .visualYaw =
                        node != eventSnapshot.resourceNodes.end()
                            ? static_cast<float>(node->visualYaw)
                            : 0.0F,
                    .visualScale =
                        node != eventSnapshot.resourceNodes.end()
                            ? static_cast<float>(node->visualScale)
                            : 1.0F,
                    .position = center,
                    .remaining = 0.42,
                    .duration = 0.42,
                });
            }
        } else if (event.type == GameEventType::EnemySplit) {
            addEffect(
                PresentationEffectType::SplitBurst,
                event.position, 1.05,
                0.9F + static_cast<float>(event.amount) * 0.08F,
                event.entityId);
            const double distance = std::hypot(
                event.position.x - eventSnapshot.playerPosition.x,
                event.position.z - eventSnapshot.playerPosition.z);
            const double proximity = std::clamp(
                1.0 - distance / 22.0, 0.0, 1.0);
            addCameraShake(0.16, 0.035 + 0.045 * proximity);
            addCameraImpulse({0.0, 0.018 * proximity, 0.0});
        } else if (event.type == GameEventType::Explosion) {
            const double distance = std::hypot(
                event.position.x - eventSnapshot.playerPosition.x,
                event.position.z - eventSnapshot.playerPosition.z);
            const double proximity = std::clamp(
                1.0 - distance / 30.0, 0.0, 1.0);
            addEffect(PresentationEffectType::Explosion, event.position, 1.25);
            const double burnRadius = event.intensity > 0.0
                ? event.intensity
                : 4.0;
            constexpr std::size_t MaxBurnsPerExplosion = 32U;
            std::size_t burnCount = 0U;
            for (const EnemyInstance& enemy : eventSnapshot.enemies) {
                if (!enemy.active || burnCount >= MaxBurnsPerExplosion) {
                    continue;
                }
                const double deltaX =
                    enemy.position.x - event.position.x;
                const double deltaZ =
                    enemy.position.z - event.position.z;
                if (deltaX * deltaX + deltaZ * deltaZ >
                    burnRadius * burnRadius) {
                    continue;
                }
                const double duration = 2.35 +
                    static_cast<double>(enemy.id.index % 5U) * 0.09;
                const auto existing = std::find_if(
                    effects_.begin(), effects_.end(),
                    [&enemy](const PresentationEffect& effect) {
                        return effect.type ==
                                   PresentationEffectType::EnemyBurn &&
                            effect.entityId == enemy.id;
                    });
                const Vec3 burnPosition{
                    enemy.position.x,
                    enemy.position.y + enemy.worldSurfaceHeight,
                    enemy.position.z,
                };
                if (existing != effects_.end()) {
                    existing->position = burnPosition;
                    existing->remaining = duration;
                    existing->duration = duration;
                } else {
                    addEffect(
                        PresentationEffectType::EnemyBurn,
                        burnPosition, duration, 1.0F, enemy.id);
                }
                ++burnCount;
            }
            addCameraShake(0.34, 0.055 + 0.17 * proximity);
            addCameraImpulse({0.0, 0.035 * proximity,
                              -0.025 * proximity});
        } else if (
            event.type == GameEventType::BuildingDestroyed ||
            event.type ==
                GameEventType::ModularBuildingDestroyed) {
            if (event.building) {
                constexpr double Duration = 0.52;
                const std::uint8_t connections =
                    event.building->type ==
                            BuildingType::Wall
                        ? wallConnectionMask(
                              eventSnapshot.buildings,
                              event.building
                                  ->gridPosition,
                              event.building
                                  ->baseHeight)
                        : 0U;
                soldBuildingVisuals_.push_back({
                    .building = *event.building,
                    .platformFrame = std::nullopt,
                    .modularWall = std::nullopt,
                    .ramp = std::nullopt,
                    .wallConnections = connections,
                    .remaining = Duration,
                    .duration = Duration,
                });
            }
            if (event.platformFrame || event.modularWall ||
                event.ramp) {
                constexpr double Duration = 0.52;
                soldBuildingVisuals_.push_back({
                    .building = std::nullopt,
                    .platformFrame = event.platformFrame,
                    .modularWall = event.modularWall,
                    .ramp = event.ramp,
                    .remaining = Duration,
                    .duration = Duration,
                });
            }
            addEffect(PresentationEffectType::Debris, event.position, 0.8);
            addCameraShake(0.18, 0.08);
        } else if (
            (event.type == GameEventType::BuildingDamaged ||
             event.type ==
                 GameEventType::ModularBuildingDamaged) &&
                   event.entityId) {
            addEffect(
                PresentationEffectType::BuildingDamaged,
                event.position, 0.3, 1.0F,
                event.entityId);
            recentlyDamagedBuilding_ = *event.entityId;
            damagedBuildingHealthBarRemaining_ = 1.6;
            const auto building = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [&event](const BuildingInstance& candidate) {
                    return candidate.id == *event.entityId;
                });
            const auto attacker =
                event.sourceId
                    ? std::find_if(
                          eventSnapshot.enemies.begin(),
                          eventSnapshot.enemies.end(),
                          [&event](const EnemyInstance& enemy) {
                              return enemy.id ==
                                     *event.sourceId;
                          })
                    : eventSnapshot.enemies.end();
            if (building != eventSnapshot.buildings.end() &&
                attacker != eventSnapshot.enemies.end()) {
                const Vec3 center =
                    buildingWorldPosition(*building);
                const double deltaX =
                    center.x - attacker->position.x;
                const double deltaZ =
                    center.z - attacker->position.z;
                const double length = std::sqrt(
                    deltaX * deltaX + deltaZ * deltaZ);
                if (length > 1e-6) {
                    constexpr double Duration = 0.28;
                    buildingImpactVisuals_.push_back({
                        .id = *event.entityId,
                        .direction = {
                            deltaX / length, 0.0,
                            deltaZ / length,
                        },
                        .remaining = Duration,
                        .duration = Duration,
                    });
                }
            }
        } else if (event.type == GameEventType::BossRamImpact) {
            addEffect(PresentationEffectType::RamImpact, event.position, 0.7);
            addCameraShake(0.35, 0.2);
        } else if (event.type == GameEventType::CoreDamaged) {
            addCameraShake(0.1, 0.04);
            addCameraImpulse({0.0, -0.008, 0.012});
        } else if (event.type == GameEventType::PlayerLanded) {
            landingResponseDuration_ = 0.24;
            landingResponseRemaining_ = landingResponseDuration_;
            landingResponseStrength_ = std::clamp(
                (event.intensity - 1.0) / 7.0,
                0.25, 1.0);
            addCameraImpulse({0.0,
                              -0.012 * landingResponseStrength_,
                              0.0});
        } else if (event.type == GameEventType::BuildingPlaced &&
                   event.buildingType) {
            grassClearAreas_.push_back({
                .center = {
                    static_cast<float>(event.position.x),
                    static_cast<float>(event.position.z),
                },
                .innerRadius = static_cast<float>(
                    buildingFootprintHalfExtent(
                        *event.buildingType) +
                    0.18),
                .amount = 1.0F,
            });
            float effectScale = 1.0F;
            if (*event.buildingType == BuildingType::Core) {
                effectScale = 1.45F;
            } else if (*event.buildingType == BuildingType::Wall ||
                       *event.buildingType == BuildingType::Gate) {
                effectScale = 0.78F;
            } else if (
                *event.buildingType == BuildingType::SlowTrap ||
                *event.buildingType == BuildingType::SpikeTrap) {
                effectScale = 0.68F;
            }
            addEffect(PresentationEffectType::BuildingPlaced,
                      event.position, 0.7, effectScale,
                      event.entityId);
        } else if (event.type == GameEventType::BuildingUpgraded &&
                   event.buildingType) {
            if (event.entityId) {
                buildingStatsUpgradeEntity_ = *event.entityId;
                buildingStatsUpgradeRemaining_ =
                    buildingStatsUpgradeDuration_;
            }
            float effectScale = 1.0F;
            if (*event.buildingType == BuildingType::Core) {
                effectScale = 1.45F;
            } else if (*event.buildingType == BuildingType::Wall ||
                       *event.buildingType == BuildingType::Gate) {
                effectScale = 0.78F;
            } else if (
                *event.buildingType == BuildingType::SlowTrap ||
                *event.buildingType == BuildingType::SpikeTrap) {
                effectScale = 0.68F;
            }
            effectScale *= 1.15F;
            addEffect(PresentationEffectType::BuildingUpgrade,
                      event.position, 0.85, effectScale);
        } else if (event.type ==
                   GameEventType::WorldLandmarkActivated) {
            addEffect(
                PresentationEffectType::BuildingUpgrade,
                event.position, 1.05, 1.8F,
                event.entityId);
            addCameraImpulse({0.0, 0.025, -0.012});
        } else if (event.type ==
                   GameEventType::AnvilRepairShockwave) {
            addEffect(
                PresentationEffectType::RepairShockwave,
                event.position, 0.72,
                static_cast<float>(event.intensity),
                event.entityId);
            addCameraImpulse({0.0, 0.018, -0.01});
        } else if (event.type ==
                       GameEventType::BuildingRepaired &&
                   event.entityId && event.buildingType) {
            addEffect(
                PresentationEffectType::BuildingRepaired,
                event.position, 0.62,
                static_cast<float>(
                    buildingFootprintHalfExtent(
                        *event.buildingType)));
            targetHealthBar_.notifyRepair(*event.entityId);
            recentlyDamagedBuilding_ = *event.entityId;
            damagedBuildingHealthBarRemaining_ = 1.25;
        } else if (
            event.type ==
                GameEventType::ModularBuildingRepaired &&
            event.entityId) {
            addEffect(
                PresentationEffectType::BuildingRepaired,
                event.position, 0.62, 1.0F);
            targetHealthBar_.notifyRepair(*event.entityId);
            recentlyDamagedBuilding_ = *event.entityId;
            damagedBuildingHealthBarRemaining_ = 1.25;
        } else if (event.type == GameEventType::BuildingSold &&
                   event.entityId) {
            if (pendingSoldBuildingVisual_ &&
                pendingSoldBuildingVisual_->id ==
                    *event.entityId) {
                constexpr double Duration = 0.52;
                soldBuildingVisuals_.push_back({
                    .building = *pendingSoldBuildingVisual_,
                    .platformFrame = std::nullopt,
                    .modularWall = std::nullopt,
                    .ramp = std::nullopt,
                    .wallConnections =
                        pendingSoldWallConnections_,
                    .remaining = Duration,
                    .duration = Duration,
                });
                addEffect(
                    PresentationEffectType::Debris,
                    event.position, 0.62,
                    static_cast<float>(
                        buildingFootprintHalfExtent(
                            pendingSoldBuildingVisual_->type)));
            }
            pendingSoldBuildingVisual_.reset();
            pendingSoldWallConnections_ = 0U;
        } else if (
            event.type ==
            GameEventType::BuildingSellRejected) {
            pendingSoldBuildingVisual_.reset();
            pendingSoldWallConnections_ = 0U;
        }
        if (event.type == GameEventType::ResourceHit ||
            event.type == GameEventType::ResourceCollected) {
            addCameraShake(0.1, 0.06);
            if (event.resourceType && event.amount > 0) {
                addResourceGainVisual(
                    *event.resourceType, event.position,
                    event.amount);
            }
        } else if (event.type == GameEventType::ResourceGranted &&
                   event.resourceType) {
            if (*event.resourceType == ResourceType::Wood) {
                woodHudBounceRemaining_ = 0.28;
            } else if (*event.resourceType == ResourceType::Stone) {
                stoneHudBounceRemaining_ = 0.28;
            } else if (*event.resourceType == ResourceType::Crystal) {
                crystalHudBounceRemaining_ = 0.28;
            }
            if (event.entityId && event.buildingType &&
                (*event.buildingType ==
                     BuildingType::LumberMill ||
                 *event.buildingType ==
                     BuildingType::Quarry) &&
                event.amount > 0) {
                addProductionVisual(
                    *event.entityId,
                    *event.resourceType == ResourceType::Wood
                        ? UiResourceIcon::Wood
                        : UiResourceIcon::Stone,
                    event.amount);
            }
        } else if (
            event.type == GameEventType::CrystalProduced &&
            event.entityId && event.amount > 0) {
            addProductionVisual(
                *event.entityId, UiResourceIcon::Crystal,
                event.amount);
        } else if (event.type == GameEventType::CoinCollected &&
                   event.amount > 0) {
            coinHudBounceRemaining_ = 0.32;
        } else if (event.type == GameEventType::PickaxeHit) {
            Vec3 numberPosition = event.position;
            if (event.entityId) {
                numberPosition = enemyDamageAnchor(
                    *event.entityId, numberPosition);
            } else {
                numberPosition.y += 1.2;
            }
            addFloatingDamageNumber(
                numberPosition, event.damage, event.critical);
        }
        constexpr std::size_t MaxDestroyedEnemyVisuals =
            192U;
        if (event.type == GameEventType::EnemyKilled &&
            event.entityId &&
            destroyedEnemyVisuals_.size() <
                MaxDestroyedEnemyVisuals) {
            const auto enemy = std::find_if(
                eventSnapshot.enemies.begin(),
                eventSnapshot.enemies.end(),
                [&event](const EnemyInstance& candidate) {
                    return candidate.id == *event.entityId;
                });
            if (enemy != eventSnapshot.enemies.end()) {
                constexpr double DeathDuration = 0.9;
                destroyedEnemyVisuals_.push_back({
                    .type = enemy->type,
                    .eliteAffixes = enemy->eliteAffixes,
                    .position = enemy->position,
                    .surfaceHeightOffset =
                        enemy->surfaceHeightOffset,
                    .yaw = enemy->yaw,
                    .remaining = DeathDuration,
                    .duration = DeathDuration,
                });
            }
        }
        if (event.type == GameEventType::PlayerDamaged) {
            addDamageIndicator(event.position, eventSnapshot, false);
            playerDamageFlashRemaining_ = 0.18;
            const double deltaX =
                event.position.x - eventSnapshot.playerPosition.x;
            const double deltaZ =
                event.position.z - eventSnapshot.playerPosition.z;
            const double length = std::hypot(deltaX, deltaZ);
            if (length > 1e-6) {
                const double rightX =
                    std::cos(eventSnapshot.playerYaw);
                const double rightZ =
                    std::sin(eventSnapshot.playerYaw);
                addCameraImpulse({
                    -(deltaX * rightX + deltaZ * rightZ) /
                        length * 0.025,
                    0.012,
                    0.018,
                });
            }
        } else if (event.type == GameEventType::CoreDamaged) {
            addDamageIndicator(event.position, eventSnapshot, false);
        } else if (event.type == GameEventType::BossRamImpact) {
            addDamageIndicator(event.position, eventSnapshot, true);
        } else if (event.type == GameEventType::LootCollected) {
            if (event.lootRarity && event.lootUpgradeEffect) {
                addLootPickupEffect(
                    event.position, *event.lootRarity,
                    *event.lootUpgradeEffect, event.entityId);
            } else {
                addEffect(PresentationEffectType::LootCollected,
                          event.position, 0.96, 0.88F,
                          event.entityId);
            }
            addCameraImpulse({0.0, 0.012, -0.008});
        } else if (event.type == GameEventType::BattlePotionActivated) {
            addLootPickupEffect(
                event.position, LootRarity::Rare,
                LootUpgradeEffect::Potion, std::nullopt);
            addCameraImpulse({0.0, 0.025, -0.018});
        }

        std::string message;
        if (event.type == GameEventType::BuildingUpgraded && event.buildingType) {
            message =
                std::string(buildingDisplayName(
                    *event.buildingType)) +
                " upgraded";
        } else if (event.type == GameEventType::BuildingUpgradeRejected &&
                   event.upgradeError) {
            message = upgradeErrorMessage(*event.upgradeError);
        } else if (event.type == GameEventType::BuildingRepaired && event.buildingType) {
            message =
                std::string(buildingDisplayName(
                    *event.buildingType)) +
                " repaired";
        } else if (
            event.type ==
            GameEventType::ModularBuildingRepaired) {
            message = "Structure repaired";
        } else if ((event.type == GameEventType::BuildingRepairRejected ||
                    event.type == GameEventType::BuildingSellRejected) &&
                   event.buildingActionError) {
            if (*event.buildingActionError ==
                    BuildingActionError::Cooldown &&
                event.intensity > 0.0) {
                message = "Repair ready in " +
                    std::to_string(std::max(
                        1, static_cast<int>(
                               std::ceil(event.intensity)))) +
                    "s";
            } else {
                message = buildingActionErrorMessage(
                    *event.buildingActionError);
            }
        } else if (event.type == GameEventType::BuildingSold && event.buildingType) {
            message =
                std::string(buildingDisplayName(
                    *event.buildingType)) +
                " sold";
        } else if (event.type == GameEventType::WeaponUpgraded) {
            message = "Rifle upgraded to level " + std::to_string(event.amount);
        } else if (event.type == GameEventType::WeaponUpgradeRejected &&
                   event.weaponUpgradeError) {
            message = weaponUpgradeErrorMessage(*event.weaponUpgradeError);
        } else if (event.type == GameEventType::GateToggleRejected) {
            message = "Gate blocked or not under crosshair";
        } else if (event.type == GameEventType::PlayerDied) {
            message = event.amount > 0
                          ? "You died: " +
                                std::to_string(event.amount) +
                                " resources lost"
                          : "You died";
        } else if (event.type == GameEventType::PlayerRespawned) {
            message = "Respawned at Core";
        } else if (event.type == GameEventType::RopeFallSaved) {
            message = "Safety Rope saved you from a fatal fall";
        } else if (event.type == GameEventType::BattlePotionActivated) {
            message = "BERSERK: attack speed, movement speed and lifesteal";
        } else if (event.type == GameEventType::BloodHarvestTriggered) {
            message = "BLOOD HARVEST: +" +
                std::to_string(event.amount) + " health";
        } else if (event.type == GameEventType::ChestOpened &&
                   event.critical) {
            message = "Chest Key: opened free";
        } else if (event.type == GameEventType::WaveRewardGranted) {
            message =
                "Night cleared: +" +
                std::to_string(event.amount) +
                " Crystals";
        } else if (
            event.type == GameEventType::EarlyWaveBonusGranted) {
            message =
                "Early wave bonus: +" +
                std::to_string(event.amount) +
                " Crystals";
            if (event.coinAmount > 0) {
                message += "  +" +
                    std::to_string(event.coinAmount) +
                    " Coins";
                coinHudBounceRemaining_ = 0.32;
            }
            if (event.insightAmount > 0.0) {
                message += "  +" +
                    std::to_string(static_cast<int>(
                        std::lround(event.insightAmount))) +
                    " Insight";
            }
        } else if (event.type == GameEventType::IntroSkillObjectiveCompleted) {
            message = introGatherRewardMessage(
                simulation_.insightSystem().config()
                    .introGatherObjective);
        } else if (event.type == GameEventType::SkillUnlocked) {
            message = "Skill unlocked";
        } else if (event.type == GameEventType::BuildingFortified) {
            message = "Building fortified for 10 seconds";
        } else if (event.type == GameEventType::ChestOpenRejected) {
            message = "Not enough Coins";
        } else if (event.type == GameEventType::ChestRerolled) {
            message = event.amount == 0
                ? "Chest Key: free reroll used"
                : "Chest reward rerolled: -" +
                      std::to_string(event.amount) + " Coins";
        } else if (
            event.type == GameEventType::ChestRerollAlreadyUsed) {
            message = "Reroll already used";
        } else if (
            event.type == GameEventType::ChestRerollUnavailable) {
            message = "Reroll unavailable";
        } else if (event.type == GameEventType::ChestRevealed) {
            message = "Nearest chest revealed: -" +
                std::to_string(event.amount) + " Coins";
        } else if (event.type == GameEventType::BombPurchased) {
            message = "+" + std::to_string(event.coinAmount) +
                " bombs: -" +
                std::to_string(event.amount) + " Coins";
        } else if (event.type == GameEventType::AllBuildingsRepaired) {
            message = "All structures repaired: -" +
                std::to_string(event.amount) + " Coins";
        } else if (event.type == GameEventType::EconomyPurchaseRejected) {
            message = "Not enough Coins";
        } else if (event.type ==
                   GameEventType::WorldLandmarkActivated) {
            message = event.resourceType == ResourceType::Stone
                ? "Mine activated — producing Stone"
                : "Lumber mill activated — producing Wood";
        } else if (event.type == GameEventType::CrystalStorageFull) {
            message = "Crystal storage full";
        } else if (event.type == GameEventType::ResourceStorageFull &&
                   event.resourceType) {
            message = *event.resourceType == ResourceType::Wood
                ? "Wood storage full"
                : *event.resourceType == ResourceType::Stone
                    ? "Stone storage full"
                    : "Crystal storage full";
        } else if (event.type == GameEventType::LootCollected &&
                   event.lootRarity && event.lootUpgradeEffect) {
            message = std::string(lootRarityName(*event.lootRarity)) +
                " " + lootUpgradeName(*event.lootUpgradeEffect) +
                " — " + lootUpgradeDescription(*event.lootUpgradeEffect);
        }
        if (!message.empty()) {
            const bool lootDescription =
                event.type == GameEventType::LootCollected;
            setStatusMessage(
                std::move(message), lootDescription ? 4.0 : 2.5,
                lootDescription);
        }
    }
}

} // namespace ian
