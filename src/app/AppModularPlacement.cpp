#include "app/App.hpp"

#include "buildings/RampPlacementDirection.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr int MaximumModularLineLength = 48;

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

Vec3 cellHit(
    GridCoord cell, const TerrainHeightfield& terrain,
    double cellSize) {
    const double x =
        (static_cast<double>(cell.x) + 0.5) * cellSize;
    const double z =
        (static_cast<double>(cell.z) + 0.5) * cellSize;
    return {x, terrain.getHeight(x, z), z};
}

std::vector<GridCoord> modularLine(
    GridCoord start, GridCoord end, int spacing) {
    const int deltaX = end.x - start.x;
    const int deltaZ = end.z - start.z;
    const bool alongX =
        std::abs(deltaX) >= std::abs(deltaZ);
    const int distance =
        alongX ? std::abs(deltaX) : std::abs(deltaZ);
    const int count = std::min(
        distance / spacing + 1,
        MaximumModularLineLength);
    const int direction =
        (alongX ? deltaX : deltaZ) >= 0 ? 1 : -1;
    std::vector<GridCoord> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        GridCoord cell = start;
        if (alongX) {
            cell.x += index * spacing * direction;
        } else {
            cell.z += index * spacing * direction;
        }
        result.push_back(cell);
    }
    return result;
}

} // namespace

void App::clearModularPlacementDrag() {
    modularDragStart_.reset();
    modularDragEnd_.reset();
    modularDragStorey_.reset();
    modularDragPiece_.reset();
    modularDragHits_.clear();
    modularPlatformDragPreviews_.clear();
    modularWallDragPreviews_.clear();
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
    modularRearmAnchor_.reset();
    modularVerticalRearmBlocked_ = false;
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
        bool targetIsAimed = false;
        bool targetIsWithinRetentionMargin = false;
        if (snapshot.aimedModularBuilding) {
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
        if (!targetFrame && rampSocketFrame_) {
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
                mostViewAlignedRampEdgeSocket(
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
                    if (floorAim &&
                        !previousContainsFloorAim) {
                        chosen =
                            nearestRampEdgeSocketToPoint(
                                targetFrame->anchor,
                                targetFrame->floorHeight,
                                cellSize, *floorAim);
                    }
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
                         previousAlignment + 0.28 >=
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
                    targetIsWithinRetentionMargin) {
                    rampSocketLostGraceRemaining_ =
                        RampSocketLostGraceSeconds;
                }
                edgeTarget = rampEdgeTarget(
                    *targetFrame, *chosen, cellSize);
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
            modularVerticalRearmBlocked_ = false;
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
        platformFramePreview_.reset();
        wallPreview_.reset();
        modularVerticalRearmBlocked_ = false;
        if (rampPreview_) {
            if (!modularPreviewAnchor_ ||
                *modularPreviewAnchor_ !=
                    rampPreview_->anchor) {
                placementSnapPulseRemaining_ = 0.18;
            }
            modularPreviewAnchor_ =
                rampPreview_->anchor;
        }
        return;
    }

    const auto rawHit = simulation_.terrain().raycast(
        snapshot.playerPosition, lookDirection, 12.0);
    if (!rawHit) {
        platformFramePreview_.reset();
        wallPreview_.reset();
        rampPreview_.reset();
        foundationTerrainHit_.reset();
        modularPreviewAnchor_.reset();
        modularVerticalRearmBlocked_ = false;
        if (!IsKeyDown(KEY_LEFT_SHIFT)) {
            modularSnapHit_.reset();
            modularSnapMarker_.reset();
        }
        return;
    }

    std::optional<Vec3> edgeExtensionHit;
    if (modularBuildPiece_ ==
            ModularBuildPiece::PlatformFrame &&
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
            modularEdgeHoverFrame_ = frame->id;
            if (std::abs(lookDirection.y) > 1e-5) {
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
                        edgeExtensionHit = cellHit(
                            extension, terrain,
                            cellSize);
                    }
                }
            }
        }
    }

    const auto evaluate =
        [this](Vec3 hit) {
            switch (modularBuildPiece_) {
            case ModularBuildPiece::PlatformFrame: {
                const auto placement =
                    simulation_.previewPlatformFrame(hit);
                return std::pair{
                    placement.anchor,
                    placement.valid()};
            }
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
    if (edgeExtensionHit) {
        chosenHit = *edgeExtensionHit;
        const auto edge =
            evaluate(chosenHit);
        chosenAnchor = edge.first;
        chosenValid = edge.second;
        modularSnapHit_ = chosenHit;
        modularSnapMarker_ = chosenHit;
    }

    if (!modularPreviewAnchor_ ||
        *modularPreviewAnchor_ != chosenAnchor) {
        placementSnapPulseRemaining_ = 0.18;
    }
    modularPreviewAnchor_ = chosenAnchor;
    foundationTerrainHit_ = chosenHit;
    if (modularRearmAnchor_ &&
        (modularRearmAnchor_->x != chosenAnchor.x ||
         modularRearmAnchor_->z != chosenAnchor.z)) {
        modularRearmAnchor_.reset();
    }
    const bool verticalOverride =
        IsKeyDown(KEY_LEFT_CONTROL) ||
        IsKeyDown(KEY_RIGHT_CONTROL);
    modularVerticalRearmBlocked_ =
        modularBuildPiece_ ==
            ModularBuildPiece::PlatformFrame &&
        modularRearmAnchor_ &&
        modularRearmAnchor_->x == chosenAnchor.x &&
        modularRearmAnchor_->z == chosenAnchor.z &&
        !verticalOverride;

    switch (modularBuildPiece_) {
    case ModularBuildPiece::PlatformFrame:
        if (modularVerticalRearmBlocked_) {
            platformFramePreview_.reset();
        } else {
            platformFramePreview_ =
                simulation_.previewPlatformFrame(
                    chosenHit);
        }
        wallPreview_.reset();
        rampPreview_.reset();
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
                ModularBuildPiece::PlatformFrame &&
            platformFramePreview_) {
            modularDragEnd_ =
                platformFramePreview_->anchor;
        } else if (
            *modularDragPiece_ ==
                ModularBuildPiece::Wall &&
            wallPreview_) {
            modularDragEnd_ = wallPreview_->anchor;
        }
        rebuildModularPlacementLine();
    }
}

void App::beginModularPlacementDrag() {
    if (modularBuildPiece_ ==
            ModularBuildPiece::PlatformFrame &&
        platformFramePreview_ &&
        platformFramePreview_->valid()) {
        modularDragStart_ =
            platformFramePreview_->anchor;
        modularDragStorey_ =
            platformFramePreview_->storey;
    } else if (
        modularBuildPiece_ == ModularBuildPiece::Wall &&
        wallPreview_ && wallPreview_->valid()) {
        modularDragStart_ = wallPreview_->anchor;
        modularDragStorey_ = wallPreview_->storey;
    } else {
        return;
    }
    modularDragEnd_ = modularDragStart_;
    modularDragPiece_ = modularBuildPiece_;
    rebuildModularPlacementLine();
}

void App::rebuildModularPlacementLine() {
    modularDragHits_.clear();
    modularPlatformDragPreviews_.clear();
    modularWallDragPreviews_.clear();
    if (!modularDragStart_ || !modularDragEnd_ ||
        !modularDragStorey_ || !modularDragPiece_) {
        return;
    }

    const int spacing =
        *modularDragPiece_ ==
                ModularBuildPiece::PlatformFrame
            ? PlatformFrameWidthCells
            : 1;
    const auto cells = modularLine(
        *modularDragStart_, *modularDragEnd_,
        spacing);
    const TerrainHeightfield& terrain =
        simulation_.terrain();
    const double cellSize = terrain.config().cellSize;
    modularDragHits_.reserve(cells.size());

    Rotation wallRotation = modularRotation_;
    if (*modularDragPiece_ ==
            ModularBuildPiece::Wall &&
        cells.size() > 1U) {
        wallRotation =
            cells.front().x != cells.back().x
                ? Rotation::Deg0
                : Rotation::Deg90;
    }
    for (const GridCoord cell : cells) {
        const Vec3 hit =
            cellHit(cell, terrain, cellSize);
        if (*modularDragPiece_ ==
            ModularBuildPiece::PlatformFrame) {
            PlatformFramePlacement preview =
                simulation_.previewPlatformFrame(hit);
            if (preview.storey !=
                *modularDragStorey_) {
                continue;
            }
            modularDragHits_.push_back(hit);
            modularPlatformDragPreviews_.push_back(
                preview);
        } else {
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
        }
    }
}

bool App::finishModularPlacementDrag() {
    if (!modularDragPiece_) {
        return false;
    }
    const double cellSize =
        simulation_.terrain().config().cellSize;
    bool placedAny = false;
    if (*modularDragPiece_ ==
        ModularBuildPiece::PlatformFrame) {
        for (std::size_t index = 0;
             index < modularDragHits_.size() &&
             index <
                 modularPlatformDragPreviews_.size();
             ++index) {
            if (!modularPlatformDragPreviews_[index]
                     .valid()) {
                continue;
            }
            const PlatformFramePlacement current =
                simulation_.previewPlatformFrame(
                    modularDragHits_[index]);
            if (!current.valid() ||
                !modularDragStorey_ ||
                current.storey !=
                    *modularDragStorey_) {
                continue;
            }
            const auto frame =
                simulation_.placePlatformFrame(
                    modularDragHits_[index]);
            if (!frame) {
                continue;
            }
            placedAny = true;
            const Vec3 center =
                platformFrameCenter(*frame, cellSize);
            addEffect(
                PresentationEffectType::BuildingPlaced,
                center, 0.7, 1.25F);
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
    } else {
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
                0.7, 0.78F);
        }
    }
    const std::optional<GridCoord> placedEnd =
        modularDragEnd_;
    clearModularPlacementDrag();
    modularSnapHit_.reset();
    modularSnapMarker_.reset();
    if (placedAny) {
        if (placedEnd &&
            modularBuildPiece_ ==
                ModularBuildPiece::PlatformFrame) {
            modularRearmAnchor_ = placedEnd;
            modularVerticalRearmBlocked_ = true;
        }
        audio_.playUiConfirm();
    }
    return placedAny;
}

} // namespace ian
