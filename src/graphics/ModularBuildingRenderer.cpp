#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/Renderer.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {
namespace {

constexpr Color platformFrameColor{174, 121, 67, 255};
constexpr Color wallColor{179, 105, 55, 255};
constexpr Color rampColor{211, 166, 101, 255};
constexpr Color validPreviewColor{109, 229, 142, 168};
constexpr Color warningPreviewColor{242, 188, 72, 168};
constexpr Color invalidPreviewColor{231, 82, 76, 168};
constexpr Color unsupportedColor{204, 55, 51, 255};
constexpr Color selectedColor{255, 229, 132, 255};

bool previewObstructed(ModularPlacementError error) {
    return error == ModularPlacementError::Occupied ||
           error == ModularPlacementError::ResourceBlocked;
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
    Color color, bool includeSupports) {
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
    if (includeSupports && renderer != nullptr &&
        renderer->drawPlatformFrameModel(
            {centerX, static_cast<float>(floorHeight),
             centerZ},
            color, widthX * 0.5F)) {
        return;
    }
    DrawCube(
        {centerX, static_cast<float>(floorHeight - 0.09),
         centerZ},
        widthX, 0.18F, widthZ, color);
    const float beamY =
        static_cast<float>(floorHeight - 0.24);
    constexpr float BeamThickness = 0.14F;
    DrawCube(
        {centerX, beamY, static_cast<float>(minimumZ)},
        widthX, BeamThickness, BeamThickness, color);
    DrawCube(
        {centerX, beamY, static_cast<float>(maximumZ)},
        widthX, BeamThickness, BeamThickness, color);
    DrawCube(
        {static_cast<float>(minimumX), beamY, centerZ},
        BeamThickness, BeamThickness, widthZ, color);
    DrawCube(
        {static_cast<float>(maximumX), beamY, centerZ},
        BeamThickness, BeamThickness, widthZ, color);

    if (!includeSupports) {
        return;
    }
    for (const FoundationSupport& support : supports) {
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

void drawRamp(GridCoord anchor, Rotation rotation,
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
    rlPushMatrix();
    rlTranslatef(
        static_cast<float>(
            (anchor.x + widthCells * 0.5) * cellSize),
        static_cast<float>(
            (bottomHeight + topHeight) * 0.5),
        static_cast<float>(
            (anchor.z + depthCells * 0.5) * cellSize));
    rlRotatef(yaw * RAD2DEG, 0.0F, 1.0F, 0.0F);
    rlRotatef(-angle, 1.0F, 0.0F, 0.0F);
    DrawCube({0.0F, 0.0F, 0.0F},
             width, 0.16F, length,
             color);
    rlPopMatrix();
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
    drawPlatformFrame(
        renderer,
        placement.floorHeight, placement.supports,
        color, true);
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

} // namespace

void ModularBuildingRenderer::setRenderer(
    Renderer* renderer) {
    renderer_ = renderer;
}

void ModularBuildingRenderer::drawWorld(
    const ModularBuildingView& buildings,
    const ModularBuildingPreviewView& preview) const {
    for (const PlatformFrameInstance& frame :
         buildings.platformFrames) {
        const double bottom = std::min({
            frame.supports[0].bottom.y,
            frame.supports[1].bottom.y,
            frame.supports[2].bottom.y,
            frame.supports[3].bottom.y,
        });
        const Vec3 origin{
            (frame.anchor.x + 1.0) *
                buildings.cellSize,
            bottom,
            (frame.anchor.z + 1.0) *
                buildings.cellSize,
        };
        drawScaled(
            origin,
            animationScale(
                frame.id, buildings.animationScales),
            [&] {
                drawPlatformFrame(
                    renderer_,
                    frame.floorHeight, frame.supports,
                    Fade(
                        instanceColor(
                            frame.id,
                            frame.supportState,
                            platformFrameColor,
                            buildings.selected),
                        buildings.alpha),
                    true);
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
        drawScaled(
            origin,
            animationScale(
                ramp.id, buildings.animationScales),
            [&] {
                drawRamp(
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

    const bool obstructedSinglePreview =
        (preview.platformFrame &&
         previewObstructed(preview.platformFrame->error)) ||
        (preview.wall &&
         previewObstructed(preview.wall->error)) ||
        (preview.ramp &&
         previewObstructed(preview.ramp->error));
    bool translatedPreview = false;
    if (preview.visualOrigin &&
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
            if (previewObstructed(placement.error)) {
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
            if (previewObstructed(placement.error)) {
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
            if (previewObstructed(placement.error)) {
                drawOccupiedRampFootprint(
                    placement, buildings.cellSize);
                continue;
            }
            if (placement.topHeight <=
                placement.bottomHeight) {
                continue;
            }
            drawRamp(
                placement.anchor, placement.rotation,
                placement.bottomHeight,
                placement.topHeight,
                buildings.cellSize,
                previewColor(placement.valid()));
        }
    } else if (preview.platformFrame) {
        if (previewObstructed(
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
        if (previewObstructed(preview.wall->error)) {
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
        if (previewObstructed(preview.ramp->error)) {
            drawOccupiedRampFootprint(
                *preview.ramp, buildings.cellSize);
        } else {
            drawRamp(
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
}

void ModularBuildingRenderer::drawShadow(
    const ModularBuildingView& buildings) const {
    for (const PlatformFrameInstance& frame :
         buildings.platformFrames) {
        const double bottom = std::min({
            frame.supports[0].bottom.y,
            frame.supports[1].bottom.y,
            frame.supports[2].bottom.y,
            frame.supports[3].bottom.y,
        });
        drawScaled(
            {
                (frame.anchor.x + 1.0) *
                    buildings.cellSize,
                bottom,
                (frame.anchor.z + 1.0) *
                    buildings.cellSize,
            },
            animationScale(
                frame.id, buildings.animationScales),
            [&] {
                drawPlatformFrame(
                    renderer_,
                    frame.floorHeight, frame.supports,
                    WHITE, true);
            });
    }
    for (const WallInstance& wall : buildings.walls) {
        drawScaled(
            {
                (wall.anchor.x + 0.5) *
                    buildings.cellSize,
                wall.bottomHeight,
                (wall.anchor.z + 0.5) *
                    buildings.cellSize,
            },
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
        drawScaled(
            {
                (ramp.anchor.x + widthCells * 0.5) *
                    buildings.cellSize,
                ramp.bottomHeight,
                (ramp.anchor.z + depthCells * 0.5) *
                    buildings.cellSize,
            },
            animationScale(
                ramp.id, buildings.animationScales),
            [&] {
                drawRamp(
                    ramp.anchor, ramp.rotation,
                    ramp.bottomHeight, ramp.topHeight,
                    buildings.cellSize, WHITE);
            });
    }
}

} // namespace ian
