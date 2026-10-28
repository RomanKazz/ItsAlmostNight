#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace ian {

enum class TutorialObjective;

struct TutorialHintVisual {
    std::optional<TutorialObjective> objective;
    float opacity{};
    float slideX{};
    float slideY{};
    float checkProgress{};
    float completionPulse{};
};

// Presentation only: a disappearing instruction is not necessarily completed
// (for example, switching back from construction to equipment).
struct TutorialHintPresentation {
    enum class Phase { Enter, Active, Complete, Exit };
    std::optional<TutorialObjective> displayed;
    Phase phase{Phase::Enter};
    double seconds{};
    bool checked{};

    void update(std::optional<TutorialObjective> wanted, bool completed, double delta) {
        if (!std::isfinite(delta) || delta <= 0) return;
        if (!displayed) {
            if (!wanted) return;
            displayed = wanted; phase = Phase::Enter; seconds = 0; checked = false;
        }
        seconds += std::min(delta, .10);
        switch (phase) {
        case Phase::Enter:
            if (seconds >= .20) { phase = Phase::Active; seconds = 0; }
            break;
        case Phase::Active:
            if (completed) { checked = true; phase = Phase::Complete; seconds = 0; }
            else if (wanted != displayed) { phase = Phase::Exit; seconds = 0; }
            break;
        case Phase::Complete:
            if (seconds >= .52) { phase = Phase::Exit; seconds = 0; }
            break;
        case Phase::Exit:
            if (seconds >= .18) {
                if (checked && wanted == displayed) wanted.reset();
                displayed = wanted; phase = Phase::Enter; seconds = 0; checked = false;
            }
            break;
        }
    }

    [[nodiscard]] TutorialHintVisual visual() const {
        TutorialHintVisual result{.objective = displayed, .opacity = displayed ? 1.0F : 0.0F};
        const auto ease = [](double value) {
            const float t = static_cast<float>(std::clamp(value, 0.0, 1.0));
            return t * t * (3 - 2 * t);
        };
        if (phase == Phase::Enter) {
            result.opacity = ease(seconds / .20);
            result.slideX = (1 - result.opacity) * 22;
            result.slideY = (1 - result.opacity) * 5;
        } else if (phase == Phase::Exit) {
            const float t = ease(seconds / .18);
            result.opacity = 1 - t; result.slideY = -10 * t;
        }
        result.checkProgress = checked ? (phase == Phase::Complete ? ease(seconds / .16) : 1.0F) : 0.0F;
        if (phase == Phase::Complete)
            result.completionPulse = static_cast<float>(std::sin(std::min(seconds / .32, 1.0) * 3.141592653589793));
        return result;
    }
};

} // namespace ian
