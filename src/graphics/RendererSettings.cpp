#include "graphics/Renderer.hpp"

#include <algorithm>
#include <array>

namespace ian {
void Renderer::cycleQuality() {
    switch (settings_.quality) {
    case GraphicsQuality::Low:
        settings_.quality = GraphicsQuality::Medium;
        settings_.shadowMapSize = 1024;
        settings_.shadowDistance = 44.0F;
        break;
    case GraphicsQuality::Medium:
        settings_.quality = GraphicsQuality::High;
        settings_.shadowMapSize = 2048;
        settings_.shadowDistance = 55.0F;
        break;
    case GraphicsQuality::High:
        settings_.quality = GraphicsQuality::Low;
        settings_.shadowMapSize = 512;
        settings_.shadowDistance = 32.0F;
        break;
    }
}

void Renderer::cycleShadowQuality() {
    if (settings_.shadowMapSize < 1024) {
        settings_.shadowMapSize = 1024;
    } else if (settings_.shadowMapSize < 2048) {
        settings_.shadowMapSize = 2048;
    } else {
        settings_.shadowMapSize = 512;
    }
}

void Renderer::cycleFrameRateLimit() {
    switch (settings_.frameRateLimit) {
    case 60:
        settings_.frameRateLimit = 120;
        break;
    case 120:
        settings_.frameRateLimit = 144;
        break;
    case 144:
        settings_.frameRateLimit = 0;
        break;
    default:
        settings_.frameRateLimit = 60;
        break;
    }
    applyFrameRateLimit();
}

void Renderer::applyFrameRateLimit() const {
    SetTargetFPS(settings_.frameRateLimit);
}

void Renderer::cycleAoStrength() {
    if (settings_.aoStrength < 0.1F) {
        settings_.aoStrength = 0.2F;
    } else if (settings_.aoStrength < 0.25F) {
        settings_.aoStrength = 0.3F;
    } else if (settings_.aoStrength < 0.34F) {
        settings_.aoStrength = 0.35F;
    } else {
        settings_.aoStrength = 0.0F;
    }
}

void Renderer::adjustPixelSize(int direction) {
    constexpr int PixelSizes[]{1, 2, 3, 4, 6, 8};
    const auto current =
        std::lower_bound(std::begin(PixelSizes), std::end(PixelSizes),
                         settings_.pixelSize);
    const auto currentIndex =
        current == std::end(PixelSizes)
            ? static_cast<int>(std::size(PixelSizes)) - 1
            : static_cast<int>(current - std::begin(PixelSizes));
    const int nextIndex =
        std::clamp(currentIndex + direction, 0,
                   static_cast<int>(std::size(PixelSizes)) - 1);
    settings_.pixelSize = PixelSizes[nextIndex];
}

} // namespace ian
