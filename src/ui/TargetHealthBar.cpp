#include "ui/TargetHealthBar.hpp"

#include "combat/CombatDisplayUnits.hpp"
#include "presentation/PortalArrivalVisibility.hpp"

#include "game/Simulation.hpp"
#include "ui/TargetHealthBarAnchor.hpp"
#include "ui/UiText.hpp"
#include "ui/WorldBillboard.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/VisualBiomes.hpp"

#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
namespace {

constexpr float FadeInSeconds = 0.035F;
constexpr float FadeOutSeconds = 0.045F;
constexpr double TargetHealthBarDuration = 1.8;
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

Color scaledColor(Color color, float scale) {
    return {
        static_cast<unsigned char>(std::clamp(
            std::lround(static_cast<float>(color.r) * scale),
            0L, 255L)),
        static_cast<unsigned char>(std::clamp(
            std::lround(static_cast<float>(color.g) * scale),
            0L, 255L)),
        static_cast<unsigned char>(std::clamp(
            std::lround(static_cast<float>(color.b) * scale),
            0L, 255L)),
        color.a,
    };
}

bool visibleScreenPoint(Vector2 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           point.x >= 0.0F && point.y >= 0.0F &&
           point.x <= static_cast<float>(GetScreenWidth()) &&
           point.y <= static_cast<float>(GetScreenHeight());
}

Color resourceHealthColor(const ResourceNode& resource, const visual_biomes::Layout& biomes) {
    switch (resource.type) {
    case ResourceType::Wood: {
        const auto weights = visual_biomes::weights(biomes,
            static_cast<float>(resource.position.x), static_cast<float>(resource.position.z));
        const auto biome = static_cast<std::size_t>(
            std::max_element(weights.begin(), weights.end()) - weights.begin());
        // Use the same discrete region as foliage, with extra brightness for UI.
        const auto color = visual_biomes::shiftPalette(biomes.leaves[biome], 0, 0.12F, 0.15F);
        return {static_cast<unsigned char>(std::lround(color[0] * 255)),
                static_cast<unsigned char>(std::lround(color[1] * 255)),
                static_cast<unsigned char>(std::lround(color[2] * 255)), 255};
    }
    case ResourceType::Stone:
        return {120, 161, 190, 255};
    case ResourceType::Crystal:
        return {218, 65, 225, 255};
    case ResourceType::Barrel:
    case ResourceType::Crate:
    case ResourceType::ItemCrate:
        return {232, 172, 62, 255};
    }
    return {232, 172, 62, 255};
}

Color enemyHealthColor(const EnemyInstance& enemy) {
    if (hasEliteAffix(
            enemy.eliteAffixes, EliteAffix::Warden)) {
        return {72, 169, 244, 255};
    }
    if (hasEliteAffix(
            enemy.eliteAffixes, EliteAffix::Volatile)) {
        return {244, 151, 46, 255};
    }
    if (hasEliteAffix(
            enemy.eliteAffixes, EliteAffix::Berserker)) {
        return {239, 70, 61, 255};
    }
    return {224, 66, 58, 255};
}

} // namespace

void TargetHealthBar::draw(const SimulationSnapshot& snapshot,
                           const Camera3D& camera,
                           const TerrainHeightfield& terrain,
                           EnemyBoundsProvider enemyBoundsProvider,
                           std::optional<EntityId> upgradePromptTarget,
                           BuildingHeightProvider buildingHeightProvider) {
    const auto biomes = visual_biomes::generate(terrain.seed(), terrain.config().terrainWorldSize);
    screenHealthText_.reset();
    screenLevelText_.reset();
    ++healthAnimationFrame_;
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
                return enemyHealthBarWorldAnchor(*bounds,enemy.eliteAffixes!=0U);
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
        if(!presentation::portalArrivalVisible(enemy,{camera.position.x,camera.position.y,camera.position.z})) return;
        const Vector3 anchor = enemyAnchor(enemy);
        drawBillboard(
            {TargetKind::Enemy, enemy.id},
            anchor,
            enemy.health, enemy.maxHealth,
            enemyHealthColor(enemy), camera, 0, opacity);
    };

    const auto resourceAnchor = [&](const ResourceNode& resource) {
        const float visualScale =
            static_cast<float>(resource.visualScale);
        const float AnchorHeight = resource.type == ResourceType::Wood
            ? 1.95F * visualScale * TreeVisualHeightScale
            : resource.type == ResourceType::Barrel
                ? 1.04F * visualScale
                : resource.type == ResourceType::Crate ||
                      resource.type == ResourceType::ItemCrate
                    ? 0.94F * visualScale
                    : 1.45F * visualScale;
        // Resource meshes are placed on the current terrain surface at
        // render time; simulation Y can contain a stale authored offset.
        const float ground = static_cast<float>(terrain.getHeight(
            resource.position.x, resource.position.z));
        return Vector3{
            static_cast<float>(resource.position.x),
            ground + AnchorHeight,
            static_cast<float>(resource.position.z)};
    };

    ++resourceHealthFrame_;
    for (const ResourceNode& resource : snapshot.resourceNodes) {
        if (!resource.active) {
            continue;
        }
        auto [entry, inserted] = resourceHealthVisibility_.try_emplace(
            entityKey(resource.id),
            HealthVisibility{
                .remaining = 0.0,
                .lastSeenFrame = resourceHealthFrame_,
            });
        entry->second.lastSeenFrame = resourceHealthFrame_;
        entry->second.remaining = std::max(
            0.0, entry->second.remaining - frameSeconds);
        (void)inserted;
    }
    std::erase_if(
        resourceHealthVisibility_,
        [this](const auto& entry) {
            return entry.second.lastSeenFrame != resourceHealthFrame_;
        });

    // Power Swing can damage several resources while only one remains under
    // the crosshair. Each hit gets the same finite visibility timer.
    constexpr double ResourceHealthBarRange = 18.0;
    constexpr std::size_t MaximumWoundedResourceBars = 32;
    std::size_t woundedResourceBars = 0;
    for (const ResourceNode& resource : snapshot.resourceNodes) {
        const auto visibility = resourceHealthVisibility_.find(
            entityKey(resource.id));
        if (!resource.active || isDestructibleProp(resource.type) ||
            visibility == resourceHealthVisibility_.end() ||
            visibility->second.remaining <= 0.0 ||
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
        const float opacity = static_cast<float>(std::clamp(
            visibility->second.remaining / EnemyHealthBarFadeSeconds,
            0.0, 1.0));
        drawBillboard(
            {TargetKind::Resource, resource.id},
            resourceAnchor(resource),
            resource.health, resource.maxHealth,
            resourceHealthColor(resource, biomes),
            camera, 0, opacity);
        if (++woundedResourceBars >= MaximumWoundedResourceBars) {
            break;
        }
    }

    // Player hit events drive visibility. Automated defenses do not leave
    // bars covering every wounded enemy on the battlefield.
    ++enemyHealthFrame_;
    for (const EnemyInstance& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        const std::uint64_t key = entityKey(enemy.id);
        auto [entry, inserted] = enemyHealthVisibility_.try_emplace(
            key,
            HealthVisibility{
                .remaining = 0.0,
                .lastSeenFrame = enemyHealthFrame_,
            });
        HealthVisibility& visibility = entry->second;
        visibility.lastSeenFrame = enemyHealthFrame_;
        visibility.remaining = std::max(
            0.0, visibility.remaining - frameSeconds);
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
        const bool alwaysVisibleBoss =
            enemy.type == EnemyType::Boss;
        if (!enemy.active ||
            (!alwaysVisibleBoss &&
             (visibility == enemyHealthVisibility_.end() ||
              visibility->second.remaining <= 0.0)) ||
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
        const float opacity = alwaysVisibleBoss
            ? 1.0F
            : static_cast<float>(std::clamp(
                  visibility->second.remaining /
                      EnemyHealthBarFadeSeconds,
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
        if (resource != snapshot.resourceNodes.end() && !isDestructibleProp(resource->type)) {
            candidate = Visual{
                .target = {TargetKind::Resource, resource->id},
                .anchorPosition = resourceAnchor(*resource),
                .health = resource->health,
                .maxHealth = resource->maxHealth,
                .fillColor = resourceHealthColor(*resource, biomes),
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
                buildingHealthBarWorldAnchor(*building, buildingHeightProvider
                    ? std::optional<double>{buildingHeightProvider(*building)} : std::nullopt);
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
            if (upgradePromptTarget == building->id) {
                const Vector3 forward = Vector3Normalize(
                    Vector3Subtract(camera.target, camera.position));
                const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
                const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
                const bool producer =
                    building->type == BuildingType::CrystalMine ||
                    building->type == BuildingType::LumberMill ||
                    building->type == BuildingType::Quarry;
                candidate->anchorPosition = Vector3Add(candidate->anchorPosition,
                    Vector3Scale(up, BuildingHealthBarUpgradeLift +
                        (producer ? 0.20F : 0.0F)));
            }
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
        if (enemy != snapshot.enemies.end() && presentation::portalArrivalVisible(*enemy,
            {camera.position.x,camera.position.y,camera.position.z})) {
            candidate = Visual{
                .target = {TargetKind::Enemy, enemy->id},
                .anchorPosition = enemyAnchor(*enemy),
                .health = enemy->health,
                .maxHealth = enemy->maxHealth,
                .fillColor = enemyHealthColor(*enemy),
                .showHealthText = false,
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
            visual.fillColor, camera,
            candidate ? visual.buildingLevel : 0,
            opacity_, visual.showHealthText);
    }
    if (!candidate && opacity_ <= 0.005F) {
        activeVisual_.reset();
    }
    std::erase_if(
        healthAnimations_, [this](const auto& entry) {
            return entry.second.lastSeenFrame != healthAnimationFrame_;
        });
}

void TargetHealthBar::reset() {
    activeVisual_.reset();
    opacity_ = 0.0F;
    enemyHealthVisibility_.clear();
    enemyHealthFrame_ = 0;
    resourceHealthVisibility_.clear();
    resourceHealthFrame_ = 0;
    healthAnimations_.clear();
    healthAnimationFrame_ = 0;
}

void TargetHealthBar::notifyRepair(EntityId id) {
    repairTarget_ = id;
    repairPulseRemaining_ = repairPulseDuration_;
}

void TargetHealthBar::notifyEnemyHit(EntityId id, double damage) {
    auto [entry, inserted] = enemyHealthVisibility_.try_emplace(
        entityKey(id),
        HealthVisibility{
            .remaining = TargetHealthBarDuration,
            .lastSeenFrame = enemyHealthFrame_,
    });
    entry->second.remaining = TargetHealthBarDuration;
    entry->second.pendingDamage += std::max(0.0, damage);
    (void)inserted;
}

void TargetHealthBar::notifyResourceHit(EntityId id) {
    auto [entry, inserted] = resourceHealthVisibility_.try_emplace(
        entityKey(id),
        HealthVisibility{
            .remaining = TargetHealthBarDuration,
            .lastSeenFrame = resourceHealthFrame_,
        });
    entry->second.remaining = TargetHealthBarDuration;
    (void)inserted;
}

void TargetHealthBar::drawBillboard(
    Target target, Vector3 anchorPosition, double health,
    double maxHealth, Color fillColor, const Camera3D& camera,
    int buildingLevel, float opacity,
    bool showHealthText) {
    if (maxHealth <= 0.0) {
        return;
    }
    const bool primaryEnemy =
        target.kind == TargetKind::Enemy && showHealthText;

    double initialDisplayedHealth = health;
    if (target.kind == TargetKind::Enemy) {
        const auto visibility = enemyHealthVisibility_.find(
            entityKey(target.id));
        if (visibility != enemyHealthVisibility_.end()) {
            initialDisplayedHealth = std::min(
                maxHealth,
                health + visibility->second.pendingDamage);
            visibility->second.pendingDamage = 0.0;
        }
    }
    auto [animation, inserted] = healthAnimations_.try_emplace(
        target,
        HealthAnimation{
            .displayedHealth = initialDisplayedHealth,
            .lastSeenFrame = healthAnimationFrame_,
        });
    HealthAnimation& healthAnimation = animation->second;
    healthAnimation.lastSeenFrame = healthAnimationFrame_;
    if (!inserted) {
        const bool repairing =
            repairTarget_ && *repairTarget_ == target.id &&
            repairPulseRemaining_ > 0.0;
        const double blend =
            1.0 - std::exp(
                -(repairing ? 28.0 : 3.2) *
                static_cast<double>(GetFrameTime()));
        healthAnimation.displayedHealth +=
            (health - healthAnimation.displayedHealth) * blend;
        if (std::abs(healthAnimation.displayedHealth - health) < 0.01) {
            healthAnimation.displayedHealth = health;
        }
    }

    const Vector3 viewForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraToAnchor = Vector3Subtract(
        anchorPosition, camera.position);
    if (Vector3DotProduct(cameraToAnchor, viewForward) <= 0.01F) {
        return;
    }
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(viewForward, camera.up));
    const Vector3 cameraUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, viewForward));
    const Vector3 towardCamera = Vector3Negate(viewForward);

    if (target.kind == TargetKind::Resource) {
        // With routine mining prompts hidden, keep the bar just above the
        // resource instead of leaving it floating in the old prompt stack.
        anchorPosition = Vector3Add(
            anchorPosition,
            Vector3Scale(cameraUp, 0.18F));
    }

    float pulseScale = 1.0F;
    if (repairTarget_ && *repairTarget_ == target.id &&
        repairPulseRemaining_ > 0.0) {
        const float progress = static_cast<float>(
            1.0 - repairPulseRemaining_ / repairPulseDuration_);
        pulseScale += std::sin(progress * PI) *
                      (1.0F - progress) * 0.05F;
    }
    // Use one established world-space size for every target health bar.
    constexpr float WorldBarScale = 0.67F;
    pulseScale *= WorldBarScale;
    constexpr float WorldWidth = 1.62F;
    constexpr float WorldHeight = 0.23F;
    constexpr float InnerInset = 0.035F;
    constexpr float InnerHeight = 0.175F;
    constexpr float FillHeight = 0.13F;
    constexpr float OuterRadius = 0.06F;
    constexpr float InnerRadius = 0.04F;
    constexpr float FillRadius = 0.04F;
    constexpr float LayerOffset = 0.004F;
    const float outerWidth = WorldWidth * pulseScale;
    const float outerHeight = WorldHeight * pulseScale;
    const float innerWidth =
        (WorldWidth - InnerInset * 2.0F) * pulseScale;
    const float innerHeight = InnerHeight * pulseScale;
    const float fillHeight = FillHeight * pulseScale;
    const float outerRadius = OuterRadius * pulseScale;
    const float innerRadius = InnerRadius * pulseScale;
    const float fillRadius = FillRadius * pulseScale;
    const float layerOffset = LayerOffset;
    const float displayedFraction = static_cast<float>(
        std::clamp(
            healthAnimation.displayedHealth / maxHealth, 0.0, 1.0));
    const float actualFraction = static_cast<float>(
        std::clamp(health / maxHealth, 0.0, 1.0));
    if (target.kind == TargetKind::Building ||
        target.kind == TargetKind::Foundation) {
        const float safeHealth = std::clamp(
            (actualFraction - 0.18F) / 0.52F, 0.0F, 1.0F);
        const float smoothHealth = safeHealth * safeHealth *
            (3.0F - 2.0F * safeHealth);
        fillColor = ColorLerp(
            {235, 75, 57, 255},
            {82, 210, 103, 255},
            smoothHealth);
    }
    const Color outerColor = primaryEnemy
        ? Color{57, 36, 25, 255}
        : Color{49, 49, 49, 255};
    const Color innerColor = primaryEnemy
        ? scaledColor(fillColor, 0.62F)
        : scaledColor(fillColor, 0.78F);
    const Color activeColor = primaryEnemy
        ? ColorLerp(
              fillColor, Color{255, 188, 155, 255}, 0.38F)
        : ColorLerp(fillColor, WHITE, 0.32F);
    const Color trailColor = ColorLerp(fillColor, WHITE, 0.68F);

    const Vector3 barAnchor = anchorPosition;
    const Vector3 outerCenter = Vector3Add(
        barAnchor,
        Vector3Scale(towardCamera, layerOffset));
    drawWorldBillboardRoundedRectangle(
        outerCenter, outerWidth, outerHeight, outerRadius,
        withOpacity(outerColor, opacity),
        cameraRight, cameraUp);
    const Vector3 innerCenter = Vector3Add(
        outerCenter, Vector3Scale(towardCamera, layerOffset));
    drawWorldBillboardRoundedRectangle(
        innerCenter, innerWidth, innerHeight, innerRadius,
        withOpacity(innerColor, opacity),
        cameraRight, cameraUp);

    const auto drawFill =
        [&](float fraction, Color color, float depthLayers) {
            if (fraction <= 0.0F) {
                return;
            }
            const float width = innerWidth * fraction;
            const float centerOffset =
                -innerWidth * 0.5F + width * 0.5F;
            Vector3 center = Vector3Add(
                innerCenter,
                Vector3Scale(cameraRight, centerOffset));
            center = Vector3Add(
                center,
                Vector3Scale(cameraUp, 0.018F * pulseScale));
            center = Vector3Add(
                center, Vector3Scale(
                    towardCamera, layerOffset * depthLayers));
            drawWorldBillboardRoundedRectangle(
                center, width, fillHeight,
                std::min(fillRadius, width * 0.5F),
                withOpacity(color, opacity),
                cameraRight, cameraUp);
        };
    if (displayedFraction > actualFraction + 0.001F) {
        drawFill(displayedFraction, trailColor, 1.0F);
    }
    drawFill(actualFraction, activeColor, 2.0F);

    if (primaryEnemy) {
        const int roundedHealth = health > 0.0
            ? std::max(1, combatDisplayValue(health))
            : 0;
        const int roundedMaxHealth = std::max(
            1, combatDisplayValue(maxHealth));
        const std::string healthText =
            std::to_string(roundedHealth) + "/" +
            std::to_string(roundedMaxHealth);
        const Vector2 screenCenter = GetWorldToScreen(
            barAnchor, camera);
        const Vector2 screenTop = GetWorldToScreen(
            Vector3Add(
                barAnchor,
                Vector3Scale(cameraUp, outerHeight * 0.5F)),
            camera);
        const Vector2 screenBottom = GetWorldToScreen(
            Vector3Subtract(
                barAnchor,
                Vector3Scale(cameraUp, outerHeight * 0.5F)),
            camera);
        const float pixelHeight = Vector2Distance(
            screenTop, screenBottom);
        // drawStyledSdfUiText multiplies the requested size by 1.68. This
        // ratio exactly matches the 24 px text inside the player's 36 px bar.
        const float fontSize = std::clamp(
            pixelHeight * (24.0F / 36.0F) / 1.68F,
            7.0F, 22.0F);
        if (visibleScreenPoint(screenCenter)) {
            screenHealthText_ = ScreenHealthText{
                .text = healthText,
                .center = screenCenter,
                .fontSize = fontSize,
                .opacity = opacity,
            };
        }
    }

    if (buildingLevel > 0) {
        const std::string levelText =
            "LEVEL " + std::to_string(buildingLevel);
        const Vector3 labelCenter = Vector3Add(
            Vector3Add(
                anchorPosition,
                Vector3Scale(cameraUp,
                             outerHeight * 0.5F +
                                 0.16F)),
            Vector3Scale(towardCamera, layerOffset * 4.0F));
        const Vector2 screenCenter = GetWorldToScreen(
            labelCenter, camera);
        const Vector2 screenTop = GetWorldToScreen(
            Vector3Add(labelCenter,
                       Vector3Scale(cameraUp, 0.14F)), camera);
        const float pixelHeight = Vector2Distance(
            screenCenter, screenTop) * 2.0F;
        if (visibleScreenPoint(screenCenter)) {
            screenLevelText_ = ScreenHealthText{
                .text = levelText,
                .center = screenCenter,
                .fontSize = std::clamp(
                    pixelHeight / 1.68F, 9.0F, 25.0F),
                .opacity = opacity,
            };
        }
    }
}

void TargetHealthBar::drawScreenOverlay() const {
    if (screenLevelText_) {
        const ScreenHealthText& level = *screenLevelText_;
        const Vector2 measured = measureUiText(
            level.text, level.fontSize);
        SdfUiTextStyle style{};
        style.fillColor = withOpacity(
            {255, 235, 174, 255}, level.opacity);
        style.outlineColor = withOpacity(
            {49, 36, 28, 255}, level.opacity);
        style.shadowColor = withOpacity(
            {20, 16, 14, 210}, level.opacity);
        style.shadowOffsetPixels = {0.0F, 3.0F};
        style.outlinePixels = 2.0F;
        drawStyledSdfUiText(
            level.text,
            {level.center.x - measured.x * 0.5F,
             level.center.y - measured.y * 0.5F},
            level.fontSize, style);
    }
    if (!screenHealthText_) return;
    const ScreenHealthText& text = *screenHealthText_;
    const Vector2 measured = measureUiText(
        text.text, text.fontSize);
    SdfUiTextStyle style{};
    style.fillColor = withOpacity(UiWhite, text.opacity);
    style.outlineColor = withOpacity(
        {49, 49, 49, 255}, text.opacity);
    style.shadowColor = withOpacity(
        {82, 82, 82, 255}, text.opacity);
    style.shadowOffsetPixels = {0.0F, 2.0F};
    style.outlinePixels = 2.0F;
    drawStyledSdfUiText(
        text.text,
        {text.center.x - measured.x * 0.5F,
         text.center.y - measured.y * 0.5F},
        text.fontSize, style);
}

} // namespace ian
