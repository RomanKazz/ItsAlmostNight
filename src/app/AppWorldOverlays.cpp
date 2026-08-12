#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

using namespace app_detail;

void App::drawWorldOverlays(
    const SimulationSnapshot& snapshot,
    const WorldLighting& lighting) {
    const auto dragPlacementSurface =
        [this](
            BuildingType type,
            GridPosition position) {
            return placementDragSurface_
                       ? simulation_
                             .previewPlacementSurface(
                                 type, position,
                                 placementDragSurface_
                                     ->height)
                       : simulation_
                             .previewPlacementSurface(
                                 type, position);
        };
    const auto dragPlacement =
        [this](
            BuildingType type,
            GridPosition position) {
            return placementDragSurface_
                       ? simulation_.previewPlacement(
                             type, position,
                             placementDragSurface_
                                 ->height)
                       : simulation_.previewPlacement(
                             type, position);
        };
    const auto drawDirectionArrow =
        [](Vector3 start, Vector3 end) {
            const Vector3 delta = Vector3Subtract(end, start);
            const float length = Vector3Length(delta);
            if (length < 0.2F) {
                return;
            }
            const Vector3 direction =
                Vector3Scale(delta, 1.0F / length);
            const Vector3 side{
                -direction.z, 0.0F, direction.x};
            constexpr Color ArrowColor{255, 205, 74, 245};
            DrawCylinderEx(
                start, end, 0.035F, 0.035F, 8,
                ArrowColor);
            const Vector3 headBase = Vector3Subtract(
                end, Vector3Scale(direction, 0.42F));
            DrawCylinderEx(
                end,
                Vector3Add(
                    headBase, Vector3Scale(side, 0.25F)),
                0.045F, 0.018F, 8, ArrowColor);
            DrawCylinderEx(
                end,
                Vector3Subtract(
                    headBase, Vector3Scale(side, 0.25F)),
                0.045F, 0.018F, 8, ArrowColor);
        };
    const bool hasModularPreview =
        foundationBuildMode_ &&
        (platformFramePreview_ || wallPreview_ ||
         rampPreview_ ||
         !modularPlatformDragPreviews_.empty() ||
         !modularWallDragPreviews_.empty() ||
         !modularRampDragPreviews_.empty() ||
         foundationTerrainHit_);
    if (hasModularPreview) {
        WorldMaterialState material{};
        material.bakedAo = 0.88F;
        material.screenAoAmount = 0.0F;
        renderer_->beginWorldShader(lighting);
        renderer_->setWorldMaterial(material);
        modularBuildingRenderer_.drawWorld(
            {
                {}, {}, {}, {},
                simulation_.terrain().config().cellSize,
                std::nullopt, {}, 1.0F,
            },
            {
                platformFramePreview_ &&
                        !modularDragPiece_
                    ? &*platformFramePreview_
                    : nullptr,
                wallPreview_ && !modularDragPiece_
                    ? &*wallPreview_
                    : nullptr,
                rampPreview_ && !modularDragPiece_
                    ? &*rampPreview_
                    : nullptr,
                modularPlatformDragPreviews_,
                modularWallDragPreviews_,
                modularRampDragPreviews_,
                foundationTerrainHit_
                    ? &*foundationTerrainHit_
                    : nullptr,
                simulation_.terrain().config()
                    .maxWoodSupportLength,
                (wallPreview_ || rampPreview_)
                    ? std::optional<float>{
                          static_cast<float>(
                              placementRotationYaw_)}
                    : std::nullopt,
                !modularDragPiece_ &&
                        modularPreviewVisualOrigin_
                    ? &*modularPreviewVisualOrigin_
                    : nullptr,
            });
        renderer_->endWorldShader();
    }
    if (modularDragStart_ && modularDragEnd_ &&
        modularDragPiece_) {
        const double cellSize =
            simulation_.terrain().config().cellSize;
        const double centerOffset =
            *modularDragPiece_ == ModularBuildPiece::Wall
                ? 0.5
                : 1.0;
        const float height = static_cast<float>(
            modularDragPlaneHeight_.value_or(0.0) + 0.22);
        drawDirectionArrow(
            {static_cast<float>(
                 (modularDragStart_->x + centerOffset) *
                 cellSize),
             height,
             static_cast<float>(
                 (modularDragStart_->z + centerOffset) *
                 cellSize)},
            {static_cast<float>(
                 (modularDragEnd_->x + centerOffset) *
                 cellSize),
             height,
             static_cast<float>(
                 (modularDragEnd_->z + centerOffset) *
                 cellSize)});
    }
    if (wallDragStart_ && wallDragEnd_ &&
        placementDragType_) {
        const auto cells = placementLine(
            *placementDragType_, *wallDragStart_,
            *wallDragEnd_, placementDragAxis_);
        if (cells.size() > 1U) {
            const Vec3 first = buildingWorldPosition(
                *placementDragType_, cells.front());
            const Vec3 last = buildingWorldPosition(
                *placementDragType_, cells.back());
            drawDirectionArrow(
                {static_cast<float>(first.x),
                 static_cast<float>(
                     dragPlacementSurface(
                         *placementDragType_, cells.front())
                         .height +
                     0.22),
                 static_cast<float>(first.z)},
                {static_cast<float>(last.x),
                 static_cast<float>(
                     dragPlacementSurface(
                         *placementDragType_, cells.back())
                         .height +
                     0.22),
                 static_cast<float>(last.z)});
        }
    }
    if (foundationBuildMode_ &&
        (modularBuildPiece_ ==
             ModularBuildPiece::FloorPlatform ||
         modularBuildPiece_ ==
             ModularBuildPiece::Ramp) &&
        modularEdgeHoverFrame_) {
        const EntityId target =
            *modularEdgeHoverFrame_;
        const auto frame = std::find_if(
            snapshot.platformFrames.begin(),
            snapshot.platformFrames.end(),
            [target](
                const PlatformFrameInstance& candidate) {
                return candidate.id == target;
            });
        if (frame != snapshot.platformFrames.end()) {
            const double cellSize =
                simulation_.terrain()
                    .config().cellSize;
            const double minimumX =
                frame->anchor.x * cellSize;
            const double minimumZ =
                frame->anchor.z * cellSize;
            const double maximumX =
                (frame->anchor.x +
                 PlatformFrameWidthCells) *
                cellSize;
            const double maximumZ =
                (frame->anchor.z +
                 PlatformFrameWidthCells) *
                cellSize;
            const double centerX =
                (minimumX + maximumX) * 0.5;
            const double centerZ =
                (minimumZ + maximumZ) * 0.5;
            struct EdgeVisual {
                Vector3 center;
                Vector3 start;
                Vector3 end;
                GridCoord neighborAnchor;
            };
            const float edgeHeight =
                static_cast<float>(
                    frame->floorHeight + 0.055);
            const std::array<EdgeVisual, 4> edges{{
                    {
                        .center = {
                            static_cast<float>(minimumX),
                            edgeHeight,
                            static_cast<float>(centerZ),
                        },
                        .start = {
                            static_cast<float>(minimumX),
                            edgeHeight,
                            static_cast<float>(minimumZ),
                        },
                        .end = {
                            static_cast<float>(minimumX),
                            edgeHeight,
                            static_cast<float>(maximumZ),
                        },
                        .neighborAnchor = {
                            frame->anchor.x -
                                PlatformFrameWidthCells,
                            frame->anchor.yLevel,
                            frame->anchor.z,
                        },
                    },
                    {
                        .center = {
                            static_cast<float>(maximumX),
                            edgeHeight,
                            static_cast<float>(centerZ),
                        },
                        .start = {
                            static_cast<float>(maximumX),
                            edgeHeight,
                            static_cast<float>(minimumZ),
                        },
                        .end = {
                            static_cast<float>(maximumX),
                            edgeHeight,
                            static_cast<float>(maximumZ),
                        },
                        .neighborAnchor = {
                            frame->anchor.x +
                                PlatformFrameWidthCells,
                            frame->anchor.yLevel,
                            frame->anchor.z,
                        },
                    },
                    {
                        .center = {
                            static_cast<float>(centerX),
                            edgeHeight,
                            static_cast<float>(minimumZ),
                        },
                        .start = {
                            static_cast<float>(minimumX),
                            edgeHeight,
                            static_cast<float>(minimumZ),
                        },
                        .end = {
                            static_cast<float>(maximumX),
                            edgeHeight,
                            static_cast<float>(minimumZ),
                        },
                        .neighborAnchor = {
                            frame->anchor.x,
                            frame->anchor.yLevel,
                            frame->anchor.z -
                                PlatformFrameWidthCells,
                        },
                    },
                    {
                        .center = {
                            static_cast<float>(centerX),
                            edgeHeight,
                            static_cast<float>(maximumZ),
                        },
                        .start = {
                            static_cast<float>(minimumX),
                            edgeHeight,
                            static_cast<float>(maximumZ),
                        },
                        .end = {
                            static_cast<float>(maximumX),
                            edgeHeight,
                            static_cast<float>(maximumZ),
                        },
                        .neighborAnchor = {
                            frame->anchor.x,
                            frame->anchor.yLevel,
                            frame->anchor.z +
                                PlatformFrameWidthCells,
                        },
                    },
                }};
            for (const EdgeVisual& edge : edges) {
                const bool selected =
                    modularEdgeExtensionAnchor_ &&
                    modularEdgeExtensionAnchor_->x ==
                        edge.neighborAnchor.x &&
                    modularEdgeExtensionAnchor_->z ==
                        edge.neighborAnchor.z;
                const Color edgeColor =
                    selected
                        ? platformFramePreview_ &&
                                  !platformFramePreview_
                                       ->valid()
                              ? Color{255, 88, 76, 245}
                              : Color{126, 255, 158, 245}
                        : Color{235, 240, 226, 135};
                DrawCylinderEx(
                    edge.start, edge.end,
                    selected ? 0.035F : 0.012F,
                    selected ? 0.035F : 0.012F,
                    8, edgeColor);
                DrawCircle3D(
                    edge.center,
                    selected ? 0.15F : 0.09F,
                    {1.0F, 0.0F, 0.0F}, 90.0F,
                    edgeColor);
            }
        }
    }
    if (foundationBuildMode_ &&
        foundationTerrainHit_) {
        bool placementValid = true;
        if (platformFramePreview_) {
            placementValid =
                platformFramePreview_->valid();
        } else if (wallPreview_) {
            placementValid = wallPreview_->valid();
        } else if (rampPreview_) {
            placementValid = rampPreview_->valid();
        }
        double snapHeight =
            foundationTerrainHit_->y;
        if (platformFramePreview_) {
            snapHeight =
                platformFramePreview_->floorHeight;
        } else if (wallPreview_) {
            snapHeight = wallPreview_->bottomHeight;
        } else if (rampPreview_) {
            snapHeight = rampPreview_->bottomHeight;
        }
        const float pulse =
            0.03F +
            0.025F * static_cast<float>(
                std::sin(
                    snapshot.elapsedSeconds * 7.0));
        DrawCircle3D(
            {
                static_cast<float>(
                    modularSnapMarker_
                        ? modularSnapMarker_->x
                        : foundationTerrainHit_->x),
                static_cast<float>(
                    snapHeight + 0.035),
                static_cast<float>(
                    modularSnapMarker_
                        ? modularSnapMarker_->z
                        : foundationTerrainHit_->z),
            },
            0.18F + pulse,
            {1.0F, 0.0F, 0.0F}, 90.0F,
            placementValid
                ? Color{135, 244, 169, 190}
                : Color{255, 88, 76, 220});
    }
    if (snapshot.buildingPreview) {
        const Vec3 targetCenter = buildingWorldPosition(
            snapshot.buildingPreview->type,
            snapshot.buildingPreview->gridPosition);
        const Vector2 visualCenter = repelInvalidPreview(
            placementPreviewCenter_.value_or(Vector2{
                static_cast<float>(targetCenter.x),
                static_cast<float>(targetCenter.z),
            }),
            *snapshot.buildingPreview,
            snapshot.playerPosition);
        drawBuildGrid(
            {static_cast<float>(snapshot.playerPosition.x), 0.0F,
             static_cast<float>(snapshot.playerPosition.z)},
            snapshot.worldLimit,
            simulation_.terrain());
        if (wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            snapshot.buildingPreview->type ==
                placementDragType_) {
            const BuildingType dragType =
                *placementDragType_;
            const auto cells =
                placementLine(
                    dragType, *wallDragStart_,
                    *wallDragEnd_, placementDragAxis_);
            const ResourceCost cost =
                snapshot.buildingCosts[
                    static_cast<std::size_t>(
                        dragType)];
            for (std::size_t index = 0;
                 index < cells.size(); ++index) {
                const BuildingPlatformSurface surface =
                    dragPlacementSurface(
                        dragType, cells[index]);
                BuildingPreview cellPreview{
                    .type = dragType,
                    .gridPosition = cells[index],
                    .rotation =
                        snapshot.buildingPreview->rotation,
                    .placement = dragPlacement(
                        dragType, cells[index]),
                    .baseHeight = surface.height,
                    .platformStorey = surface.storey,
                    .foundationBottomHeight =
                        surface.foundationBottomHeight,
                };
                const int count =
                    static_cast<int>(index + 1U);
                const bool lineAffordable =
                    snapshot.unlimitedResources ||
                    (snapshot.wood >= cost.wood * count &&
                     snapshot.stone >= cost.stone * count &&
                     snapshot.gold >= cost.gold * count);
                if (!lineAffordable &&
                    cellPreview.placement.valid()) {
                    cellPreview.placement.error =
                        PlacementError::
                            InsufficientResources;
                }
                const Vec3 center =
                    buildingWorldPosition(
                        dragType, cells[index]);
                drawPlacementFootprint(
                    cellPreview,
                    {static_cast<float>(center.x),
                     static_cast<float>(center.z)},
                    static_cast<float>(
                        cellPreview.rotation) *
                        PI * 0.5F);
            }
        } else {
            drawPlacementFootprint(
                *snapshot.buildingPreview, visualCenter,
                static_cast<float>(placementRotationYaw_));
        }
        if (!wallDragStart_ &&
            snapshot.buildingPreview->placement.valid() &&
            placementSnapPulseRemaining_ > 0.0) {
            const float progress = static_cast<float>(
                1.0 -
                placementSnapPulseRemaining_ / 0.18);
            const float fade =
                1.0F - std::clamp(progress, 0.0F, 1.0F);
            const auto alpha =
                static_cast<unsigned char>(
                    std::lround(fade * 210.0F));
            const float radius =
                static_cast<float>(
                    buildingFootprintHalfExtent(
                        snapshot.buildingPreview->type)) +
                0.12F + progress * 0.48F;
            DrawCircle3D(
                {visualCenter.x,
                 static_cast<float>(
                     snapshot.buildingPreview
                         ->baseHeight) +
                     0.09F,
                 visualCenter.y},
                radius, {1.0F, 0.0F, 0.0F}, 90.0F,
                {103, 255, 145, alpha});
        }
    }
    drawBuildingTacticalOverlay(snapshot);
    for (const auto& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        const Vector3 enemyPosition{
            static_cast<float>(enemy.position.x),
            static_cast<float>(enemy.position.y),
            static_cast<float>(enemy.position.z),
        };
        float width = 0.8F;
        float height = 1.6F;
        if (enemy.type == EnemyType::Fast) {
            width = 0.65F;
            height = 1.35F;
        } else if (enemy.type == EnemyType::Heavy) {
            width = 1.15F;
            height = 2.0F;
        } else if (enemy.type == EnemyType::Boss) {
            width = 2.0F;
            height = 3.2F;
        } else if (enemy.type == EnemyType::Ranged) {
            width = 0.75F;
            height = 1.55F;
        } else if (enemy.type == EnemyType::Sapper) {
            width = 0.86F;
            height = 1.5F;
        } else if (enemy.type == EnemyType::Flying) {
            width = 0.72F;
            height = 1.0F;
        } else if (enemy.type == EnemyType::Splitter) {
            width = 1.45F;
            height = 2.35F;
        } else if (enemy.type == EnemyType::Splitling) {
            width = 0.65F;
            height = 1.05F;
        }
        if (enemy.state == EnemyState::BossRamWindup) {
            const float pulse =
                0.12F + static_cast<float>(std::sin(snapshot.elapsedSeconds * 18.0)) * 0.06F;
            DrawCubeWires(enemyPosition, width + pulse, height + pulse,
                          width + pulse, ORANGE);
        }
    }
    if (snapshot.buildingPreview) {
        const auto& preview = *snapshot.buildingPreview;
        BuildingPreview visualPreview = preview;
        const Vec3 targetCenter = buildingWorldPosition(
            preview.type, preview.gridPosition);
        Vector2 modelCenter =
            placementPreviewCenter_.value_or(Vector2{
                static_cast<float>(targetCenter.x),
                static_cast<float>(targetCenter.z),
            });
        if (wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            preview.type == *placementDragType_) {
            const auto cells =
                placementLine(
                    preview.type, *wallDragStart_,
                    *wallDragEnd_, placementDragAxis_);
            if (!cells.empty()) {
                const Vec3 lineEnd =
                    buildingWorldPosition(
                        preview.type,
                        cells.back());
                modelCenter = {
                    static_cast<float>(lineEnd.x),
                    static_cast<float>(lineEnd.z),
                };
                visualPreview.gridPosition = cells.back();
                visualPreview.placement =
                    dragPlacement(
                        preview.type, cells.back());
                const BuildingPlatformSurface surface =
                    dragPlacementSurface(
                        preview.type, cells.back());
                visualPreview.baseHeight =
                    surface.height;
                visualPreview.platformStorey =
                    surface.storey;
                visualPreview.foundationBottomHeight =
                    surface.foundationBottomHeight;
                const ResourceCost cost =
                    snapshot.buildingCosts[
                        static_cast<std::size_t>(
                            preview.type)];
                const int count =
                    static_cast<int>(cells.size());
                const bool affordable =
                    snapshot.unlimitedResources ||
                    (snapshot.wood >= cost.wood * count &&
                     snapshot.stone >= cost.stone * count &&
                     snapshot.gold >= cost.gold * count);
                if (!affordable &&
                    visualPreview.placement.valid()) {
                    visualPreview.placement.error =
                        PlacementError::
                            InsufficientResources;
                }
            }
        }
        const Vector2 visualCenter = repelInvalidPreview(
            modelCenter, visualPreview,
            snapshot.playerPosition);
        const float x = visualCenter.x;
        const float z = visualCenter.y;
        Color color =
            placementColor(
                visualPreview.placement.error, false);
        color.a = 150;
        const float yaw =
            static_cast<float>(placementRotationYaw_);
        WorldMaterialState previewMaterial{};
        previewMaterial.baseColor = {
            static_cast<float>(color.r) / 255.0F,
            static_cast<float>(color.g) / 255.0F,
            static_cast<float>(color.b) / 255.0F,
            static_cast<float>(color.a) / 255.0F,
        };
        previewMaterial.bakedAo = 0.85F;
        previewMaterial.screenAoAmount = 0.0F;
        const bool drawPreviewModel =
            !placementPreviewObstructed(
                visualPreview.placement.error);
        if (drawPreviewModel) {
            rlDrawRenderBatchActive();
            BeginBlendMode(BLEND_ALPHA);
            rlDisableDepthMask();
            renderer_->beginWorldShader(lighting);
            renderer_->setWorldMaterial(
                previewMaterial);
        const Vec3 visualTargetCenter =
            buildingWorldPosition(
                visualPreview.type,
                visualPreview.gridPosition);
        if (const auto foundation =
                automaticBuildingFoundation(
                    visualPreview.type,
                    visualPreview.gridPosition,
                    visualPreview.baseHeight,
                    visualPreview
                        .foundationBottomHeight,
                    simulation_.terrain()
                        .config().cellSize)) {
            rlPushMatrix();
            rlTranslatef(
                x - static_cast<float>(
                        visualTargetCenter.x),
                0.0F,
                z - static_cast<float>(
                        visualTargetCenter.z));
            modularBuildingRenderer_.drawWorld(
                {
                    std::span<
                        const PlatformFrameInstance>{
                        &*foundation, 1U},
                    {}, {}, {},
                    simulation_.terrain()
                        .config().cellSize,
                    std::nullopt, {}, 1.0F,
                },
                {});
            rlPopMatrix();
        }
        if (wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            *placementDragType_ == preview.type) {
            const auto cells = placementLine(
                preview.type, *wallDragStart_,
                *wallDragEnd_, placementDragAxis_);
            for (std::size_t index = 0;
                 index + 1U < cells.size(); ++index) {
                if (placementPreviewObstructed(
                        dragPlacement(
                            preview.type, cells[index])
                            .error)) {
                    continue;
                }
                const BuildingPlatformSurface surface =
                    dragPlacementSurface(
                        preview.type, cells[index]);
                const auto foundation =
                    automaticBuildingFoundation(
                        preview.type, cells[index],
                        surface.height,
                        surface.foundationBottomHeight,
                        simulation_.terrain()
                            .config().cellSize);
                if (!foundation) {
                    continue;
                }
                modularBuildingRenderer_.drawWorld(
                    {
                        std::span<
                            const PlatformFrameInstance>{
                            &*foundation, 1U},
                        {}, {}, {},
                        simulation_.terrain()
                            .config().cellSize,
                        std::nullopt, {}, 1.0F,
                    },
                    {});
            }
        }
        rlPushMatrix();
        rlTranslatef(
            0.0F,
            static_cast<float>(
                visualPreview.baseHeight),
            0.0F);
        const float targetYaw =
            static_cast<float>(preview.rotation) * PI * 0.5F;
        const float yawDifference = std::abs(
            std::atan2(
                std::sin(targetYaw - yaw),
                std::cos(targetYaw - yaw)));
        if (yawDifference > 0.06F &&
            (preview.type == BuildingType::Turret ||
             preview.type == BuildingType::Cannon)) {
            WorldMaterialState ghostMaterial =
                previewMaterial;
            ghostMaterial.baseColor.w = 0.12F;
            renderer_->setWorldMaterial(ghostMaterial);
            rlDrawRenderBatchActive();
            rlDisableDepthMask();
            if (preview.type == BuildingType::Turret) {
                static_cast<void>(
                    renderer_->drawCrossbow(
                        {x, 0.0F, z}, targetYaw,
                        {255, 255, 255, 70}));
            } else {
                static_cast<void>(
                    renderer_->drawCannon(
                        {x, 0.0F, z}, targetYaw, 0.0F,
                        {255, 255, 255, 70}));
            }
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
        }
        renderer_->setWorldMaterial(previewMaterial);

        if (wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            *placementDragType_ == preview.type &&
            preview.type != BuildingType::Wall) {
            const auto cells = placementLine(
                preview.type, *wallDragStart_,
                *wallDragEnd_, placementDragAxis_);
            const ResourceCost cost =
                snapshot.buildingCosts[
                    static_cast<std::size_t>(preview.type)];
            for (std::size_t index = 0;
                 index + 1U < cells.size(); ++index) {
                PlacementResult placement =
                    dragPlacement(
                        preview.type, cells[index]);
                const int count =
                    static_cast<int>(index + 1U);
                const bool affordable =
                    snapshot.unlimitedResources ||
                    (snapshot.wood >= cost.wood * count &&
                     snapshot.stone >= cost.stone * count &&
                     snapshot.gold >= cost.gold * count);
                if (!affordable && placement.valid()) {
                    placement.error =
                        PlacementError::InsufficientResources;
                }
                if (placementPreviewObstructed(
                        placement.error)) {
                    continue;
                }
                Color cellColor =
                    placementColor(
                        placement.error, false);
                cellColor.a = 110;
                WorldMaterialState cellMaterial =
                    previewMaterial;
                cellMaterial.baseColor = {
                    static_cast<float>(cellColor.r) / 255.0F,
                    static_cast<float>(cellColor.g) / 255.0F,
                    static_cast<float>(cellColor.b) / 255.0F,
                    static_cast<float>(cellColor.a) / 255.0F,
                };
                renderer_->setWorldMaterial(cellMaterial);
                const Vec3 center = buildingWorldPosition(
                    preview.type, cells[index]);
                const BuildingPlatformSurface surface =
                    dragPlacementSurface(
                        preview.type, cells[index]);
                const Vector3 position{
                    static_cast<float>(center.x),
                    static_cast<float>(
                        surface.height -
                        visualPreview.baseHeight),
                    static_cast<float>(center.z)};
                if (preview.type == BuildingType::Turret) {
                    static_cast<void>(
                        renderer_->drawCrossbow(
                            position, yaw));
                } else if (
                    preview.type == BuildingType::Cannon) {
                    static_cast<void>(
                        renderer_->drawCannon(
                            position, yaw, 0.0F));
                } else if (
                    preview.type == BuildingType::GoldMine ||
                    preview.type == BuildingType::LumberMill ||
                    preview.type == BuildingType::Quarry) {
                    static_cast<void>(
                        renderer_->drawResourceProducer(
                            preview.type, position, yaw));
                } else if (
                    preview.type == BuildingType::SlowTrap) {
                    DrawCube(
                        {position.x, position.y + 0.08F,
                         position.z},
                        1.0F, 0.16F, 1.0F, WHITE);
                } else if (
                    preview.type == BuildingType::SpikeTrap) {
                    static_cast<void>(renderer_->drawSpikeTrap(
                        position, yaw, -1.0F));
                } else if (
                    preview.type == BuildingType::WoodStorage ||
                    preview.type == BuildingType::StoneStorage ||
                    preview.type == BuildingType::CrystalStorage) {
                    DrawCube(
                        {position.x, position.y + 0.75F,
                         position.z},
                        1.8F, 1.5F, 1.8F, WHITE);
                    DrawCube(
                        {position.x, position.y + 1.62F,
                         position.z},
                        1.25F, 0.24F, 1.25F, WHITE);
                } else if ((preview.rotation % 2U) == 0U) {
                    DrawCube(
                        {position.x - 0.38F, 1.0F, position.z},
                        0.22F, 2.0F, 1.0F, WHITE);
                    DrawCube(
                        {position.x + 0.38F, 1.0F, position.z},
                        0.22F, 2.0F, 1.0F, WHITE);
                    DrawCube(
                        {position.x, 1.0F, position.z},
                        0.55F, 1.7F, 0.18F, WHITE);
                } else {
                    DrawCube(
                        {position.x, 1.0F, position.z - 0.38F},
                        1.0F, 2.0F, 0.22F, WHITE);
                    DrawCube(
                        {position.x, 1.0F, position.z + 0.38F},
                        1.0F, 2.0F, 0.22F, WHITE);
                    DrawCube(
                        {position.x, 1.0F, position.z},
                        0.18F, 1.7F, 0.55F, WHITE);
                }
            }
            renderer_->setWorldMaterial(previewMaterial);
        }

        if (preview.type == BuildingType::Core) {
            if (!renderer_->drawCore({x, 0.0F, z}, yaw)) {
                DrawCube({x, 1.25F, z}, 2.0F, 2.5F, 2.0F,
                         WHITE);
            }
        } else if (preview.type == BuildingType::Turret) {
            if (!renderer_->drawCrossbow({x, 0.0F, z}, yaw)) {
                DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                         WHITE);
                DrawCylinder({x, 1.45F, z}, 0.42F, 0.32F,
                             0.7F, 8, WHITE);
                DrawCube({x, 1.55F, z - 0.55F}, 0.18F,
                         0.18F, 1.0F, WHITE);
            }
        } else if (preview.type == BuildingType::Cannon) {
            if (!renderer_->drawCannon({x, 0.0F, z}, yaw, 0.0F)) {
                DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                         WHITE);
                DrawSphere({x, 1.35F, z}, 0.48F, WHITE);
                DrawCube({x, 1.45F, z - 0.75F}, 0.28F,
                         0.28F, 1.4F, WHITE);
            }
        } else if (
            preview.type == BuildingType::GoldMine ||
            preview.type == BuildingType::LumberMill ||
            preview.type == BuildingType::Quarry) {
            if (!renderer_->drawResourceProducer(
                    preview.type, {x, 0.0F, z}, yaw)) {
                DrawCube({x, 0.275F, z}, 1.0F, 0.55F,
                         1.0F, WHITE);
            }
        } else if (
            preview.type == BuildingType::WoodStorage ||
            preview.type == BuildingType::StoneStorage ||
            preview.type == BuildingType::CrystalStorage) {
            DrawCube({x, 0.75F, z}, 1.8F, 1.5F, 1.8F,
                     WHITE);
            DrawCube({x, 1.62F, z}, 1.25F, 0.24F, 1.25F,
                     WHITE);
        } else if (preview.type == BuildingType::SlowTrap) {
            DrawCube({x, 0.08F, z}, 1.0F, 0.16F, 1.0F,
                     WHITE);
        } else if (preview.type == BuildingType::SpikeTrap) {
            if (!renderer_->drawSpikeTrap(
                    {x, 0.0F, z}, yaw, -1.0F)) {
                DrawCube({x, 0.08F, z}, 1.0F, 0.16F, 1.0F,
                         WHITE);
            }
        } else if (preview.type == BuildingType::Wall) {
            if (wallDragStart_ && wallDragEnd_) {
                const auto cells =
                    placementLine(
                        BuildingType::Wall,
                        *wallDragStart_, *wallDragEnd_,
                        placementDragAxis_);
                const ResourceCost cost =
                    snapshot.buildingCosts[
                        static_cast<std::size_t>(
                            BuildingType::Wall)];
                for (std::size_t index = 0;
                     index < cells.size(); ++index) {
                    const PlacementResult placement =
                        dragPlacement(
                            BuildingType::Wall,
                            cells[index]);
                    if (placementPreviewObstructed(
                            placement.error)) {
                        continue;
                    }
                    const int count =
                        static_cast<int>(index + 1U);
                    const bool affordable =
                        snapshot.unlimitedResources ||
                        (snapshot.wood >=
                             cost.wood * count &&
                         snapshot.stone >=
                             cost.stone * count &&
                         snapshot.gold >=
                             cost.gold * count);
                    const bool valid =
                        placement.valid() && affordable;
                    WorldMaterialState cellMaterial =
                        previewMaterial;
                    Color cellColor =
                        placementColor(
                            valid
                                ? PlacementError::None
                                : placement.valid()
                                      ? PlacementError::
                                            InsufficientResources
                                      : placement.error,
                            false);
                    cellColor.a = 110;
                    cellMaterial.baseColor = {
                        static_cast<float>(cellColor.r) /
                            255.0F,
                        static_cast<float>(cellColor.g) /
                            255.0F,
                        static_cast<float>(cellColor.b) /
                            255.0F,
                        static_cast<float>(cellColor.a) /
                            255.0F,
                    };
                    renderer_->setWorldMaterial(cellMaterial);

                    std::uint8_t connections =
                        wallConnectionMask(
                            snapshot.buildings,
                            cells[index],
                            simulation_
                                .previewPlacementSurface(
                                    preview.type,
                                    cells[index],
                                    placementDragSurface_
                                        ? placementDragSurface_
                                              ->height
                                        : visualPreview
                                              .baseHeight)
                                .height);
                    if (index > 0U) {
                        connections |= wallConnectionToward(
                            cells[index],
                            cells[index - 1U]);
                    }
                    if (index + 1U < cells.size()) {
                        connections |= wallConnectionToward(
                            cells[index],
                            cells[index + 1U]);
                    }
                    const Vec3 cellCenter =
                        buildingWorldPosition(
                            BuildingType::Wall,
                            cells[index]);
                    const BuildingPlatformSurface surface =
                        dragPlacementSurface(
                            preview.type,
                            cells[index]);
                    const float heightOffset =
                        static_cast<float>(
                            surface.height -
                            visualPreview.baseHeight);
                    if (!renderer_->drawWall(
                            {static_cast<float>(
                                 cellCenter.x),
                             heightOffset,
                             static_cast<float>(
                                 cellCenter.z)},
                            connections, yaw)) {
                        DrawCube(
                            {static_cast<float>(
                                 cellCenter.x),
                             heightOffset + 1.0F,
                             static_cast<float>(
                                 cellCenter.z)},
                            1.0F, 2.0F, 1.0F, WHITE);
                    }
                }
            } else {
                const std::uint8_t connections =
                    wallConnectionMask(
                        snapshot.buildings,
                        visualPreview.gridPosition,
                        visualPreview.baseHeight);
                if (!renderer_->drawWall(
                        {x, 0.0F, z}, connections, yaw)) {
                    DrawCube({x, 1.0F, z}, 1.0F, 2.0F,
                             1.0F, WHITE);
                }
            }
        } else if ((preview.rotation % 2U) == 0U) {
            DrawCube({x - 0.38F, 1.0F, z}, 0.22F, 2.0F,
                     1.0F, WHITE);
            DrawCube({x + 0.38F, 1.0F, z}, 0.22F, 2.0F,
                     1.0F, WHITE);
            DrawCube({x, 1.0F, z}, 0.55F, 1.7F, 0.18F,
                     WHITE);
        } else {
            DrawCube({x, 1.0F, z - 0.38F}, 1.0F, 2.0F,
                     0.22F, WHITE);
            DrawCube({x, 1.0F, z + 0.38F}, 1.0F, 2.0F,
                     0.22F, WHITE);
            DrawCube({x, 1.0F, z}, 0.18F, 1.7F, 0.55F,
                     WHITE);
        }
        rlPopMatrix();
            renderer_->endWorldShader();
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
            EndBlendMode();
        }

    }
    drawCancelledPlacementPreview(lighting);
    if (!structuralRiskIds_.empty()) {
        const float pulse =
            0.045F +
            0.025F * static_cast<float>(
                std::sin(snapshot.elapsedSeconds * 7.0));
        constexpr Color RiskColor{255, 190, 62, 235};
        const double cellSize =
            simulation_.terrain().config().cellSize;
        for (const EntityId target : structuralRiskIds_) {
            if (const auto building = std::ranges::find_if(
                    snapshot.buildings,
                    [target](const BuildingInstance& item) {
                        return item.id == target;
                    });
                building != snapshot.buildings.end()) {
                const Vec3 center =
                    buildingWorldPosition(*building);
                const float width = static_cast<float>(
                    buildingFootprintHalfExtent(
                        building->type) *
                    2.0);
                DrawCubeWires(
                    {static_cast<float>(center.x),
                     static_cast<float>(center.y + 1.1),
                     static_cast<float>(center.z)},
                    width + pulse, 2.2F + pulse,
                    width + pulse, RiskColor);
                continue;
            }
            if (const auto frame = std::ranges::find_if(
                    snapshot.platformFrames,
                    [target](const PlatformFrameInstance& item) {
                        return item.id == target;
                    });
                frame != snapshot.platformFrames.end()) {
                DrawCubeWires(
                    {static_cast<float>(
                         (frame->anchor.x + 1.0) * cellSize),
                     static_cast<float>(frame->floorHeight),
                     static_cast<float>(
                         (frame->anchor.z + 1.0) * cellSize)},
                    static_cast<float>(
                        PlatformFrameWidthCells * cellSize) +
                        pulse,
                    0.24F + pulse,
                    static_cast<float>(
                        PlatformFrameWidthCells * cellSize) +
                        pulse,
                    RiskColor);
                continue;
            }
            if (const auto wall = std::ranges::find_if(
                    snapshot.modularWalls,
                    [target](const WallInstance& item) {
                        return item.id == target;
                    });
                wall != snapshot.modularWalls.end()) {
                DrawCubeWires(
                    {static_cast<float>(
                         (wall->anchor.x + 0.5) * cellSize),
                     static_cast<float>(
                         (wall->bottomHeight +
                          wall->topHeight) *
                         0.5),
                     static_cast<float>(
                         (wall->anchor.z + 0.5) * cellSize)},
                    static_cast<float>(cellSize) + pulse,
                    static_cast<float>(
                        wall->topHeight - wall->bottomHeight) +
                        pulse,
                    static_cast<float>(cellSize) + pulse,
                    RiskColor);
                continue;
            }
            const auto ramp = std::ranges::find_if(
                snapshot.ramps,
                [target](const RampInstance& item) {
                    return item.id == target;
                });
            if (ramp == snapshot.ramps.end()) {
                continue;
            }
            const bool alongZ =
                ramp->rotation == Rotation::Deg0 ||
                ramp->rotation == Rotation::Deg180;
            const int widthCells =
                alongZ ? ModularRampWidthCells
                       : ModularRampRunCells;
            const int depthCells =
                alongZ ? ModularRampRunCells
                       : ModularRampWidthCells;
            DrawCubeWires(
                {static_cast<float>(
                     (ramp->anchor.x + widthCells * 0.5) *
                     cellSize),
                 static_cast<float>(
                     (ramp->bottomHeight + ramp->topHeight) *
                     0.5),
                 static_cast<float>(
                     (ramp->anchor.z + depthCells * 0.5) *
                     cellSize)},
                static_cast<float>(widthCells * cellSize) +
                    pulse,
                static_cast<float>(
                    ramp->topHeight - ramp->bottomHeight) +
                    pulse,
                static_cast<float>(depthCells * cellSize) +
                    pulse,
                RiskColor);
        }
    }
    if (removalDragActive_ &&
        !removalDragTargets_.empty()) {
        const float pulse =
            0.04F +
            0.025F * static_cast<float>(
                std::sin(
                    snapshot.elapsedSeconds * 8.0));
        const Color removalColor{
            255, 76, 69, 235};
        const double cellSize =
            simulation_.terrain().config().cellSize;
        for (const EntityId target :
             removalDragTargets_) {
            const auto building = std::find_if(
                snapshot.buildings.begin(),
                snapshot.buildings.end(),
                [target](
                    const BuildingInstance& candidate) {
                    return candidate.id == target;
                });
            if (building != snapshot.buildings.end()) {
                const Vec3 center =
                    buildingWorldPosition(*building);
                const float width =
                    static_cast<float>(
                        buildingFootprintHalfExtent(
                            building->type) *
                        2.0) +
                    pulse;
                DrawCubeWires(
                    {
                        static_cast<float>(center.x),
                        static_cast<float>(
                            center.y + 1.1),
                        static_cast<float>(center.z),
                    },
                    width, 2.2F + pulse, width,
                    removalColor);
                continue;
            }
            const auto frame = std::find_if(
                snapshot.platformFrames.begin(),
                snapshot.platformFrames.end(),
                [target](
                    const PlatformFrameInstance&
                        candidate) {
                    return candidate.id == target;
                });
            if (frame !=
                snapshot.platformFrames.end()) {
                DrawCubeWires(
                    {
                        static_cast<float>(
                            (frame->anchor.x + 1.0) *
                            cellSize),
                        static_cast<float>(
                            frame->floorHeight),
                        static_cast<float>(
                            (frame->anchor.z + 1.0) *
                            cellSize),
                    },
                    static_cast<float>(
                        PlatformFrameWidthCells *
                        cellSize) +
                        pulse,
                    0.24F + pulse,
                    static_cast<float>(
                        PlatformFrameWidthCells *
                        cellSize) +
                        pulse,
                    removalColor);
                continue;
            }
            const auto wall = std::find_if(
                snapshot.modularWalls.begin(),
                snapshot.modularWalls.end(),
                [target](
                    const WallInstance& candidate) {
                    return candidate.id == target;
                });
            if (wall !=
                snapshot.modularWalls.end()) {
                DrawCubeWires(
                    {
                        static_cast<float>(
                            (wall->anchor.x + 0.5) *
                            cellSize),
                        static_cast<float>(
                            (wall->bottomHeight +
                             wall->topHeight) *
                            0.5),
                        static_cast<float>(
                            (wall->anchor.z + 0.5) *
                            cellSize),
                    },
                    static_cast<float>(cellSize) +
                        pulse,
                    static_cast<float>(
                        wall->topHeight -
                        wall->bottomHeight) +
                        pulse,
                    static_cast<float>(cellSize) +
                        pulse,
                    removalColor);
                continue;
            }
            const auto ramp = std::find_if(
                snapshot.ramps.begin(),
                snapshot.ramps.end(),
                [target](
                    const RampInstance& candidate) {
                    return candidate.id == target;
                });
            if (ramp == snapshot.ramps.end()) {
                continue;
            }
            const bool alongZ =
                ramp->rotation == Rotation::Deg0 ||
                ramp->rotation == Rotation::Deg180;
            const int widthCells =
                alongZ ? ModularRampWidthCells
                       : ModularRampRunCells;
            const int depthCells =
                alongZ ? ModularRampRunCells
                       : ModularRampWidthCells;
            DrawCubeWires(
                {
                    static_cast<float>(
                        (ramp->anchor.x +
                         widthCells * 0.5) *
                        cellSize),
                    static_cast<float>(
                        (ramp->bottomHeight +
                         ramp->topHeight) *
                        0.5),
                    static_cast<float>(
                        (ramp->anchor.z +
                         depthCells * 0.5) *
                        cellSize),
                },
                static_cast<float>(
                    widthCells * cellSize) +
                    pulse,
                static_cast<float>(
                    ramp->topHeight -
                    ramp->bottomHeight) +
                    pulse,
                static_cast<float>(
                    depthCells * cellSize) +
                    pulse,
                removalColor);
        }
    }
    if (showColliders_) {
        for (const auto& collider : snapshot.collisionBoxes) {
            const float width = static_cast<float>(collider.maxX - collider.minX);
            const float depth = static_cast<float>(collider.maxZ - collider.minZ);
            const Vector3 center{
                static_cast<float>((collider.minX + collider.maxX) * 0.5),
                1.0F,
                static_cast<float>((collider.minZ + collider.maxZ) * 0.5),
            };
            DrawCubeWires(center, width, 2.0F, depth, MAGENTA);
        }
    }
    if (showFlowField_) {
        for (const auto& sample : snapshot.flowDebugVectors) {
            const Vector3 start{
                static_cast<float>(sample.position.x),
                static_cast<float>(sample.position.y),
                static_cast<float>(sample.position.z),
            };
            if (sample.blocked) {
                DrawCubeWires(start, 0.45F, 0.08F, 0.45F, RED);
                continue;
            }
            const Vector3 end{
                start.x + static_cast<float>(sample.direction.x) * 0.75F,
                start.y,
                start.z + static_cast<float>(sample.direction.z) * 0.75F,
            };
            const Color color =
                sample.terrainCost >= FlowField::WallTraversalCost
                    ? ORANGE
                    : (sample.terrainCost > 1.0 ? YELLOW : LIME);
            DrawLine3D(start, end, color);
        }
    }
    if (showSpatialHash_) {
        std::array<GridPosition, EnemySystem::MaxEnemies> occupiedCells{};
        std::size_t occupiedCount = 0;
        for (const auto& enemy : snapshot.enemies) {
            if (!enemy.active) {
                continue;
            }
            const GridPosition cell{
                static_cast<int>(std::floor(
                    (enemy.position.x - SpatialHash::MinimumCoordinate) /
                    SpatialHash::CellSize)),
                static_cast<int>(std::floor(
                    (enemy.position.z - SpatialHash::MinimumCoordinate) /
                    SpatialHash::CellSize)),
            };
            const bool exists =
                std::find(occupiedCells.begin(),
                          occupiedCells.begin() +
                              static_cast<std::ptrdiff_t>(occupiedCount),
                          cell) !=
                occupiedCells.begin() +
                    static_cast<std::ptrdiff_t>(occupiedCount);
            if (!exists && occupiedCount < occupiedCells.size()) {
                occupiedCells[occupiedCount++] = cell;
            }
        }
        for (std::size_t index = 0; index < occupiedCount; ++index) {
            const float x = static_cast<float>(
                SpatialHash::MinimumCoordinate +
                (static_cast<double>(occupiedCells[index].x) + 0.5) *
                    SpatialHash::CellSize);
            const float z = static_cast<float>(
                SpatialHash::MinimumCoordinate +
                (static_cast<double>(occupiedCells[index].z) + 0.5) *
                    SpatialHash::CellSize);
            DrawCubeWires({x, 0.05F, z}, static_cast<float>(SpatialHash::CellSize),
                          0.1F, static_cast<float>(SpatialHash::CellSize),
                          PURPLE);
        }
    }
}

} // namespace ian
