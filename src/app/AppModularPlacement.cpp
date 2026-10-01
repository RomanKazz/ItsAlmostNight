#include "app/App.hpp"
#include "app/ModularPlacementGeometry.hpp"

#include "buildings/RampPlacementDirection.hpp"

#include <raylib.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ian {
using namespace modular_placement_detail;
namespace {

struct RampEdgeTarget {
    EntityId frameId;
    Vec3 supportHit;
    Vec3 edgeMarker;
    GridCoord neighborAnchor;
    Rotation rotation;
};

Rotation oppositeRotation(Rotation rotation) {
    return static_cast<Rotation>(
        (static_cast<int>(rotation) + 2) % 4);
}

RampEdgeTarget rampEdgeTarget(
    const PlatformFrameInstance& frame,
    const RampEdgeSocket& socket, double cellSize) {
    const auto sockets = platformRampEdgeSockets(
        frame.anchor, frame.floorHeight, cellSize);
    const RampEdgeSocket& lowEdge =
        sockets[static_cast<std::size_t>(
            oppositeRotation(socket.rotation))];
    return RampEdgeTarget{
        .frameId = frame.id,
        .supportHit = {
            (frame.anchor.x + 0.5) * cellSize,
            frame.floorHeight,
            (frame.anchor.z + 0.5) * cellSize,
        },
        .edgeMarker = lowEdge.position,
        .neighborAnchor = lowEdge.neighborAnchor,
        .rotation = socket.rotation,
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

Vec3 modularDragReferencePoint(
    GridCoord start, ModularBuildPiece piece,
    double height, double cellSize) {
    const double offset =
        piece == ModularBuildPiece::Wall ? 0.5 : 1.0;
    return {
        (start.x + offset) * cellSize,
        height,
        (start.z + offset) * cellSize,
    };
}

Vec3 stableElevatedDragAim(
    Vec3 planeAim, Vec3 viewer, Vec3 lookDirection,
    Vec3 dragReference, double maximumDistance) {
    double deltaX = planeAim.x - dragReference.x;
    double deltaZ = planeAim.z - dragReference.z;
    const double distance = std::hypot(deltaX, deltaZ);
    if (maximumDistance <= 0.0 ||
        distance <= maximumDistance) {
        return planeAim;
    }

    // Near-parallel ray/plane intersections explode toward the horizon.
    // Use the point where the view ray passes closest to the drag anchor;
    // this preserves "left/right of anchor" instead of converting it into
    // a long forward line.
    const Vec3 toReference{
        dragReference.x - viewer.x,
        dragReference.y - viewer.y,
        dragReference.z - viewer.z,
    };
    const double rayDistance = std::max(
        0.0,
        toReference.x * lookDirection.x +
            toReference.y * lookDirection.y +
            toReference.z * lookDirection.z);
    Vec3 stable{
        viewer.x + lookDirection.x * rayDistance,
        planeAim.y,
        viewer.z + lookDirection.z * rayDistance,
    };
    deltaX = stable.x - dragReference.x;
    deltaZ = stable.z - dragReference.z;
    const double stableDistance = std::hypot(deltaX, deltaZ);
    if (stableDistance > maximumDistance) {
        const double scale = maximumDistance / stableDistance;
        stable.x = dragReference.x + deltaX * scale;
        stable.z = dragReference.z + deltaZ * scale;
    }
    return stable;
}

void hashDragPreviewValue(
    std::uint64_t& hash, std::uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL +
        (hash << 6U) + (hash >> 2U);
}

} // namespace

void App::clearModularPlacementDrag() {
    modularDragStart_.reset();
    modularDragEnd_.reset();
    modularDragOrigin_.reset();
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
    modularDragPreviewKey_.reset();
    modularDragHits_.clear();
    modularPlatformDragPreviews_.clear();
    modularWallDragPreviews_.clear();
    modularRampDragPreviews_.clear();
    modularDragUsesAutomaticFoundation_ = false;
    modularAutomaticFoundationPreviews_.clear();
}

void App::selectModularBuildPiece(
    ModularBuildPiece piece) {
    if (!modularBuildHotbarIndex(piece)) {
        piece = ModularBuildPiece::Foundation;
    }
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
    if (enabled) {
        placementPreviewEntranceSeconds_ = 0.0;
    }
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
        placementDragCandidateEnd_.reset();
        placementDragCandidateFrames_ = 0;
        placementDragLookMovement_ = 0.0;
        placementDragExtended_ = false;
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

    if (!modularDragUsesAutomaticFoundation_) {
        modularAutomaticFoundationPreviews_.clear();
    }

    modularEdgeHoverFrame_.reset();
    modularEdgeExtensionAnchor_.reset();

    if (modularBuildPiece_ == ModularBuildPiece::Ramp) {
        if (modularDragUsesAutomaticFoundation_ &&
            modularDragPlaneHeight_ && modularDragStart_) {
            const auto planeAim = rampSocketAimOnFloor(
                snapshot.playerPosition, lookDirection,
                *modularDragPlaneHeight_);
            if (planeAim) {
                const Vec3 dragReference =
                    modularDragReferencePoint(
                        *modularDragStart_,
                        *modularDragPiece_,
                        *modularDragPlaneHeight_,
                        cellSize);
                const Vec3 boundedPlaneAim =
                    stableElevatedDragAim(
                        *planeAim,
                        snapshot.playerPosition,
                        lookDirection, dragReference,
                        terrain.config()
                            .buildPreviewDistance);
                updateModularDragAxis(
                    boundedPlaneAim, cellSize);
                GridCoord dragEnd{
                    snapPlatformFrameAxis(
                        static_cast<int>(std::floor(
                            boundedPlaneAim.x /
                            cellSize))),
                    0,
                    snapPlatformFrameAxis(
                        static_cast<int>(std::floor(
                            boundedPlaneAim.z /
                            cellSize))),
                };
                if (modularDragAxis_ ==
                    PlacementLineAxis::X) {
                    dragEnd.z =
                        modularDragStart_->z;
                } else if (modularDragAxis_ ==
                           PlacementLineAxis::Z) {
                    dragEnd.x =
                        modularDragStart_->x;
                }
                if (dragEnd.x == modularDragStart_->x &&
                    dragEnd.z == modularDragStart_->z) {
                    modularDragCandidateEnd_.reset();
                    modularDragCandidateFrames_ = 0;
                } else if (!modularDragExtended_) {
                    constexpr double
                        RampDragMinimumLookPixels = 3.0;
                    if (modularDragLookMovement_ <
                        RampDragMinimumLookPixels) {
                        dragEnd = *modularDragStart_;
                        modularDragCandidateEnd_.reset();
                        modularDragCandidateFrames_ = 0;
                    } else if (
                        modularDragCandidateEnd_ &&
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
                        RampDragConfirmationFrames = 2;
                    if (modularDragCandidateFrames_ <
                        RampDragConfirmationFrames) {
                        dragEnd = *modularDragStart_;
                    } else {
                        modularDragExtended_ = true;
                    }
                }
                modularDragEnd_ = dragEnd;
                foundationTerrainHit_ = boundedPlaneAim;
                rebuildModularPlacementLine();
            }
            return;
        }
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
                const Vec3 dragReference =
                    modularDragReferencePoint(
                        *modularDragStart_,
                        *modularDragPiece_,
                        *modularDragPlaneHeight_,
                        cellSize);
                const Vec3 boundedFloorAim =
                    stableElevatedDragAim(
                        *floorAim,
                        snapshot.playerPosition,
                        lookDirection,
                        dragReference,
                        terrain.config()
                            .buildPreviewDistance);
                updateModularDragAxis(
                    boundedFloorAim, cellSize);
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
                        boundedFloorAim,
                        lookDirection, cellSize));
                if (!targetFrame) {
                    targetFrame = frameAtAnchor(GridCoord{
                        snapPlatformFrameAxis(
                            static_cast<int>(std::floor(
                                boundedFloorAim.x / cellSize))),
                        0,
                        snapPlatformFrameAxis(
                            static_cast<int>(std::floor(
                                boundedFloorAim.z / cellSize))),
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
                const Vec3 dragReference =
                    modularDragReferencePoint(
                        *modularDragStart_,
                        *modularDragPiece_,
                        *modularDragPlaneHeight_,
                        cellSize);
                const Vec3 boundedDragPlaneHit =
                    stableElevatedDragAim(
                        *dragPlaneHit,
                        snapshot.playerPosition,
                        lookDirection,
                        dragReference,
                        terrain.config()
                            .buildPreviewDistance);
                updateModularDragAxis(
                    boundedDragPlaneHit, cellSize);
                modularDragEnd_ =
                    rampSupportAnchorAtAim(
                        boundedDragPlaneHit,
                        lookDirection, cellSize);
                if (modularDragAxis_ ==
                    PlacementLineAxis::X) {
                    modularDragEnd_->z =
                        modularDragStart_->z;
                } else if (modularDragAxis_ ==
                           PlacementLineAxis::Z) {
                    modularDragEnd_->x =
                        modularDragStart_->x;
                }
                foundationTerrainHit_ =
                    boundedDragPlaneHit;
                modularSnapHit_ = boundedDragPlaneHit;
                modularSnapMarker_ =
                    boundedDragPlaneHit;
                rampPreview_.reset();
                platformFramePreview_.reset();
                wallPreview_.reset();
                rebuildModularPlacementLine();
                return;
            }
        }
        if (!edgeTarget) {
            const double terrainRayDistance = std::max(
                12.0,
                modularStoreyHeight(terrain.config()) *
                        static_cast<double>(
                            terrain.config().maxStoreys) +
                    12.0);
            const auto terrainHit = terrain.raycast(
                snapshot.playerPosition, lookDirection,
                terrainRayDistance);
            if (terrainHit) {
                const RampFoundationPlacement composite =
                    simulation_.previewRampWithFoundation(
                        *terrainHit, modularRotation_);
                modularAutomaticFoundationPreviews_.assign(
                    composite.foundations.begin(),
                    composite.foundations.end());
                rampPreview_ = composite.ramp;
                foundationTerrainHit_ = *terrainHit;
                modularSnapHit_ = *terrainHit;
                modularSnapMarker_.reset();
                modularPreviewAnchor_ =
                    composite.foundations[0].anchor;
                platformFramePreview_.reset();
                wallPreview_.reset();
                return;
            }
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
            if (modularDragAxis_ ==
                PlacementLineAxis::X) {
                modularDragEnd_->z =
                    modularDragStart_->z;
            } else if (modularDragAxis_ ==
                       PlacementLineAxis::Z) {
                modularDragEnd_->x =
                    modularDragStart_->x;
            }
            rebuildModularPlacementLine();
        }
        return;
    }

    const bool elevatedDrag =
        modularDragPiece_ &&
        modularDragPlaneHeight_ &&
        modularDragStorey_ &&
        *modularDragStorey_ > 0;
    std::optional<Vec3> rawHit;
    if (elevatedDrag) {
        const Vec3 dragReference =
            modularDragReferencePoint(
                *modularDragStart_,
                *modularDragPiece_,
                *modularDragPlaneHeight_,
                cellSize);
        if (*modularDragPiece_ ==
            ModularBuildPiece::FloorPlatform) {
            rawHit = elevatedPlatformDragAim(
                snapshot.playerPosition,
                lookDirection,
                *modularDragPlaneHeight_,
                terrain.config().buildPreviewDistance);
        } else {
            const auto dragPlaneHit =
                rampSocketAimOnFloor(
                    snapshot.playerPosition,
                    lookDirection,
                    *modularDragPlaneHeight_);
            if (dragPlaneHit) {
                rawHit = stableElevatedDragAim(
                    *dragPlaneHit,
                    snapshot.playerPosition,
                    lookDirection, dragReference,
                    terrain.config().buildPreviewDistance);
            }
        }
        // Looking parallel to, or away from, the working plane keeps the
        // previous endpoint. Never fall back to the terrain below it.
        if (!rawHit) {
            return;
        }
    } else {
        const double terrainRayDistance = std::max(
            12.0,
            modularStoreyHeight(terrain.config()) *
                    static_cast<double>(
                        terrain.config().maxStoreys) +
                12.0);
        rawHit = simulation_.terrain().raycast(
            snapshot.playerPosition, lookDirection,
            terrainRayDistance);
    }

    if (rawHit && modularBuildPiece_ == ModularBuildPiece::Foundation) {
        rawHit = foundationPlacementHit(*rawHit, cellSize);
    }

    if (rawHit &&
        isPlatformBuildPiece(
            modularBuildPiece_) &&
        modularDragPiece_ ==
            modularBuildPiece_ &&
        modularDragStart_ &&
        modularDragPlaneHeight_) {
        updateModularDragAxis(*rawHit, cellSize);
        // Once a platform drag has begun, use its fixed storey
        // grid directly. The general valid-cell magnet may choose
        // opposite sides of an exact grid edge on press/release,
        // turning a stationary click into a two-platform drag.
        const bool foundationDrag =
            modularBuildPiece_ == ModularBuildPiece::Foundation;
        GridCoord dragEnd{
            foundationDrag
                ? snapPlatformFrameWorldAxis(rawHit->x, cellSize)
                : snapPlatformFrameAxis(static_cast<int>(std::floor(rawHit->x / cellSize))),
            0,
            foundationDrag
                ? snapPlatformFrameWorldAxis(rawHit->z, cellSize)
                : snapPlatformFrameAxis(static_cast<int>(std::floor(rawHit->z / cellSize))),
        };
        if (!foundationDrag &&
            (dragEnd.x != modularDragStart_->x ||
             dragEnd.z != modularDragStart_->z)) {
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

    // Floor placement needs a continuous grid target. Exact visual
    // picking contains intentional gaps between platform parts and
    // may briefly return no hit while crossing a mesh or grid edge.
    // Keep exact picking for outline/actions; use the broad-phase
    // platform proxy for placement only.
    std::optional<EntityId> platformPlacementTarget =
        snapshot.aimedModularBuilding;
    if (isPlatformBuildPiece(modularBuildPiece_) &&
        snapshot.aimedModularBuildingCandidate) {
        const EntityId candidate =
            *snapshot.aimedModularBuildingCandidate;
        const bool candidateIsFrame = std::any_of(
            snapshot.platformFrames.begin(),
            snapshot.platformFrames.end(),
            [candidate](
                const PlatformFrameInstance& frame) {
                return frame.id == candidate;
            });
        if (candidateIsFrame) {
            platformPlacementTarget = candidate;
        }
    }
    if (modularBuildPiece_ ==
            ModularBuildPiece::Foundation &&
        !modularDragPiece_ &&
        platformPlacementTarget) {
        const EntityId aimed = *platformPlacementTarget;
        const auto frame = std::find_if(
            snapshot.platformFrames.begin(),
            snapshot.platformFrames.end(),
            [aimed](const PlatformFrameInstance& candidate) {
                return candidate.id == aimed &&
                       candidate.storey == 0;
            });
        if (frame != snapshot.platformFrames.end()) {
            const auto edge = platformEdgeSnapAtAim(
                frame->anchor, frame->floorHeight,
                snapshot.playerPosition,
                lookDirection, cellSize);
            if (edge) {
                const Vec3 targetHit{
                    (edge->extensionAnchor.x + 0.5) *
                        cellSize,
                    terrain.getHeight(
                        (edge->extensionAnchor.x + 0.5) *
                            cellSize,
                        (edge->extensionAnchor.z + 0.5) *
                            cellSize),
                    (edge->extensionAnchor.z + 0.5) *
                        cellSize,
                };
                modularEdgeHoverFrame_ = frame->id;
                modularEdgeExtensionAnchor_ =
                    edge->extensionAnchor;
                platformFramePreview_ =
                    simulation_.previewFoundation(
                        targetHit);
                foundationTerrainHit_ = targetHit;
                modularSnapHit_ = targetHit;
                modularSnapMarker_ = edge->marker;
                if (!modularPreviewAnchor_ ||
                    *modularPreviewAnchor_ !=
                        edge->extensionAnchor) {
                    placementSnapPulseRemaining_ = 0.18;
                }
                modularPreviewAnchor_ =
                    edge->extensionAnchor;
                wallPreview_.reset();
                rampPreview_.reset();
                return;
            }
        }
    }
    if (modularBuildPiece_ ==
            ModularBuildPiece::FloorPlatform &&
        !modularDragPiece_ &&
        platformPlacementTarget) {
        const EntityId aimed = *platformPlacementTarget;
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
            constexpr double PlatformSlabThickness = 0.20;
            const bool aimingAtUnderside =
                usePlatformEdges &&
                snapshot.playerPosition.y <
                    frame->floorHeight -
                        PlatformSlabThickness - 0.02 &&
                lookDirection.y > 0.0;
            if (usePlatformEdges) {
                modularEdgeHoverFrame_ = frame->id;
            }
            std::optional<GridCoord> targetAnchor;
            int targetStorey = frame->storey + 1;
            double targetFloorHeight =
                frame->floorHeight +
                modularStoreyHeight(terrain.config());
            std::optional<PlatformEdgeSnap> edge;
            if (usePlatformEdges) {
                edge = platformEdgeSnapAtAim(
                    frame->anchor, frame->floorHeight,
                    snapshot.playerPosition,
                    lookDirection, cellSize,
                    aimingAtUnderside);
            }
            if (edge) {
                modularEdgeExtensionAnchor_ =
                    edge->extensionAnchor;
                targetAnchor = edge->extensionAnchor;
                targetStorey = frame->storey;
                targetFloorHeight = frame->floorHeight;
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
            modularSnapMarker_ = edge
                ? std::optional<Vec3>{edge->marker}
                : std::optional<Vec3>{targetHit};
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
        if (rawHit) {
            const FloorPlatformFoundationPlacement composite =
                simulation_.previewFloorPlatformWithFoundation(
                    *rawHit);
            modularAutomaticFoundationPreviews_ = {
                composite.foundation};
            platformFramePreview_ = composite.floor;
            foundationTerrainHit_ = *rawHit;
            modularSnapHit_ = *rawHit;
            modularSnapMarker_.reset();
            modularPreviewAnchor_ =
                composite.foundation.anchor;
            wallPreview_.reset();
            rampPreview_.reset();
            return;
        }
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
        modularSnapHit_.reset();
        modularSnapMarker_.reset();
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

    bool keptPrevious = false;
    if (modularSnapHit_) {
        const double deltaX =
            rawHit->x - modularSnapHit_->x;
        const double deltaZ =
            rawHit->z - modularSnapHit_->z;
        const auto previous =
            evaluate(*modularSnapHit_);
        constexpr double HysteresisCells = 0.62;
        if (modularBuildPiece_ != ModularBuildPiece::Foundation &&
            previous.second &&
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
    if (!keptPrevious &&
        (!chosenValid || modularBuildPiece_ != ModularBuildPiece::Foundation)) {
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
        if (modularBuildPiece_ == ModularBuildPiece::Foundation) {
            const double halfWidth = PlatformFrameWidthCells * 0.5 * cellSize;
            modularSnapMarker_->x += halfWidth;
            modularSnapMarker_->z += halfWidth;
        }
    } else {
        modularSnapHit_.reset();
        modularSnapMarker_.reset();
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
            updateModularDragAxis(*rawHit, cellSize);
            modularDragEnd_ = wallPreview_->anchor;
            if (modularDragAxis_ ==
                PlacementLineAxis::X) {
                modularDragEnd_->z =
                    modularDragStart_->z;
            } else if (modularDragAxis_ ==
                       PlacementLineAxis::Z) {
                modularDragEnd_->x =
                    modularDragStart_->x;
            }
        }
        rebuildModularPlacementLine();
    }
}

void App::beginModularPlacementDrag() {
    if (!modularAutomaticFoundationPreviews_.empty() &&
        foundationTerrainHit_ &&
        ((modularBuildPiece_ ==
              ModularBuildPiece::FloorPlatform &&
          platformFramePreview_ &&
          platformFramePreview_->valid()) ||
         (modularBuildPiece_ ==
              ModularBuildPiece::Ramp &&
          rampPreview_ && rampPreview_->valid())) &&
        std::ranges::all_of(
            modularAutomaticFoundationPreviews_,
            [](const PlatformFramePlacement& foundation) {
                return foundation.valid();
            })) {
        modularDragStart_ =
            modularAutomaticFoundationPreviews_.front().anchor;
        modularDragEnd_ = modularDragStart_;
        modularDragOrigin_ = foundationTerrainHit_;
        modularDragStorey_ =
            modularBuildPiece_ ==
                    ModularBuildPiece::FloorPlatform
                ? platformFramePreview_->storey
                : rampPreview_->targetStorey;
        modularDragTargetFloorHeight_ =
            modularAutomaticFoundationPreviews_
                .front().floorHeight;
        modularDragPlaneHeight_ =
            modularAutomaticFoundationPreviews_
                .front().floorHeight;
        modularDragPiece_ = modularBuildPiece_;
        modularDragRotation_ = modularRotation_;
        modularDragUsesAutomaticFoundation_ = true;
        modularDragAxis_.reset();
        modularDragCandidateEnd_.reset();
        modularDragCandidateFrames_ = 0;
        modularDragLookMovement_ = 0.0;
        modularDragExtended_ = false;
        rebuildModularPlacementLine();
        return;
    }
    if (isPlatformBuildPiece(modularBuildPiece_) &&
        platformFramePreview_ &&
        platformFramePreview_->valid()) {
        modularDragStart_ =
            platformFramePreview_->anchor;
        modularDragStorey_ =
            platformFramePreview_->storey;
        modularDragTargetFloorHeight_ =
            platformFramePreview_->floorHeight;
        const bool floorFromBelow =
            modularBuildPiece_ ==
                ModularBuildPiece::FloorPlatform &&
            modularEdgeHoverFrame_ &&
            simulation_.snapshot().playerPosition.y <
                platformFramePreview_->floorHeight - 0.22;
        modularDragPlaneHeight_ =
            modularBuildPiece_ ==
                    ModularBuildPiece::FloorPlatform &&
                !floorFromBelow
                ? platformFramePreview_->floorHeight -
                      modularStoreyHeight(
                          simulation_.terrain().config())
                : platformFramePreview_->floorHeight;
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
    modularDragOrigin_ = foundationTerrainHit_;
    if (modularDragPlaneHeight_ &&
        modularDragStorey_ && *modularDragStorey_ > 0) {
        const double cellSize = simulation_.terrain().config().cellSize;
        modularDragOrigin_ = modularDragReferencePoint(
            *modularDragStart_, modularBuildPiece_,
            *modularDragPlaneHeight_, cellSize);
        if (modularBuildPiece_ == ModularBuildPiece::Foundation) {
            modularDragOrigin_ = foundationPlacementHit(*modularDragOrigin_, cellSize);
        }
    } else if (modularDragPlaneHeight_) {
        const auto& snapshot = simulation_.snapshot();
        const double cosPitch =
            std::cos(snapshot.playerPitch);
        const Vec3 lookDirection{
            std::sin(snapshot.playerYaw) * cosPitch,
            std::sin(snapshot.playerPitch),
            -std::cos(snapshot.playerYaw) * cosPitch,
        };
        if (const auto planeAim = rampSocketAimOnFloor(
                snapshot.playerPosition, lookDirection,
                *modularDragPlaneHeight_)) {
            modularDragOrigin_ = modularBuildPiece_ == ModularBuildPiece::Foundation
                ? foundationPlacementHit(*planeAim, simulation_.terrain().config().cellSize)
                : *planeAim;
        }
    }
    modularDragPiece_ = modularBuildPiece_;
    modularDragAxis_.reset();
    modularDragCandidateEnd_.reset();
    modularDragCandidateFrames_ = 0;
    modularDragLookMovement_ = 0.0;
    modularDragExtended_ = false;
    rebuildModularPlacementLine();
}

void App::updateModularDragAxis(
    Vec3 aimHit, double cellSize) {
    if (!modularDragStart_ || !modularDragOrigin_ ||
        cellSize <= 0.0) {
        return;
    }
    const double deltaX =
        (aimHit.x - modularDragOrigin_->x) / cellSize;
    const double deltaZ =
        (aimHit.z - modularDragOrigin_->z) / cellSize;
    constexpr double ActivationDistanceCells = 0.18;
    if (!modularDragAxis_ &&
        std::max(std::abs(deltaX), std::abs(deltaZ)) <
            ActivationDistanceCells) {
        return;
    }
    constexpr double AxisSwitchMarginCells = 0.32;
    modularDragAxis_ = stabilizePlacementLineAxis(
        deltaX, deltaZ, modularDragAxis_,
        AxisSwitchMarginCells);
}

void App::rebuildModularPlacementLine() {
    if (!modularDragStart_ || !modularDragEnd_ ||
        !modularDragStorey_ || !modularDragPiece_) {
        modularDragPreviewKey_.reset();
        modularDragHits_.clear();
        modularPlatformDragPreviews_.clear();
        modularWallDragPreviews_.clear();
        modularRampDragPreviews_.clear();
        return;
    }

    int spacing =
        *modularDragPiece_ ==
                    ModularBuildPiece::Wall
            ? 1
            : PlatformFrameWidthCells;
    modularDragAxis_ = stabilizePlacementLineAxis(
        modularDragEnd_->x - modularDragStart_->x,
        modularDragEnd_->z - modularDragStart_->z,
        modularDragAxis_, spacing);
    if (*modularDragPiece_ ==
            ModularBuildPiece::Ramp &&
        modularDragAxis_) {
        const Rotation rotation =
            modularDragRotation_.value_or(
                modularRotation_);
        const bool rampRunsAlongZ =
            rotation == Rotation::Deg0 ||
            rotation == Rotation::Deg180;
        spacing = *modularDragAxis_ ==
                          PlacementLineAxis::X
                      ? (rampRunsAlongZ
                             ? ModularRampWidthCells
                             : ModularRampRunCells)
                      : (rampRunsAlongZ
                             ? ModularRampRunCells
                             : ModularRampWidthCells);
    }
    const auto& snapshot = simulation_.snapshot();
    std::uint64_t previewKey = 0xcbf29ce484222325ULL;
    const auto addSigned = [&previewKey](int value) {
        hashDragPreviewValue(
            previewKey,
            static_cast<std::uint64_t>(
                static_cast<std::int64_t>(value)));
    };
    addSigned(modularDragStart_->x);
    addSigned(modularDragStart_->z);
    addSigned(modularDragEnd_->x);
    addSigned(modularDragEnd_->z);
    addSigned(*modularDragStorey_);
    addSigned(static_cast<int>(*modularDragPiece_));
    addSigned(modularDragAxis_
                  ? static_cast<int>(*modularDragAxis_) + 1
                  : 0);
    addSigned(static_cast<int>(modularRotation_));
    addSigned(modularDragRotation_
                  ? static_cast<int>(*modularDragRotation_) + 1
                  : 0);
    addSigned(snapshot.wood);
    addSigned(snapshot.stone);
    addSigned(snapshot.crystals);
    hashDragPreviewValue(
        previewKey, simulation_.structuralRevision());
    hashDragPreviewValue(
        previewKey,
        std::bit_cast<std::uint64_t>(
            snapshot.playerPosition.x));
    hashDragPreviewValue(
        previewKey,
        std::bit_cast<std::uint64_t>(
            snapshot.playerPosition.z));
    hashDragPreviewValue(
        previewKey,
        std::bit_cast<std::uint64_t>(
            modularDragTargetFloorHeight_.value_or(0.0)));
    if (modularDragPreviewKey_ == previewKey) {
        return;
    }
    modularDragPreviewKey_ = previewKey;

    modularDragHits_.clear();
    modularPlatformDragPreviews_.clear();
    modularWallDragPreviews_.clear();
    modularRampDragPreviews_.clear();
    if (modularDragUsesAutomaticFoundation_) {
        modularAutomaticFoundationPreviews_.clear();
    }
    auto cells = ian::placementLine(
        *modularDragStart_, *modularDragEnd_,
        spacing, modularDragAxis_);
    const TerrainHeightfield& terrain =
        simulation_.terrain();
    const double cellSize = terrain.config().cellSize;
    if (modularDragUsesAutomaticFoundation_ &&
        modularDragTargetFloorHeight_) {
        modularDragHits_.reserve(cells.size());
        for (const GridCoord cell : cells) {
            const Vec3 hit = cellHit(
                cell, terrain, cellSize);
            if (*modularDragPiece_ ==
                ModularBuildPiece::FloorPlatform) {
                const FloorPlatformFoundationPlacement
                    composite = simulation_
                        .previewFloorPlatformWithFoundation(
                            hit,
                            *modularDragTargetFloorHeight_);
                modularDragHits_.push_back(hit);
                modularPlatformDragPreviews_.push_back(
                    composite.floor);
                modularAutomaticFoundationPreviews_
                    .push_back(composite.foundation);
            } else if (*modularDragPiece_ ==
                       ModularBuildPiece::Ramp) {
                const RampFoundationPlacement composite =
                    simulation_.previewRampWithFoundation(
                        hit,
                        modularDragRotation_.value_or(
                            modularRotation_),
                        *modularDragTargetFloorHeight_);
                modularDragHits_.push_back(hit);
                modularRampDragPreviews_.push_back(
                    composite.ramp);
                modularAutomaticFoundationPreviews_.insert(
                    modularAutomaticFoundationPreviews_.end(),
                    composite.foundations.begin(),
                    composite.foundations.end());
            }
        }
        return;
    }
    if (*modularDragPiece_ ==
            ModularBuildPiece::FloorPlatform &&
        modularDragTargetFloorHeight_) {
        cells = contiguousPlacementPrefix(
            std::move(cells),
            [this](GridCoord cell) {
                return simulation_.previewFloorPlatform(
                           cell, *modularDragStorey_,
                           *modularDragTargetFloorHeight_)
                           .error !=
                       ModularPlacementError::NoSupport;
            });
    }
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
                simulation_.previewFoundationAtHeight(
                    hit,
                    *modularDragTargetFloorHeight_);
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

} // namespace ian
