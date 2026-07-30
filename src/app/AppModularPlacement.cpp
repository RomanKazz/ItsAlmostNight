#include "app/App.hpp"

#include "buildings/RampPlacementDirection.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

struct RampEdgeTarget {
    EntityId frameId;
    Vec3 supportHit;
    Vec3 edgeMarker;
    GridCoord neighborAnchor;
    Rotation rotation;
};

RampEdgeTarget rampEdgeTarget(
    const PlatformFrameInstance& frame,
    const RampEdgeSocket& socket, double cellSize) {
    return RampEdgeTarget{
        .frameId = frame.id,
        .supportHit = {
            (frame.anchor.x + 0.5) * cellSize,
            frame.floorHeight,
            (frame.anchor.z + 0.5) * cellSize,
        },
        .edgeMarker = socket.position,
        .neighborAnchor = socket.neighborAnchor,
        .rotation = socket.rotation,
    };
}

Vec3 platformFrameCenter(
    const PlatformFrameInstance& frame,
    double cellSize) {
    return {
        (frame.anchor.x + 1.0) * cellSize,
        frame.floorHeight,
        (frame.anchor.z + 1.0) * cellSize,
    };
}

Vec3 wallCenter(
    const WallInstance& wall, double cellSize) {
    return {
        (wall.anchor.x + 0.5) * cellSize,
        wall.bottomHeight,
        (wall.anchor.z + 0.5) * cellSize,
    };
}

Vec3 rampCenter(
    const RampInstance& ramp, double cellSize) {
    const bool alongZ =
        ramp.rotation == Rotation::Deg0 ||
        ramp.rotation == Rotation::Deg180;
    const int widthCells =
        alongZ ? ModularRampWidthCells
               : ModularRampRunCells;
    const int depthCells =
        alongZ ? ModularRampRunCells
               : ModularRampWidthCells;
    return {
        (ramp.anchor.x + widthCells * 0.5) *
            cellSize,
        ramp.bottomHeight,
        (ramp.anchor.z + depthCells * 0.5) *
            cellSize,
    };
}

bool sameRampFootprint(
    const RampPlacement& placement,
    const RampInstance& ramp) {
    const bool placementAlongZ =
        placement.rotation == Rotation::Deg0 ||
        placement.rotation == Rotation::Deg180;
    const bool rampAlongZ =
        ramp.rotation == Rotation::Deg0 ||
        ramp.rotation == Rotation::Deg180;
    return placement.anchor.x == ramp.anchor.x &&
           placement.anchor.z == ramp.anchor.z &&
           placement.targetStorey ==
               ramp.targetStorey &&
           placementAlongZ == rampAlongZ;
}

GridCoord platformAnchorBeyondRampTop(
    const RampInstance& ramp) {
    return rampTopPlatformAnchor(
        ramp.anchor, ramp.rotation);
}

Vec3 rampTopEdgeCenter(
    const RampInstance& ramp, double cellSize) {
    const GridCoord anchor =
        platformAnchorBeyondRampTop(ramp);
    Vec3 center{
        (anchor.x + 1.0) * cellSize,
        ramp.topHeight,
        (anchor.z + 1.0) * cellSize,
    };
    const Vec3 outward =
        rampSocketOutwardDirection(ramp.rotation);
    center.x -= outward.x * cellSize;
    center.z -= outward.z * cellSize;
    return center;
}

double distanceSquaredFromAimRay(
    Vec3 point, Vec3 origin, Vec3 direction) {
    const Vec3 offset{
        point.x - origin.x,
        point.y - origin.y,
        point.z - origin.z,
    };
    const double forward =
        offset.x * direction.x +
        offset.y * direction.y +
        offset.z * direction.z;
    if (forward <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    const Vec3 rejection{
        offset.x - direction.x * forward,
        offset.y - direction.y * forward,
        offset.z - direction.z * forward,
    };
    return rejection.x * rejection.x +
           rejection.y * rejection.y +
           rejection.z * rejection.z;
}

Vec3 cellHit(
    GridCoord cell, const TerrainHeightfield& terrain,
    double cellSize) {
    const double x =
        (static_cast<double>(cell.x) + 0.5) * cellSize;
    const double z =
        (static_cast<double>(cell.z) + 0.5) * cellSize;
    return {x, terrain.getHeight(x, z), z};
}

bool isPlatformBuildPiece(ModularBuildPiece piece) {
    return piece == ModularBuildPiece::Foundation ||
           piece == ModularBuildPiece::FloorPlatform;
}

} // namespace

void App::clearModularPlacementDrag() {
    modularDragStart_.reset();
    modularDragEnd_.reset();
    modularDragStorey_.reset();
    modularDragTargetFloorHeight_.reset();
    modularDragPlaneHeight_.reset();
    modularDragRotation_.reset();
    modularDragPiece_.reset();
    modularDragAxis_.reset();
    modularDragCandidateEnd_.reset();
    modularDragCandidateFrames_ = 0;
    modularDragLookMovement_ = 0.0;
    modularDragExtended_ = false;
    modularDragHits_.clear();
    modularPlatformDragPreviews_.clear();
    modularWallDragPreviews_.clear();
    modularRampDragPreviews_.clear();
}

void App::selectModularBuildPiece(
    ModularBuildPiece piece) {
    modularBuildPiece_ = piece;
    clearModularPlacementDrag();
    platformFramePreview_.reset();
    wallPreview_.reset();
    rampPreview_.reset();
    foundationTerrainHit_.reset();
    modularSnapHit_.reset();
    modularSnapMarker_.reset();
    modularPreviewAnchor_.reset();
    rampSocketFrame_.reset();
    rampSocketRotation_.reset();
    rampSocketLostGraceRemaining_ = 0.0;
    rampSocketManualOverrideRemaining_ = 0.0;
}

void App::setFoundationBuildMode(bool enabled) {
    foundationBuildMode_ = enabled;
    platformFramePreview_.reset();
    wallPreview_.reset();
    rampPreview_.reset();
    foundationTerrainHit_.reset();
    clearModularPlacementDrag();
    modularSnapHit_.reset();
    modularSnapMarker_.reset();
    modularEdgeHoverFrame_.reset();
    modularEdgeExtensionAnchor_.reset();
    rampSocketFrame_.reset();
    rampSocketRotation_.reset();
    rampSocketLostGraceRemaining_ = 0.0;
    rampSocketManualOverrideRemaining_ = 0.0;
    modularPreviewAnchor_.reset();
    modularPreviewVisualOrigin_.reset();
    buildingRotationWheelAccumulator_ = 0.0;
    buildingRotationCooldownRemaining_ = 0.0;

    if (enabled) {
        pendingBuildingCancel_ = true;
        pendingBuildingSelection_.reset();
        pendingBuildingPlacement_.reset();
        wallDragStart_.reset();
        wallDragEnd_.reset();
        placementDragType_.reset();
        placementDragSurface_.reset();
        placementDragAxis_.reset();
        pendingWallPlacements_.clear();
        placementPreviewCenter_.reset();
        placementPreviewGrid_.reset();
        placementPreviewType_.reset();
        placementSnapPulseRemaining_ = 0.0;
        placementRotationYaw_ =
            static_cast<float>(
                static_cast<int>(modularRotation_)) *
            PI * 0.5F;
    }
}

void App::updateModularPlacementPreview(
    const SimulationSnapshot& snapshot) {
    const double cosPitch =
        std::cos(snapshot.playerPitch);
    const Vec3 lookDirection{
        std::sin(snapshot.playerYaw) * cosPitch,
        std::sin(snapshot.playerPitch),
        -std::cos(snapshot.playerYaw) * cosPitch,
    };
    const TerrainHeightfield& terrain =
        simulation_.terrain();
    const double cellSize = terrain.config().cellSize;

    modularEdgeHoverFrame_.reset();
    modularEdgeExtensionAnchor_.reset();

    if (modularBuildPiece_ == ModularBuildPiece::Ramp) {
        std::optional<RampEdgeTarget> edgeTarget;
        const PlatformFrameInstance* targetFrame =
            nullptr;
        std::optional<RampEdgeSocket>
            acquiredSocket;
        bool targetIsAimed = false;
        bool targetIsWithinRetentionMargin = false;
        bool targetWasSocketAcquired = false;
        const bool rampDragActive =
            modularDragPiece_ ==
                ModularBuildPiece::Ramp &&
            modularDragStorey_ &&
            modularDragPlaneHeight_;
        if (rampDragActive) {
            const auto floorAim =
                rampSocketAimOnFloor(
                    snapshot.playerPosition,
                    lookDirection,
                    *modularDragPlaneHeight_);
            if (floorAim) {
                const int supportStorey =
                    *modularDragStorey_ - 1;
                const double supportFloorHeight =
                    *modularDragPlaneHeight_;
                const auto frameAtAnchor =
                    [&snapshot, supportStorey,
                     supportFloorHeight](
                        GridCoord anchor)
                    -> const PlatformFrameInstance* {
                    const auto frame = std::find_if(
                        snapshot.platformFrames.begin(),
                        snapshot.platformFrames.end(),
                        [anchor, supportStorey,
                         supportFloorHeight](
                            const PlatformFrameInstance&
                                candidate) {
                            return candidate.anchor.x ==
                                       anchor.x &&
                                   candidate.anchor.z ==
                                       anchor.z &&
                                   candidate.storey ==
                                       supportStorey &&
                                   std::abs(
                                       candidate.floorHeight -
                                       supportFloorHeight) <=
                                       1e-6;
                        });
                    return frame !=
                                   snapshot.platformFrames.end()
                               ? &*frame
                               : nullptr;
                };
                targetFrame = frameAtAnchor(
                    rampSupportAnchorAtAim(
                        *floorAim, lookDirection, cellSize));
                if (!targetFrame) {
                    targetFrame = frameAtAnchor(GridCoord{
                        snapPlatformFrameAxis(
                            static_cast<int>(std::floor(
                                floorAim->x / cellSize))),
                        0,
                        snapPlatformFrameAxis(
                            static_cast<int>(std::floor(
                                floorAim->z / cellSize))),
                    });
                }
                targetIsAimed = targetFrame != nullptr;
            }
        }
        if (!targetFrame && !rampDragActive &&
            snapshot.aimedModularBuilding) {
            const EntityId aimed =
                *snapshot.aimedModularBuilding;
            const auto frame = std::find_if(
                snapshot.platformFrames.begin(),
                snapshot.platformFrames.end(),
                [aimed](
                    const PlatformFrameInstance& candidate) {
                    return candidate.id == aimed;
                });
            if (frame != snapshot.platformFrames.end()) {
                targetFrame = &*frame;
                const auto floorAim =
                    rampSocketAimOnFloor(
                        snapshot.playerPosition,
                        lookDirection,
                        frame->floorHeight);
                if (floorAim) {
                    const GridCoord shiftedAnchor =
                        rampSupportAnchorAtAim(
                            *floorAim,
                            lookDirection,
                            cellSize);
                    const int targetStorey =
                        frame->storey;
                    const double targetFloorHeight =
                        frame->floorHeight;
                    const auto shiftedFrame =
                        std::find_if(
                            snapshot.platformFrames.begin(),
                            snapshot.platformFrames.end(),
                            [shiftedAnchor,
                             targetStorey,
                             targetFloorHeight](
                                const PlatformFrameInstance&
                                    candidate) {
                                return candidate.anchor.x ==
                                           shiftedAnchor.x &&
                                       candidate.anchor.z ==
                                           shiftedAnchor.z &&
                                       candidate.storey ==
                                           targetStorey &&
                                       std::abs(
                                           candidate.floorHeight -
                                           targetFloorHeight) <=
                                           1e-6;
                            });
                    if (shiftedFrame !=
                        snapshot.platformFrames.end()) {
                        targetFrame = &*shiftedFrame;
                    }
                }
                targetIsAimed = true;
            }
        }
        if (!targetFrame && !rampDragActive &&
            rampSocketFrame_) {
            const auto retained = std::find_if(
                snapshot.platformFrames.begin(),
                snapshot.platformFrames.end(),
                [this](
                    const PlatformFrameInstance& candidate) {
                    return candidate.id ==
                           *rampSocketFrame_;
                });
            if (retained != snapshot.platformFrames.end()) {
                if (rampSocketRotation_) {
                    const auto sockets =
                        platformRampEdgeSockets(
                            retained->anchor,
                            retained->floorHeight,
                            cellSize);
                    const auto previous = std::find_if(
                        sockets.begin(), sockets.end(),
                        [this](const RampEdgeSocket& socket) {
                            return socket.rotation ==
                                   *rampSocketRotation_;
                        });
                    const auto floorAim =
                        rampSocketAimOnFloor(
                            snapshot.playerPosition,
                            lookDirection,
                            retained->floorHeight);
                    targetIsWithinRetentionMargin =
                        previous != sockets.end() &&
                        ((floorAim &&
                          rampSocketContainsFloorAim(
                              *previous, *floorAim,
                              cellSize)) ||
                         (!floorAim &&
                          rampSocketAimScore(
                              *previous,
                              snapshot.playerPosition,
                              lookDirection) <=
                              RampSocketRetentionAimScore));
                }
                if (targetIsWithinRetentionMargin ||
                    rampSocketLostGraceRemaining_ > 0.0) {
                    targetFrame = &*retained;
                }
            }
        }
        if (!targetFrame && !rampDragActive) {
            double bestScore =
                RampSocketAcquisitionAimScore;
            for (const PlatformFrameInstance& frame :
                 snapshot.platformFrames) {
                const double centerX =
                    (frame.anchor.x + 1.0) *
                    cellSize;
                const double centerZ =
                    (frame.anchor.z + 1.0) *
                    cellSize;
                if (std::hypot(
                        centerX -
                            snapshot.playerPosition.x,
                        centerZ -
                            snapshot.playerPosition.z) >
                    terrain.config()
                        .buildPreviewDistance +
                        cellSize) {
                    continue;
                }
                for (const RampEdgeSocket& socket :
                     platformRampEdgeSockets(
                         frame.anchor,
                         frame.floorHeight,
                         cellSize)) {
                    const double score =
                        rampSocketAimScore(
                            socket,
                            snapshot.playerPosition,
                            lookDirection);
                    if (score > bestScore) {
                        continue;
                    }
                    bestScore = score;
                    targetFrame = &frame;
                    acquiredSocket = socket;
                    targetWasSocketAcquired = true;
                }
            }
        }
        if (targetFrame) {
            const bool changedFrame =
                !rampSocketFrame_ ||
                *rampSocketFrame_ != targetFrame->id;
            if (changedFrame) {
                rampSocketManualOverrideRemaining_ =
                    0.0;
            }
            const auto sockets =
                platformRampEdgeSockets(
                    targetFrame->anchor,
                    targetFrame->floorHeight,
                    cellSize);
            std::optional<RampEdgeSocket> chosen =
                acquiredSocket
                    ? acquiredSocket
                    : mostViewAlignedRampEdgeSocket(
                          targetFrame->anchor,
                          targetFrame->floorHeight,
                          cellSize,
                          lookDirection);
            if (!changedFrame &&
                rampSocketRotation_) {
                const auto previous = std::find_if(
                    sockets.begin(), sockets.end(),
                    [this](
                        const RampEdgeSocket& socket) {
                        return socket.rotation ==
                               *rampSocketRotation_;
                    });
                if (previous != sockets.end()) {
                    const auto floorAim =
                        rampSocketAimOnFloor(
                            snapshot.playerPosition,
                            lookDirection,
                            targetFrame->floorHeight);
                    const bool previousContainsFloorAim =
                        floorAim &&
                        rampSocketContainsFloorAim(
                            *previous, *floorAim,
                            cellSize);
                    const double previousAlignment =
                        rampSocketViewAlignment(
                            *previous,
                            lookDirection);
                    const double chosenAlignment =
                        chosen
                            ? rampSocketViewAlignment(
                                  *chosen,
                                  lookDirection)
                            : -1.0;
                    if (!targetIsAimed ||
                        rampSocketManualOverrideRemaining_ >
                            0.0 ||
                        previousContainsFloorAim ||
                        (!floorAim &&
                         previousAlignment +
                                 RampSocketDirectionSwitchMargin >=
                             chosenAlignment)) {
                        chosen = *previous;
                    }
                }
            }
            if (chosen) {
                rampSocketFrame_ = targetFrame->id;
                rampSocketRotation_ =
                    chosen->rotation;
                if (targetIsAimed ||
                    targetIsWithinRetentionMargin ||
                    targetWasSocketAcquired) {
                    rampSocketLostGraceRemaining_ =
                        RampSocketLostGraceSeconds;
                }
                edgeTarget = rampEdgeTarget(
                    *targetFrame, *chosen, cellSize);
            }
        }
        if (!edgeTarget &&
            modularDragPiece_ ==
                ModularBuildPiece::Ramp &&
            modularDragPlaneHeight_) {
            const auto dragPlaneHit =
                rampSocketAimOnFloor(
                    snapshot.playerPosition,
                    lookDirection,
                    *modularDragPlaneHeight_);
            if (dragPlaneHit) {
                modularDragEnd_ =
                    rampSupportAnchorAtAim(
                        *dragPlaneHit,
                        lookDirection, cellSize);
                foundationTerrainHit_ =
                    *dragPlaneHit;
                modularSnapHit_ = *dragPlaneHit;
                modularSnapMarker_ =
                    *dragPlaneHit;
                rampPreview_.reset();
                platformFramePreview_.reset();
                wallPreview_.reset();
                rebuildModularPlacementLine();
                return;
            }
        }
        if (!edgeTarget) {
            rampSocketFrame_.reset();
            rampSocketRotation_.reset();
            rampSocketLostGraceRemaining_ = 0.0;
            rampSocketManualOverrideRemaining_ = 0.0;
            platformFramePreview_.reset();
            wallPreview_.reset();
            rampPreview_.reset();
            foundationTerrainHit_.reset();
            modularSnapHit_.reset();
            modularSnapMarker_.reset();
            modularPreviewAnchor_.reset();
            modularPreviewVisualOrigin_.reset();
            return;
        }

        modularRotation_ = edgeTarget->rotation;
        modularEdgeHoverFrame_ = edgeTarget->frameId;
        modularEdgeExtensionAnchor_ =
            edgeTarget->neighborAnchor;
        foundationTerrainHit_ =
            edgeTarget->supportHit;
        modularSnapHit_ = edgeTarget->supportHit;
        modularSnapMarker_ = edgeTarget->edgeMarker;
        rampPreview_ = simulation_.previewRamp(
            edgeTarget->supportHit,
            edgeTarget->rotation);
        if (rampPreview_ &&
            std::any_of(
                snapshot.ramps.begin(),
                snapshot.ramps.end(),
                [this](const RampInstance& ramp) {
                    return sameRampFootprint(
                        *rampPreview_, ramp);
                })) {
            rampPreview_.reset();
            foundationTerrainHit_.reset();
            modularSnapHit_.reset();
            modularSnapMarker_.reset();
            modularPreviewAnchor_.reset();
            modularPreviewVisualOrigin_.reset();
            return;
        }
        platformFramePreview_.reset();
        wallPreview_.reset();
        if (rampPreview_) {
            if (!modularPreviewAnchor_ ||
                *modularPreviewAnchor_ !=
                    rampPreview_->anchor) {
                placementSnapPulseRemaining_ = 0.18;
            }
            modularPreviewAnchor_ =
                rampPreview_->anchor;
        }
        if (modularDragPiece_ ==
                ModularBuildPiece::Ramp) {
            modularDragEnd_ = GridCoord{
                static_cast<int>(std::floor(
                    edgeTarget->supportHit.x /
                    cellSize)),
                0,
                static_cast<int>(std::floor(
                    edgeTarget->supportHit.z /
                    cellSize)),
            };
            modularDragEnd_->x =
                snapPlatformFrameAxis(
                    modularDragEnd_->x);
            modularDragEnd_->z =
                snapPlatformFrameAxis(
                    modularDragEnd_->z);
            rebuildModularPlacementLine();
        }
        return;
    }

    const double terrainRayDistance = std::max(
        12.0,
        modularStoreyHeight(terrain.config()) *
                static_cast<double>(
                    terrain.config().maxStoreys) +
            12.0);
    auto rawHit = simulation_.terrain().raycast(
        snapshot.playerPosition, lookDirection,
        terrainRayDistance);
    if (modularDragPiece_ &&
        modularDragPlaneHeight_) {
        // Keep the drag axis on the floor where it began. A
        // terrain hit below an elevated floor otherwise skews
        // the X/Z endpoint and can flip the dominant line axis.
        if (const auto dragPlaneHit =
                rampSocketAimOnFloor(
                    snapshot.playerPosition,
                    lookDirection,
                    *modularDragPlaneHeight_)) {
            rawHit = *dragPlaneHit;
        }
    }

    if (rawHit &&
        isPlatformBuildPiece(
            modularBuildPiece_) &&
        modularDragPiece_ ==
            modularBuildPiece_ &&
        modularDragStart_ &&
        modularDragPlaneHeight_) {
        // Once a platform drag has begun, use its fixed storey
        // grid directly. The general valid-cell magnet may choose
        // opposite sides of an exact grid edge on press/release,
        // turning a stationary click into a two-platform drag.
        GridCoord dragEnd{
            snapPlatformFrameAxis(
                static_cast<int>(std::floor(
                    rawHit->x / cellSize))),
            0,
            snapPlatformFrameAxis(
                static_cast<int>(std::floor(
                    rawHit->z / cellSize))),
        };
        if (dragEnd.x != modularDragStart_->x ||
            dragEnd.z != modularDragStart_->z) {
            const double startCenterX =
                (modularDragStart_->x + 1.0) *
                cellSize;
            const double startCenterZ =
                (modularDragStart_->z + 1.0) *
                cellSize;
            const double endCenterX =
                (dragEnd.x + 1.0) * cellSize;
            const double endCenterZ =
                (dragEnd.z + 1.0) * cellSize;
            const double distanceToStart =
                std::hypot(
                    rawHit->x - startCenterX,
                    rawHit->z - startCenterZ);
            const double distanceToEnd =
                std::hypot(
                    rawHit->x - endCenterX,
                    rawHit->z - endCenterZ);
            constexpr double
                PlatformDragSwitchHysteresisCells =
                    0.28;
            if (distanceToEnd +
                    PlatformDragSwitchHysteresisCells *
                        cellSize >=
                distanceToStart) {
                dragEnd = *modularDragStart_;
            }
        }
        if (dragEnd.x == modularDragStart_->x &&
            dragEnd.z == modularDragStart_->z) {
            modularDragCandidateEnd_.reset();
            modularDragCandidateFrames_ = 0;
        } else {
            constexpr double
                PlatformDragMinimumLookPixels = 3.0;
            if (modularDragLookMovement_ <
                PlatformDragMinimumLookPixels) {
                dragEnd = *modularDragStart_;
                modularDragCandidateEnd_.reset();
                modularDragCandidateFrames_ = 0;
            }
        }
        modularDragAxis_ =
            stabilizePlacementLineAxis(
                dragEnd.x - modularDragStart_->x,
                dragEnd.z - modularDragStart_->z,
                modularDragAxis_,
                PlatformFrameWidthCells);
        if (modularDragAxis_ ==
            PlacementLineAxis::X) {
            dragEnd.z = modularDragStart_->z;
        } else if (
            modularDragAxis_ ==
            PlacementLineAxis::Z) {
            dragEnd.x = modularDragStart_->x;
        }
        // Debounce only click-to-drag transition. Requiring
        // confirmation after every endpoint change makes the
        // preview collapse to its first cell while moving.
        if ((dragEnd.x != modularDragStart_->x ||
             dragEnd.z != modularDragStart_->z) &&
            !modularDragExtended_) {
            if (modularDragCandidateEnd_ &&
                modularDragCandidateEnd_->x ==
                    dragEnd.x &&
                modularDragCandidateEnd_->z ==
                    dragEnd.z) {
                ++modularDragCandidateFrames_;
            } else {
                modularDragCandidateEnd_ = dragEnd;
                modularDragCandidateFrames_ = 1;
            }
            constexpr int
                PlatformDragConfirmationFrames = 2;
            if (modularDragCandidateFrames_ <
                PlatformDragConfirmationFrames) {
                dragEnd = *modularDragStart_;
            } else {
                modularDragExtended_ = true;
            }
        } else if (modularDragExtended_) {
            modularDragCandidateEnd_ = dragEnd;
            modularDragCandidateFrames_ = 0;
        }
        modularDragEnd_ = dragEnd;
        const Vec3 snappedHit{
            (modularDragEnd_->x + 1.0) * cellSize,
            *modularDragPlaneHeight_,
            (modularDragEnd_->z + 1.0) * cellSize,
        };
        foundationTerrainHit_ = snappedHit;
        modularSnapHit_ = snappedHit;
        modularSnapMarker_ = snappedHit;
        platformFramePreview_.reset();
        wallPreview_.reset();
        rampPreview_.reset();
        rebuildModularPlacementLine();
        return;
    }

    if (modularBuildPiece_ ==
            ModularBuildPiece::FloorPlatform &&
        snapshot.aimedModularBuilding) {
        const EntityId aimed =
            *snapshot.aimedModularBuilding;
        const auto frame = std::find_if(
            snapshot.platformFrames.begin(),
            snapshot.platformFrames.end(),
            [aimed](
                const PlatformFrameInstance& candidate) {
                return candidate.id == aimed;
                });
        if (frame != snapshot.platformFrames.end()) {
            const bool usePlatformEdges =
                frame->storey > 0;
            if (usePlatformEdges) {
                modularEdgeHoverFrame_ = frame->id;
            }
            std::optional<GridCoord> targetAnchor;
            int targetStorey = frame->storey + 1;
            double targetFloorHeight =
                frame->floorHeight +
                modularStoreyHeight(terrain.config());
            if (usePlatformEdges &&
                std::abs(lookDirection.y) > 1e-5) {
                const double distance =
                    (frame->floorHeight -
                     snapshot.playerPosition.y) /
                    lookDirection.y;
                if (distance > 0.0) {
                    const double hitX =
                        snapshot.playerPosition.x +
                        lookDirection.x * distance;
                    const double hitZ =
                        snapshot.playerPosition.z +
                        lookDirection.z * distance;
                    const double centerX =
                        (frame->anchor.x + 1.0) *
                        cellSize;
                    const double centerZ =
                        (frame->anchor.z + 1.0) *
                        cellSize;
                    const double localX =
                        hitX - centerX;
                    const double localZ =
                        hitZ - centerZ;
                    constexpr double EdgeThreshold =
                        0.42;
                    if (std::max(
                            std::abs(localX),
                            std::abs(localZ)) >=
                        cellSize * EdgeThreshold) {
                        GridCoord extension =
                            frame->anchor;
                        if (std::abs(localX) >=
                            std::abs(localZ)) {
                            extension.x +=
                                localX >= 0.0
                                    ? PlatformFrameWidthCells
                                    : -PlatformFrameWidthCells;
                        } else {
                            extension.z +=
                                localZ >= 0.0
                                    ? PlatformFrameWidthCells
                                    : -PlatformFrameWidthCells;
                        }
                        modularEdgeExtensionAnchor_ =
                            extension;
                        targetAnchor = extension;
                        targetStorey = frame->storey;
                        targetFloorHeight =
                            frame->floorHeight;
                    }
                }
            }
            if (!targetAnchor) {
                targetAnchor = frame->anchor;
            }
            platformFramePreview_ =
                simulation_.previewFloorPlatform(
                    *targetAnchor, targetStorey,
                    targetFloorHeight);
            const Vec3 targetHit{
                (targetAnchor->x + 1.0) * cellSize,
                targetFloorHeight,
                (targetAnchor->z + 1.0) * cellSize,
            };
            foundationTerrainHit_ = targetHit;
            modularSnapHit_ = targetHit;
            modularSnapMarker_ = targetHit;
            if (!modularPreviewAnchor_ ||
                *modularPreviewAnchor_ != *targetAnchor) {
                placementSnapPulseRemaining_ = 0.18;
            }
            modularPreviewAnchor_ = *targetAnchor;
            wallPreview_.reset();
            rampPreview_.reset();
            return;
        }

        const auto ramp = std::find_if(
            snapshot.ramps.begin(),
            snapshot.ramps.end(),
            [aimed](const RampInstance& candidate) {
                return candidate.id == aimed;
            });
        if (ramp != snapshot.ramps.end()) {
            const Vec3 edgeCenter =
                rampTopEdgeCenter(*ramp, cellSize);
            constexpr double RampEdgeAimRadiusCells =
                0.9;
            if (distanceSquaredFromAimRay(
                    edgeCenter,
                    snapshot.playerPosition,
                    lookDirection) <=
                cellSize * cellSize *
                    RampEdgeAimRadiusCells *
                    RampEdgeAimRadiusCells) {
                const GridCoord targetAnchor =
                    platformAnchorBeyondRampTop(*ramp);
                modularEdgeExtensionAnchor_ =
                    targetAnchor;
                platformFramePreview_ =
                    simulation_.previewFloorPlatform(
                        targetAnchor,
                        ramp->targetStorey,
                        ramp->topHeight);
                const Vec3 targetHit{
                    (targetAnchor.x + 1.0) *
                        cellSize,
                    ramp->topHeight,
                    (targetAnchor.z + 1.0) *
                        cellSize,
                };
                foundationTerrainHit_ = targetHit;
                modularSnapHit_ = targetHit;
                modularSnapMarker_ = targetHit;
                if (!modularPreviewAnchor_ ||
                    *modularPreviewAnchor_ !=
                        targetAnchor) {
                    placementSnapPulseRemaining_ = 0.18;
                }
                modularPreviewAnchor_ = targetAnchor;
                wallPreview_.reset();
                rampPreview_.reset();
                return;
            }
        }
    }

    if (modularBuildPiece_ ==
        ModularBuildPiece::FloorPlatform) {
        platformFramePreview_.reset();
        wallPreview_.reset();
        rampPreview_.reset();
        foundationTerrainHit_.reset();
        modularSnapHit_.reset();
        modularSnapMarker_.reset();
        modularPreviewAnchor_.reset();
        return;
    }

    if (!rawHit) {
        platformFramePreview_.reset();
        wallPreview_.reset();
        rampPreview_.reset();
        foundationTerrainHit_.reset();
        modularPreviewAnchor_.reset();
        if (!IsKeyDown(KEY_LEFT_SHIFT)) {
            modularSnapHit_.reset();
            modularSnapMarker_.reset();
        }
        return;
    }

    const auto evaluate =
        [this](Vec3 hit) {
            switch (modularBuildPiece_) {
            case ModularBuildPiece::Foundation: {
                const auto placement =
                    simulation_.previewFoundation(hit);
                return std::pair{
                    placement.anchor,
                    placement.valid()};
            }
            case ModularBuildPiece::FloorPlatform:
                return std::pair{GridCoord{}, false};
            case ModularBuildPiece::Wall: {
                const auto placement =
                    simulation_.previewWall(
                        hit, modularRotation_);
                return std::pair{
                    placement.anchor,
                    placement.valid()};
            }
            case ModularBuildPiece::Ramp: {
                const auto placement =
                    simulation_.previewRamp(
                        hit, modularRotation_);
                return std::pair{
                    placement.anchor,
                    placement.valid()};
            }
            }
            return std::pair{GridCoord{}, false};
        };

    Vec3 chosenHit = *rawHit;
    auto [chosenAnchor, chosenValid] =
        evaluate(chosenHit);

    const bool lockCurrent =
        IsKeyDown(KEY_LEFT_SHIFT) &&
        modularSnapHit_.has_value();
    if (lockCurrent) {
        chosenHit = *modularSnapHit_;
        const auto locked = evaluate(chosenHit);
        chosenAnchor = locked.first;
        chosenValid = locked.second;
    } else {
        bool keptPrevious = false;
        if (modularSnapHit_) {
            const double deltaX =
                rawHit->x - modularSnapHit_->x;
            const double deltaZ =
                rawHit->z - modularSnapHit_->z;
            const auto previous =
                evaluate(*modularSnapHit_);
            constexpr double HysteresisCells = 0.62;
            if (previous.second &&
                deltaX * deltaX + deltaZ * deltaZ <=
                    cellSize * cellSize *
                        HysteresisCells *
                        HysteresisCells) {
                chosenHit = *modularSnapHit_;
                chosenAnchor = previous.first;
                chosenValid = true;
                keptPrevious = true;
            }
        }
        if (!keptPrevious) {
            const int rawX = static_cast<int>(
                std::floor(rawHit->x / cellSize));
            const int rawZ = static_cast<int>(
                std::floor(rawHit->z / cellSize));
            double bestDistance =
                std::numeric_limits<double>::max();
            std::optional<Vec3> bestHit;
            GridCoord bestAnchor{};
            constexpr int SearchRadiusCells = 2;
            constexpr double MagnetRadiusCells = 1.15;
            for (int offsetZ = -SearchRadiusCells;
                 offsetZ <= SearchRadiusCells; ++offsetZ) {
                for (int offsetX = -SearchRadiusCells;
                     offsetX <= SearchRadiusCells; ++offsetX) {
                    const Vec3 candidate = cellHit(
                        {rawX + offsetX, 0,
                         rawZ + offsetZ},
                        terrain, cellSize);
                    const double deltaX =
                        candidate.x - rawHit->x;
                    const double deltaZ =
                        candidate.z - rawHit->z;
                    const double distance =
                        deltaX * deltaX +
                        deltaZ * deltaZ;
                    if (distance >
                            cellSize * cellSize *
                                MagnetRadiusCells *
                                MagnetRadiusCells ||
                        distance >= bestDistance) {
                        continue;
                    }
                    const auto candidateResult =
                        evaluate(candidate);
                    if (!candidateResult.second) {
                        continue;
                    }
                    bestDistance = distance;
                    bestHit = candidate;
                    bestAnchor = candidateResult.first;
                }
            }
            if (bestHit) {
                chosenHit = *bestHit;
                chosenAnchor = bestAnchor;
                chosenValid = true;
            }
        }
        if (chosenValid) {
            modularSnapHit_ = chosenHit;
            modularSnapMarker_ = chosenHit;
        } else {
            modularSnapHit_.reset();
            modularSnapMarker_.reset();
        }
    }
    if (!modularPreviewAnchor_ ||
        *modularPreviewAnchor_ != chosenAnchor) {
        placementSnapPulseRemaining_ = 0.18;
    }
    modularPreviewAnchor_ = chosenAnchor;
    foundationTerrainHit_ = chosenHit;

    switch (modularBuildPiece_) {
    case ModularBuildPiece::Foundation:
        platformFramePreview_ =
            simulation_.previewFoundation(
                chosenHit);
        wallPreview_.reset();
        rampPreview_.reset();
        break;
    case ModularBuildPiece::FloorPlatform:
        break;
    case ModularBuildPiece::Wall:
        wallPreview_ = simulation_.previewWall(
            chosenHit, modularRotation_);
        platformFramePreview_.reset();
        rampPreview_.reset();
        break;
    case ModularBuildPiece::Ramp:
        rampPreview_ = simulation_.previewRamp(
            chosenHit, modularRotation_);
        platformFramePreview_.reset();
        wallPreview_.reset();
        break;
    }

    if (modularDragPiece_) {
        if (*modularDragPiece_ ==
                ModularBuildPiece::Wall &&
            wallPreview_) {
            modularDragEnd_ = wallPreview_->anchor;
        }
        rebuildModularPlacementLine();
    }
}

void App::beginModularPlacementDrag() {
    if (isPlatformBuildPiece(modularBuildPiece_) &&
        platformFramePreview_ &&
        platformFramePreview_->valid()) {
        modularDragStart_ =
            platformFramePreview_->anchor;
        modularDragStorey_ =
            platformFramePreview_->storey;
        modularDragPlaneHeight_ =
            platformFramePreview_->floorHeight;
        if (modularBuildPiece_ ==
            ModularBuildPiece::FloorPlatform) {
            modularDragTargetFloorHeight_ =
                platformFramePreview_->floorHeight;
        }
    } else if (
        modularBuildPiece_ == ModularBuildPiece::Wall &&
        wallPreview_ && wallPreview_->valid()) {
        modularDragStart_ = wallPreview_->anchor;
        modularDragStorey_ = wallPreview_->storey;
        modularDragPlaneHeight_ =
            wallPreview_->bottomHeight;
    } else if (
        modularBuildPiece_ == ModularBuildPiece::Ramp &&
        rampPreview_ && rampPreview_->valid() &&
        foundationTerrainHit_) {
        const double cellSize =
            simulation_.terrain().config().cellSize;
        modularDragStart_ = GridCoord{
            static_cast<int>(std::floor(
                foundationTerrainHit_->x / cellSize)),
            0,
            static_cast<int>(std::floor(
                foundationTerrainHit_->z / cellSize)),
        };
        modularDragStart_->x =
            snapPlatformFrameAxis(
                modularDragStart_->x);
        modularDragStart_->z =
            snapPlatformFrameAxis(
                modularDragStart_->z);
        modularDragStorey_ =
            rampPreview_->targetStorey;
        modularDragPlaneHeight_ =
            rampPreview_->bottomHeight;
        modularDragRotation_ =
            rampPreview_->rotation;
    } else {
        return;
    }
    modularDragEnd_ = modularDragStart_;
    modularDragPiece_ = modularBuildPiece_;
    modularDragAxis_.reset();
    modularDragCandidateEnd_.reset();
    modularDragCandidateFrames_ = 0;
    modularDragLookMovement_ = 0.0;
    modularDragExtended_ = false;
    rebuildModularPlacementLine();
}

void App::rebuildModularPlacementLine() {
    modularDragHits_.clear();
    modularPlatformDragPreviews_.clear();
    modularWallDragPreviews_.clear();
    modularRampDragPreviews_.clear();
    if (!modularDragStart_ || !modularDragEnd_ ||
        !modularDragStorey_ || !modularDragPiece_) {
        return;
    }

    const int spacing =
        *modularDragPiece_ ==
                    ModularBuildPiece::Wall
            ? 1
            : PlatformFrameWidthCells;
    modularDragAxis_ = stabilizePlacementLineAxis(
        modularDragEnd_->x - modularDragStart_->x,
        modularDragEnd_->z - modularDragStart_->z,
        modularDragAxis_, spacing);
    const auto cells = ian::placementLine(
        *modularDragStart_, *modularDragEnd_,
        spacing, modularDragAxis_);
    const TerrainHeightfield& terrain =
        simulation_.terrain();
    const auto snapshot = simulation_.snapshot();
    const double cellSize = terrain.config().cellSize;
    modularDragHits_.reserve(cells.size());

    Rotation wallRotation = modularRotation_;
    if (*modularDragPiece_ ==
            ModularBuildPiece::Wall &&
        cells.size() > 1U && modularDragAxis_) {
        wallRotation =
            *modularDragAxis_ ==
                    PlacementLineAxis::X
                ? Rotation::Deg0
                : Rotation::Deg90;
    }
    for (const GridCoord cell : cells) {
        const Vec3 hit =
            cellHit(cell, terrain, cellSize);
        if (*modularDragPiece_ ==
            ModularBuildPiece::Foundation) {
            PlatformFramePlacement preview =
                simulation_.previewFoundation(hit);
            if (preview.storey !=
                *modularDragStorey_) {
                continue;
            }
            modularDragHits_.push_back(hit);
            modularPlatformDragPreviews_.push_back(
                preview);
        } else if (
            *modularDragPiece_ ==
                ModularBuildPiece::FloorPlatform &&
            modularDragTargetFloorHeight_) {
            PlatformFramePlacement preview =
                simulation_.previewFloorPlatform(
                    cell, *modularDragStorey_,
                    *modularDragTargetFloorHeight_);
            modularDragHits_.push_back(Vec3{
                (cell.x + 1.0) * cellSize,
                *modularDragTargetFloorHeight_,
                (cell.z + 1.0) * cellSize,
            });
            modularPlatformDragPreviews_.push_back(
                preview);
        } else if (
            *modularDragPiece_ ==
            ModularBuildPiece::Wall) {
            WallPlacement preview =
                simulation_.previewWall(
                    hit, wallRotation);
            if (preview.storey !=
                *modularDragStorey_) {
                continue;
            }
            modularDragHits_.push_back(hit);
            modularWallDragPreviews_.push_back(
                preview);
        } else if (
            *modularDragPiece_ ==
            ModularBuildPiece::Ramp) {
            const Rotation rotation =
                modularDragRotation_.value_or(
                    modularRotation_);
            RampPlacement preview =
                simulation_.previewRamp(
                    hit, rotation);
            if (preview.targetStorey !=
                    *modularDragStorey_ ||
                std::any_of(
                    snapshot.ramps.begin(),
                    snapshot.ramps.end(),
                    [&preview](
                        const RampInstance& ramp) {
                        return sameRampFootprint(
                            preview, ramp);
                    })) {
                continue;
            }
            modularDragHits_.push_back(hit);
            modularRampDragPreviews_.push_back(
                preview);
        }
    }
}

bool App::finishModularPlacementDrag() {
    if (!modularDragPiece_) {
        return false;
    }
    const double cellSize =
        simulation_.terrain().config().cellSize;
    constexpr double PlacementBounceDelay = 0.065;
    std::size_t placedOrdinal = 0;
    const auto bounceDelayFor =
        [bounceStep = PlacementBounceDelay](
            std::size_t ordinal) {
            return static_cast<double>(ordinal) *
                bounceStep;
        };
    bool placedAny = false;
    if (isPlatformBuildPiece(
            *modularDragPiece_)) {
        const std::size_t previewCount =
            modularDragExtended_
                ? std::min(
                      modularDragHits_.size(),
                      modularPlatformDragPreviews_
                          .size())
                : std::min<std::size_t>(
                      1U,
                      std::min(
                          modularDragHits_.size(),
                          modularPlatformDragPreviews_
                              .size()));
        for (std::size_t index = 0;
             index < previewCount;
             ++index) {
            if (!modularPlatformDragPreviews_[index]
                     .valid()) {
                continue;
            }
            const PlatformFramePlacement current =
                *modularDragPiece_ ==
                        ModularBuildPiece::Foundation
                    ? simulation_.previewFoundation(
                          modularDragHits_[index])
                    : simulation_.previewFloorPlatform(
                          modularPlatformDragPreviews_
                              [index]
                                  .anchor,
                          *modularDragStorey_,
                          *modularDragTargetFloorHeight_);
            if (!current.valid() ||
                !modularDragStorey_ ||
                current.storey !=
                    *modularDragStorey_) {
                continue;
            }
            const auto frame =
                *modularDragPiece_ ==
                        ModularBuildPiece::Foundation
                    ? simulation_.placeFoundation(
                          modularDragHits_[index])
                    : simulation_.placeFloorPlatform(
                          current.anchor,
                          current.storey,
                          current.floorHeight);
            if (!frame) {
                continue;
            }
            placedAny = true;
            const double startDelay =
                bounceDelayFor(placedOrdinal);
            const Vec3 center =
                platformFrameCenter(*frame, cellSize);
            addEffect(
                PresentationEffectType::BuildingPlaced,
                center, 0.7, 1.25F,
                std::nullopt, startDelay);
            ++placedOrdinal;
            if (frame->storey == 0) {
                grassClearAreas_.push_back({
                    .center = {
                        static_cast<float>(center.x),
                        static_cast<float>(center.z),
                    },
                    .innerRadius =
                        static_cast<float>(
                            cellSize * 1.35),
                    .amount = 0.0F,
                });
            }
        }
    } else if (
        *modularDragPiece_ ==
        ModularBuildPiece::Wall) {
        for (std::size_t index = 0;
             index < modularDragHits_.size() &&
             index < modularWallDragPreviews_.size();
             ++index) {
            const WallPlacement& preview =
                modularWallDragPreviews_[index];
            if (!preview.valid()) {
                continue;
            }
            const WallPlacement current =
                simulation_.previewWall(
                    modularDragHits_[index],
                    preview.rotation);
            if (!current.valid() ||
                !modularDragStorey_ ||
                current.storey !=
                    *modularDragStorey_) {
                continue;
            }
            const auto wall = simulation_.placeWall(
                modularDragHits_[index],
                preview.rotation);
            if (!wall) {
                continue;
            }
            placedAny = true;
            addEffect(
                PresentationEffectType::BuildingPlaced,
                wallCenter(*wall, cellSize),
                0.7, 0.78F, std::nullopt,
                bounceDelayFor(placedOrdinal));
            ++placedOrdinal;
        }
    } else if (
        *modularDragPiece_ ==
        ModularBuildPiece::Ramp) {
        for (std::size_t index = 0;
             index < modularRampDragPreviews_.size() &&
             index < modularDragHits_.size();
             ++index) {
            const RampPlacement& preview =
                modularRampDragPreviews_[index];
            if (!preview.valid()) {
                continue;
            }
            const RampPlacement current =
                simulation_.previewRamp(
                    modularDragHits_[index],
                    preview.rotation);
            if (!current.valid() ||
                !modularDragStorey_ ||
                current.targetStorey !=
                    *modularDragStorey_) {
                continue;
            }
            const auto ramp =
                simulation_.placeRamp(
                    modularDragHits_[index],
                    preview.rotation);
            if (!ramp) {
                continue;
            }
            placedAny = true;
            addEffect(
                PresentationEffectType::BuildingPlaced,
                rampCenter(*ramp, cellSize),
                0.7, 1.35F, std::nullopt,
                bounceDelayFor(placedOrdinal));
            ++placedOrdinal;
        }
    }
    clearModularPlacementDrag();
    modularSnapHit_.reset();
    modularSnapMarker_.reset();
    if (placedAny) {
        audio_.playUiConfirm();
    }
    return placedAny;
}

} // namespace ian
