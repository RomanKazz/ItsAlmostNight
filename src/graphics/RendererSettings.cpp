#include "graphics/Renderer.hpp"

#include <algorithm>
#include <array>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

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

void Renderer::applyFrameRateLimit() {
#if defined(__APPLE__)
    // Renderer owns presentation pacing. Disable raylib's usleep limiter.
    SetTargetFPS(0);
    ClearWindowState(FLAG_VSYNC_HINT);
    pacedFrameRate_ = 0;
    frameDeadline_ = 0U;
#else
    SetTargetFPS(settings_.frameRateLimit);
#endif
}

void Renderer::paceFrame() {
    framePacingMilliseconds_ = 0.0;
#if defined(__APPLE__)
    const int framesPerSecond = settings_.frameRateLimit;
    if (framesPerSecond <= 0) {
        pacedFrameRate_ = 0;
        frameDeadline_ = 0U;
        return;
    }

    if (machTimebaseNumer_ == 0U || machTimebaseDenom_ == 0U) {
        mach_timebase_info_data_t timebase{};
        static_cast<void>(mach_timebase_info(&timebase));
        machTimebaseNumer_ = std::max(timebase.numer, 1U);
        machTimebaseDenom_ = std::max(timebase.denom, 1U);
    }
    const auto nanosecondsToTicks = [this](std::uint64_t nanoseconds) {
        return nanoseconds * machTimebaseDenom_ / machTimebaseNumer_;
    };

    const std::uint64_t now = mach_absolute_time();
    if (pacedFrameRate_ != framesPerSecond || frameDeadline_ == 0U) {
        pacedFrameRate_ = framesPerSecond;
        frameDeadline_ = now;
    }
    const std::uint64_t period = nanosecondsToTicks(
        1'000'000'000ULL /
        static_cast<std::uint64_t>(framesPerSecond));
    const std::uint64_t target = frameDeadline_ + period;
    if (now >= target) {
        // A slow frame or focus change starts a new cadence instead of
        // creating several short catch-up frames.
        frameDeadline_ = now;
        return;
    }

    // mach_wait_until can oversleep enough to be visible at 60 Hz. Sleep for
    // the coarse part, then use a very short spin only for the precision tail.
    constexpr std::uint64_t SpinTailNanoseconds = 400'000ULL;
    const std::uint64_t spinTail = nanosecondsToTicks(
        SpinTailNanoseconds);
    const std::uint64_t coarseTarget = target > spinTail
        ? target - spinTail
        : target;
    if (now < coarseTarget) {
        static_cast<void>(mach_wait_until(coarseTarget));
    }

    std::uint64_t presentedAt = mach_absolute_time();
    while (presentedAt < target) {
        presentedAt = mach_absolute_time();
    }

    // Do not compensate a scheduler oversleep with a visibly short frame.
    constexpr std::uint64_t LateToleranceNanoseconds = 250'000ULL;
    const std::uint64_t lateTolerance = nanosecondsToTicks(
        LateToleranceNanoseconds);
    frameDeadline_ = presentedAt > target + lateTolerance
        ? presentedAt
        : target;
    const std::uint64_t pacingTicks = presentedAt - now;
    framePacingMilliseconds_ =
        static_cast<double>(pacingTicks) *
        static_cast<double>(machTimebaseNumer_) /
        static_cast<double>(machTimebaseDenom_) /
        1'000'000.0;
#endif
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
