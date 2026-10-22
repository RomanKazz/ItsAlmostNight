#include "game/Simulation.hpp"

#include "core/DeterministicRandom.hpp"
#include "core/Geometry.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ResourceWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double LootProximityPickupRadius = 2.0;

[[nodiscard]] constexpr bool isMeleeAttackWeapon(
    PlayerWeapon weapon) {
    return weapon == PlayerWeapon::Axe ||
           weapon == PlayerWeapon::Pickaxe ||
           weapon == PlayerWeapon::Club;
}

} // namespace

int Simulation::resourceCapacity(ResourceCapacityType resource) const {
    const auto core = buildings_.core();
    int capacity = 0;
    if (core) {
        const double base = coreResourceCapacity(resource,
            static_cast<std::uint8_t>(coreStorageBaseLevel_));
        capacity = static_cast<int>(std::min(
            static_cast<double>(std::numeric_limits<int>::max()),
            std::ceil(base * (1.0 + 0.20 * coreCardPowers_[3]))));
    } else if (resource == ResourceCapacityType::Wood) {
        capacity = 60;
    } else if (resource == ResourceCapacityType::Stone) {
        capacity = 30;
    } else if (resource == ResourceCapacityType::Crystal) {
        capacity = 10;
    }
    if (playerClass_ == PlayerClass::Prospector) {
        capacity = static_cast<int>(std::min(
            static_cast<double>(std::numeric_limits<int>::max()),
            std::ceil(static_cast<double>(capacity) * classLevelBonuses().carryCapacity)));
    }
    return capacity;
}

void Simulation::clampResourcesToCapacity() {
    if (unlimitedResources_) {
        return;
    }
    wood_ = std::clamp(
        wood_, 0,
        resourceCapacity(ResourceCapacityType::Wood));
    stone_ = std::clamp(
        stone_, 0,
        resourceCapacity(ResourceCapacityType::Stone));
    crystals_ = std::clamp(
        crystals_, 0,
        resourceCapacity(ResourceCapacityType::Crystal));
}

void Simulation::addWood(int amount) {
    if (unlimitedResources_) {
        wood_ = saturatingAdd(wood_, std::max(0, amount));
        return;
    }
    wood_ = std::min(
        resourceCapacity(ResourceCapacityType::Wood),
        saturatingAdd(wood_, std::max(0, amount)));
}

void Simulation::addStone(int amount) {
    if (unlimitedResources_) {
        stone_ = saturatingAdd(stone_, std::max(0, amount));
        return;
    }
    stone_ = std::min(
        resourceCapacity(ResourceCapacityType::Stone),
        saturatingAdd(stone_, std::max(0, amount)));
}

void Simulation::addCrystals(int amount) {
    if (unlimitedResources_) {
        crystals_ = saturatingAdd(crystals_, std::max(0, amount));
        return;
    }
    crystals_ = std::min(
        resourceCapacity(ResourceCapacityType::Crystal),
        saturatingAdd(crystals_, std::max(0, amount)));
}

bool Simulation::hasStorageSpace(ResourceType resource) const {
    if (!isHarvestableResource(resource)) {
        return true;
    }
    if (unlimitedResources_) {
        return true;
    }
    int pending = 0;
    for (const PendingResourceGrant& grant : pendingResourceGrants_) {
        if (grant.type == resource) {
            pending = saturatingAdd(pending, grant.amount);
        }
    }
    if (resource == ResourceType::Wood) {
        return saturatingAdd(wood_, pending) <
            resourceCapacity(ResourceCapacityType::Wood);
    }
    if (resource == ResourceType::Stone) {
        return saturatingAdd(stone_, pending) <
            resourceCapacity(ResourceCapacityType::Stone);
    }
    return saturatingAdd(crystals_, pending) <
        resourceCapacity(ResourceCapacityType::Crystal);
}

double Simulation::resourceToolEfficiency(
    PlayerWeapon tool, ResourceType resource) const {
    if (isDestructibleProp(resource)) {
        return 1.0;
    }
    if (resource == ResourceType::Crystal) {
        return tool == PlayerWeapon::Pickaxe ? 1.0 : 0.0;
    }
    if (tool == PlayerWeapon::LegacyBareHands) return 0.0;
    if ((tool == PlayerWeapon::Axe &&
         resource == ResourceType::Wood) ||
        (tool == PlayerWeapon::Pickaxe &&
         resource == ResourceType::Stone)) {
        return 1.0;
    }
    if (tool == PlayerWeapon::Axe) {
        return gameplay_.axeStoneEfficiency;
    }
    if (tool == PlayerWeapon::Pickaxe) {
        return gameplay_.pickaxeWoodEfficiency;
    }
    return 0.0;
}

void Simulation::advanceBuildingProduction(double deltaSeconds) {
    const auto resourceFor = [](BuildingType type) {
        return type == BuildingType::LumberMill
            ? ResourceType::Wood : type == BuildingType::Quarry
                ? ResourceType::Stone : ResourceType::Crystal;
    };
    const auto availableSpace = [this, &resourceFor](BuildingType type) {
        const ResourceType resource = resourceFor(type);
        const ResourceCapacityType capacityType = resource == ResourceType::Wood
            ? ResourceCapacityType::Wood : resource == ResourceType::Stone
                ? ResourceCapacityType::Stone : ResourceCapacityType::Crystal;
        const int current = resource == ResourceType::Wood ? wood_ :
            resource == ResourceType::Stone ? stone_ : crystals_;
        return unlimitedResources_ ? std::numeric_limits<int>::max() :
            std::max(0, resourceCapacity(capacityType) - current);
    };
    const auto grant = [this, &resourceFor, &availableSpace](
                           EntityId id, BuildingType type, int produced) {
        const int amount = std::min(produced, availableSpace(type));
        if (amount <= 0) return;
        const auto building = std::ranges::find(buildings_.buildings(),
            id, &BuildingInstance::id);
        if (building == buildings_.buildings().end()) return;
        const ResourceType resource = resourceFor(type);
        if (resource == ResourceType::Wood) addWood(amount);
        else if (resource == ResourceType::Stone) addStone(amount);
        else addCrystals(amount);
        events_.push_back({
            .type = resource == ResourceType::Crystal
                ? GameEventType::CrystalProduced : GameEventType::ResourceGranted,
            .entityId = id,
            .resourceType = resource,
            .buildingType = type,
            .position = buildingWorldPosition(*building),
            .amount = amount,
        });
    };
    for (const CrystalProduced& produced : crystalMines_.tick(deltaSeconds))
        grant(produced.mineId, produced.buildingType, produced.amount);
    // Old saves may contain stock from the retired local producer buffers.
    for (const CrystalMineRuntime& producer : crystalMines_.mines()) {
        if (producer.stored <= 0) continue;
        const int amount = crystalMines_.collect(producer.buildingId,
            availableSpace(producer.buildingType));
        grant(producer.buildingId, producer.buildingType, amount);
    }
}

void Simulation::dropPropLoot(const ResourceHit& hit, Vec3 position, std::uint64_t rewardSeed) {
    if (hit.type == ResourceType::ItemCrate) {
        if (!coinPickups_.spawnMetaSilver(position, rewardSeed ^ 0x6a09e667f3bcc909ULL, terrain_))
            events_.push_back({.type=GameEventType::MetaSilverCollected,.position=position,.amount=1});
        // Meta rewards cannot be farmed by rewinding to the start of a night.
        if (waveStartCheckpoint_) {
            const auto current = std::ranges::find(resources_.nodes(),hit.nodeId,&ResourceNode::id);
            if (current != resources_.nodes().end())
                for (auto& node : waveStartCheckpoint_->resourceNodes)
                    if (node.id == hit.nodeId) node = *current;
        }
    }
    const double rewardRoll = unitRandom(rewardSeed);
    const int baseCoins = hit.type == ResourceType::Barrel
        ? 3 + static_cast<int>(rewardSeed % 5ULL)
        : hit.type == ResourceType::Crate
            ? 5 + static_cast<int>(rewardSeed % 7ULL)
            : 7 + static_cast<int>(rewardSeed % 9ULL);
    const int coins = static_cast<int>(std::lround(
        static_cast<double>(baseCoins)));
    bool droppedItem = false;
    LootRarity rarity = LootRarity::Common;
    if (hit.type == ResourceType::Crate) {
        droppedItem = rewardRoll < 0.16;
        rarity = rewardRoll < 0.025
            ? LootRarity::Rare : LootRarity::Common;
    } else if (hit.type == ResourceType::ItemCrate) {
        droppedItem = rewardRoll < 0.62;
        rarity = rewardRoll < 0.025
            ? LootRarity::Legendary
            : rewardRoll < 0.16
                ? LootRarity::Rare
                : LootRarity::Common;
    }
    if (droppedItem) {
        lootChests_.spawnLooseLoot(
            position, rarity, rewardSeed);
    } else {
        coinPickups_.spawnValue(
            position, coins, rewardSeed, terrain_,
            0.5);
    }
    const double medkitChance =
        hit.type == ResourceType::Barrel ? 0.05 : 0.08;
    const std::uint64_t medkitSeed = rewardSeed ^
        0xe7037ed1a0b428dbULL;
    if (unitRandom(medkitSeed) < medkitChance) {
        if (droppedItem) {
            coinPickups_.spawnValue(
                position, coins, rewardSeed,
                terrain_, 0.5);
        }
        coinPickups_.spawnHeart(
            position, medkitSeed,
            terrain_, 0.5);
    }
}

void Simulation::updatePlayerActions(
    double deltaSeconds, const PlayerCommand& command) {
    double productionDeltaSeconds = deltaSeconds;
    if (challengeActive()) {
        productionDeltaSeconds = 0.0;
    }
    advanceBuildingProduction(productionDeltaSeconds);

    const Vec3 direction = lookDirection(playerYaw_, playerPitch_);
    aimedBuilding_ = buildings_.raycast(playerPosition_, direction, 4.0);
    aimedModularBuilding_ = foundations_.raycast(
        playerPosition_, direction, 6.0);
    if (command.overrideAimedBuilding) {
        aimedBuilding_ =
            command.aimedBuildingOverride;
    }
    if (command.overrideAimedModularBuilding) {
        aimedModularBuilding_ =
            command.aimedModularBuildingOverride;
    }
    PlayerWeapon heldTool = playerWeapons_.selectedWeapon();
    aimedResource_ = resources_.raycast(
        playerPosition_, direction,
        gameplay_.resourceGatherRange);
    if (command.overrideAimedResource) {
        aimedResource_ = command.aimedResourceOverride;
    }
    const double enemyAimRange =
        playerWeapons_.selectedWeapon() == PlayerWeapon::Rifle
            ? playerWeapons_.rifleRange()
            : playerWeapons_.selectedWeapon() == PlayerWeapon::IceWand
                ? iceWand_.maximumRange()
            : playerWeapons_.selectedWeapon() == PlayerWeapon::FireWand
                ? fireWand_.maximumRange()
                : gameplay_.pickaxeRange;
    const auto aimedEnemyHit = enemies_.raycastHit(
        playerPosition_, direction, enemyAimRange, &terrain_);
    aimedEnemy_ = aimedEnemyHit
        ? std::optional<EntityId>{aimedEnemyHit->id}
        : std::nullopt;
    bool resourceInFront = false;
    if (aimedResource_) {
        const auto node = std::ranges::find(
            resources_.nodes(), *aimedResource_, &ResourceNode::id);
        if (node != resources_.nodes().end() && node->active) {
            const auto resourceDistance =
                command.aimedResourceHitPositionOverride
                    ? std::optional<double>{geometry::length(geometry::subtract(
                          *command.aimedResourceHitPositionOverride,
                          playerPosition_))}
                    : geometry::raySphereDistance(
                          playerPosition_, direction, node->position,
                          node->radius * (isDestructibleProp(node->type)
                              ? DestructiblePropColliderScale : 1.0));
            resourceInFront = resourceDistance &&
                (!aimedEnemyHit || *resourceDistance < aimedEnemyHit->distance);
        }
        if (resourceInFront) aimedEnemy_.reset();
        else if (aimedEnemy_) aimedResource_.reset();
    }
    // Smart Tools acts only on the foreground target, never on a resource
    // obscured by an enemy.
    if (command.usePickaxe && !command.selectWeapon &&
        !selectedBuilding_ && aimedResource_) {
        const auto node = std::ranges::find(
            resources_.nodes(), *aimedResource_, &ResourceNode::id);
        if (node != resources_.nodes().end() &&
            isHarvestableResource(node->type)) {
            const PlayerWeapon desiredTool = node->type == ResourceType::Wood
                ? PlayerWeapon::Axe : PlayerWeapon::Pickaxe;
            if (heldTool != desiredTool) {
                playerWeapons_.selectWeapon(desiredTool);
                heldTool = desiredTool;
                selectedBuilding_.reset();
                buildingPreview_.reset();
            }
        }
    }
    if (!aimedEnemy_ && !resourceInFront && isMeleeAttackWeapon(heldTool)) {
        // A first-person ray is too brittle for close combat: animation,
        // uneven arena ground and short enemies can move the capsule just
        // outside the crosshair between input and the damage frame. Keep
        // ranged weapons precise, but give melee a narrow horizontal sweep
        // matching the visible weapon arc.
        constexpr double MeleeAimAssistHalfAngle = 0.36;
        aimedEnemy_ = enemies_.nearestEnemyInArc(
            playerPosition_, gameplay_.pickaxeRange,
            playerYaw_, MeleeAimAssistHalfAngle,
            false);
    }
    if (command.useConsumable && heldTool == PlayerWeapon::Bomb &&
        bombs_.throwBomb(playerPosition_, direction)) {
        events_.push_back({
            .type = GameEventType::ConsumableUsed,
            .position = playerPosition_,
        });
    }
    if (command.fireRifle && !selectedBuilding_) {
        const auto fire = playerWeapons_.fireRifle(
            playerPosition_, direction, enemies_,
            playerDamageMultiplier_ * runPlayerDamageMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::Damage) *
                playerClassDamageMultiplier());
        if (fire) {
            events_.push_back({
                .type = GameEventType::WeaponFired,
                .position = fire->hitPosition,
            });
            if (fire->targetId) {
                events_.push_back({
                    .type = GameEventType::ProjectileHit,
                    .entityId = fire->targetId,
                    .position = fire->hitPosition,
                    .damage = fire->damage,
                });
                if (fire->killed) {
                    events_.push_back(enemyKilledEvent(
                        fire->targetId, fire->targetType, fire->targetEliteAffixes,
                        fire->hitPosition));
                    aimedEnemy_.reset();
                }
            }
        }
    }
    if (command.fireIceWand && heldTool == PlayerWeapon::IceWand &&
        !selectedBuilding_ && iceWand_.requestFire(playerPosition_, direction)) {
        events_.push_back({
            .type = GameEventType::IceWandChargeStarted,
            .position = playerPosition_,
        });
    }
    if (command.fireFireWand && heldTool == PlayerWeapon::FireWand &&
        !selectedBuilding_ && fireWand_.requestFire(playerPosition_, direction)) {
        events_.push_back({
            .type = GameEventType::FireWandChargeStarted,
            .position = playerPosition_,
        });
    }
    constexpr double PickaxeInputBufferSeconds = 0.14;
    const bool meleeTool = isMeleeAttackWeapon(heldTool);
    if (command.usePickaxe && meleeTool && !selectedBuilding_) {
        pickaxeInputBufferRemaining_ = PickaxeInputBufferSeconds;
    } else {
        pickaxeInputBufferRemaining_ = std::max(
            0.0,
            pickaxeInputBufferRemaining_ - deltaSeconds);
    }
    if (pickaxeInputBufferRemaining_ > 0.0 &&
        meleeTool &&
        !selectedBuilding_ && pickaxeCooldownRemaining_ <= 0.0) {
        pickaxeInputBufferRemaining_ = 0.0;
        const bool quickTool = heldTool == PlayerWeapon::Axe ||
            heldTool == PlayerWeapon::Pickaxe;
        const double toolSpeed = quickTool
            ? (1.0 + 0.15 * playerPassivePower(RunUpgradeEffect::GatheringSpeed))
            : 1.0;
        pickaxeCooldownRemaining_ = gameplay_.pickaxeCooldown /
            std::max(
                0.05, (quickTool ? toolAttackSpeedMultiplier_ : playerAttackSpeedMultiplier_) *
                    temporaryAttackSpeedMultiplier() * toolSpeed * classLevelBonuses().meleeSpeed *
                    (heldTool == PlayerWeapon::Club ? playerWeaponLevelSpeed(playerWeaponLevel(heldTool)) : 1.0) *
                    (playerClass_ == PlayerClass::Berserker
                         ? playerClassDamageMultiplier() : 1.0));
        const std::uint64_t attackSeed = mixBits64(
            tick_ ^ (pickaxeAttackSequence_++ *
                     0x9e3779b97f4a7c15ULL));
        const double variation =
            (unitRandom(attackSeed) * 2.0 - 1.0) *
            gameplay_.pickaxeDamageVariation;
        const bool critical =
            unitRandom(
                attackSeed ^ 0xd1b54a32d192ed03ULL) <
            gameplay_.pickaxeCriticalChance;
        const bool clubAttack = heldTool == PlayerWeapon::Club;
        double toolMultiplier = playerWeaponLevelDamage(heldTool,playerWeaponLevel(heldTool));
        if (heldTool == PlayerWeapon::LegacyBareHands) toolMultiplier = 0.0;
        else if (clubAttack) toolMultiplier *= club_.damageMultiplier;
        const double damage = playerDamageMultiplier_ *
            runPlayerDamageMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::Damage) *
            playerClassDamageMultiplier() *
            toolMultiplier *
            gameplay_.pickaxeDamage * (1.0 + variation) *
            (critical ? 2.0 : 1.0);
        const double enemyDamage = damage *
            meleeEnemyDamageMultiplier(heldTool);
        if (clubAttack || classAbilityState_.rageRemaining > 0) {
            Vec3 impactPosition{
                playerPosition_.x + direction.x * gameplay_.pickaxeRange,
                playerPosition_.y + direction.y * gameplay_.pickaxeRange,
                playerPosition_.z + direction.z * gameplay_.pickaxeRange,
            };
            if (aimedEnemy_) {
                if (const auto target = enemies_.enemy(*aimedEnemy_)) {
                    impactPosition = target->position;
                }
            }
            const auto results = enemies_.damageInRadius(
                impactPosition, club_.areaRadius * (classAbilityState_.rageRemaining > 0 ? 1.5 : 1.0),
                clubAttack ? damage : enemyDamage, club_.knockbackStrength,
                playerPosition_,
                club_.maxDamagePerAttack);
            bool nailHitRegistered = false;
            for (const auto& result : results) {
                events_.push_back({
                    .type = GameEventType::PickaxeHit,
                    .entityId = result.id,
                    .position = result.position,
                    .targetPosition = aimedEnemyHit &&
                            aimedEnemyHit->id == result.id
                        ? std::optional<Vec3>{aimedEnemyHit->position}
                        : std::nullopt,
                    .damage = result.damage,
                    .critical = critical,
                });
                if (!nailHitRegistered) {
                    nailHitRegistered = true;
                }
                if (result.killed) {
                    events_.push_back(enemyKilledEvent(
                        result.id, result.type, result.eliteAffixes,
                        result.position));
                }
                if (aimedEnemy_ && result.id == *aimedEnemy_ &&
                    result.killed) {
                    aimedEnemy_.reset();
                }
            }
        } else if (aimedEnemy_) {
            const auto result = enemies_.damage(
                *aimedEnemy_, enemyDamage);
            if (result) {
                events_.push_back({
                    .type = GameEventType::PickaxeHit,
                    .entityId = result->id,
                    .position = result->position,
                    .targetPosition = aimedEnemyHit &&
                            aimedEnemyHit->id == result->id
                        ? std::optional<Vec3>{aimedEnemyHit->position}
                        : std::nullopt,
                    .damage = result->damage,
                    .critical = critical,
                });
            }
            if (result && result->killed) {
                events_.push_back(enemyKilledEvent(
                    result->id, result->type, result->eliteAffixes,
                    result->position));
                aimedEnemy_.reset();
            }
        } else if (aimedResource_) {
            const EntityId primaryTarget = *aimedResource_;
            std::vector<EntityId> resourceTargets{
                primaryTarget};
            for (const EntityId targetId : resourceTargets) {
                const auto targetBeforeHit = std::ranges::find(
                    resources_.nodes(), targetId, &ResourceNode::id);
                const bool largeDeposit =
                    targetBeforeHit != resources_.nodes().end() &&
                    targetBeforeHit->yield >= 30;
                if (targetBeforeHit != resources_.nodes().end() &&
                    isHarvestableResource(targetBeforeHit->type) &&
                    !hasStorageSpace(targetBeforeHit->type)) {
                    events_.push_back({
                        .type = GameEventType::ResourceStorageFull,
                        .entityId = targetBeforeHit->id,
                        .resourceType = targetBeforeHit->type,
                        .position = targetBeforeHit->position,
                    });
                    continue;
                }
                Vec3 impactPosition =
                    targetId == primaryTarget &&
                            command.aimedResourceHitPositionOverride
                    ? *command.aimedResourceHitPositionOverride
                    : resourceImpactPosition(
                          resources_.nodes(), targetId,
                          playerPosition_, direction);
                const std::optional<Vec3> resourceCenter =
                    targetBeforeHit != resources_.nodes().end()
                    ? std::optional<Vec3>{targetBeforeHit->position}
                    : std::nullopt;
                const double resourceDamage =
                    targetBeforeHit != resources_.nodes().end()
                    ? damage * gameplay_.resourceToolDamageMultiplier *
                          resourceToolEfficiency(
                          heldTool, targetBeforeHit->type)
                    : 0.0;
                const auto hit =
                    resources_.damage(targetId, resourceDamage);
                if (!hit) {
                    continue;
                }
                events_.push_back({
                    .type =
                        hit->collected ? GameEventType::ResourceCollected : GameEventType::ResourceHit,
                    .entityId = hit->nodeId,
                    .resourceType = hit->type,
                    .position = impactPosition,
                    .targetPosition = resourceCenter,
                    .amount = hit->amount,
                    .damage = resourceDamage,
                    .critical = critical,
                    .matchingTool =
                        (heldTool == PlayerWeapon::Axe &&
                         hit->type == ResourceType::Wood) ||
                        (heldTool == PlayerWeapon::Pickaxe &&
                         (hit->type == ResourceType::Stone ||
                          hit->type == ResourceType::Crystal)),
                    .largeDeposit = largeDeposit,
                    .night = state_ == RunState::Sunset || state_ == RunState::Wave,
                });
                if (hit->amount > 0) {
                    pendingResourceGrants_.push_back({
                        .type = hit->type,
                        .position = impactPosition,
                        .amount = hit->amount,
                        .remaining =
                            ResourcePickupFlightSeconds,
                    });
                }
                if (hit->collected) {
                    if (isHarvestableResource(hit->type)) chainHarvestResources(
                        hit->nodeId,hit->type,hit->position,resourceDamage);
                    if (hit->type == ResourceType::Wood) {
                        launchSawSplinters(
                            hit->nodeId, hit->position);
                    }
                    if (isDestructibleProp(hit->type)) {
                        const std::uint64_t rewardSeed = mixBits64(
                            attackSeed ^
                            (static_cast<std::uint64_t>(targetId.index)
                             << 17U));
                        dropPropLoot(*hit, impactPosition, rewardSeed);
                    }
                    if (targetId == primaryTarget) {
                        aimedResource_.reset();
                    }
                }
            }
        } else if (heldTool == PlayerWeapon::Axe ||
                   heldTool == PlayerWeapon::Pickaxe) {
            events_.push_back({
                .type = GameEventType::ResourceGatherMissed,
                .position = playerPosition_,
                .night = state_ == RunState::Sunset || state_ == RunState::Wave,
            });
        }
    }

    const auto bombExplosions = bombs_.tick(deltaSeconds, enemies_, &terrain_);
    for (const auto& explosion : bombExplosions) {
        events_.push_back({
            .type = GameEventType::Explosion,
            .entityId = explosion.projectileId,
            .position = explosion.position,
            .amount = explosion.killedCount,
            .intensity = explosion.radius,
        });
    }
    for (const auto& hit : bombs_.hits()) {
        events_.push_back({
            .type = GameEventType::ProjectileHit,
            .entityId = hit.id,
            .position = hit.position,
            .damage = hit.damage,
        });
        if (hit.killed) {
            events_.push_back(enemyKilledEvent(
                hit.id, hit.type, hit.eliteAffixes,
                hit.position));
        }
    }

    iceWand_.tick(
        deltaSeconds, enemies_, &terrain_,
        std::span<const BuildingInstance>{buildings_.buildings()},
        std::span<const MapObstacle>{map_.obstacles});
    for (const auto& launch : iceWand_.launches()) {
        events_.push_back({
            .type = GameEventType::IceWandFired,
            .entityId = launch.projectileId,
            .position = launch.position,
        });
    }
    for (const auto& hit : iceWand_.hits()) {
        events_.push_back({
            .type = GameEventType::IceWandHit,
            .entityId = hit.enemyId,
            .sourceId = hit.projectileId,
            .position = hit.position,
            .damage = hit.damage,
            .critical = hit.alreadyFrozen,
        });
        if (hit.killed) {
            events_.push_back(enemyKilledEvent(
                hit.enemyId, hit.enemyType, hit.enemyEliteAffixes,
                hit.position, hit.projectileId));
            if (aimedEnemy_ && *aimedEnemy_ == hit.enemyId) {
                aimedEnemy_.reset();
            }
        }
    }
    for (const auto& impact : iceWand_.impacts()) {
        events_.push_back({
            .type = GameEventType::IceWandImpact,
            .entityId = impact.projectileId,
            .position = impact.position,
            .amount = impact.hitCount,
            .intensity = static_cast<double>(impact.killedCount),
        });
    }

    fireWand_.tick(
        deltaSeconds, enemies_, &terrain_,
        std::span<const BuildingInstance>{buildings_.buildings()},
        std::span<const MapObstacle>{map_.obstacles});
    for (const auto& launch : fireWand_.launches()) {
        events_.push_back({
            .type = GameEventType::FireWandFired,
            .entityId = launch.projectileId,
            .position = launch.position,
        });
    }
    for (const auto& hit : fireWand_.hits()) {
        events_.push_back({
            .type = GameEventType::FireWandHit,
            .entityId = hit.enemyId,
            .sourceId = hit.projectileId,
            .position = hit.position,
            .damage = hit.damage,
            .intensity = hit.periodicBurn
                ? 0.0
                : fireWand_.burnDuration(),
        });
        if (!hit.periodicBurn) {
        }
        if (hit.killed) {
            events_.push_back(enemyKilledEvent(
                hit.enemyId, hit.enemyType, hit.enemyEliteAffixes,
                hit.position, hit.projectileId));
            if (aimedEnemy_ && *aimedEnemy_ == hit.enemyId) {
                aimedEnemy_.reset();
            }
        }
    }
    for (const auto& impact : fireWand_.impacts()) {
        events_.push_back({
            .type = GameEventType::FireWandImpact,
            .entityId = impact.projectileId,
            .position = impact.position,
            .amount = impact.hitCount,
            .intensity = static_cast<double>(impact.killedCount),
        });
    }

    lootChests_.tick(deltaSeconds);
    aimedChest_ = lootChests_.raycastChest(
        playerPosition_, direction, 4.5);
    aimedLoot_ = lootChests_.raycastLoot(
        playerPosition_, direction, 5.0);
    const bool landmarkInteraction =
        command.overrideAimedWorldLandmark &&
        command.aimedWorldLandmarkOverride.has_value();
    if (command.interact && !landmarkInteraction) {
        if (aimedLoot_) {
            if (const auto pickup = lootChests_.collect(*aimedLoot_))
                applyLootPickup(*pickup);
        } else if (aimedChest_) {
            int availableCurrency = unlimitedResources_
                ? std::numeric_limits<int>::max()
                : coins_;
            const auto chest = std::ranges::find(
                lootChests_.chests(), *aimedChest_,
                &LootChestInstance::id);
            const bool paidChest =
                chest != lootChests_.chests().end() &&
                lootChests_.openingCost(*chest) > 0;
            const bool useFreeKey =
                !unlimitedResources_ && paidChest &&
                freeChestOpeningAvailable_ &&
                lootStacks_[lootUpgradeIndex(
                    LootUpgradeEffect::Key)] > 0;
            const ChestOpenResult result =
                lootChests_.open(
                    *aimedChest_, availableCurrency, useFreeKey);
            if (!unlimitedResources_) {
                coins_ = availableCurrency;
            }
            if (result == ChestOpenResult::Opened && useFreeKey) {
                freeChestOpeningAvailable_ = false;
            }
            events_.push_back({
                .type = result == ChestOpenResult::Opened
                    ? GameEventType::ChestOpened
                    : GameEventType::ChestOpenRejected,
                .entityId = aimedChest_,
                .position = chest != lootChests_.chests().end()
                    ? chest->position : Vec3{},
                .critical = result == ChestOpenResult::Opened && useFreeKey,
            });
            if (result == ChestOpenResult::Opened)
                aimedChest_.reset();
        } else if (aimedResource_ && !selectedBuilding_) {
            const auto node = std::ranges::find(resources_.nodes(), *aimedResource_, &ResourceNode::id);
            // Precise client aim may select authored geometry, but cannot open
            // a remote container through an arbitrary ID override.
            if (node != resources_.nodes().end() && node->active && isDestructibleProp(node->type) &&
                geometry::length(geometry::subtract(node->position, playerPosition_)) <=
                    gameplay_.resourceGatherRange*DestructiblePropInteractionRangeMultiplier + node->radius*node->visualScale*DestructiblePropColliderScale &&
                (!command.aimedResourceHitPositionOverride ||
                 geometry::length(geometry::subtract(*command.aimedResourceHitPositionOverride, playerPosition_)) <=
                    gameplay_.resourceGatherRange*DestructiblePropInteractionRangeMultiplier + .05)) {
                const Vec3 center = node->position;
                const std::uint64_t seed = mixBits64(tick_ ^
                    (static_cast<std::uint64_t>(node->id.index) << 17U) ^
                    (static_cast<std::uint64_t>(node->id.generation) << 32U) ^ node->respawnGeneration);
                if (const auto hit = resources_.breakProp(node->id)) {
                    events_.push_back({.type=GameEventType::ResourceCollected,.entityId=hit->nodeId,
                        .resourceType=hit->type,.position=center,.targetPosition=center});
                    dropPropLoot(*hit, center, seed);
                    aimedResource_.reset();
                }
            }
        }
    }
    if (command.revealNearestChest) {
        const bool affordable = unlimitedResources_ ||
            coins_ >= economy_.chestRevealCoinCost;
        const auto revealed = affordable
            ? lootChests_.revealNearest(playerPosition_)
            : std::nullopt;
        if (revealed) {
            if (!unlimitedResources_)
                coins_ -= economy_.chestRevealCoinCost;
            events_.push_back({
                .type = GameEventType::ChestRevealed,
                .position = *revealed,
                .amount = economy_.chestRevealCoinCost,
            });
        } else {
            events_.push_back({
                .type = GameEventType::EconomyPurchaseRejected,
                .amount = economy_.chestRevealCoinCost,
            });
        }
    }
    if (!playerRespawning_) {
        if (const auto pickup =
                lootChests_.collectNearby(
                    playerPosition_, LootProximityPickupRadius))
            applyLootPickup(*pickup);
    }
}

} // namespace ian
