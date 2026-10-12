#pragma once

#include "core/Types.hpp"
#include "game/LootChestSystem.hpp"
#include "presentation/BulletAppearance.hpp"
#include "resources/ResourceSystem.hpp"

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
    ResourceHit,
    ResourceImpactDecal,
    ResourceFragment,
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
    PlayerArrivalDust,
    RamImpact,
    LootCollected,
    BuildingPlaced,
    BuildingUpgrade,
    BuildingDamaged,
    BuildingRepaired,
    RepairShockwave,
    EnemyMagicCast,
    EnemyMagicImpact,
    RoyalImpact,
    PropBreak,
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
    presentation::BulletAppearance bulletAppearance{};
    int variant{};
    std::optional<LootRarity> lootRarity;
    std::optional<LootUpgradeEffect> lootUpgradeEffect;
    std::optional<ResourceType> resourceType;
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
    double startDelayRemaining{};
    presentation::BulletAppearance bulletAppearance{};
    bool turretBullet{};
    bool shrapnel{};
    std::optional<EntityId> flashTarget;
};

struct FloatingDamageNumber {
    Vec3 position;
    double damage;
    double remaining;
    double duration;
    double startDelayRemaining{};
    float horizontalDrift;
    float verticalRise;
    float initialRotationDegrees;
    float rotationTravelDegrees;
    float scaleVariation;
    float popOvershoot;
    bool critical;
};

} // namespace ian
