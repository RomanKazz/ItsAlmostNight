#pragma once

#include "core/Types.hpp"
#include "game/LootChestSystem.hpp"

#include <optional>

namespace ian {

enum class ActionMode {
    Equipment,
    Buildings,
};

[[nodiscard]] inline constexpr bool actionModeUsesEquipment(
    ActionMode mode) {
    return mode == ActionMode::Equipment;
}

[[nodiscard]] inline constexpr const char*
actionModeLabel(ActionMode mode) {
    switch (mode) {
    case ActionMode::Equipment: return "EQUIPMENT";
    case ActionMode::Buildings: return "BUILDINGS";
    }
    return "EQUIPMENT";
}

enum class PresentationEffectType {
    Hit,
    EnemyHitImpact,
    ResourceBurst,
    ResourceHitWood,
    ResourceHitStone,
    ResourceDestroyedWood,
    ResourceDestroyedStone,
    Explosion,
    EnemyBurn,
    SplitBurst,
    IceImpact,
    IceCrack,
    FireImpact,
    ChainLightning,
    SawSplinter,
    EliteSpawn,
    VolatileCharge,
    Debris,
    LandingDust,
    RamImpact,
    LootCollected,
    BuildingPlaced,
    BuildingUpgrade,
    BuildingDamaged,
    BuildingRepaired,
    RepairShockwave,
};

struct PresentationEffect {
    PresentationEffectType type;
    std::optional<EntityId> entityId;
    Vec3 position;
    std::optional<Vec3> targetPosition;
    double remaining;
    double duration;
    double startDelayRemaining{};
    float scale{1.0F};
    int variant{};
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
    Vec3 direction;
    double remaining;
    double duration;
    bool turretBullet{};
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
