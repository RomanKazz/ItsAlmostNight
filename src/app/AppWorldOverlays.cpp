#include "buildings/TowerRig.hpp"
#include "app/App.hpp"
#include "app/RepairStationDrawing.hpp"
#include "app/DecoyBeaconDrawing.hpp"
#include "app/WindBlowerDrawing.hpp"
#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "graphics/BuildingFoundationVisual.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

using namespace app_detail;

namespace {

constexpr bool DrawModularSnapDiagnostics = false;

float placementEntranceProgress(double seconds) {
    return std::clamp(
        static_cast<float>(seconds / 0.34),
        0.0F, 1.0F);
}

float easeOutBack(float progress) {
    constexpr float Overshoot = 1.35F;
    const float shifted = progress - 1.0F;
    return 1.0F +
        (Overshoot + 1.0F) * shifted * shifted * shifted +
        Overshoot * shifted * shifted;
}

void drawTerrainTacticalRing(
    const TerrainHeightfield& terrain,
    Vector3 center, float radius, Color color) {
    if (radius <= 0.0F) return;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(radius * 4.0F)), 72, 144);
    const float bandWidth = std::clamp(
        radius * 0.035F, 0.72F, 1.18F);
    constexpr std::array<float, 7> BandOffsets{
        -1.0F, -0.72F, -0.46F, -0.25F,
        -0.11F, -0.035F, 0.0F};
    constexpr std::array<float, 7> BandOpacity{
        0.0F, 0.035F, 0.11F, 0.24F,
        0.40F, 0.54F, 0.60F};
    const auto vertex =
        [&terrain, center, radius, bandWidth](
            float angle, float offset) {
            const float ringRadius =
                radius + offset * bandWidth;
            const float x =
                center.x + std::cos(angle) * ringRadius;
            const float z =
                center.z + std::sin(angle) * ringRadius;
            return Vector3{
                x,
                static_cast<float>(terrain.getHeight(x, z)) + 0.085F,
                z};
        };
    const auto emit = [&color](Vector3 point, float opacity) {
        const auto alpha = static_cast<unsigned char>(std::lround(
            static_cast<float>(color.a) * opacity));
        rlColor4ub(color.r, color.g, color.b, alpha);
        rlVertex3f(point.x, point.y, point.z);
    };

    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int segment = 0; segment < segments; ++segment) {
        const float angle0 = 2.0F * PI *
            static_cast<float>(segment) /
            static_cast<float>(segments);
        const float angle1 = 2.0F * PI *
            static_cast<float>(segment + 1) /
            static_cast<float>(segments);
        for (std::size_t band = 0;
             band + 1U < BandOffsets.size(); ++band) {
            const Vector3 p00 = vertex(
                angle0, BandOffsets[band]);
            const Vector3 p01 = vertex(
                angle1, BandOffsets[band]);
            const Vector3 p10 = vertex(
                angle0, BandOffsets[band + 1U]);
            const Vector3 p11 = vertex(
                angle1, BandOffsets[band + 1U]);
            emit(p00, BandOpacity[band]);
            emit(p11, BandOpacity[band + 1U]);
            emit(p10, BandOpacity[band + 1U]);
            emit(p00, BandOpacity[band]);
            emit(p01, BandOpacity[band]);
            emit(p11, BandOpacity[band + 1U]);
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
}

} // namespace

void App::drawWorldOverlays(
    const SimulationSnapshot& snapshot,
    const WorldLighting& lighting) {
    renderer_->endWorldShader();
    renderer_->beginColorOnlyWorldEffects();
    wind_blower_drawing::draw(snapshot, gameplayCamera_, userSettings_.accessibility.reduceFlashes);
    decoy_beacon_drawing::draw(snapshot, gameplayCamera_, userSettings_.accessibility.reduceFlashes, simulation_.terrain());
    repair_station_drawing::draw(snapshot, gameplayCamera_, userSettings_.accessibility.reduceFlashes, simulation_.terrain());
    renderer_->endColorOnlyWorldEffects();
    const auto dragPreviewForCell = [this](BuildingType type, GridPosition position) -> const BuildingPreview* {
        if (!wallDragStart_ || placementDragType_ != type) return nullptr;
        const auto found = std::ranges::find(placementDragPreviews_, position, &BuildingPreview::gridPosition);
        return found == placementDragPreviews_.end() ? nullptr : &*found;
    };
    const auto dragPlacementSurface =
        [this, &dragPreviewForCell](
            BuildingType type,
            GridPosition position) {
            if (const auto* preview = dragPreviewForCell(type, position)) {
                return BuildingPlatformSurface{preview->baseHeight, preview->foundationBottomHeight, preview->platformStorey};
            }
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
        [this, &dragPreviewForCell](
            BuildingType type,
            GridPosition position) {
            if (const auto* preview = dragPreviewForCell(type, position)) return preview->placement;
            return placementDragSurface_
                       ? simulation_.previewPlacement(
                             type, position,
                             placementDragSurface_
                                 ->height)
                       : simulation_.previewPlacement(
                             type, position);
        };
    const bool hasModularPreview =
        foundationBuildMode_ &&
        (platformFramePreview_ || wallPreview_ ||
         rampPreview_ ||
         !modularAutomaticFoundationPreviews_.empty() ||
         !modularPlatformDragPreviews_.empty() ||
         !modularWallDragPreviews_.empty() ||
         !modularRampDragPreviews_.empty() ||
         foundationTerrainHit_);
    if (hasModularPreview) {
        const float entranceProgress = placementEntranceProgress(
            placementPreviewEntranceSeconds_);
        const float previewScale = modularDragPiece_
            ? 1.0F
            : std::clamp(easeOutBack(entranceProgress),
                         0.0F, 1.08F);
        if (previewScale > 0.01F) {
        const Vec3 pivot = modularPreviewVisualOrigin_.value_or(
            foundationTerrainHit_.value_or(Vec3{}));
        rlPushMatrix();
        rlTranslatef(static_cast<float>(pivot.x),
                     static_cast<float>(pivot.y),
                     static_cast<float>(pivot.z));
        rlScalef(previewScale, previewScale, previewScale);
        rlTranslatef(-static_cast<float>(pivot.x),
                     -static_cast<float>(pivot.y),
                     -static_cast<float>(pivot.z));
        WorldMaterialState material{};
        material.bakedAo = 0.88F;
        material.screenAoAmount = 0.0F;
        renderer_->beginWorldShader(lighting);
        renderer_->setWorldMaterial(material);
        std::optional<Vec3> automaticFoundationVisualOffset;
        if (!modularDragPiece_ &&
            modularPreviewVisualOrigin_) {
            std::optional<Vec3> targetOrigin;
            bool targetCanTranslate = false;
            const double cellSize =
                simulation_.terrain().config().cellSize;
            if (platformFramePreview_) {
                targetOrigin = {
                    (platformFramePreview_->anchor.x + 1.0) *
                        cellSize,
                    platformFramePreview_->floorHeight,
                    (platformFramePreview_->anchor.z + 1.0) *
                        cellSize,
                };
                targetCanTranslate =
                    platformFramePreview_->error !=
                    ModularPlacementError::Occupied;
            } else if (rampPreview_) {
                const bool alongZ =
                    rampPreview_->rotation == Rotation::Deg0 ||
                    rampPreview_->rotation == Rotation::Deg180;
                const int widthCells =
                    alongZ ? ModularRampWidthCells
                           : ModularRampRunCells;
                const int depthCells =
                    alongZ ? ModularRampRunCells
                           : ModularRampWidthCells;
                targetOrigin = {
                    (rampPreview_->anchor.x +
                     widthCells * 0.5) *
                        cellSize,
                    rampPreview_->bottomHeight,
                    (rampPreview_->anchor.z +
                     depthCells * 0.5) *
                        cellSize,
                };
                targetCanTranslate =
                    rampPreview_->error !=
                    ModularPlacementError::Occupied;
            }
            if (targetOrigin && targetCanTranslate) {
                automaticFoundationVisualOffset = {
                    modularPreviewVisualOrigin_->x -
                        targetOrigin->x,
                    modularPreviewVisualOrigin_->y -
                        targetOrigin->y,
                    modularPreviewVisualOrigin_->z -
                        targetOrigin->z,
                };
            }
        }
        for (const PlatformFramePlacement& foundation :
             modularAutomaticFoundationPreviews_) {
            modularBuildingRenderer_.drawWorld(
                {
                    {}, {}, {}, {},
                    simulation_.terrain()
                        .config().cellSize,
                    std::nullopt, {}, 1.0F,
                },
                {
                    .platformFrame =
                        &foundation,
                    .maximumWoodSupportLength =
                        simulation_.terrain()
                            .config().maxWoodSupportLength,
                    .visualOffset =
                        automaticFoundationVisualOffset
                            ? &*automaticFoundationVisualOffset
                            : nullptr,
                });
        }
        modularBuildingRenderer_.drawWorld(
            {
                {}, {}, {}, {},
                simulation_.terrain().config().cellSize,
                std::nullopt, {}, 1.0F,
            },
            {
                platformFramePreview_ &&
                        (!modularDragPiece_ ||
                         modularDragUsesAutomaticFoundation_)
                    ? &*platformFramePreview_
                    : nullptr,
                wallPreview_ &&
                        (!modularDragPiece_ ||
                         modularDragUsesAutomaticFoundation_)
                    ? &*wallPreview_
                    : nullptr,
                rampPreview_ &&
                        (!modularDragPiece_ ||
                         modularDragUsesAutomaticFoundation_)
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
                (!modularDragPiece_ ||
                 modularDragUsesAutomaticFoundation_) &&
                        modularPreviewVisualOrigin_
                    ? &*modularPreviewVisualOrigin_
                    : nullptr,
            });
        renderer_->endWorldShader();
        rlPopMatrix();
        }
    }
    if (!foundationBuildMode_ &&
        outgoingPreviewRemaining_ > 0.0 &&
        (outgoingPlatformPreview_ || outgoingWallPreview_ ||
         outgoingRampPreview_)) {
        const float scale = smoothstep(
            0.0F, 1.0F,
            static_cast<float>(outgoingPreviewRemaining_ / 0.20));
        if (scale > 0.01F) {
            const double cellSize =
                simulation_.terrain().config().cellSize;
            Vec3 pivot = outgoingModularPreviewOrigin_.value_or(
                Vec3{});
            if (!outgoingModularPreviewOrigin_) {
                if (outgoingPlatformPreview_) {
                    pivot = {
                        (outgoingPlatformPreview_->anchor.x + 1.0) *
                            cellSize,
                        outgoingPlatformPreview_->floorHeight,
                        (outgoingPlatformPreview_->anchor.z + 1.0) *
                            cellSize};
                } else if (outgoingWallPreview_) {
                    pivot = {
                        (outgoingWallPreview_->anchor.x + 0.5) *
                            cellSize,
                        outgoingWallPreview_->bottomHeight,
                        (outgoingWallPreview_->anchor.z + 0.5) *
                            cellSize};
                } else if (outgoingRampPreview_) {
                    pivot = {
                        (outgoingRampPreview_->anchor.x + 1.0) *
                            cellSize,
                        outgoingRampPreview_->bottomHeight,
                        (outgoingRampPreview_->anchor.z + 1.0) *
                            cellSize};
                }
            }
            rlPushMatrix();
            rlTranslatef(static_cast<float>(pivot.x),
                         static_cast<float>(pivot.y),
                         static_cast<float>(pivot.z));
            rlScalef(scale, scale, scale);
            rlTranslatef(-static_cast<float>(pivot.x),
                         -static_cast<float>(pivot.y),
                         -static_cast<float>(pivot.z));
            renderer_->beginWorldShader(lighting);
            WorldMaterialState material{};
            material.bakedAo = 0.88F;
            material.screenAoAmount = 0.0F;
            renderer_->setWorldMaterial(material);
            modularBuildingRenderer_.drawWorld(
                {{}, {}, {}, {}, cellSize, std::nullopt, {}, 1.0F},
                {
                    .platformFrame = outgoingPlatformPreview_
                        ? &*outgoingPlatformPreview_ : nullptr,
                    .wall = outgoingWallPreview_
                        ? &*outgoingWallPreview_ : nullptr,
                    .ramp = outgoingRampPreview_
                        ? &*outgoingRampPreview_ : nullptr,
                    .maximumWoodSupportLength =
                        simulation_.terrain().config()
                            .maxWoodSupportLength,
                    .rotationYaw = static_cast<float>(
                        placementRotationYaw_),
                    .visualOrigin = outgoingModularPreviewOrigin_
                        ? &*outgoingModularPreviewOrigin_ : nullptr,
                });
            renderer_->endWorldShader();
            rlPopMatrix();
        }
    }
    if (DrawModularSnapDiagnostics &&
        foundationBuildMode_ &&
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
    if (DrawModularSnapDiagnostics &&
        foundationBuildMode_ &&
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
    if ((snapshot.selectedBuilding || foundationBuildMode_) &&
        snapshot.coreBuildRadius > 0) {
        const auto core = std::ranges::find(
            snapshot.buildings, BuildingType::Core,
            &BuildingInstance::type);
        if (core != snapshot.buildings.end()) {
            const Vec3 center = buildingWorldPosition(*core);
            drawTerrainTacticalRing(
                simulation_.terrain(),
                {static_cast<float>(center.x), 0.0F,
                 static_cast<float>(center.z)},
                static_cast<float>(snapshot.coreBuildRadius),
                {255, 252, 244, 190});
        }
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
            {visualCenter.x, 0.0F, visualCenter.y},
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
            const std::uint8_t lineRotation =
                alignsWithPlacementLine(dragType) &&
                        cells.size() > 1U &&
                        placementDragAxis_
                    ? placementLineRotation(
                          *placementDragAxis_)
                    : snapshot.buildingPreview->rotation;
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
                    .rotation = lineRotation,
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
                     snapshot.crystals >= cost.crystals * count);
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
                    static_cast<float>(buildingRotationYaw(
                        cellPreview.type,
                        cellPreview.rotation)));
            }
        } else {
            drawPlacementFootprint(
                *snapshot.buildingPreview, visualCenter,
                static_cast<float>(placementRotationYaw_));
        }
        if (!wallDragStart_ &&
            !placementPreviewObstructed(
                snapshot.buildingPreview->placement.error) &&
            (isDirectionalDefense(snapshot.buildingPreview->type) || snapshot.buildingPreview->type == BuildingType::TeslaTower || snapshot.buildingPreview->type == BuildingType::RepairStation || snapshot.buildingPreview->type == BuildingType::DecoyBeacon)) {
            const BuildingType type =
                snapshot.buildingPreview->type;
            const bool artillery =
                type == BuildingType::Cannon ||
                type == BuildingType::Catapult || type == BuildingType::Mortar;
            const std::uint8_t previewLevel = (type == BuildingType::Flamethrower || type == BuildingType::MissileLauncher || type == BuildingType::TeslaTower || type == BuildingType::LaserTower || type == BuildingType::Mortar || type == BuildingType::RepairStation || type == BuildingType::DecoyBeacon || type == BuildingType::WindBlower)
                ? std::max<std::uint8_t>(1, snapshot.buildingBlueprintLevels[static_cast<std::size_t>(type)]) : 1;
            const float range = type == BuildingType::DecoyBeacon
                ? static_cast<float>(decoyBeaconRadius(previewLevel)) : type == BuildingType::RepairStation
                ? static_cast<float>(repairStationRadius(previewLevel))
                : (type == BuildingType::Flamethrower || type == BuildingType::MissileLauncher || type == BuildingType::TeslaTower || type == BuildingType::LaserTower || type == BuildingType::Mortar || type == BuildingType::WindBlower) && snapshot.buildingPreviewFinalAttackStats
                ? static_cast<float>(snapshot.buildingPreviewFinalAttackStats->range * ((type == BuildingType::Flamethrower || type == BuildingType::WindBlower) ? snapshot.buildingPreviewFinalAttackStats->size : 1.0))
                : artillery
                ? static_cast<float>(
                      CannonSystem::attackRange(type, 1))
                : static_cast<float>(
                      TowerSystem::attackRange(type, 1));
            renderer_->drawTacticalGroundSector(
                {visualCenter.x,
                 static_cast<float>(
                     snapshot.buildingPreview->baseHeight) + 0.085F,
                 visualCenter.y},
                range,
                static_cast<float>(placementRotationYaw_),
                (type == BuildingType::RepairStation || type == BuildingType::DecoyBeacon) ? 360.0F : static_cast<float>(defenseAttackArcDegrees(type, previewLevel)),
                {228, 239, 226, 92},
                type == BuildingType::MissileLauncher ? static_cast<float>(MissileLauncherMinimumRange)
                    : artillery
                    ? static_cast<float>(
                          CannonSystem::minimumRange(type, 1))
                    : 0.0F);
        }
    }
    // Attack coverage is immediate hover feedback. Opening the building
    // context card pins the same target, but must not be required to reveal
    // its firing sector or trap trigger radius.
    drawBuildingTacticalOverlay(*renderer_, snapshot);
    const bool outgoingPreview =
        actionMode_ != ActionMode::Buildings &&
        outgoingBuildingPreview_ &&
        outgoingPreviewRemaining_ > 0.0;
    std::optional<BuildingPreview> dragPreviewFallback;
    if (actionMode_ == ActionMode::Buildings &&
        wallDragStart_ && placementDragType_ &&
        (!snapshot.buildingPreview ||
         snapshot.buildingPreview->type != *placementDragType_)) {
        const GridPosition cell =
            wallDragEnd_.value_or(*wallDragStart_);
        const BuildingPlatformSurface surface =
            dragPlacementSurface(*placementDragType_, cell);
        dragPreviewFallback = BuildingPreview{
            .type = *placementDragType_,
            .gridPosition = cell,
            .rotation = 0,
            .placement = dragPlacement(*placementDragType_, cell),
            .baseHeight = surface.height,
            .platformStorey = surface.storey,
            .foundationBottomHeight =
                surface.foundationBottomHeight,
            .foundationPreview = dragPreviewForCell(*placementDragType_, cell)
                ? dragPreviewForCell(*placementDragType_, cell)->foundationPreview
                : simulation_.previewBuildingFoundation(*placementDragType_, cell,
                    placementDragSurface_ ? std::optional<double>{placementDragSurface_->height} : std::nullopt),
        };
    }
    const BuildingPreview* activePreview = outgoingPreview
        ? &*outgoingBuildingPreview_
        : actionMode_ == ActionMode::Buildings &&
                snapshot.buildingPreview &&
                (!placementDragType_ ||
                 snapshot.buildingPreview->type ==
                     *placementDragType_)
            ? &*snapshot.buildingPreview
            : dragPreviewFallback
                ? &*dragPreviewFallback
                : nullptr;
    if (activePreview) {
        const auto& preview = *activePreview;
        BuildingPreview visualPreview = preview;
        std::optional<std::uint8_t>
            automaticLineRotation;
        const Vec3 targetCenter = buildingWorldPosition(
            preview.type, preview.gridPosition);
        Vector2 modelCenter =
            (outgoingPreview ? outgoingPreviewCenter_
                             : placementPreviewCenter_).value_or(Vector2{
                static_cast<float>(targetCenter.x),
                static_cast<float>(targetCenter.z),
            });
        if (!outgoingPreview && wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            preview.type == *placementDragType_) {
            const auto cells =
                placementLine(
                    preview.type, *wallDragStart_,
                    *wallDragEnd_, placementDragAxis_);
            if (!cells.empty()) {
                if (alignsWithPlacementLine(preview.type) &&
                    cells.size() > 1U &&
                    placementDragAxis_) {
                    automaticLineRotation =
                        placementLineRotation(
                            *placementDragAxis_);
                    visualPreview.rotation =
                        *automaticLineRotation;
                }
                const Vec3 lineEnd =
                    buildingWorldPosition(
                        preview.type,
                        cells.back());
                modelCenter = {
                    static_cast<float>(lineEnd.x),
                    static_cast<float>(lineEnd.z),
                };
                visualPreview.gridPosition = cells.back();
                visualPreview.foundationPreview = dragPreviewForCell(preview.type, cells.back())
                    ? dragPreviewForCell(preview.type, cells.back())->foundationPreview
                    : simulation_.previewBuildingFoundation(preview.type, cells.back(),
                        placementDragSurface_ ? std::optional<double>{placementDragSurface_->height} : std::nullopt);
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
                if (const auto* staged = dragPreviewForCell(preview.type, cells.back())) visualPreview = *staged;
            }
        }
        const Vector2 visualCenter = repelInvalidPreview(
            modelCenter, visualPreview,
            snapshot.playerPosition);
        const float x = visualCenter.x;
        const float z = visualCenter.y;
        const float entranceProgress = outgoingPreview
            ? std::clamp(static_cast<float>(
                  outgoingPreviewRemaining_ / 0.20),
                  0.0F, 1.0F)
            : placementEntranceProgress(
                  placementPreviewEntranceSeconds_);
        const bool draggingLine =
            !outgoingPreview && wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            preview.type == *placementDragType_;
        const float entranceFade = draggingLine
            ? 1.0F
            : outgoingPreview
            ? smoothstep(0.0F, 1.0F, entranceProgress)
            : smoothstep(0.0F, 0.58F, entranceProgress);
        const float previewScale = draggingLine
            ? 1.0F
            : outgoingPreview
            ? entranceFade
            : std::clamp(easeOutBack(entranceProgress),
                         0.0F, 1.08F);
        const float previewLift = draggingLine
            ? 0.0F
            : -0.14F * (1.0F -
                        easeOutBack(entranceProgress));
        const float opacityPulse =
            0.975F + 0.025F * static_cast<float>(
                std::sin(snapshot.elapsedSeconds * 3.4));
        Color color =
            placementColor(
                visualPreview.placement.error, false);
        color.a = static_cast<unsigned char>(std::lround(
            118.0F * entranceFade));
        const float yaw = automaticLineRotation
            ? static_cast<float>(buildingRotationYaw(
                  visualPreview.type,
                  *automaticLineRotation))
            : static_cast<float>(placementRotationYaw_);
        WorldMaterialState previewMaterial{};
        previewMaterial.baseColor = {
            static_cast<float>(color.r) / 255.0F,
            static_cast<float>(color.g) / 255.0F,
            static_cast<float>(color.b) / 255.0F,
            static_cast<float>(color.a) / 255.0F,
        };
        previewMaterial.bakedAo = 0.85F;
        previewMaterial.screenAoAmount = 0.0F;
        // Transparent ghost pieces overlap heavily. Screen-space ink edges
        // turn those internal intersections into black scratches, so keep
        // outlines for real objects but exclude placement previews.
        previewMaterial.inkOutlineEligible = false;
        previewMaterial.ghostAmount = 1.0F;
        previewMaterial.ghostTint = {
            static_cast<float>(color.r) / 255.0F,
            static_cast<float>(color.g) / 255.0F,
            static_cast<float>(color.b) / 255.0F};
        previewMaterial.ghostOpacity =
            0.36F * entranceFade * opacityPulse;
        if (visualPreview.placement.valid()) {
            previewMaterial.baseColor = {
                0.72F, 1.0F, 0.82F,
                0.46F * entranceFade};
            previewMaterial.bakedAo = 1.0F;
            previewMaterial.ghostAmount = 1.0F;
            previewMaterial.ghostTint = {
                0.18F, 0.90F, 0.48F};
            previewMaterial.ghostOpacity =
                0.36F * entranceFade * opacityPulse;
        }
        const bool finalCellObstructed =
            placementPreviewObstructed(
                visualPreview.placement.error);
        // A blocked endpoint must not hide the valid cells already included
        // in the drag gesture.
        const bool drawPreviewModel =
            draggingLine || !finalCellObstructed;
        if (drawPreviewModel && previewScale > 0.01F) {
            rlDrawRenderBatchActive();
            BeginBlendMode(BLEND_ALPHA);
            rlDisableDepthMask();
            renderer_->beginWorldShader(lighting);
            renderer_->setWorldMaterial(
                previewMaterial);
        rlPushMatrix();
        rlTranslatef(x, static_cast<float>(
            visualPreview.baseHeight), z);
        rlScalef(previewScale, previewScale, previewScale);
        rlTranslatef(-x, -static_cast<float>(
            visualPreview.baseHeight), -z);
        rlTranslatef(0.0F, previewLift, 0.0F);
        const Vec3 visualTargetCenter =
            buildingWorldPosition(
                visualPreview.type,
                visualPreview.gridPosition);
        if (const auto foundation =
                !finalCellObstructed
                    ? makeBuildingFoundationPreviewVisual(visualPreview,
                          simulation_.terrain().config().cellSize)
                    : std::nullopt) {
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
                BuildingPreview cellPreview{
                    .type = preview.type, .gridPosition = cells[index], .rotation = preview.rotation,
                    .placement = dragPlacement(preview.type, cells[index]),
                    .baseHeight = surface.height, .platformStorey = surface.storey,
                    .foundationBottomHeight = surface.foundationBottomHeight,
                    .foundationPreview = dragPreviewForCell(preview.type, cells[index])
                        ? dragPreviewForCell(preview.type, cells[index])->foundationPreview
                        : simulation_.previewBuildingFoundation(preview.type, cells[index],
                            placementDragSurface_ ? std::optional<double>{placementDragSurface_->height} : std::nullopt),
                };
                const auto foundation = makeBuildingFoundationPreviewVisual(cellPreview,
                    simulation_.terrain().config().cellSize);
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
        renderer_->setWorldMaterial(previewMaterial);

        if (wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            *placementDragType_ == preview.type &&
            preview.type != BuildingType::Wall) {
            const auto cells = placementLine(
                preview.type, *wallDragStart_,
                *wallDragEnd_, placementDragAxis_);
            for (std::size_t index = 0;
                 index + 1U < cells.size(); ++index) {
                PlacementResult placement =
                    dragPlacement(
                        preview.type, cells[index]);
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
                cellMaterial.ghostTint = {
                    static_cast<float>(cellColor.r) / 255.0F,
                    static_cast<float>(cellColor.g) / 255.0F,
                    static_cast<float>(cellColor.b) / 255.0F,
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
                } else if (preview.type == BuildingType::WindBlower) {
                    static_cast<void>(renderer_->drawWindBlower(position,yaw,WHITE,1,yaw));
                } else if (preview.type == BuildingType::DecoyBeacon) {
                    static_cast<void>(renderer_->drawDecoyBeacon(position, yaw));
                } else if (preview.type == BuildingType::RepairStation) {
                    static_cast<void>(renderer_->drawRepairStation(position, yaw));
                } else if (preview.type == BuildingType::Mortar) {
                    static_cast<void>(renderer_->drawMortar(position, yaw, WHITE, 1, 0, {},
                        snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
                } else if (preview.type == BuildingType::LaserTower) {
                    static_cast<void>(renderer_->drawLaserTower(position, yaw, WHITE, 1, 0, {},
                        snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
                } else if (preview.type == BuildingType::MissileLauncher) {
                    static_cast<void>(renderer_->drawMissileLauncher(position, yaw, WHITE, 1, 0, {},
                        snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
                } else if (preview.type == BuildingType::Flamethrower) {
                    static_cast<void>(renderer_->drawFlamethrower(position, yaw, WHITE, 1, 0, {},
                        snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
                } else if (preview.type == BuildingType::GunTurret) {
                    static_cast<void>(renderer_->drawGunTurret(
                        position, yaw, yaw));
                } else if (preview.type == BuildingType::TeslaTower) {
                    static_cast<void>(renderer_->drawTeslaTower(position, yaw, WHITE, 1, 0, {},
                        snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
                } else if (preview.type == BuildingType::MissileLauncher) {
                    static_cast<void>(renderer_->drawMissileLauncher(position, yaw, WHITE, 1, 0, {},
                        snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
                } else if (preview.type == BuildingType::Flamethrower) {
                    static_cast<void>(renderer_->drawFlamethrower(position, yaw));
                } else if (preview.type == BuildingType::GunTurret) {
                    static_cast<void>(renderer_->drawGunTurret(
                        position, yaw, yaw));
                } else if (
                    preview.type == BuildingType::Cannon) {
                    static_cast<void>(
                        renderer_->drawCannon(
                            position, yaw, 0.0F));
                } else if (
                    preview.type == BuildingType::Catapult) {
                    static_cast<void>(renderer_->drawCatapult(
                        position, yaw, 0.0F));
                } else if (
                    preview.type == BuildingType::CrystalMine ||
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
                } else if (preview.type == BuildingType::Gate &&
                           renderer_->drawGate(
                               position, yaw, 0.0F)) {
                    // The imported closed gate preview was submitted.
                } else if (preview.type == BuildingType::Gate && (visualPreview.rotation % 2U) == 0U) {
                    DrawCube(
                        {position.x - 0.38F, 1.0F, position.z},
                        0.22F, 2.0F, 1.0F, WHITE);
                    DrawCube(
                        {position.x + 0.38F, 1.0F, position.z},
                        0.22F, 2.0F, 1.0F, WHITE);
                    DrawCube(
                        {position.x, 1.0F, position.z},
                        0.55F, 1.7F, 0.18F, WHITE);
                } else if (preview.type == BuildingType::Gate) {
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

        if (draggingLine && finalCellObstructed &&
            preview.type != BuildingType::Wall) {
            // Earlier visible cells were drawn above.
        } else if (preview.type == BuildingType::Core) {
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
        } else if (preview.type == BuildingType::WindBlower) {
            static_cast<void>(renderer_->drawWindBlower({x,0,z},yaw,WHITE,1,yaw));
        } else if (preview.type == BuildingType::DecoyBeacon) {
            static_cast<void>(renderer_->drawDecoyBeacon({x, 0.0F, z}, yaw));
        } else if (preview.type == BuildingType::RepairStation) {
            static_cast<void>(renderer_->drawRepairStation({x, 0.0F, z}, yaw));
        } else if (preview.type == BuildingType::Mortar) {
            static_cast<void>(renderer_->drawMortar({x, 0.0F, z}, yaw, WHITE, 1, 0, {},
                snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
        } else if (preview.type == BuildingType::LaserTower) {
            static_cast<void>(renderer_->drawLaserTower({x, 0.0F, z}, yaw, WHITE, 1, 0, {},
                snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
        } else if (preview.type == BuildingType::MissileLauncher) {
            static_cast<void>(renderer_->drawMissileLauncher({x, 0.0F, z}, yaw, WHITE, 1, 0, {},
                snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
        } else if (preview.type == BuildingType::Flamethrower) {
            static_cast<void>(renderer_->drawFlamethrower({x, 0.0F, z}, yaw, WHITE, 1, 0, {},
                snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
        } else if (preview.type == BuildingType::GunTurret) {
            if (!renderer_->drawGunTurret(
                    {x, 0.0F, z}, yaw, yaw)) {
                DrawCube({x, 0.55F, z}, 1.1F, 1.1F, 1.1F,
                         WHITE);
            }
        } else if (preview.type == BuildingType::Cannon) {
            if (!renderer_->drawCannon({x, 0.0F, z}, yaw, 0.0F)) {
                DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                         WHITE);
                DrawSphere({x, 1.35F, z}, 0.48F, WHITE);
                DrawCube({x, 1.45F, z - 0.75F}, 0.28F,
                         0.28F, 1.4F, WHITE);
            }
        } else if (preview.type == BuildingType::Catapult) {
            if (!renderer_->drawCatapult(
                    {x, 0.0F, z}, yaw, 0.0F)) {
                DrawCube({x, 0.45F, z}, 1.2F, 0.9F,
                         1.2F, WHITE);
            }
        } else if (preview.type == BuildingType::TeslaTower) {
            static_cast<void>(renderer_->drawTeslaTower({x, 0.0F, z}, yaw, WHITE, 1, 0, {},
                snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
        } else if (preview.type == BuildingType::MissileLauncher) {
            static_cast<void>(renderer_->drawMissileLauncher({x, 0.0F, z}, yaw, WHITE, 1, 0, {},
                snapshot.buildingBlueprintLevels[static_cast<std::size_t>(preview.type)]));
        } else if (preview.type == BuildingType::Flamethrower) {
            static_cast<void>(renderer_->drawFlamethrower({x, 0.0F, z}, yaw));
        } else if (preview.type == BuildingType::GunTurret) {
            if (!renderer_->drawGunTurret(
                    {x, 0.0F, z}, yaw, yaw)) {
                DrawCube({x, 0.55F, z}, 1.1F, 1.1F, 1.1F,
                         WHITE);
            }
        } else if (preview.type == BuildingType::Cannon) {
            if (!renderer_->drawCannon({x, 0.0F, z}, yaw, 0.0F)) {
                DrawCube({x, 0.6F, z}, 1.0F, 1.2F, 1.0F,
                         WHITE);
                DrawSphere({x, 1.35F, z}, 0.48F, WHITE);
                DrawCube({x, 1.45F, z - 0.75F}, 0.28F,
                         0.28F, 1.4F, WHITE);
            }
        } else if (preview.type == BuildingType::Catapult) {
            if (!renderer_->drawCatapult(
                    {x, 0.0F, z}, yaw, 0.0F)) {
                DrawCube({x, 0.45F, z}, 1.2F, 0.9F,
                         1.2F, WHITE);
            }
        } else if (
            preview.type == BuildingType::CrystalMine ||
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
                         snapshot.crystals >=
                             cost.crystals * count);
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
                    cellMaterial.ghostTint = {
                        static_cast<float>(cellColor.r) /
                            255.0F,
                        static_cast<float>(cellColor.g) /
                            255.0F,
                        static_cast<float>(cellColor.b) /
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
        } else if (preview.type == BuildingType::Gate &&
                   renderer_->drawGate(
                       {x, 0.0F, z}, yaw, 0.0F)) {
            // The imported closed gate preview was submitted.
        } else if (preview.type == BuildingType::Gate && (visualPreview.rotation % 2U) == 0U) {
            DrawCube({x - 0.38F, 1.0F, z}, 0.22F, 2.0F,
                     1.0F, WHITE);
            DrawCube({x + 0.38F, 1.0F, z}, 0.22F, 2.0F,
                     1.0F, WHITE);
            DrawCube({x, 1.0F, z}, 0.55F, 1.7F, 0.18F,
                     WHITE);
        } else if (preview.type == BuildingType::Gate) {
            DrawCube({x, 1.0F, z - 0.38F}, 1.0F, 2.0F,
                     0.22F, WHITE);
            DrawCube({x, 1.0F, z + 0.38F}, 1.0F, 2.0F,
                     0.22F, WHITE);
            DrawCube({x, 1.0F, z}, 0.18F, 1.7F, 0.55F,
                     WHITE);
        }
        rlPopMatrix();
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
