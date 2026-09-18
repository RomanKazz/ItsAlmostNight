#pragma once

#include "buildings/FoundationSystem.hpp"

#include <optional>
#include <span>
#include <vector>

namespace ian {

[[nodiscard]] std::vector<std::array<bool, 4>>
platformSupportVisibilityMasks(
    std::span<const PlatformFrameInstance> platformFrames,
    std::span<const SharedSupport> sharedSupports);
[[nodiscard]] std::vector<std::array<double, 4>>
platformSupportVisualLengths(
    std::span<const PlatformFrameInstance> platformFrames,
    std::span<const SharedSupport> sharedSupports);
[[nodiscard]] std::vector<std::array<bool, 4>>
platformEdgeVisibilityMasks(
    std::span<const PlatformFrameInstance> platformFrames);

class Renderer;

struct ModularAnimationScale {
    EntityId id;
    float scale{1.0F};
};

struct ModularBuildingView {
    ModularBuildingView(
        std::span<const PlatformFrameInstance> platformFramesValue = {},
        std::span<const WallInstance> wallsValue = {},
        std::span<const RampInstance> rampsValue = {},
        std::span<const SharedSupport> sharedSupportsValue = {},
        double cellSizeValue = 1.0,
        std::optional<EntityId> selectedValue = std::nullopt,
        std::span<const ModularAnimationScale> animationScalesValue = {},
        float alphaValue = 1.0F,
        std::optional<Vec3> cameraPositionValue = std::nullopt,
        Vec3 cameraForwardValue = {},
        double maximumDrawDistanceValue = 0.0)
        : platformFrames(platformFramesValue), walls(wallsValue),
          ramps(rampsValue), sharedSupports(sharedSupportsValue),
          cellSize(cellSizeValue), selected(selectedValue),
          animationScales(animationScalesValue), alpha(alphaValue),
          cameraPosition(cameraPositionValue),
          cameraForward(cameraForwardValue),
          maximumDrawDistance(maximumDrawDistanceValue) {}

    std::span<const PlatformFrameInstance> platformFrames;
    std::span<const WallInstance> walls;
    std::span<const RampInstance> ramps;
    std::span<const SharedSupport> sharedSupports;
    double cellSize{1.0};
    std::optional<EntityId> selected;
    std::span<const ModularAnimationScale>
        animationScales;
    float alpha{1.0F};
    std::optional<Vec3> cameraPosition;
    Vec3 cameraForward{};
    double maximumDrawDistance{};
    // A selection pass may draw only one frame, but its silhouette still
    // depends on neighbours and shared supports from the complete structure.
    std::span<const PlatformFrameInstance>
        platformVisibilityContext;
    std::span<const SharedSupport> sharedSupportVisibilityContext;
};

struct ModularBuildingPreviewView {
    const PlatformFramePlacement* platformFrame{};
    const WallPlacement* wall{};
    const RampPlacement* ramp{};
    std::span<const PlatformFramePlacement>
        platformFrameLine;
    std::span<const WallPlacement> wallLine;
    std::span<const RampPlacement> rampLine;
    const Vec3* terrainHit{};
    double maximumWoodSupportLength{2.4};
    std::optional<float> rotationYaw;
    const Vec3* visualOrigin{};
    const Vec3* visualOffset{};
};

class ModularBuildingRenderer {
  public:
    void setRenderer(Renderer* renderer);
    void drawWorld(
        const ModularBuildingView& buildings,
        const ModularBuildingPreviewView& preview) const;
    void drawShadow(
        const ModularBuildingView& buildings) const;

  private:
    Renderer* renderer_{};
};

} // namespace ian
