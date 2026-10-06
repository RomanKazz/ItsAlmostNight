#pragma once

#include <algorithm>
#include <cmath>

namespace ian::presentation {

// A visual drop only: the simulation stays at its safe spawn position.
class PlayerArrival {
public:
    static constexpr double FallSeconds = 1.05;
    static constexpr double SettleSeconds = 0.50;
    static constexpr double Duration = FallSeconds + SettleSeconds;

    struct CameraMotion {
        double height{};
        double compression{};
        double pitch{};
        double roll{};
        float fov{};
    };

    void start() { elapsed_ = 0.0; }
    void reset() { elapsed_ = Duration; }
    [[nodiscard]] bool active() const { return elapsed_ < Duration; }

    // Crossing the impact boundary emits exactly one puff, even on a long frame.
    [[nodiscard]] bool advance(double seconds) {
        const double previous = elapsed_;
        elapsed_ = std::min(Duration, elapsed_ + std::max(0.0, seconds));
        return previous < FallSeconds && elapsed_ >= FallSeconds;
    }

    [[nodiscard]] CameraMotion cameraMotion() const {
        if (!active()) return {};
        constexpr double Pi = 3.14159265358979323846;
        if (elapsed_ < FallSeconds) {
            const double progress = elapsed_ / FallSeconds;
            const double ease = progress * progress * (3.0 - 2.0 * progress);
            return {
                .height = 4.6 * (1.0 - std::pow(progress, 2.1)),
                .pitch = -0.08 - 0.12 * ease,
                .roll = 0.018 * std::sin(progress * Pi),
                .fov = static_cast<float>(2.5 * ease),
            };
        }
        const double progress = (elapsed_ - FallSeconds) / SettleSeconds;
        const double ease = progress * progress * (3.0 - 2.0 * progress);
        return {
            .compression = -0.24 * std::sin(progress * Pi) *
                std::exp(-1.3 * progress),
            .pitch = -0.20 * (1.0 - ease) +
                0.035 * std::sin(progress * 2.0 * Pi) * (1.0 - progress),
            .roll = 0.020 * std::sin(progress * 2.0 * Pi) * (1.0 - progress),
            .fov = static_cast<float>(2.5 * (1.0 - ease)),
        };
    }

private:
    double elapsed_{Duration};
};

} // namespace ian::presentation
