#include "ui/TargetHealthBar.hpp"

#include "game/Simulation.hpp"
#include "ui/TargetHealthBarAnchor.hpp"
#include "ui/UiText.hpp"
#include "ui/WorldBillboard.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace ian {
void TargetHealthBar::draw(const SimulationSnapshot& snapshot,
                           const Camera3D& camera) {
    repairPulseRemaining_ = std::max(
        0.0,
        repairPulseRemaining_ -
            static_cast<double>(GetFrameTime()));
    if (repairPulseRemaining_ <= 0.0) {
        repairTarget_.reset();
    }
    if (snapshot.aimedResource) {
        const auto resource = std::find_if(
            snapshot.resourceNodes.begin(),
            snapshot.resourceNodes.end(),
            [&snapshot](const ResourceNode& candidate) {
                return candidate.active &&
                       candidate.id == *snapshot.aimedResource;
            });
        if (resource != snapshot.resourceNodes.end()) {
            constexpr float AnchorHeight = 1.12F;
            drawBillboard(
                {TargetKind::Resource, resource->id},
                {static_cast<float>(resource->position.x),
                 static_cast<float>(resource->position.y) +
                     AnchorHeight,
                 static_cast<float>(resource->position.z)},
                resource->health, resource->maxHealth,
                {235, 186, 55, 255}, camera);
            return;
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
            drawBillboard(
                {TargetKind::Building, building->id},
                {static_cast<float>(anchor.x),
                 static_cast<float>(anchor.y),
                 static_cast<float>(anchor.z)},
                building->health, building->maxHealth,
                {82, 210, 103, 255}, camera,
                static_cast<int>(building->level));
            return;
        }
    } else if (snapshot.aimedModularBuilding) {
        const auto frame = std::find_if(
            snapshot.platformFrames.begin(),
            snapshot.platformFrames.end(),
            [&snapshot](
                const PlatformFrameInstance& candidate) {
                return candidate.id ==
                       *snapshot.aimedModularBuilding;
            });
        if (frame != snapshot.platformFrames.end()) {
            const double cellSize =
                snapshot.worldCellSize;
            drawBillboard(
                {TargetKind::Foundation, frame->id},
                {
                    static_cast<float>(
                        (frame->anchor.x +
                         PlatformFrameWidthCells * 0.5) *
                        cellSize),
                    static_cast<float>(
                        frame->floorHeight + 0.38),
                    static_cast<float>(
                        (frame->anchor.z +
                         PlatformFrameWidthCells * 0.5) *
                        cellSize),
                },
                frame->health, frame->maxHealth,
                {82, 210, 103, 255}, camera);
            return;
        }
        const auto wall = std::find_if(
            snapshot.modularWalls.begin(),
            snapshot.modularWalls.end(),
            [&snapshot](const WallInstance& candidate) {
                return candidate.id ==
                       *snapshot.aimedModularBuilding;
            });
        if (wall != snapshot.modularWalls.end()) {
            drawBillboard(
                {TargetKind::Foundation, wall->id},
                {
                    static_cast<float>(
                        (wall->anchor.x + 0.5) *
                        snapshot.worldCellSize),
                    static_cast<float>(
                        wall->topHeight + 0.34),
                    static_cast<float>(
                        (wall->anchor.z + 0.5) *
                        snapshot.worldCellSize),
                },
                wall->health, wall->maxHealth,
                {82, 210, 103, 255}, camera);
            return;
        }
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
            const int widthCells =
                alongZ ? ModularRampWidthCells
                       : ModularRampRunCells;
            const int depthCells =
                alongZ ? ModularRampRunCells
                       : ModularRampWidthCells;
            drawBillboard(
                {TargetKind::Foundation, ramp->id},
                {
                    static_cast<float>(
                        (ramp->anchor.x +
                         widthCells * 0.5) *
                        snapshot.worldCellSize),
                    static_cast<float>(
                        ramp->topHeight + 0.34),
                    static_cast<float>(
                        (ramp->anchor.z +
                         depthCells * 0.5) *
                        snapshot.worldCellSize),
                },
                ramp->health, ramp->maxHealth,
                {82, 210, 103, 255}, camera);
            return;
        }
    } else if (snapshot.aimedEnemy) {
        const auto enemy = std::find_if(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [&snapshot](const EnemyInstance& candidate) {
                return candidate.active &&
                       candidate.id == *snapshot.aimedEnemy;
            });
        if (enemy != snapshot.enemies.end()) {
            float anchorOffset = 1.24F;
            if (enemy->type == EnemyType::Fast) {
                anchorOffset = 1.04F;
            } else if (enemy->type == EnemyType::Heavy) {
                anchorOffset = 1.52F;
            } else if (enemy->type == EnemyType::Boss) {
                anchorOffset = 2.52F;
            } else if (enemy->type == EnemyType::Ranged) {
                anchorOffset = 1.18F;
            } else if (enemy->type == EnemyType::Sapper) {
                anchorOffset = 1.14F;
            } else if (enemy->type == EnemyType::Flying) {
                anchorOffset = 0.92F;
            }
            drawBillboard(
                {TargetKind::Enemy, enemy->id},
                {static_cast<float>(enemy->position.x),
                 static_cast<float>(enemy->position.y) +
                     anchorOffset,
                 static_cast<float>(enemy->position.z)},
                enemy->health, enemy->maxHealth,
                {224, 66, 58, 255}, camera);
            return;
        }
    }
    reset();
}

void TargetHealthBar::reset() {
    target_.reset();
}

void TargetHealthBar::notifyRepair(EntityId id) {
    repairTarget_ = id;
    repairPulseRemaining_ = repairPulseDuration_;
}

void TargetHealthBar::drawBillboard(
    Target target, Vector3 anchorPosition, double health,
    double maxHealth, Color fillColor, const Camera3D& camera,
    int buildingLevel) {
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
            1.0 -
            std::exp(
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
            1.0 -
            repairPulseRemaining_ / repairPulseDuration_);
        pulseScale +=
            std::sin(progress * PI) *
            (1.0F - progress) * 0.1F;
    }
    const float OuterWidth = 1.26F * pulseScale;
    const float OuterHeight = 0.252F * pulseScale;
    constexpr float Border = 0.035F;
    constexpr float LayerOffset = 0.004F;
    const float InnerWidth = OuterWidth - Border * 2.0F;
    const float InnerHeight = OuterHeight - Border * 2.0F;
    const float displayedFraction = static_cast<float>(
        std::clamp(displayedHealth_ / maxHealth, 0.0, 1.0));
    const float actualFraction = static_cast<float>(
        std::clamp(health / maxHealth, 0.0, 1.0));

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    drawWorldBillboardQuad(anchorPosition, OuterWidth, OuterHeight, WHITE,
             cameraRight, cameraUp);
    const Vector3 innerCenter = Vector3Add(
        anchorPosition, Vector3Scale(towardCamera, LayerOffset));
    drawWorldBillboardQuad(innerCenter, InnerWidth, InnerHeight,
             {25, 22, 20, 255}, cameraRight, cameraUp);

    const auto drawSegment =
        [&](float leftFraction, float rightFraction,
            Color color, float depthLayers) {
            if (rightFraction <= leftFraction) {
                return;
            }
            const float width =
                InnerWidth *
                (rightFraction - leftFraction);
            const float centerOffset =
                -InnerWidth * 0.5F +
                InnerWidth * leftFraction +
                width * 0.5F;
            Vector3 center = Vector3Add(
                innerCenter,
                Vector3Scale(cameraRight, centerOffset));
            center = Vector3Add(
                center,
                Vector3Scale(
                    towardCamera,
                    LayerOffset * depthLayers));
            drawWorldBillboardQuad(center, width, InnerHeight, color,
                     cameraRight, cameraUp);
        };
    drawSegment(
        actualFraction, displayedFraction,
        {255, 137, 62, 150}, 1.0F);
    drawSegment(
        0.0F, actualFraction, fillColor, 2.0F);

    const Vector3 borderCenter = Vector3Add(
        innerCenter, Vector3Scale(towardCamera, LayerOffset));
    drawWorldBillboardQuad(
        Vector3Add(
            borderCenter,
            Vector3Scale(cameraUp,
                         (OuterHeight - Border) * 0.5F)),
        OuterWidth, Border, WHITE, cameraRight, cameraUp);
    drawWorldBillboardQuad(
        Vector3Subtract(
            borderCenter,
            Vector3Scale(cameraUp,
                         (OuterHeight - Border) * 0.5F)),
        OuterWidth, Border, WHITE, cameraRight, cameraUp);
    drawWorldBillboardQuad(
        Vector3Subtract(
            borderCenter,
            Vector3Scale(cameraRight,
                         (OuterWidth - Border) * 0.5F)),
        Border, InnerHeight, WHITE, cameraRight, cameraUp);
    drawWorldBillboardQuad(
        Vector3Add(
            borderCenter,
            Vector3Scale(cameraRight,
                         (OuterWidth - Border) * 0.5F)),
        Border, InnerHeight, WHITE, cameraRight, cameraUp);
    if (buildingLevel > 0) {
        const std::string levelText =
            "LEVEL " + std::to_string(buildingLevel);
        const Vector3 labelCenter = Vector3Add(
            Vector3Add(
                anchorPosition,
                Vector3Scale(cameraUp,
                             OuterHeight * 0.5F + 0.22F)),
            Vector3Scale(towardCamera, LayerOffset * 4.0F));
        rlDrawRenderBatchActive();
        drawWorldBillboardText(
            levelText, labelCenter, 0.28F, camera,
            cameraRight, cameraUp, {255, 235, 174, 255});
    }
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
}

} // namespace ian
