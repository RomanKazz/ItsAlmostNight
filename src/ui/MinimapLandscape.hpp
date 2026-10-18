#pragma once

#include <raylib.h>
#include <cstdint>

namespace ian {
struct SimulationSnapshot;

// The landscape is static for a terrain seed; only the cropped view moves.
class MinimapLandscape {
public:
    void draw(const SimulationSnapshot& snapshot, Rectangle bounds, double radius);
    void shutdown();
    void prepare(const SimulationSnapshot& snapshot) { update(snapshot); }
private:
    void update(const SimulationSnapshot& snapshot);
    Texture2D texture_{};
    const float* samples_{};
    std::uint32_t seed_{};
    int resolution_{};
    double worldSize_{};
};
} // namespace ian
