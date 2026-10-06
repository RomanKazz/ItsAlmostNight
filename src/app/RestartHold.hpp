#pragma once

#include <algorithm>
#include <cmath>

namespace ian {

// Holding continuously repeats restarts; menus still require a fresh press.
class RestartHold {
public:
    static constexpr double HoldSeconds = 1.0;

    void resetProgress() { elapsed_ = 0.0; }

    void cancel(bool keyHeld) {
        resetProgress();
        awaitingRelease_ = keyHeld;
    }

    [[nodiscard]] bool update(bool keyHeld, bool allowed, double frameSeconds) {
        if (!keyHeld || !allowed) {
            cancel(keyHeld);
            return false;
        }
        if (awaitingRelease_) return false;
        if (std::isfinite(frameSeconds)) {
            // A stalled frame must not turn a short press into a restart.
            elapsed_ += std::clamp(frameSeconds, 0.0, 0.1);
        }
        if (elapsed_ + 1e-9 < HoldSeconds) return false;
        resetProgress();
        return true;
    }

    [[nodiscard]] double progress() const { return elapsed_ / HoldSeconds; }

private:
    double elapsed_{};
    bool awaitingRelease_{};
};

} // namespace ian
