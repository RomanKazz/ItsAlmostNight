#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace ian {

void App::updateHoverTarget(const SimulationSnapshot& snapshot,
                            double frameSeconds) {
    constexpr double HoverGraceSeconds = 0.2;
    const auto clearHover = [this]() {
        hoveredResource_.reset();
        interactionResourceAim_.reset();
        hoveredBuilding_.reset();
        hoveredEnemy_.reset();
        hoveredBuildingUpgradeCost_.reset();
        hoveredBuildingStats_.reset();
        hoverGraceRemaining_ = 0.0;
        buildingHoverSeconds_ = 0.0;
    };
    if (snapshot.selectedBuilding) {
        clearHover();
        return;
    }

    // Combat already gives an aimed enemy priority over gathering. Mirror
    // that priority in presentation so a resource behind an enemy cannot
    // steal both its outline and health bar.
    if (snapshot.aimedEnemy) {
        hoveredResource_.reset();
        interactionResourceAim_.reset();
        hoveredBuilding_.reset();
        hoveredEnemy_ = snapshot.aimedEnemy;
        hoveredBuildingUpgradeCost_.reset();
        hoveredBuildingStats_.reset();
        buildingHoverSeconds_ = 0.0;
        hoverGraceRemaining_ = HoverGraceSeconds;
        return;
    }

    std::optional<EntityId> visualResource = snapshot.aimedResource;
    if (interactionResourceAim_) {
        const bool stillActive = std::any_of(
            snapshot.resourceNodes.begin(), snapshot.resourceNodes.end(),
            [this](const ResourceNode& resource) {
                return resource.active &&
                       resource.id == *interactionResourceAim_;
            });
        if (stillActive) {
            visualResource = interactionResourceAim_;
        }
    }
    if (visualResource) {
        hoveredResource_ = visualResource;
        hoveredBuilding_.reset();
        hoveredEnemy_.reset();
        hoveredBuildingUpgradeCost_.reset();
        hoveredBuildingStats_.reset();
        buildingHoverSeconds_ = 0.0;
        hoverGraceRemaining_ = HoverGraceSeconds;
        return;
    }
    if (snapshot.aimedBuilding) {
        if (hoveredBuilding_ == snapshot.aimedBuilding) {
            buildingHoverSeconds_ += frameSeconds;
        } else {
            buildingHoverSeconds_ = frameSeconds;
        }
        hoveredResource_.reset();
        hoveredBuilding_ = snapshot.aimedBuilding;
        hoveredEnemy_.reset();
        hoveredBuildingUpgradeCost_ =
            snapshot.aimedBuildingUpgradeCost;
        hoveredBuildingStats_ =
            snapshot.aimedBuildingStats;
        if (buildingContextCardTarget_ ==
            snapshot.aimedBuilding) {
            buildingContextCardUpgradeCost_ =
                snapshot.aimedBuildingUpgradeCost;
            buildingContextCardStats_ =
                snapshot.aimedBuildingStats;
        }
        hoverGraceRemaining_ = HoverGraceSeconds;
        return;
    }
    hoverGraceRemaining_ =
        std::max(0.0, hoverGraceRemaining_ - frameSeconds);
    const bool resourceValid =
        !hoveredResource_ ||
        std::any_of(
            snapshot.resourceNodes.begin(), snapshot.resourceNodes.end(),
            [this](const ResourceNode& resource) {
                return resource.active &&
                       resource.id == *hoveredResource_;
            });
    const bool buildingValid =
        !hoveredBuilding_ ||
        std::any_of(
            snapshot.buildings.begin(), snapshot.buildings.end(),
            [this](const BuildingInstance& building) {
                return building.id == *hoveredBuilding_;
            });
    const bool enemyValid =
        !hoveredEnemy_ ||
        std::any_of(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [this](const EnemyInstance& enemy) {
                return enemy.active && enemy.id == *hoveredEnemy_;
            });
    if (hoverGraceRemaining_ <= 0.0 || !resourceValid ||
        !buildingValid || !enemyValid) {
        clearHover();
    }
}

void App::startEnemyHitFlash(EntityId id, double elapsedSeconds) {
    const std::uint64_t key =
        (static_cast<std::uint64_t>(id.generation) << 32U) |
        static_cast<std::uint64_t>(id.index);
    auto [flash, inserted] = enemyHitFlashById_.try_emplace(key);
    if (inserted || elapsedSeconds >= flash->second.nextAllowedAt) {
        flash->second.startedAt = elapsedSeconds;
        flash->second.nextAllowedAt =
            elapsedSeconds + EnemyHitFlashInterval;
    }
}

void App::chooseHarvestSwingAnimation(FirstPersonToolVisual visual) {
    toolSwingAlternateAnimation_ =
        renderer_ &&
        (visual == FirstPersonToolVisual::Axe ||
         visual == FirstPersonToolVisual::Pickaxe ||
         visual == FirstPersonToolVisual::Club) &&
        renderer_->hasAlternateHarvestSwingAnimation() &&
        GetRandomValue(0, 1) != 0;
}

void App::addEffect(PresentationEffectType type, Vec3 position,
                    double duration, float scale,
                    std::optional<EntityId> entityId,
                    double startDelay) {
    constexpr std::size_t MaxEffects = 128;
    if (effects_.size() >= MaxEffects) {
        const auto disposableImpact = std::find_if(
            effects_.begin(), effects_.end(),
            [](const PresentationEffect& effect) {
                return effect.type == PresentationEffectType::Hit ||
                    effect.type ==
                        PresentationEffectType::EnemyHitImpact;
            });
        // High-volume hit flashes may replace one another, but must not evict
        // an explosion, loot pickup, building cue, or other rare feedback.
        effects_.erase(disposableImpact != effects_.end()
            ? disposableImpact : effects_.begin());
    }
    effects_.push_back({
        .type = type,
        .entityId = entityId,
        .position = position,
        .remaining = duration,
        .duration = duration,
        .startDelayRemaining =
            std::max(0.0, startDelay),
        .scale = scale,
    });
}

void App::addLootPickupEffect(
    Vec3 position, LootRarity rarity,
    LootUpgradeEffect effect,
    std::optional<EntityId> lootId) {
    constexpr std::size_t MaxEffects = 128;
    if (effects_.size() >= MaxEffects) {
        effects_.erase(effects_.begin());
    }
    constexpr double Duration = 0.96;
    effects_.push_back({
        .type = PresentationEffectType::LootCollected,
        .entityId = lootId,
        .position = position,
        .remaining = Duration,
        .duration = Duration,
        .scale = rarity == LootRarity::Legendary
            ? 1.34F
            : rarity == LootRarity::Rare
            ? 1.18F
            : rarity == LootRarity::Uncommon ? 1.02F : 0.88F,
        .lootRarity = rarity,
        .lootUpgradeEffect = effect,
    });
}

void App::addCameraShake(double duration, double strength) {
    cameraShakeRemaining_ = std::max(cameraShakeRemaining_, duration);
    cameraShakeStrength_ = std::max(cameraShakeStrength_, strength);
}

void App::addCameraImpulse(Vec3 localOffset) {
    cameraImpulseOffset_.x = std::clamp(
        cameraImpulseOffset_.x + localOffset.x,
        -0.06, 0.06);
    cameraImpulseOffset_.y = std::clamp(
        cameraImpulseOffset_.y + localOffset.y,
        -0.07, 0.07);
    cameraImpulseOffset_.z = std::clamp(
        cameraImpulseOffset_.z + localOffset.z,
        -0.06, 0.06);
}

void App::addDamageIndicator(Vec3 sourcePosition,
                             const SimulationSnapshot& snapshot, bool severe) {
    const double offsetX = sourcePosition.x - snapshot.playerPosition.x;
    const double offsetZ = sourcePosition.z - snapshot.playerPosition.z;
    const double worldAngle = std::atan2(offsetX, -offsetZ);
    const double relativeAngle =
        std::atan2(std::sin(worldAngle - snapshot.playerYaw),
                   std::cos(worldAngle - snapshot.playerYaw));
    for (auto& indicator : damageIndicators_) {
        const double difference =
            std::atan2(std::sin(indicator.relativeAngle - relativeAngle),
                       std::cos(indicator.relativeAngle - relativeAngle));
        if (std::abs(difference) < 0.2) {
            indicator.relativeAngle = relativeAngle;
            indicator.severe = indicator.severe || severe;
            indicator.duration = indicator.severe ? 1.4 : 1.0;
            indicator.remaining = indicator.duration;
            return;
        }
    }
    constexpr std::size_t MaxDamageIndicators = 12;
    if (damageIndicators_.size() >= MaxDamageIndicators) {
        damageIndicators_.erase(damageIndicators_.begin());
    }
    const double duration = severe ? 1.4 : 1.0;
    damageIndicators_.push_back({
        .relativeAngle = relativeAngle,
        .remaining = duration,
        .duration = duration,
        .severe = severe,
    });
}

void App::addFloatingDamageNumber(
    Vec3 position, double damage, bool critical) {
    constexpr std::size_t MaximumNumbers = 32;
    if (floatingDamageNumbers_.size() >= MaximumNumbers) {
        floatingDamageNumbers_.erase(
            floatingDamageNumbers_.begin());
    }
    std::uint32_t randomState =
        (++floatingDamageSequence_) * 0x9e3779b9U;
    randomState ^= static_cast<std::uint32_t>(
        std::lround((position.x + 256.0) * 47.0));
    randomState ^= static_cast<std::uint32_t>(
        std::lround((position.z + 256.0) * 71.0)) << 1U;
    const auto random01 = [&randomState]() {
        randomState ^= randomState << 13U;
        randomState ^= randomState >> 17U;
        randomState ^= randomState << 5U;
        return static_cast<float>(randomState & 0xffffU) /
            65535.0F;
    };
    const float direction =
        (floatingDamageSequence_ % 2U) == 0U ? -1.0F : 1.0F;
    const double duration = critical
        ? 0.62 + static_cast<double>(random01()) * 0.10
        : 0.50 + static_cast<double>(random01()) * 0.10;
    const float initialRotation =
        direction * (3.0F + random01() * 6.0F);
    const float rotationTravel =
        -direction * (7.0F + random01() * 8.0F);
    floatingDamageNumbers_.push_back({
        .position = position,
        .damage = damage,
        .remaining = duration,
        .duration = duration,
        .horizontalDrift = direction *
            ((critical ? 23.0F : 15.0F) + random01() * 14.0F),
        .verticalRise =
            (critical ? 0.82F : 0.64F) + random01() * 0.20F,
        .initialRotationDegrees = initialRotation,
        .rotationTravelDegrees = rotationTravel,
        .scaleVariation = 0.90F + random01() * 0.20F,
        .popOvershoot = 1.15F + random01() * 0.45F,
        .critical = critical,
    });
}

void App::addResourceGainVisual(
    ResourceType type, Vec3 position, int amount) {
    constexpr std::size_t MaximumVisuals = 16;
    if (resourceGainVisuals_.size() >= MaximumVisuals) {
        resourceGainVisuals_.erase(resourceGainVisuals_.begin());
    }
    constexpr double Duration = ResourcePickupFlightSeconds;
    std::uint32_t seed = static_cast<std::uint32_t>(
        resourceGainVisuals_.size() + 1U) * 0x9e3779b9U;
    seed ^= static_cast<std::uint32_t>(
        std::lround((position.x + 192.0) * 37.0));
    seed ^= static_cast<std::uint32_t>(
        std::lround((position.z + 192.0) * 53.0)) << 1U;
    const auto variation = [](std::uint32_t value) {
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        value ^= value >> 16U;
        return static_cast<float>(value & 0xffffU) / 65535.0F;
    };
    resourceGainVisuals_.push_back({
        .type = type,
        .position = position,
        .amount = amount,
        .remaining = Duration,
        .duration = Duration,
        .horizontalDrift =
            (variation(seed) * 2.0F - 1.0F) * 34.0F,
        .visualScale =
            0.88F + variation(seed ^ 0xa511e9b3U) * 0.24F,
        .rotationDegrees =
            (variation(seed ^ 0x68e31da4U) * 2.0F - 1.0F) * 8.0F,
    });
}


} // namespace ian
