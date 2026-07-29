#pragma once

#include <algorithm>
#include <optional>
#include <vector>

namespace ian::presentation {

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
