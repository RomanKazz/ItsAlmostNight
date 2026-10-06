#pragma once

#include <algorithm>
#include <cmath>

namespace ian {

// Restart is a deliberate hold, and must rearm on release even across runs.
class RestartHold {
public:
    static constexpr double HoldSeconds = 1.0;

    void cancel(bool keyHeld) {
        elapsed_ = 0.0;
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
        cancel(true);
        return true;
    }

    [[nodiscard]] double progress() const { return elapsed_ / HoldSeconds; }

private:
    double elapsed_{};
    bool awaitingRelease_{};
};

} // namespace ian
