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
    simulation_.beginModularPlacementBatch();
    if (modularDragUsesAutomaticFoundation_ &&
        modularDragTargetFloorHeight_) {
        const auto addFoundationFeedback =
            [this, cellSize, &bounceDelayFor,
             &placedOrdinal](
                const PlatformFrameInstance& foundation) {
                const Vec3 center = platformFrameCenter(
                    foundation, cellSize);
                addEffect(
                    PresentationEffectType::BuildingPlaced,
                    center, 0.7, 1.25F, std::nullopt,
                    bounceDelayFor(placedOrdinal++));
            };
        if (*modularDragPiece_ ==
            ModularBuildPiece::FloorPlatform) {
            const std::size_t previewCount =
                modularDragExtended_
                    ? std::min(
                          modularDragHits_.size(),
                          modularPlatformDragPreviews_.size())
                    : std::min<std::size_t>(
                          1U,
                          std::min(
                              modularDragHits_.size(),
                              modularPlatformDragPreviews_.size()));
            for (std::size_t index = 0;
                 index < previewCount; ++index) {
                if (!modularPlatformDragPreviews_[index]
                         .valid()) {
                    continue;
                }
                const auto built = simulation_
                    .placeFloorPlatformWithFoundation(
                        modularDragHits_[index],
                        *modularDragTargetFloorHeight_);
                if (!built) {
                    continue;
                }
                addFoundationFeedback(built->foundation);
                const Vec3 floorCenter = platformFrameCenter(
                    built->floor, cellSize);
                const double startDelay =
                    bounceDelayFor(placedOrdinal++);
                addEffect(
                    PresentationEffectType::BuildingPlaced,
                    floorCenter, 0.7, 1.25F,
                    std::nullopt, startDelay);
                audio_.queueBuildingPlaced(
                    floorCenter, startDelay);
                placedAny = true;
            }
        } else if (*modularDragPiece_ ==
                   ModularBuildPiece::Ramp) {
            const std::size_t previewCount =
                modularDragExtended_
                    ? std::min(
                          modularDragHits_.size(),
                          modularRampDragPreviews_.size())
                    : std::min<std::size_t>(
                          1U,
                          std::min(
                              modularDragHits_.size(),
                              modularRampDragPreviews_.size()));
            const Rotation rotation =
                modularDragRotation_.value_or(
                    modularRotation_);
            for (std::size_t index = 0;
                 index < previewCount; ++index) {
                if (!modularRampDragPreviews_[index]
                         .valid()) {
                    continue;
                }
                const auto built =
                    simulation_.placeRampWithFoundation(
                        modularDragHits_[index], rotation,
                        *modularDragTargetFloorHeight_);
                if (!built) {
                    continue;
                }
                for (const auto& foundation :
                     built->foundations) {
                    addFoundationFeedback(foundation);
                }
                const Vec3 placedRampCenter =
                    rampCenter(built->ramp, cellSize);
                const double startDelay =
                    bounceDelayFor(placedOrdinal++);
                addEffect(
                    PresentationEffectType::BuildingPlaced,
                    placedRampCenter,
                    0.7, 1.35F, std::nullopt,
                    startDelay);
                audio_.queueBuildingPlaced(
                    placedRampCenter, startDelay);
                placedAny = true;
            }
        }
        simulation_.endModularPlacementBatch();
        clearModularPlacementDrag();
        modularSnapHit_.reset();
        modularSnapMarker_.reset();
        return placedAny;
    }
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
                    ? simulation_.previewFoundationAtHeight(
                          modularDragHits_[index],
                          *modularDragTargetFloorHeight_)
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
                    ? simulation_.placeFoundationAtHeight(
                          modularDragHits_[index],
                          *modularDragTargetFloorHeight_)
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
            audio_.queueBuildingPlaced(center, startDelay);
            ++placedOrdinal;
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
            const Vec3 placedWallCenter =
                wallCenter(*wall, cellSize);
            const double startDelay =
                bounceDelayFor(placedOrdinal);
            addEffect(
                PresentationEffectType::BuildingPlaced,
                placedWallCenter,
                0.7, 0.78F, std::nullopt,
                startDelay);
            audio_.queueBuildingPlaced(
                placedWallCenter, startDelay);
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
            const Vec3 placedRampCenter =
                rampCenter(*ramp, cellSize);
            const double startDelay =
                bounceDelayFor(placedOrdinal);
            addEffect(
                PresentationEffectType::BuildingPlaced,
                placedRampCenter,
                0.7, 1.35F, std::nullopt,
                startDelay);
            audio_.queueBuildingPlaced(
                placedRampCenter, startDelay);
            ++placedOrdinal;
        }
    }
    simulation_.endModularPlacementBatch();
    clearModularPlacementDrag();
    modularSnapHit_.reset();
    modularSnapMarker_.reset();
    return placedAny;
}

} // namespace ian
