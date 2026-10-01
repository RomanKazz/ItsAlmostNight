#pragma once

namespace ian {

// A run chooses one attack style for all cannon instances.
enum class CannonAttackStyle { Standard, Heavy, Scatter, Cluster };

struct CannonAttackProfile {
    double damage{1.0};
    double interval{1.0};
    double radius{1.0};
    double visualScale{1.0};
    int projectileCount{1};
    int clusterCount{};
};

[[nodiscard]] constexpr CannonAttackProfile cannonAttackProfile(CannonAttackStyle style) {
    switch (style) {
    case CannonAttackStyle::Heavy: return {5.0, 3.0, 1.0, 1.7, 1, 0};
    case CannonAttackStyle::Scatter: return {0.4, 1.0, 0.55, 0.65, 5, 0};
    case CannonAttackStyle::Cluster: return {0.8, 1.0, 1.0, 1.15, 1, 3};
    case CannonAttackStyle::Standard: return {};
    }
    return {};
}

} // namespace ian
