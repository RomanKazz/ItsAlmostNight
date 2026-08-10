#include "ui/TargetHealthBar.hpp"

#include "game/Simulation.hpp"
#include "ui/TargetHealthBarAnchor.hpp"
#include "ui/WorldBillboard.hpp"
#include "world/TerrainHeightfield.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
namespace {

constexpr float FadeInSeconds = 0.035F;
constexpr float FadeOutSeconds = 0.045F;
constexpr double EnemyHealthBarDuration = 2.5;
constexpr double EnemyHealthBarFadeSeconds = 0.3;

std::uint64_t entityKey(EntityId id) {
    return (static_cast<std::uint64_t>(id.generation) << 32U) |
           static_cast<std::uint64_t>(id.index);
}

float approach(float current, float target, float seconds,
               float deltaSeconds) {
    const float blend = 1.0F - std::exp(
        -deltaSeconds / std::max(seconds, 0.001F));
    return current + (target - current) * blend;
}

unsigned char alphaScale(unsigned char alpha, float opacity) {
    return static_cast<unsigned char>(std::clamp(
        std::lround(static_cast<float>(alpha) * opacity),
        0L, 255L));
}

Color withOpacity(Color color, float opacity) {
    color.a = alphaScale(color.a, opacity);
    return color;
}

} // namespace

void TargetHealthBar::draw(const SimulationSnapshot& snapshot,
                           const Camera3D& camera,
                           const TerrainHeightfield& terrain,
                           EnemyBoundsProvider enemyBoundsProvider) {
    const double frameSeconds = static_cast<double>(std::clamp(
        GetFrameTime(), 0.0F, 0.10F));
    repairPulseRemaining_ = std::max(
        0.0,
        repairPulseRemaining_ - frameSeconds);
    if (repairPulseRemaining_ <= 0.0) {
        repairTarget_.reset();
    }

    const auto enemyAnchor = [&](const EnemyInstance& enemy) {
        if (enemyBoundsProvider) {
            const std::optional<BoundingBox> bounds =
                enemyBoundsProvider(enemy);
            if (bounds && std::isfinite(bounds->min.x) &&
                std::isfinite(bounds->min.y) &&
                std::isfinite(bounds->min.z) &&
                std::isfinite(bounds->max.x) &&
                std::isfinite(bounds->max.y) &&
                std::isfinite(bounds->max.z) &&
                bounds->min.x <= bounds->max.x &&
                bounds->min.y <= bounds->max.y &&
                bounds->min.z <= bounds->max.z) {
                return Vector3{
                    (bounds->min.x + bounds->max.x) * 0.5F,
                    bounds->max.y + 0.12F,
                    (bounds->min.z + bounds->max.z) * 0.5F};
            }
        }
        // Kept only for callers that do not have a renderer provider yet;
        // App always supplies the geometry-derived path above.
        return Vector3{
            static_cast<float>(enemy.position.x),
            static_cast<float>(terrain.getHeight(
                enemy.position.x, enemy.position.z) +
                enemy.position.y + enemy.surfaceHeightOffset + 1.0),
            static_cast<float>(enemy.position.z)};
    };

    const auto drawEnemyBar = [&](const EnemyInstance& enemy,
                                  float opacity = 1.0F) {
        drawBillboard(
            {TargetKind::Enemy, enemy.id},
            enemyAnchor(enemy),
            enemy.health, enemy.maxHealth,
            {224, 66, 58, 255}, camera, 0, opacity);
    };

    const auto resourceAnchor = [&](const ResourceNode& resource) {
        const float AnchorHeight = isDestructibleProp(resource.type)
            ? 0.82F * static_cast<float>(resource.visualScale)
            : 1.12F;
        // Resource meshes are placed on the current terrain surface at
        // render time; simulation Y can contain a stale authored offset.
        const float ground = static_cast<float>(terrain.getHeight(
            resource.position.x, resource.position.z));
        return Vector3{
            static_cast<float>(resource.position.x),
            ground + AnchorHeight,
            static_cast<float>(resource.position.z)};
    };

    const auto drawResourceBar = [&](const ResourceNode& resource) {
        drawBillboard(
            {TargetKind::Resource, resource.id},
            resourceAnchor(resource),
            resource.health, resource.maxHealth,
            {235, 186, 55, 255}, camera);
    };

    // Power Swing can damage several resources while only one remains under
    // the crosshair. Keep bars visible for every nearby wounded resource so
    // secondary hits receive the same health feedback as the primary hit.
    constexpr double ResourceHealthBarRange = 18.0;
    constexpr std::size_t MaximumWoundedResourceBars = 32;
    std::size_t woundedResourceBars = 0;
    for (const ResourceNode& resource : snapshot.resourceNodes) {
        if (!resource.active || resource.health >= resource.maxHealth ||
            (snapshot.aimedResource &&
             resource.id == *snapshot.aimedResource)) {
            continue;
        }
        const double deltaX =
            resource.position.x - snapshot.playerPosition.x;
        const double deltaZ =
            resource.position.z - snapshot.playerPosition.z;
        if (deltaX * deltaX + deltaZ * deltaZ >
            ResourceHealthBarRange * ResourceHealthBarRange) {
            continue;
        }
        drawResourceBar(resource);
        if (++woundedResourceBars >= MaximumWoundedResourceBars) {
            break;
        }
    }

    // Remember actual health decreases instead of treating every wounded
    // enemy as permanently recent. A new hit refreshes the visibility timer.
    ++enemyHealthFrame_;
    for (const EnemyInstance& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        const std::uint64_t key = entityKey(enemy.id);
        auto [entry, inserted] = enemyHealthVisibility_.try_emplace(
            key,
            EnemyHealthVisibility{
                .previousHealth = enemy.maxHealth,
                .remaining = 0.0,
                .lastSeenFrame = enemyHealthFrame_,
            });
        EnemyHealthVisibility& visibility = entry->second;
        visibility.lastSeenFrame = enemyHealthFrame_;
        visibility.remaining = std::max(
            0.0, visibility.remaining - frameSeconds);
        if (enemy.health + 0.001 < visibility.previousHealth) {
            visibility.remaining = EnemyHealthBarDuration;
        }
        visibility.previousHealth = enemy.health;
        (void)inserted;
    }
    std::erase_if(
        enemyHealthVisibility_,
        [this](const auto& entry) {
            return entry.second.lastSeenFrame != enemyHealthFrame_;
        });

    // Recently hit enemies keep their own bars, even when the crosshair
    // targets a different enemy, resource, or building.
    constexpr double EnemyHealthBarRange = 36.0;
    constexpr std::size_t MaximumWoundedEnemyBars = 64;
    const bool aimedEnemyIsPrimaryTarget =
        snapshot.aimedEnemy &&
        !snapshot.aimedResource &&
        !snapshot.aimedBuilding &&
        !snapshot.aimedModularBuilding;
    std::size_t woundedEnemyBars = 0;
    for (const EnemyInstance& enemy : snapshot.enemies) {
        const auto visibility = enemyHealthVisibility_.find(
            entityKey(enemy.id));
        if (!enemy.active ||
            visibility == enemyHealthVisibility_.end() ||
            visibility->second.remaining <= 0.0 ||
            (aimedEnemyIsPrimaryTarget &&
             enemy.id == *snapshot.aimedEnemy)) {
            continue;
        }
        const double deltaX = enemy.position.x - snapshot.playerPosition.x;
        const double deltaZ = enemy.position.z - snapshot.playerPosition.z;
        if (deltaX * deltaX + deltaZ * deltaZ >
            EnemyHealthBarRange * EnemyHealthBarRange) {
            continue;
        }
        const float opacity = static_cast<float>(std::clamp(
            visibility->second.remaining / EnemyHealthBarFadeSeconds,
            0.0, 1.0));
        drawEnemyBar(enemy, opacity);
        if (++woundedEnemyBars >= MaximumWoundedEnemyBars) {
            break;
        }
    }

    std::optional<Visual> candidate;
    if (snapshot.aimedResource) {
        const auto resource = std::find_if(
            snapshot.resourceNodes.begin(), snapshot.resourceNodes.end(),
            [&snapshot](const ResourceNode& candidate) {
                return candidate.active &&
                       candidate.id == *snapshot.aimedResource;
            });
        if (resource != snapshot.resourceNodes.end()) {
            candidate = Visual{
                .target = {TargetKind::Resource, resource->id},
                .anchorPosition = resourceAnchor(*resource),
                .health = resource->health,
                .maxHealth = resource->maxHealth,
                .fillColor = {235, 186, 55, 255},
            };
        }
    } else if (snapshot.aimedBuilding) {
        const auto building = std::find_if(
            snapshot.buildings.begin(), snapshot.buildings.end(),
            [&snapshot](const BuildingInstance& candidate) {
                return candidate.id == *snapshot.aimedBuilding;
            });
        if (building != snapshot.buildings.end()) {
            const Vec3 anchor =
                buildingHealthBarWorldAnchor(*building);
            candidate = Visual{
                .target = {TargetKind::Building, building->id},
                .anchorPosition = {
                    static_cast<float>(anchor.x),
                    static_cast<float>(anchor.y),
                    static_cast<float>(anchor.z)},
                .health = building->health,
                .maxHealth = building->maxHealth,
                .fillColor = {82, 210, 103, 255},
                .buildingLevel = static_cast<int>(building->level),
            };
        }
    } else if (snapshot.aimedModularBuilding) {
        const auto frame = std::find_if(
            snapshot.platformFrames.begin(),
            snapshot.platformFrames.end(),
            [&snapshot](const PlatformFrameInstance& candidate) {
                return candidate.id ==
                       *snapshot.aimedModularBuilding;
            });
        if (frame != snapshot.platformFrames.end()) {
            const double cellSize = snapshot.worldCellSize;
            candidate = Visual{
                .target = {TargetKind::Foundation, frame->id},
                .anchorPosition = {
                    static_cast<float>(
                        (frame->anchor.x +
                         PlatformFrameWidthCells * 0.5) *
                        cellSize),
                    static_cast<float>(frame->floorHeight + 0.38),
                    static_cast<float>(
                        (frame->anchor.z +
                         PlatformFrameWidthCells * 0.5) *
                        cellSize),
                },
                .health = frame->health,
                .maxHealth = frame->maxHealth,
                .fillColor = {82, 210, 103, 255},
            };
        }
        if (!candidate) {
            const auto wall = std::find_if(
                snapshot.modularWalls.begin(),
                snapshot.modularWalls.end(),
                [&snapshot](const WallInstance& candidate) {
                    return candidate.id ==
                           *snapshot.aimedModularBuilding;
                });
            if (wall != snapshot.modularWalls.end()) {
                candidate = Visual{
                    .target = {TargetKind::Foundation, wall->id},
                    .anchorPosition = {
                    static_cast<float>(
                        (wall->anchor.x + 0.5) *
                        snapshot.worldCellSize),
                    static_cast<float>(wall->topHeight + 0.34),
                    static_cast<float>(
                        (wall->anchor.z + 0.5) *
                        snapshot.worldCellSize),
                    },
                    .health = wall->health,
                    .maxHealth = wall->maxHealth,
                    .fillColor = {82, 210, 103, 255},
                };
            }
        }
        if (!candidate) {
            const auto ramp = std::find_if(
                snapshot.ramps.begin(), snapshot.ramps.end(),
                [&snapshot](const RampInstance& candidate) {
                    return candidate.id ==
                           *snapshot.aimedModularBuilding;
                });
            if (ramp != snapshot.ramps.end()) {
                const bool alongZ =
                    ramp->rotation == Rotation::Deg0 ||
                    ramp->rotation == Rotation::Deg180;
                const int widthCells = alongZ
                    ? ModularRampWidthCells : ModularRampRunCells;
                const int depthCells = alongZ
                    ? ModularRampRunCells : ModularRampWidthCells;
                candidate = Visual{
                    .target = {TargetKind::Foundation, ramp->id},
                    .anchorPosition = {
                    static_cast<float>(
                        (ramp->anchor.x + widthCells * 0.5) *
                        snapshot.worldCellSize),
                    static_cast<float>(ramp->topHeight + 0.34),
                    static_cast<float>(
                        (ramp->anchor.z + depthCells * 0.5) *
                        snapshot.worldCellSize),
                    },
                    .health = ramp->health,
                    .maxHealth = ramp->maxHealth,
                    .fillColor = {82, 210, 103, 255},
                };
            }
        }
    } else if (snapshot.aimedEnemy) {
        const auto enemy = std::find_if(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [&snapshot](const EnemyInstance& candidate) {
                return candidate.active &&
                       candidate.id == *snapshot.aimedEnemy;
        });
        if (enemy != snapshot.enemies.end()) {
            candidate = Visual{
                .target = {TargetKind::Enemy, enemy->id},
                .anchorPosition = enemyAnchor(*enemy),
                .health = enemy->health,
                .maxHealth = enemy->maxHealth,
                .fillColor = {224, 66, 58, 255},
            };
        }
    }

    const float deltaSeconds = std::clamp(
        GetFrameTime(), 0.0F, 0.10F);
    if (candidate) {
        const bool targetChanged =
            !activeVisual_ || activeVisual_->target != candidate->target;
        if (targetChanged) {
            activeVisual_ = candidate;
            target_.reset();
            opacity_ = 0.0F;
        } else {
            *activeVisual_ = *candidate;
        }
        opacity_ = approach(
            opacity_, 1.0F, FadeInSeconds, deltaSeconds);
    } else {
        opacity_ = approach(
            opacity_, 0.0F, FadeOutSeconds, deltaSeconds);
    }

    if (activeVisual_ && opacity_ > 0.005F) {
        const Visual& visual = *activeVisual_;
        drawBillboard(
            visual.target, visual.anchorPosition,
            visual.health, visual.maxHealth,
            visual.fillColor, camera, visual.buildingLevel,
            opacity_);
    }
    if (!candidate && opacity_ <= 0.005F) {
        activeVisual_.reset();
        target_.reset();
    }
}

void TargetHealthBar::reset() {
    target_.reset();
    activeVisual_.reset();
    opacity_ = 0.0F;
    enemyHealthVisibility_.clear();
    enemyHealthFrame_ = 0;
}

void TargetHealthBar::notifyRepair(EntityId id) {
    repairTarget_ = id;
    repairPulseRemaining_ = repairPulseDuration_;
}

void TargetHealthBar::notifyEnemyHit(EntityId id) {
    auto [entry, inserted] = enemyHealthVisibility_.try_emplace(
        entityKey(id),
        EnemyHealthVisibility{
            .previousHealth = 0.0,
            .remaining = EnemyHealthBarDuration,
            .lastSeenFrame = enemyHealthFrame_,
        });
    entry->second.remaining = EnemyHealthBarDuration;
    (void)inserted;
}

void TargetHealthBar::drawBillboard(
    Target target, Vector3 anchorPosition, double health,
    double maxHealth, Color fillColor, const Camera3D& camera,
    int buildingLevel, float opacity) {
    if (maxHealth <= 0.0) {
        return;
    }

    if (!target_ || *target_ != target) {
        target_ = target;
        displayedHealth_ = health;
    } else {
        const bool repairing =
            repairTarget_ && *repairTarget_ == target.id &&
            repairPulseRemaining_ > 0.0;
        const double blend =
            1.0 - std::exp(
                -(repairing ? 28.0 : 12.0) *
                static_cast<double>(GetFrameTime()));
        displayedHealth_ += (health - displayedHealth_) * blend;
        if (std::abs(displayedHealth_ - health) < 0.01) {
            displayedHealth_ = health;
        }
    }

    const Vector3 viewForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(viewForward, camera.up));
    const Vector3 cameraUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, viewForward));
    const Vector3 towardCamera = Vector3Negate(viewForward);

    float pulseScale = 1.0F;
    if (repairTarget_ && *repairTarget_ == target.id &&
        repairPulseRemaining_ > 0.0) {
        const float progress = static_cast<float>(
            1.0 - repairPulseRemaining_ / repairPulseDuration_);
        pulseScale += std::sin(progress * PI) *
                      (1.0F - progress) * 0.1F;
    }
    const float outerWidth = 1.26F * pulseScale;
    const float outerHeight = 0.252F * pulseScale;
    constexpr float Border = 0.035F;
    constexpr float LayerOffset = 0.004F;
    const float innerWidth = outerWidth - Border * 2.0F;
    const float innerHeight = outerHeight - Border * 2.0F;
    const float displayedFraction = static_cast<float>(
        std::clamp(displayedHealth_ / maxHealth, 0.0, 1.0));
    const float actualFraction = static_cast<float>(
        std::clamp(health / maxHealth, 0.0, 1.0));

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    drawWorldBillboardQuad(
        anchorPosition, outerWidth, outerHeight,
        withOpacity(WHITE, opacity),
        cameraRight, cameraUp);
    const Vector3 innerCenter = Vector3Add(
        anchorPosition, Vector3Scale(towardCamera, LayerOffset));
    drawWorldBillboardQuad(
        innerCenter, innerWidth, innerHeight,
        withOpacity({25, 22, 20, 255}, opacity),
        cameraRight, cameraUp);

    const auto drawSegment =
        [&](float leftFraction, float rightFraction,
            Color color, float depthLayers) {
            if (rightFraction <= leftFraction) {
                return;
            }
            const float width =
                innerWidth * (rightFraction - leftFraction);
            const float centerOffset =
                -innerWidth * 0.5F + innerWidth * leftFraction +
                width * 0.5F;
            Vector3 center = Vector3Add(
                innerCenter,
                Vector3Scale(cameraRight, centerOffset));
            center = Vector3Add(
                center,
                Vector3Scale(
                    towardCamera, LayerOffset * depthLayers));
            drawWorldBillboardQuad(
                center, width, innerHeight,
                withOpacity(color, opacity),
                cameraRight, cameraUp);
        };
    drawSegment(actualFraction, displayedFraction,
                {255, 137, 62, 150}, 1.0F);
    drawSegment(0.0F, actualFraction, fillColor, 2.0F);

    const Vector3 borderCenter = Vector3Add(
        innerCenter, Vector3Scale(towardCamera, LayerOffset));
    drawWorldBillboardQuad(
        Vector3Add(
            borderCenter,
            Vector3Scale(cameraUp,
                         (outerHeight - Border) * 0.5F)),
        outerWidth, Border, withOpacity(WHITE, opacity),
        cameraRight, cameraUp);
    drawWorldBillboardQuad(
        Vector3Subtract(
            borderCenter,
            Vector3Scale(cameraUp,
                         (outerHeight - Border) * 0.5F)),
        outerWidth, Border, withOpacity(WHITE, opacity),
        cameraRight, cameraUp);
    drawWorldBillboardQuad(
        Vector3Subtract(
            borderCenter,
            Vector3Scale(cameraRight,
                         (outerWidth - Border) * 0.5F)),
        Border, innerHeight, withOpacity(WHITE, opacity),
        cameraRight, cameraUp);
    drawWorldBillboardQuad(
        Vector3Add(
            borderCenter,
            Vector3Scale(cameraRight,
                         (outerWidth - Border) * 0.5F)),
        Border, innerHeight, withOpacity(WHITE, opacity),
        cameraRight, cameraUp);
    if (buildingLevel > 0) {
        const std::string levelText =
            "LEVEL " + std::to_string(buildingLevel);
        const Vector3 labelCenter = Vector3Add(
            Vector3Add(
                anchorPosition,
                Vector3Scale(cameraUp,
                             outerHeight * 0.5F + 0.22F)),
            Vector3Scale(towardCamera, LayerOffset * 4.0F));
        rlDrawRenderBatchActive();
        drawWorldBillboardText(
            levelText, labelCenter, 0.28F, camera,
            cameraRight, cameraUp,
            withOpacity({255, 235, 174, 255}, opacity));
    }
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

} // namespace ian
