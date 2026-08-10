#pragma once

#include "core/Types.hpp"
#include "game/LootChestSystem.hpp"

#include <optional>

namespace ian {

enum class ActionMode {
    Tools,
    Weapons,
    Buildings,
    Modular,
};

[[nodiscard]] inline constexpr const char*
actionModeLabel(ActionMode mode) {
    switch (mode) {
    case ActionMode::Tools: return "TOOLS";
    case ActionMode::Weapons: return "WEAPONS";
    case ActionMode::Buildings: return "BUILDINGS";
    case ActionMode::Modular: return "MODULAR";
    }
    return "TOOLS";
}

enum class PresentationEffectType {
    Hit,
    ResourceBurst,
    ResourceHitWood,
    ResourceHitStone,
    ResourceDestroyedWood,
    ResourceDestroyedStone,
    Explosion,
    SplitBurst,
    IceImpact,
    IceCrack,
    Debris,
    LandingDust,
    RamImpact,
    LootCollected,
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
    double startDelayRemaining{};
    float scale{1.0F};
    std::optional<LootRarity> lootRarity;
    std::optional<LootUpgradeEffect> lootUpgradeEffect;
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
