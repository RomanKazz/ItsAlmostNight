#pragma once

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace ian {

inline constexpr float BuildingCardInteractionDistance = 4.5F;

[[nodiscard]] inline bool buildingCardInRange(Vector3 observer, Vector3 base) {
    const float distance = Vector3Distance(observer, base);
    return std::isfinite(distance) && distance <= BuildingCardInteractionDistance;
}

struct BuildingCardProjection {
    Vector2 anchor;
    float pixelsPerWorldUnit;
};

// Reject invalid anchors before projecting: points behind or beside the camera
// otherwise produce mirrored coordinates that screen clamping makes look valid.
[[nodiscard]] inline std::optional<BuildingCardProjection> projectBuildingCard(
    Vector3 anchor, Camera3D camera, Vector2 viewport) {
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const float depth = Vector3DotProduct(Vector3Subtract(anchor, camera.position), forward);
    if (!std::isfinite(depth) || depth < 0.35F ||
        !std::isfinite(camera.fovy) || camera.fovy <= 0.0F || camera.fovy >= 179.0F ||
        !std::isfinite(viewport.x) || !std::isfinite(viewport.y) ||
        viewport.x < 1.0F || viewport.y < 1.0F) return std::nullopt;
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    if (Vector3LengthSqr(right) < 0.5F) return std::nullopt;
    const int width = static_cast<int>(std::lround(viewport.x));
    const int height = static_cast<int>(std::lround(viewport.y));
    const Vector2 screen = GetWorldToScreenEx(anchor, camera, width, height);
    if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
        screen.x < 0.0F || screen.x > viewport.x ||
        screen.y < 0.0F || screen.y > viewport.y) return std::nullopt;
    const Vector2 unitScreen = GetWorldToScreenEx(Vector3Add(anchor, right), camera, width, height);
    const float pixelsPerUnit = Vector2Distance(screen, unitScreen);
    if (!std::isfinite(pixelsPerUnit) || pixelsPerUnit <= 0.0F) return std::nullopt;
    return BuildingCardProjection{screen, pixelsPerUnit};
}

struct BuildingCardTransform {
    float scale{1.0F};
    float offsetX{};
    float offsetY{};

    [[nodiscard]] Rectangle rect(float x, float y, float width, float height) const {
        return {offsetX + x * scale, offsetY + y * scale, width * scale, height * scale};
    }
    [[nodiscard]] Vector2 point(float x, float y) const {
        return {offsetX + x * scale, offsetY + y * scale};
    }
};

[[nodiscard]] inline BuildingCardTransform buildingCardTransform(
    BuildingCardProjection projection, Vector2 viewport, float animatedScale,
    float slideY, float panelWidth, float panelHeight, float buildingHalfWidth,
    bool isCore, float minimumWorldScale = 0.0F) {
    constexpr float Margin = 18.0F;
    const float pixelsPerWorldUnit = projection.pixelsPerWorldUnit;
    const float worldScale = std::max(minimumWorldScale,
        pixelsPerWorldUnit * (isCore ? 3.12F / 1100.0F : 2.0F / 760.0F));
    const float attachmentOffset = pixelsPerWorldUnit * (buildingHalfWidth + 0.015F);
    const float rightX = projection.anchor.x + attachmentOffset;
    const float leftEdge = projection.anchor.x - attachmentOffset;
    const float rightSpace = std::max(0.0F, viewport.x - Margin - rightX);
    const float leftSpace = std::max(0.0F, leftEdge - Margin);
    const float desiredWidth = panelWidth * std::min(worldScale, 1.0F);
    const bool placeRight = desiredWidth <= rightSpace ||
        (desiredWidth > leftSpace && rightSpace >= leftSpace);
    const float sideSpace = placeRight ? rightSpace : leftSpace;
    const float usableWidth = std::max(0.0F, viewport.x - Margin * 2.0F);
    const float minimumWidth = std::min(desiredWidth, usableWidth * 0.35F);
    // Cap the authored size and viewport coverage, including entrance overshoot.
    const float screenScale = std::min({1.0F,
        std::min(usableWidth, std::max(sideSpace, minimumWidth)) / panelWidth,
        viewport.x * (isCore ? 0.60F : 0.42F) / panelWidth,
        std::max(0.0F, viewport.y - Margin * 2.0F) * 0.78F / panelHeight});
    const float settledScale = std::max(0.0F, std::min(worldScale, screenScale));
    const float settledWidth = panelWidth * settledScale;
    const float settledX = std::clamp(placeRight ? rightX : leftEdge - settledWidth,
        Margin, std::max(Margin, viewport.x - Margin - settledWidth));
    // Keep the edge nearest the building fixed throughout opening and closing.
    const float pivotX = placeRight ? settledX : settledX + settledWidth;
    const float availableWidth = placeRight ? viewport.x - Margin - pivotX : pivotX - Margin;
    const float scale = std::max(0.0F, std::min({settledScale * std::max(0.0F, animatedScale),
        screenScale, availableWidth / panelWidth}));
    const float panelX = placeRight ? pivotX : pivotX - panelWidth * scale;
    const float scaledHeight = panelHeight * scale;
    const float panelY = std::clamp(projection.anchor.y - scaledHeight * 0.5F +
        pixelsPerWorldUnit * 0.18F + slideY * scale,
        Margin, std::max(Margin, viewport.y - Margin - scaledHeight));
    return {scale, panelX - 407.0F * scale, panelY - 284.0F * scale};
}

} // namespace ian
