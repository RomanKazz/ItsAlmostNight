#include "graphics/BuildingFoundationVisual.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "buildings/RampPlacementDirection.hpp"
#include "graphics/Renderer.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace ian {
namespace {

struct GridCoordHash {
    std::size_t operator()(const GridCoord& value) const {
        std::size_t seed = std::hash<int>{}(value.x);
        seed ^= std::hash<int>{}(value.yLevel) +
            0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        seed ^= std::hash<int>{}(value.z) +
            0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        return seed;
    }
};

using PlatformCells = std::unordered_set<GridCoord, GridCoordHash>;

PlatformCells occupiedPlatformCells(
    std::span<const PlatformFrameInstance> frames) {
    PlatformCells occupied;
    occupied.reserve(frames.size() * PlatformFrameWidthCells * PlatformFrameWidthCells);
    for (const auto& frame : frames) {
        for (int z = 0; z < PlatformFrameWidthCells; ++z) {
            for (int x = 0; x < PlatformFrameWidthCells; ++x) {
                occupied.insert({frame.anchor.x + x, frame.anchor.yLevel, frame.anchor.z + z});
            }
        }
    }
    return occupied;
}

bool interiorPlatformCorner(
    const PlatformCells& occupied, GridCoord corner) {
    for (int z = -1; z <= 0; ++z) {
        for (int x = -1; x <= 0; ++x) {
            if (!occupied.contains({corner.x + x, corner.yLevel, corner.z + z})) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

std::vector<std::array<bool, 4>>
platformSupportVisibilityMasks(
    std::span<const PlatformFrameInstance> platformFrames,
    std::span<const SharedSupport> sharedSupports) {
    std::unordered_map<std::uint32_t, const SharedSupport*>
        supportsById;
    supportsById.reserve(sharedSupports.size());
    for (const SharedSupport& support : sharedSupports) {
        supportsById.emplace(support.id, &support);
    }
    const auto occupied = occupiedPlatformCells(platformFrames);

    std::vector<std::array<bool, 4>> masks(
        platformFrames.size());
    for (std::size_t frameIndex = 0;
         frameIndex < platformFrames.size(); ++frameIndex) {
        const PlatformFrameInstance& frame =
            platformFrames[frameIndex];
        for (std::size_t supportIndex = 0;
             supportIndex < frame.supportIds.size();
             ++supportIndex) {
            const GridCoord corner{
                frame.anchor.x + static_cast<int>(supportIndex % 2U) * PlatformFrameWidthCells,
                frame.anchor.yLevel,
                frame.anchor.z + static_cast<int>(supportIndex / 2U) * PlatformFrameWidthCells,
            };
            // At a staggered T-junction only two corner IDs may meet, while
            // the neighbouring frame covers both other quadrants of the floor.
            if (interiorPlatformCorner(occupied, corner)) {
                continue;
            }
            const std::uint32_t id =
                frame.supportIds[supportIndex];
            const auto found = supportsById.find(id);
            if (found == supportsById.end()) {
                // Keep old or partially restored frames visible rather than
                // dropping their legs when shared-support data is absent.
                masks[frameIndex][supportIndex] = true;
                continue;
            }
            const SharedSupport& support = *found->second;
            if (!support.active || support.referenceCount >= 4U) {
                continue;
            }
            // Authored legs are inset into their own tiles. At a perimeter
            // joint the two touching halves form one wider column and connect
            // both edge rails; dropping either half leaves a visible gap.
            masks[frameIndex][supportIndex] = true;
        }
    }
    return masks;
}

std::vector<std::array<double, 4>>
platformSupportVisualLengths(
    std::span<const PlatformFrameInstance> platformFrames,
    std::span<const SharedSupport> sharedSupports) {
    std::unordered_map<std::uint32_t, const SharedSupport*>
        supportsById;
    supportsById.reserve(sharedSupports.size());
    for (const SharedSupport& support : sharedSupports) {
        supportsById.emplace(support.id, &support);
    }
    std::unordered_map<std::uint32_t, double> minimumBottoms;
    minimumBottoms.reserve(sharedSupports.size());
    std::vector<std::array<double, 4>> lengths(
        platformFrames.size());
    for (std::size_t frameIndex = 0;
         frameIndex < platformFrames.size(); ++frameIndex) {
        const PlatformFrameInstance& frame =
            platformFrames[frameIndex];
        for (std::size_t supportIndex = 0;
             supportIndex < frame.supports.size(); ++supportIndex) {
            const FoundationSupport& support =
                frame.supports[supportIndex];
            lengths[frameIndex][supportIndex] = support.length;
            const std::uint32_t id = frame.supportIds[supportIndex];
            const auto found = supportsById.find(id);
            if (found == supportsById.end() ||
                !found->second->active ||
                found->second->referenceCount >= 4U) {
                continue;
            }
            const auto [bottom, inserted] = minimumBottoms.emplace(
                id, support.bottom.y);
            if (!inserted) {
                bottom->second = std::min(
                    bottom->second, support.bottom.y);
            }
        }
    }
    for (std::size_t frameIndex = 0;
         frameIndex < platformFrames.size(); ++frameIndex) {
        const PlatformFrameInstance& frame = platformFrames[frameIndex];
        for (std::size_t supportIndex = 0;
             supportIndex < frame.supports.size(); ++supportIndex) {
            const auto bottom = minimumBottoms.find(
                frame.supportIds[supportIndex]);
            if (bottom != minimumBottoms.end()) {
                lengths[frameIndex][supportIndex] = std::max(
                    0.0,
                    frame.supports[supportIndex].top.y -
                        bottom->second);
            }
        }
    }
    return lengths;
}

std::vector<PlatformEdgeVisibilityMask>
platformEdgeVisibilityMasks(
    std::span<const PlatformFrameInstance> platformFrames) {
    const auto occupied = occupiedPlatformCells(platformFrames);

    std::vector<PlatformEdgeVisibilityMask> masks(
        platformFrames.size(), AllPlatformEdgesVisible);
    for (std::size_t frameIndex = 0;
         frameIndex < platformFrames.size(); ++frameIndex) {
        const GridCoord anchor = platformFrames[frameIndex].anchor;
        for (int offset = 0; offset < PlatformFrameWidthCells; ++offset) {
            const std::array<GridCoord, 4> neighbours{{
                {anchor.x - 1, anchor.yLevel, anchor.z + offset},
                {anchor.x + PlatformFrameWidthCells, anchor.yLevel, anchor.z + offset},
                {anchor.x + offset, anchor.yLevel, anchor.z - 1},
                {anchor.x + offset, anchor.yLevel, anchor.z + PlatformFrameWidthCells},
            }};
            for (std::size_t edge = 0; edge < neighbours.size(); ++edge) {
                masks[frameIndex][edge][static_cast<std::size_t>(offset)] =
                    !occupied.contains(neighbours[edge]);
            }
        }
    }
    return masks;
}

namespace {

constexpr Color platformFrameColor{174, 121, 67, 255};
constexpr Color wallColor{179, 105, 55, 255};
constexpr Color rampColor{211, 166, 101, 255};
constexpr Color validPreviewColor{109, 229, 142, 150};
constexpr Color warningPreviewColor{242, 188, 72, 150};
constexpr Color invalidPreviewColor{231, 82, 76, 150};
constexpr Color unsupportedColor{204, 55, 51, 255};
constexpr Color selectedColor{255, 229, 132, 255};

bool previewOccupiesExistingBuilding(
    ModularPlacementError error) {
    return error == ModularPlacementError::Occupied;
}

void drawOccupiedFootprint(
    GridCoord anchor, int widthCells, int depthCells,
    double height, double cellSize) {
    constexpr Color Fill{255, 58, 52, 92};
    constexpr Color Border{255, 88, 76, 245};
    const float minimumX =
        static_cast<float>(anchor.x * cellSize);
    const float minimumZ =
        static_cast<float>(anchor.z * cellSize);
    const float width =
        static_cast<float>(widthCells * cellSize);
    const float depth =
        static_cast<float>(depthCells * cellSize);
    const float y = static_cast<float>(height + 0.055);
    DrawPlane(
        {minimumX + width * 0.5F, y,
         minimumZ + depth * 0.5F},
        {width, depth}, Fill);
    for (int x = 0; x <= widthCells; ++x) {
        const float lineX =
            minimumX + static_cast<float>(x * cellSize);
        DrawLine3D(
            {lineX, y, minimumZ},
            {lineX, y, minimumZ + depth}, Border);
    }
    for (int z = 0; z <= depthCells; ++z) {
        const float lineZ =
            minimumZ + static_cast<float>(z * cellSize);
        DrawLine3D(
            {minimumX, y, lineZ},
            {minimumX + width, y, lineZ}, Border);
    }
}

void drawOccupiedRampFootprint(
    const RampPlacement& placement, double cellSize) {
    const bool alongZ =
        placement.rotation == Rotation::Deg0 ||
        placement.rotation == Rotation::Deg180;
    drawOccupiedFootprint(
        placement.anchor,
        alongZ ? ModularRampWidthCells
               : ModularRampRunCells,
        alongZ ? ModularRampRunCells
               : ModularRampWidthCells,
        placement.bottomHeight, cellSize);
}

void drawPlatformFrame(
    Renderer* renderer,
    double floorHeight,
    const std::array<FoundationSupport, 4>& supports,
    const std::array<double, 4>& visualSupportLengths,
    Color color,
    const std::array<bool, 4>& supportVisibility,
    const PlatformEdgeVisibilityMask& edgeVisibility) {
    const double minimumX = std::min(
        supports[0].top.x, supports[2].top.x);
    const double maximumX = std::max(
        supports[1].top.x, supports[3].top.x);
    const double minimumZ = std::min(
        supports[0].top.z, supports[1].top.z);
    const double maximumZ = std::max(
        supports[2].top.z, supports[3].top.z);
    const float centerX =
        static_cast<float>((minimumX + maximumX) * 0.5);
    const float centerZ =
        static_cast<float>((minimumZ + maximumZ) * 0.5);
    const float widthX =
        static_cast<float>(maximumX - minimumX);
    const float widthZ =
        static_cast<float>(maximumZ - minimumZ);
    std::array<float, 4> supportLengths{};
    for (std::size_t index = 0;
         index < supports.size(); ++index) {
        supportLengths[index] = static_cast<float>(
            std::max(visualSupportLengths[index], 0.0));
    }
    const Color modelTint =
        color.r == platformFrameColor.r &&
                color.g == platformFrameColor.g &&
                color.b == platformFrameColor.b
            ? Color{255, 255, 255, color.a}
            : color;
    if (renderer != nullptr &&
        renderer->drawPlatformFrameModel(
            {centerX, static_cast<float>(floorHeight),
             centerZ},
            modelTint, widthX * 0.5F,
            supportLengths, supportVisibility,
            edgeVisibility)) {
        return;
    }
    DrawCube(
        {centerX, static_cast<float>(floorHeight - 0.09),
         centerZ},
        widthX, 0.18F, widthZ, color);
    const float beamY =
        static_cast<float>(floorHeight - 0.24);
    constexpr float BeamThickness = 0.14F;
    for (std::size_t edge = 0; edge < edgeVisibility.size(); ++edge) {
        const bool alongX = edge >= 2U;
        const float fullLength = alongX ? widthX : widthZ;
        for (std::size_t segment = 0; segment < edgeVisibility[edge].size(); ++segment) {
            if (!edgeVisibility[edge][segment]) continue;
            const float segmentLength = fullLength / PlatformFrameWidthCells;
            const float offset = -fullLength * 0.5F +
                (static_cast<float>(segment) + 0.5F) * segmentLength;
            const Vector3 center = alongX
                ? Vector3{centerX + offset, beamY,
                    static_cast<float>(edge == 2U ? minimumZ : maximumZ)}
                : Vector3{static_cast<float>(edge == 0U ? minimumX : maximumX),
                    beamY, centerZ + offset};
            DrawCube(center, alongX ? segmentLength : BeamThickness,
                BeamThickness, alongX ? BeamThickness : segmentLength, color);
        }
    }

    for (std::size_t index = 0; index < supports.size(); ++index) {
        if (!supportVisibility[index]) {
            continue;
        }
        const FoundationSupport& support = supports[index];
        DrawCube(
            {
                static_cast<float>(support.top.x),
                static_cast<float>(
                    (support.top.y + support.bottom.y) * 0.5),
                static_cast<float>(support.top.z),
            },
            0.14F,
            static_cast<float>(std::max(support.length, 0.04)),
            0.14F, color);
    }
}

void drawWall(GridCoord anchor, Rotation rotation,
              double bottomHeight, double topHeight,
              double cellSize, Color color,
              std::optional<float> yawOverride =
                  std::nullopt) {
    const float yaw =
        yawOverride.value_or(
            static_cast<float>(rotation) *
            PI * 0.5F);
    rlPushMatrix();
    rlTranslatef(
        static_cast<float>((anchor.x + 0.5) * cellSize),
        static_cast<float>(
            (bottomHeight + topHeight) * 0.5),
        static_cast<float>((anchor.z + 0.5) * cellSize));
    rlRotatef(yaw * RAD2DEG, 0.0F, 1.0F, 0.0F);
    DrawCube(
        {0.0F, 0.0F, 0.0F},
        static_cast<float>(cellSize),
        static_cast<float>(topHeight - bottomHeight),
        0.14F, color);
    rlPopMatrix();
}

void drawRamp(Renderer* renderer,
              GridCoord anchor, Rotation rotation,
              double bottomHeight, double topHeight,
              double cellSize, Color color,
              std::optional<float> yawOverride =
                  std::nullopt) {
    const float rise =
        static_cast<float>(topHeight - bottomHeight);
    const float run = static_cast<float>(
        ModularRampRunCells * cellSize);
    const float width = static_cast<float>(
        ModularRampWidthCells * cellSize);
    const float length = std::sqrt(rise * rise + run * run);
    const float angle = std::atan2(rise, run) * RAD2DEG;
    const bool placedAlongZ =
        rotation == Rotation::Deg0 ||
        rotation == Rotation::Deg180;
    const int widthCells =
        placedAlongZ ? ModularRampWidthCells
                     : ModularRampRunCells;
    const int depthCells =
        placedAlongZ ? ModularRampRunCells
                     : ModularRampWidthCells;
    const float yaw =
        yawOverride.value_or(
            -static_cast<float>(rotation) *
            PI * 0.5F);
    const float centerX = static_cast<float>(
        (anchor.x + widthCells * 0.5) * cellSize);
    const float centerZ = static_cast<float>(
        (anchor.z + depthCells * 0.5) * cellSize);
    const Color modelTint =
        color.r == rampColor.r &&
                color.g == rampColor.g &&
                color.b == rampColor.b
            ? Color{255, 255, 255, color.a}
            : color;
    const bool modelDrawn =
        renderer != nullptr &&
        renderer->drawRampModel(
            {centerX, static_cast<float>(bottomHeight),
             centerZ},
            yaw, modelTint,
            static_cast<float>(cellSize));
    if (!modelDrawn) {
        rlPushMatrix();
        rlTranslatef(
            centerX,
            static_cast<float>(
                (bottomHeight + topHeight) * 0.5),
            centerZ);
        rlRotatef(yaw * RAD2DEG, 0.0F, 1.0F, 0.0F);
        rlRotatef(-angle, 1.0F, 0.0F, 0.0F);
        DrawCube({0.0F, 0.0F, 0.0F},
                 width, 0.16F, length,
                 color);
        rlPopMatrix();
    }

    const Vec3 outward =
        rampSocketOutwardDirection(rotation);
    const Vector3 side{
        static_cast<float>(-outward.z), 0.0F,
        static_cast<float>(outward.x)};
    constexpr float BeamThicknessCells = 0.16F;
    const float beamThickness =
        BeamThicknessCells * static_cast<float>(cellSize);
    const float runHalf = run * 0.5F - beamThickness * 0.5F;
    const float sideOffset =
        width * 0.5F - beamThickness * 0.5F;
    const Vector3 upperCenter{
        centerX + static_cast<float>(outward.x) * runHalf,
        static_cast<float>(bottomHeight),
        centerZ + static_cast<float>(outward.z) * runHalf,
    };
    const std::array beamCenters{
        Vector3Add(
            upperCenter, Vector3Scale(side, sideOffset)),
        Vector3Subtract(
            upperCenter, Vector3Scale(side, sideOffset)),
    };
    for (const Vector3 beamCenter : beamCenters) {
        const bool beamDrawn = renderer != nullptr &&
            renderer->drawRampSupportBeam(
                beamCenter, yaw, modelTint,
                static_cast<float>(cellSize));
        if (!beamDrawn) {
            const float fallbackHeight = std::max(
                rise - beamThickness, beamThickness);
            DrawCube(
                {beamCenter.x,
                 beamCenter.y + fallbackHeight * 0.5F,
                 beamCenter.z},
                beamThickness, fallbackHeight,
                beamThickness, color);
        }
    }
}

Color previewColor(bool valid) {
    return valid ? validPreviewColor : invalidPreviewColor;
}

void drawPlatformFramePreview(
    Renderer* renderer,
    const PlatformFramePlacement& placement,
    double maximumWoodSupportLength) {
    const double widthX = std::abs(
        placement.supports[1].top.x -
        placement.supports[0].top.x);
    const double widthZ = std::abs(
        placement.supports[2].top.z -
        placement.supports[0].top.z);
    if (widthX <= 1e-6 || widthZ <= 1e-6) {
        return;
    }
    double longestSupport = 0.0;
    for (const FoundationSupport& support :
         placement.supports) {
        longestSupport =
            std::max(longestSupport, support.length);
    }
    const bool longSupports =
        placement.storey == 0 &&
        longestSupport >
            maximumWoodSupportLength * 0.7;
    const Color color =
        !placement.valid()
            ? invalidPreviewColor
            : (longSupports ? warningPreviewColor
                            : validPreviewColor);
    std::array<double, 4> supportLengths{};
    for (std::size_t index = 0;
         index < placement.supports.size(); ++index) {
        supportLengths[index] = placement.supports[index].length;
    }
    drawPlatformFrame(
        renderer,
        placement.floorHeight, placement.supports,
        supportLengths,
        color, AllPlatformSupportsVisible,
        AllPlatformEdgesVisible);
}

Color structuralColor(
    StructuralSupportState state, Color supportedColor) {
    return state == StructuralSupportState::Supported
               ? supportedColor
               : unsupportedColor;
}

Color instanceColor(
    EntityId id, StructuralSupportState state,
    Color supportedColor,
    std::optional<EntityId> selected) {
    return selected == id
               ? selectedColor
               : structuralColor(state, supportedColor);
}

float animationScale(
    EntityId id,
    std::span<const ModularAnimationScale> scales) {
    const auto scale = std::find_if(
        scales.begin(), scales.end(),
        [id](const ModularAnimationScale& candidate) {
            return candidate.id == id;
        });
    return scale == scales.end()
               ? 1.0F
               : scale->scale;
}

BuildingFeedbackHighlight buildingFeedback(
    EntityId id, std::span<const ModularAnimationScale> scales) {
    const auto entry = std::ranges::find(scales, id, &ModularAnimationScale::id);
    return entry == scales.end() ? BuildingFeedbackHighlight{} : entry->feedback;
}

bool worldObjectVisible(
    const ModularBuildingView& view,
    Vec3 position, double radius) {
    if (!view.cameraPosition || view.maximumDrawDistance <= 0.0) {
        return true;
    }
    const double offsetX = position.x - view.cameraPosition->x;
    const double offsetZ = position.z - view.cameraPosition->z;
    const double maximumDistance = view.maximumDrawDistance + radius;
    if (offsetX * offsetX + offsetZ * offsetZ >
        maximumDistance * maximumDistance) {
        return false;
    }
    const double forwardLength = std::hypot(
        view.cameraForward.x, view.cameraForward.z);
    if (forwardLength <= 1e-9) {
        return true;
    }
    const double forwardDot =
        (offsetX * view.cameraForward.x +
         offsetZ * view.cameraForward.z) /
        forwardLength;
    return forwardDot >= -radius;
}

template <typename Draw>
void drawScaled(
    Vec3 origin, float scale, Draw&& draw) {
    if (std::abs(scale - 1.0F) < 1e-4F) {
        draw();
        return;
    }
    rlPushMatrix();
    rlTranslatef(
        static_cast<float>(origin.x),
        static_cast<float>(origin.y),
        static_cast<float>(origin.z));
    rlScalef(scale, scale, scale);
    rlTranslatef(
        static_cast<float>(-origin.x),
        static_cast<float>(-origin.y),
        static_cast<float>(-origin.z));
    draw();
    rlPopMatrix();
}

[[nodiscard]] Vec3 platformScaleOrigin(
    const PlatformFrameInstance& frame,
    std::span<const ModularAnimationScale> scales) {
    const auto animation = std::ranges::find(scales, frame.id, &ModularAnimationScale::id);
    if (animation != scales.end() && animation->scaleOrigin) {
        return *animation->scaleOrigin;
    }
    const double bottom = std::min({
        frame.supports[0].bottom.y,
        frame.supports[1].bottom.y,
        frame.supports[2].bottom.y,
        frame.supports[3].bottom.y,
    });
    const double pivotHeight = frame.storey == 0
        ? bottom
        : (bottom + frame.floorHeight) * 0.5;
    Vec3 center = platformFrameVisualCenter(frame);
    center.y = pivotHeight;
    return center;
}

} // namespace

void ModularBuildingRenderer::setRenderer(
    Renderer* renderer) {
    renderer_ = renderer;
}

void ModularBuildingRenderer::drawWorld(
    const ModularBuildingView& buildings,
    const ModularBuildingPreviewView& preview) const {
    const auto visibilityFrames =
        buildings.platformVisibilityContext.empty()
            ? buildings.platformFrames
            : buildings.platformVisibilityContext;
    const auto visibilitySupports =
        buildings.sharedSupportVisibilityContext.empty()
            ? buildings.sharedSupports
            : buildings.sharedSupportVisibilityContext;
    const auto supportVisibility =
        platformSupportVisibilityMasks(
            visibilityFrames, visibilitySupports);
    const auto supportLengths =
        platformSupportVisualLengths(
            visibilityFrames, visibilitySupports);
    const auto edgeVisibility =
        platformEdgeVisibilityMasks(visibilityFrames);
    for (std::size_t frameIndex = 0;
         frameIndex < buildings.platformFrames.size();
         ++frameIndex) {
        const PlatformFrameInstance& frame =
            buildings.platformFrames[frameIndex];
        std::size_t visibilityIndex = frameIndex;
        if (visibilityFrames.data() != buildings.platformFrames.data()) {
            const auto found = std::ranges::find(
                visibilityFrames, frame.id,
                &PlatformFrameInstance::id);
            if (found == visibilityFrames.end()) {
                continue;
            }
            visibilityIndex = static_cast<std::size_t>(
                std::distance(visibilityFrames.begin(), found));
        }
        const Vec3 origin = platformScaleOrigin(frame, buildings.animationScales);
        if (!worldObjectVisible(
                buildings, origin, buildings.cellSize * 1.5)) {
            continue;
        }
        if (renderer_) renderer_->setBuildingFeedbackHighlight(
            buildingFeedback(frame.id, buildings.animationScales));
        drawScaled(
            origin,
            animationScale(
                frame.id, buildings.animationScales),
            [&] {
                drawPlatformFrame(
                    renderer_,
                    frame.floorHeight, frame.supports,
                    supportLengths[visibilityIndex],
                    Fade(
                        instanceColor(
                            frame.id,
                            frame.supportState,
                            platformFrameColor,
                            buildings.selected),
                        buildings.alpha),
                    supportVisibility[visibilityIndex],
                    edgeVisibility[visibilityIndex]);
            });
    }
    for (const WallInstance& wall : buildings.walls) {
        const Vec3 origin{
            (wall.anchor.x + 0.5) *
                buildings.cellSize,
            wall.bottomHeight,
            (wall.anchor.z + 0.5) *
                buildings.cellSize,
        };
        if (!worldObjectVisible(
                buildings, origin, buildings.cellSize)) {
            continue;
        }
        if (renderer_) renderer_->setBuildingFeedbackHighlight(
            buildingFeedback(wall.id, buildings.animationScales));
        drawScaled(
            origin,
            animationScale(
                wall.id, buildings.animationScales),
            [&] {
                drawWall(
                    wall.anchor, wall.rotation,
                    wall.bottomHeight, wall.topHeight,
                    buildings.cellSize,
                    Fade(
                        instanceColor(
                            wall.id, wall.supportState,
                            wallColor,
                            buildings.selected),
                        buildings.alpha));
            });
    }
    for (const RampInstance& ramp : buildings.ramps) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const int widthCells =
            alongZ ? ModularRampWidthCells
                   : ModularRampRunCells;
        const int depthCells =
            alongZ ? ModularRampRunCells
                   : ModularRampWidthCells;
        const Vec3 origin{
            (ramp.anchor.x + widthCells * 0.5) *
                buildings.cellSize,
            ramp.bottomHeight,
            (ramp.anchor.z + depthCells * 0.5) *
                buildings.cellSize,
        };
        if (!worldObjectVisible(
                buildings, origin,
                buildings.cellSize * 0.5 *
                    std::hypot(widthCells, depthCells))) {
            continue;
        }
        if (renderer_) renderer_->setBuildingFeedbackHighlight(
            buildingFeedback(ramp.id, buildings.animationScales));
        drawScaled(
            origin,
            animationScale(
                ramp.id, buildings.animationScales),
            [&] {
                drawRamp(
                    renderer_,
                    ramp.anchor, ramp.rotation,
                    ramp.bottomHeight, ramp.topHeight,
                    buildings.cellSize,
                    Fade(
                        instanceColor(
                            ramp.id, ramp.supportState,
                            rampColor,
                            buildings.selected),
                        buildings.alpha));
            });
    }

    if (renderer_) renderer_->setBuildingFeedbackHighlight({});

    const bool hasGhostPreview =
        preview.platformFrame != nullptr ||
        preview.wall != nullptr ||
        preview.ramp != nullptr ||
        !preview.platformFrameLine.empty() ||
        !preview.wallLine.empty() ||
        !preview.rampLine.empty() ||
        preview.terrainHit != nullptr;
    if (hasGhostPreview) {
        // Preview geometry must test against the completed world,
        // but must not write depth itself. This keeps overlapping
        // translucent pieces stable during drag placement.
        rlDrawRenderBatchActive();
        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthMask();
        if (renderer_ != nullptr) {
            renderer_->beginGhostPreviewMaterial();
        }
    }

    const bool obstructedSinglePreview =
        (preview.wall &&
         previewOccupiesExistingBuilding(
             preview.wall->error)) ||
        (preview.ramp &&
         previewOccupiesExistingBuilding(
             preview.ramp->error));
    bool translatedPreview = false;
    if (preview.visualOffset) {
        rlPushMatrix();
        rlTranslatef(
            static_cast<float>(preview.visualOffset->x),
            static_cast<float>(preview.visualOffset->y),
            static_cast<float>(preview.visualOffset->z));
        translatedPreview = true;
    } else if (preview.visualOrigin &&
        !obstructedSinglePreview &&
        preview.platformFrameLine.empty() &&
        preview.wallLine.empty() &&
        preview.rampLine.empty()) {
        std::optional<Vec3> targetOrigin;
        if (preview.platformFrame) {
            targetOrigin = Vec3{
                (preview.platformFrame->anchor.x + 1.0) *
                    buildings.cellSize,
                preview.platformFrame->floorHeight,
                (preview.platformFrame->anchor.z + 1.0) *
                    buildings.cellSize,
            };
        } else if (preview.wall) {
            targetOrigin = Vec3{
                (preview.wall->anchor.x + 0.5) *
                    buildings.cellSize,
                preview.wall->bottomHeight,
                (preview.wall->anchor.z + 0.5) *
                    buildings.cellSize,
            };
        } else if (preview.ramp) {
            const bool alongZ =
                preview.ramp->rotation ==
                    Rotation::Deg0 ||
                preview.ramp->rotation ==
                    Rotation::Deg180;
            const int widthCells =
                alongZ ? ModularRampWidthCells
                       : ModularRampRunCells;
            const int depthCells =
                alongZ ? ModularRampRunCells
                       : ModularRampWidthCells;
            targetOrigin = Vec3{
                (preview.ramp->anchor.x +
                 widthCells * 0.5) *
                    buildings.cellSize,
                preview.ramp->bottomHeight,
                (preview.ramp->anchor.z +
                 depthCells * 0.5) *
                    buildings.cellSize,
            };
        }
        if (targetOrigin) {
            rlPushMatrix();
            rlTranslatef(
                static_cast<float>(
                    preview.visualOrigin->x -
                    targetOrigin->x),
                static_cast<float>(
                    preview.visualOrigin->y -
                    targetOrigin->y),
                static_cast<float>(
                    preview.visualOrigin->z -
                    targetOrigin->z));
            translatedPreview = true;
        }
    }

    if (!preview.platformFrameLine.empty()) {
        for (const PlatformFramePlacement& placement :
             preview.platformFrameLine) {
            if (previewOccupiesExistingBuilding(
                    placement.error)) {
                drawOccupiedFootprint(
                    placement.anchor,
                    PlatformFrameWidthCells,
                    PlatformFrameWidthCells,
                    placement.floorHeight,
                    buildings.cellSize);
                continue;
            }
            drawPlatformFramePreview(
                renderer_,
                placement,
                preview.maximumWoodSupportLength);
        }
    } else if (!preview.wallLine.empty()) {
        for (const WallPlacement& placement :
             preview.wallLine) {
            if (previewOccupiesExistingBuilding(
                    placement.error)) {
                drawOccupiedFootprint(
                    placement.anchor, 1, 1,
                    placement.bottomHeight,
                    buildings.cellSize);
                continue;
            }
            if (placement.topHeight <=
                placement.bottomHeight) {
                continue;
            }
            drawWall(
                placement.anchor, placement.rotation,
                placement.bottomHeight,
                placement.topHeight,
                buildings.cellSize,
                previewColor(placement.valid()),
                preview.rotationYaw);
        }
    } else if (!preview.rampLine.empty()) {
        for (const RampPlacement& placement :
             preview.rampLine) {
            if (previewOccupiesExistingBuilding(
                    placement.error)) {
                drawOccupiedRampFootprint(
                    placement, buildings.cellSize);
                continue;
            }
            if (placement.topHeight <=
                placement.bottomHeight) {
                continue;
            }
            drawRamp(
                renderer_,
                placement.anchor, placement.rotation,
                placement.bottomHeight,
                placement.topHeight,
                buildings.cellSize,
                previewColor(placement.valid()));
        }
    } else if (preview.platformFrame) {
        if (previewOccupiesExistingBuilding(
                preview.platformFrame->error)) {
            drawOccupiedFootprint(
                preview.platformFrame->anchor,
                PlatformFrameWidthCells,
                PlatformFrameWidthCells,
                preview.platformFrame->floorHeight,
                buildings.cellSize);
        } else {
            drawPlatformFramePreview(
                renderer_,
                *preview.platformFrame,
                preview.maximumWoodSupportLength);
        }
    } else if (preview.wall &&
               preview.wall->topHeight >
                   preview.wall->bottomHeight) {
        if (previewOccupiesExistingBuilding(
                preview.wall->error)) {
            drawOccupiedFootprint(
                preview.wall->anchor, 1, 1,
                preview.wall->bottomHeight,
                buildings.cellSize);
        } else {
            drawWall(
                preview.wall->anchor, preview.wall->rotation,
                preview.wall->bottomHeight,
                preview.wall->topHeight, buildings.cellSize,
                previewColor(preview.wall->valid()),
                preview.rotationYaw);
        }
    } else if (preview.ramp &&
               preview.ramp->topHeight >
                   preview.ramp->bottomHeight) {
        if (previewOccupiesExistingBuilding(
                preview.ramp->error)) {
            drawOccupiedRampFootprint(
                *preview.ramp, buildings.cellSize);
        } else {
            drawRamp(
                renderer_,
                preview.ramp->anchor,
                preview.ramp->rotation,
                preview.ramp->bottomHeight,
                preview.ramp->topHeight,
                buildings.cellSize,
                previewColor(preview.ramp->valid()),
                preview.rotationYaw
                    ? std::optional<float>{
                          -*preview.rotationYaw}
                    : std::nullopt);
        }
    } else if (preview.terrainHit) {
        DrawCylinder(
            {
                static_cast<float>(preview.terrainHit->x),
                static_cast<float>(
                    preview.terrainHit->y + 0.03),
                static_cast<float>(preview.terrainHit->z),
            },
            0.34F, 0.34F, 0.06F, 16,
            {231, 82, 76, 178});
    }
    if (translatedPreview) {
        rlPopMatrix();
    }
    if (hasGhostPreview) {
        rlDrawRenderBatchActive();
        if (renderer_ != nullptr) {
            renderer_->endGhostPreviewMaterial();
        }
        rlEnableDepthMask();
        EndBlendMode();
    }
}

void ModularBuildingRenderer::drawShadow(
    const ModularBuildingView& buildings) const {
    const auto supportVisibility =
        platformSupportVisibilityMasks(
            buildings.platformFrames,
            buildings.sharedSupports);
    const auto supportLengths =
        platformSupportVisualLengths(
            buildings.platformFrames,
            buildings.sharedSupports);
    const auto edgeVisibility =
        platformEdgeVisibilityMasks(buildings.platformFrames);
    for (std::size_t frameIndex = 0;
         frameIndex < buildings.platformFrames.size();
         ++frameIndex) {
        const PlatformFrameInstance& frame =
            buildings.platformFrames[frameIndex];
        const Vec3 origin = platformScaleOrigin(frame, buildings.animationScales);
        if (renderer_ != nullptr &&
            !renderer_->shadowCasterVisible(
                {static_cast<float>(origin.x),
                 static_cast<float>(origin.y),
                 static_cast<float>(origin.z)},
                static_cast<float>(buildings.cellSize * 1.5))) {
            continue;
        }
        drawScaled(
            origin,
            animationScale(
                frame.id, buildings.animationScales),
            [&] {
                drawPlatformFrame(
                    renderer_,
                    frame.floorHeight, frame.supports,
                    supportLengths[frameIndex],
                    WHITE, supportVisibility[frameIndex],
                    edgeVisibility[frameIndex]);
            });
    }
    for (const WallInstance& wall : buildings.walls) {
        const Vec3 origin{
            (wall.anchor.x + 0.5) * buildings.cellSize,
            wall.bottomHeight,
            (wall.anchor.z + 0.5) * buildings.cellSize,
        };
        if (renderer_ != nullptr &&
            !renderer_->shadowCasterVisible(
                {static_cast<float>(origin.x),
                 static_cast<float>(origin.y),
                 static_cast<float>(origin.z)},
                static_cast<float>(buildings.cellSize))) {
            continue;
        }
        drawScaled(
            origin,
            animationScale(
                wall.id, buildings.animationScales),
            [&] {
                drawWall(
                    wall.anchor, wall.rotation,
                    wall.bottomHeight, wall.topHeight,
                    buildings.cellSize, WHITE);
            });
    }
    for (const RampInstance& ramp : buildings.ramps) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const int widthCells =
            alongZ ? ModularRampWidthCells
                   : ModularRampRunCells;
        const int depthCells =
            alongZ ? ModularRampRunCells
                   : ModularRampWidthCells;
        const Vec3 origin{
            (ramp.anchor.x + widthCells * 0.5) *
                buildings.cellSize,
            ramp.bottomHeight,
            (ramp.anchor.z + depthCells * 0.5) *
                buildings.cellSize,
        };
        const float radius = static_cast<float>(
            buildings.cellSize * 0.5 *
            std::hypot(widthCells, depthCells));
        if (renderer_ != nullptr &&
            !renderer_->shadowCasterVisible(
                {static_cast<float>(origin.x),
                 static_cast<float>(origin.y),
                 static_cast<float>(origin.z)},
                radius)) {
            continue;
        }
        drawScaled(
            origin,
            animationScale(
                ramp.id, buildings.animationScales),
            [&] {
                drawRamp(
                    renderer_,
                    ramp.anchor, ramp.rotation,
                    ramp.bottomHeight, ramp.topHeight,
                    buildings.cellSize, WHITE);
            });
    }
}

} // namespace ian
