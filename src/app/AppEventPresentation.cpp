#include "app/App.hpp"
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
            if (building == eventSnapshot.buildings.end()) {
                return;
            }
            constexpr std::size_t MaximumVisuals = 12;
            if (productionVisuals_.size() >= MaximumVisuals) {
                productionVisuals_.erase(
                    productionVisuals_.begin());
            }
            constexpr double Duration = 0.95;
            const Vec3 position =
                buildingProductionVisualWorldAnchor(*building);
            productionVisuals_.push_back({
                .buildingId = buildingId,
                .icon = icon,
                .position = position,
                .amount = amount,
                .remaining = Duration,
                .duration = Duration,
            });
        };
    const auto enemyDamageAnchor =
        [this, &eventSnapshot](EntityId id, Vec3 fallback) {
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
                return fallback;
            }
            Vector3 position = enemyRenderPosition(*enemy);
            position.y += static_cast<float>(simulation_.terrain().getHeight(
                enemy->position.x, enemy->position.z));
            const BoundingBox bounds = renderer_->enemyWorldBounds(
                enemyModelVisual(enemy->type), position,
                static_cast<float>(enemy->yaw),
                enemyVisualScale(enemy->type));
            if (!world_transforms::finite(bounds)) {
                return fallback;
            }
            return Vec3{
                static_cast<double>((bounds.min.x + bounds.max.x) * 0.5F),
                static_cast<double>(bounds.max.y + 0.12F),
                static_cast<double>((bounds.min.z + bounds.max.z) * 0.5F)};
        };
    for (const auto& event : events) {
        if (event.type == GameEventType::RunStarted ||
            event.type == GameEventType::RunRestarted) {
            displayedInsight_ = 0.0;
            insightGainAmount_ = 0.0;
            insightGainRemaining_ = 0.0;
            insightPointSequenceRemaining_ = 0.0;
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
                statusMessage_ = event.treePointsGranted == 1
                    ? "SKILL POINT ACQUIRED — PRESS K"
                    : std::to_string(event.treePointsGranted) +
                          " SKILL POINTS ACQUIRED — PRESS K";
                statusMessageRemaining_ = 3.0;
            } else if (event.insightAmount >= insightConfig.hudLargeRewardThreshold &&
                       event.insightSource &&
                       *event.insightSource != InsightSource::Objective) {
                statusMessage_ = "+" + std::to_string(
                    static_cast<int>(std::lround(event.insightAmount))) +
                    " INSIGHT — " + std::string(insightSourceName(*event.insightSource));
                statusMessageRemaining_ = 1.8;
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
            statusMessage_ = "OBJECTIVE COMPLETE: " + title + "  +" +
                std::to_string(static_cast<int>(std::lround(event.intensity))) +
                " INSIGHT";
            statusMessageRemaining_ = 2.6;
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
            event.sourceId) {
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
            (event.type == GameEventType::ProjectileHit &&
             !event.sourceId);
        if (playerHit) {
            crosshairHitRemaining_ = crosshairHitDuration_;
            crosshairHitCritical_ = event.critical;
        }
        if (event.type == GameEventType::IceWandChargeStarted) {
            // Use the same authored tool-swing timeline as the other
            // first-person tools; the wand's charge glow runs in parallel.
            toolSwingUsesAxe_ = false;
            toolSwingDuration_ = toolTuning_.swingDuration;
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
        } else if (event.type == GameEventType::IceWandFired) {
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
            if (event.amount > 0) {
                hitStopRemaining_ = std::max(hitStopRemaining_, 0.025);
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
        } else if (
            event.type == GameEventType::PickaxeHit ||
            event.type == GameEventType::ResourceHit ||
            event.type ==
                GameEventType::ResourceCollected) {
            weaponRecoilDuration_ = 0.12;
            weaponRecoilRemaining_ = weaponRecoilDuration_;
            weaponRecoilStrength_ =
                event.critical ? 0.035F : 0.024F;
            addCameraImpulse({0.0, -0.006, 0.008});
        }
        const bool resourceImpact =
            event.type == GameEventType::ResourceHit ||
            event.type == GameEventType::ResourceCollected;
        if (event.critical && !resourceImpact) {
            hitStopRemaining_ =
                std::max(hitStopRemaining_, 0.045);
        }
        if (event.type == GameEventType::ResourceHit) {
            toolContactHoldRemaining_ = std::max(
                toolContactHoldRemaining_, 0.025);
        } else if (event.type == GameEventType::ResourceCollected) {
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
                GameEventType::ChestOpenRejected) {
            invalidActionRemaining_ = 0.22;
        }
        if (event.type == GameEventType::ProjectileHit &&
            event.sourceId) {
            const auto source = std::find_if(
                eventSnapshot.buildings.begin(),
                eventSnapshot.buildings.end(),
                [&event](const BuildingInstance& building) {
                    return building.id == *event.sourceId &&
                           building.type == BuildingType::Turret;
                });
            if (source != eventSnapshot.buildings.end()) {
                Vec3 origin = buildingWorldPosition(*source);
                origin.y = 1.4;
                const double deltaX = event.position.x - origin.x;
                const double deltaY = event.position.y - origin.y;
                const double deltaZ = event.position.z - origin.z;
                const double distance = std::sqrt(
                    deltaX * deltaX + deltaY * deltaY +
                    deltaZ * deltaZ);
                const double duration =
                    std::clamp(distance / 18.0, 0.08, 0.35);
                arrowVisuals_.push_back({
                    .origin = origin,
                    .target = event.position,
                    .remaining = duration,
                    .duration = duration,
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
            event.type == GameEventType::IceWandHit) {
            addEffect(PresentationEffectType::Hit,
                      event.position, 0.22, 1.0F,
                      event.entityId);
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
                            ? static_cast<std::size_t>(
                                  node->id.index %
                                  TreeVisualVariantCount)
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
                event.position, 0.72,
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
            } else {
                stoneHudBounceRemaining_ = 0.28;
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
            event.type == GameEventType::GoldProduced &&
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
            message = buildingActionErrorMessage(*event.buildingActionError);
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
        } else if (event.type == GameEventType::WaveRewardGranted) {
            message =
                "Night cleared: +" +
                std::to_string(event.amount) +
                " Crystals";
        } else if (event.type == GameEventType::IntroSkillObjectiveCompleted) {
            message = "Skill point earned - press K to open skill tree";
        } else if (event.type == GameEventType::SkillUnlocked) {
            message = "Skill unlocked";
        } else if (event.type == GameEventType::BuildingFortified) {
            message = "Building fortified for 10 seconds";
        } else if (event.type == GameEventType::ChestOpenRejected) {
            message = "Not enough Gold";
        } else if (event.type == GameEventType::LootCollected &&
                   event.lootRarity && event.lootUpgradeEffect) {
            message = std::string(lootRarityName(*event.lootRarity)) +
                " " + lootUpgradeName(*event.lootUpgradeEffect) +
                " acquired";
        }
        if (!message.empty()) {
            statusMessage_ = std::move(message);
            statusMessageRemaining_ = 2.5;
        }
    }
}

} // namespace ian
