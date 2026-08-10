#pragma once

#include "presentation/PresentationTypes.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace ian::presentation {

[[nodiscard]] inline double timelineProgress(
    double remaining, double duration) {
    return std::clamp(
        1.0 - remaining / std::max(0.01, duration),
        0.0, 1.0);
}

inline void advanceTimeline(
    std::vector<PresentationEffect>& effects,
    double deltaSeconds) {
    for (PresentationEffect& effect : effects) {
        const double activeDelta =
            std::max(
                0.0,
                deltaSeconds -
                    effect.startDelayRemaining);
        effect.startDelayRemaining =
            std::max(
                0.0,
                effect.startDelayRemaining -
                    deltaSeconds);
        effect.remaining =
            std::max(
                0.0,
                effect.remaining -
                    activeDelta);
    }
    std::erase_if(
        effects,
        [](const PresentationEffect& effect) {
            return effect.startDelayRemaining <= 0.0 &&
                   effect.remaining <= 0.0;
        });
}

template <typename Visual>
void advanceTimeline(
    std::vector<Visual>& visuals, double deltaSeconds) {
    for (Visual& visual : visuals) {
        visual.remaining =
            std::max(0.0, visual.remaining - deltaSeconds);
    }
    std::erase_if(
        visuals,
        [](const Visual& visual) {
            return visual.remaining <= 0.0;
        });
}

template <typename Visual>
void advanceTimeline(
    std::optional<Visual>& visual, double deltaSeconds) {
    if (!visual) {
        return;
    }
    visual->remaining =
        std::max(0.0, visual->remaining - deltaSeconds);
    if (visual->remaining <= 0.0) {
        visual.reset();
    }
}

} // namespace ian::presentation
