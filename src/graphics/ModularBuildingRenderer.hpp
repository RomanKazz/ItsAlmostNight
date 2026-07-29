#pragma once

#include "buildings/FoundationSystem.hpp"

#include <optional>
#include <span>

namespace ian {

struct ModularAnimationScale {
    EntityId id;
    float scale{1.0F};
};

struct ModularBuildingView {
    std::span<const PlatformFrameInstance> platformFrames;
    std::span<const WallInstance> walls;
    std::span<const RampInstance> ramps;
    std::span<const SharedSupport> sharedSupports;
    double cellSize{1.0};
    std::optional<EntityId> selected;
    std::span<const ModularAnimationScale>
        animationScales;
    float alpha{1.0F};
};

struct ModularBuildingPreviewView {
    const PlatformFramePlacement* platformFrame{};
    const WallPlacement* wall{};
    const RampPlacement* ramp{};
    std::span<const PlatformFramePlacement>
        platformFrameLine;
    std::span<const WallPlacement> wallLine;
    const Vec3* terrainHit{};
    double maximumWoodSupportLength{2.4};
    std::optional<float> rotationYaw;
    const Vec3* visualOrigin{};
    std::span<const PlatformFramePlacement>
        platformFrameColumn{};
};

class ModularBuildingRenderer {
  public:
    void drawWorld(
        const ModularBuildingView& buildings,
        const ModularBuildingPreviewView& preview) const;
    void drawShadow(
        const ModularBuildingView& buildings) const;
};

} // namespace ian
