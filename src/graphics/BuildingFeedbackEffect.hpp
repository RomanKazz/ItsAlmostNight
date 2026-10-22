#pragma once

#include <raylib.h>
#include <cstdint>

namespace ian {

enum class BuildingFeedbackKind { Repair, Upgrade };

struct BuildingFeedbackHighlight {
    // World height of the moving band, half width, and blend strength.
    Vector3 sweep{};
    Vector3 tint{};
};

[[nodiscard]] BuildingFeedbackHighlight sampleBuildingFeedbackHighlight(
    float progress, float bottomY, float topY, BuildingFeedbackKind kind);

// Draw inside a 3D color-only pass. Scene depth still hides symbols behind
// buildings, while their translucent geometry leaves the depth buffer intact.
void drawBuildingFeedbackEffect(
    Vector3 origin, Vector3 cameraPosition, float progress, float scale,
    BuildingFeedbackKind kind, std::uint32_t seed);

} // namespace ian
