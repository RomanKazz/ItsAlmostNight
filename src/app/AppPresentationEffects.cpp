#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "combat/CombatDisplayUnits.hpp"
#include "ui/UiText.hpp"
#include "ui/WorldBillboard.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {

using namespace app_detail;

namespace {

[[nodiscard]] Color alphaColor(
    Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}

[[nodiscard]] SdfUiTextStyle damageNumberStyle(
    bool critical, unsigned char alpha) {
    SdfUiTextStyle style{};
    style.fillColor = alphaColor(
        critical
            ? Color{255, 196, 48, 255}
            : Color{255, 239, 211, 255},
        alpha);
    style.outlineColor = alphaColor(
        critical
            ? Color{91, 43, 18, 255}
            : Color{62, 35, 31, 255},
        alpha);
    style.shadowColor = alphaColor(
        {15, 9, 12, 255},
        static_cast<unsigned char>(
            static_cast<unsigned int>(alpha) * 4U / 5U));
    style.shadowOffsetPixels = {3.0F, 4.0F};
    style.outlinePixels = critical ? 2.8F : 2.5F;
    style.softness = 0.62F;
    return style;
}

[[nodiscard]] SdfUiTextStyle resourceNumberStyle(
    ResourceType type, unsigned char alpha) {
    Color fill{255, 190, 82, 255};
    Color outline{69, 39, 24, 255};
    if (type == ResourceType::Stone) {
        fill = {190, 218, 236, 255};
        outline = {38, 48, 59, 255};
    } else if (type == ResourceType::Crystal) {
        fill = {255, 112, 235, 255};
        outline = {66, 26, 74, 255};
    }
    SdfUiTextStyle style{};
    style.fillColor = alphaColor(fill, alpha);
    style.outlineColor = alphaColor(outline, alpha);
    style.shadowColor = alphaColor(
        {12, 8, 12, 255},
        static_cast<unsigned char>(
            static_cast<unsigned int>(alpha) * 4U / 5U));
    style.shadowOffsetPixels = {3.0F, 4.0F};
    style.outlinePixels = 2.6F;
    style.softness = 0.62F;
    return style;
}

} // namespace

void App::drawFloatingDamageNumbers(
    const Camera3D& camera) const {
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    for (const FloatingDamageNumber& number :
         floatingDamageNumbers_) {
        if (number.startDelayRemaining > 0.0) {
            continue;
        }
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
        const float fastMotion =
            1.0F - std::pow(1.0F - progress, 2.35F);
        const Vector3 animatedPosition{
            static_cast<float>(number.position.x),
            static_cast<float>(number.position.y) +
                fastMotion * number.verticalRise,
            static_cast<float>(number.position.z),
        };
        Vector2 screenPosition =
            GetWorldToScreen(animatedPosition, camera);
        const float fadeProgress = std::clamp(
            (progress - 0.56F) / 0.44F, 0.0F, 1.0F);
        const float fade =
            1.0F - fadeProgress * fadeProgress *
                (3.0F - 2.0F * fadeProgress);
        const auto alpha = static_cast<unsigned char>(
            std::lround(fade * 255.0F));
        const float distance = Vector3Length(toNumber);
        // Damage text is drawn in screen space, so apply an explicit
        // perspective scale. The square-root curve preserves readability
        // while still making distant tower hits visibly smaller.
        constexpr float ReferenceDistance = 6.0F;
        const float distanceScale = std::clamp(
            std::sqrt(ReferenceDistance / std::max(distance, 1.0F)),
            0.38F, 1.45F);
        screenPosition.x +=
            number.horizontalDrift * fastMotion * distanceScale;
        const float popProgress =
            std::clamp(progress / 0.18F, 0.0F, 1.0F);
        const float shiftedPop = popProgress - 1.0F;
        const float popScale = 1.0F +
            (number.popOvershoot + 1.0F) *
                shiftedPop * shiftedPop * shiftedPop +
            number.popOvershoot *
                shiftedPop * shiftedPop;
        const float entranceScale =
            0.42F + 0.58F * popScale;
        const float shrinkProgress = std::clamp(
            (progress - 0.42F) / 0.58F, 0.0F, 1.0F);
        const float exitScale =
            1.0F - 0.25F * shrinkProgress * shrinkProgress;
        const float fontSize =
            (number.critical ? 50.0F : 40.0F) *
            number.scaleVariation * entranceScale *
            exitScale * distanceScale;
        const char* text = number.critical
            ? TextFormat("%d!", combatDisplayValue(number.damage))
            : TextFormat("%d", combatDisplayValue(number.damage));
        const Vector2 textSize =
            measureUiText(text, fontSize);
        const float rotation =
            number.initialRotationDegrees +
            number.rotationTravelDegrees * fastMotion;
        drawRotatedStyledUiText(
            text,
            {screenPosition.x,
             screenPosition.y + textSize.y * 0.45F},
            fontSize, rotation,
            damageNumberStyle(number.critical, alpha));
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

        constexpr float FeedbackDelaySeconds = 0.10F;
        const float elapsed = static_cast<float>(
            gain.duration - gain.remaining);
        if (elapsed < FeedbackDelaySeconds) {
            continue;
        }
        const float activeDuration = std::max(
            0.01F,
            static_cast<float>(gain.duration) - FeedbackDelaySeconds);
        const float activeProgress = std::clamp(
            (elapsed - FeedbackDelaySeconds) / activeDuration,
            0.0F, 1.0F);
        const Vector2 projected =
            GetWorldToScreen(worldPosition, camera);
        const Vector2 start{
            projected.x, projected.y - 62.0F,
        };
        const auto resourceTargets =
            hudResourceIconCenters();
        const std::size_t resourceIndex =
            gain.type == ResourceType::Wood
                ? 0U
                : gain.type == ResourceType::Crystal
                    ? 2U
                    : 1U;
        const Vector2 target =
            resourceTargets[resourceIndex];
        constexpr float RiseEnd = 0.3F;
        const float riseProgress =
            std::clamp(activeProgress / RiseEnd, 0.0F, 1.0F);
        const float riseEased =
            riseProgress * riseProgress *
            (3.0F - 2.0F * riseProgress);
        const Vector2 liftedStart{
            start.x + gain.horizontalDrift, start.y - 58.0F,
        };
        const float flightProgress = std::clamp(
            (activeProgress - RiseEnd) / (1.0F - RiseEnd),
            0.0F, 1.0F);
        // Keep the HUD flight continuous. The previous two-part curve had
        // zero velocity at its join, which looked like a short stop before
        // the icon reached the counter.
        const float eased = flightProgress;
        Vector2 position{
            start.x +
                (liftedStart.x - start.x) * riseEased,
            start.y +
                (liftedStart.y - start.y) * riseEased,
        };
        if (activeProgress >= RiseEnd) {
            position.x =
                liftedStart.x +
                (target.x - liftedStart.x) * eased;
            position.y =
                liftedStart.y +
                (target.y - liftedStart.y) * eased -
                std::sin(flightProgress * PI) * 54.0F;
        }
        const float iconSize = 104.0F - eased * 36.0F;
        const float visualScale =
            gain.visualScale + (1.0F - gain.visualScale) * eased;
        const float rotationDegrees =
            gain.rotationDegrees * (1.0F - eased);
        rlPushMatrix();
        rlTranslatef(position.x, position.y, 0.0F);
        rlRotatef(rotationDegrees, 0.0F, 0.0F, 1.0F);
        rlScalef(visualScale, visualScale, 1.0F);
        rlTranslatef(-position.x, -position.y, 0.0F);
        ui_.drawResourceIcon(
            {position.x - iconSize * 0.5F,
             position.y - iconSize * 0.5F, iconSize, iconSize},
            gain.type == ResourceType::Wood
                ? UiResourceIcon::Wood
                : gain.type == ResourceType::Crystal
                    ? UiResourceIcon::Crystal
                    : UiResourceIcon::Stone);
        rlPopMatrix();

        const float textFade = std::clamp(
            (0.7F - activeProgress) / 0.25F, 0.0F, 1.0F);
        if (gain.showAmount && textFade > 0.0F) {
            const auto alpha = static_cast<unsigned char>(
                std::lround(textFade * 255.0F));
            const std::string text =
                "+" + std::to_string(gain.amount);
            const Vector2 baseTextSize =
                measureUiText(text, 48.0F);
            const Vector2 localCenter{
                iconSize * 0.62F + baseTextSize.x * 0.5F,
                -36.0F + baseTextSize.y * 0.5F};
            const float angle = rotationDegrees * DEG2RAD;
            const float sine = std::sin(angle);
            const float cosine = std::cos(angle);
            const Vector2 textCenter{
                position.x + visualScale *
                    (localCenter.x * cosine -
                     localCenter.y * sine),
                position.y + visualScale *
                    (localCenter.x * sine +
                     localCenter.y * cosine)};
            drawRotatedStyledUiText(
                text, textCenter,
                48.0F * visualScale,
                rotationDegrees,
                resourceNumberStyle(gain.type, alpha));
        }
    }
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

std::span<const ModularAnimationScale>
App::modularAnimationScales(
    const SimulationSnapshot& snapshot) const {
    const double cellSize =
        simulation_.terrain().config().cellSize;
    std::vector<ModularAnimationScale>& scales =
        modularAnimationScaleBuffer_;
    scales.clear();
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
    return std::span<const ModularAnimationScale>{scales};
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

float App::gateOpenAmount(EntityId id, bool open) const {
    const auto visual = std::find_if(
        gateDoorVisuals_.begin(), gateDoorVisuals_.end(),
        [id](const GateDoorVisual& candidate) {
            return candidate.id == id;
        });
    return visual != gateDoorVisuals_.end()
        ? visual->openAmount * visual->openDirection
        : open ? 1.0F : 0.0F;
}

float App::flamethrowerVisualYaw(const BuildingInstance& building) const {
    const auto visual = std::ranges::find(
        flamethrowerRotationVisuals_, building.id, &FlamethrowerRotationVisual::id);
    return static_cast<float>(visual != flamethrowerRotationVisuals_.end()
        ? visual->yaw : buildingRotationYaw(building.type, building.rotation));
}

} // namespace ian
