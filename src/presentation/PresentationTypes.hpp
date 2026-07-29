#pragma once

#include "core/Types.hpp"

#include <optional>

namespace ian {

enum class PresentationEffectType {
    Hit,
    ResourceBurst,
    ResourceHitWood,
    ResourceHitStone,
    ResourceDestroyedWood,
    ResourceDestroyedStone,
    Explosion,
    Debris,
    RamImpact,
    BuildingPlaced,
    BuildingUpgrade,
    BuildingDamaged,
    BuildingRepaired,
};

struct PresentationEffect {
    PresentationEffectType type;
    std::optional<EntityId> entityId;
    Vec3 position;
    double remaining;
    double duration;
    float scale{1.0F};
};

struct DamageIndicator {
    double relativeAngle;
    double remaining;
    double duration;
    bool severe;
};

struct ArrowVisual {
    Vec3 origin;
    Vec3 target;
    double remaining;
    double duration;
};

struct FloatingDamageNumber {
    Vec3 position;
    double damage;
    double remaining;
    double duration;
    float horizontalDrift;
    bool critical;
};

} // namespace ian
