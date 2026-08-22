#include "game/Simulation.hpp"

#include "core/DeterministicRandom.hpp"
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
    return weapon == PlayerWeapon::BareHands ||
           weapon == PlayerWeapon::Axe ||
           weapon == PlayerWeapon::Pickaxe ||
           weapon == PlayerWeapon::Club ||
           weapon == PlayerWeapon::Hammer;
}

} // namespace

int Simulation::resourceCapacity(BuildingType storageType) const {
    const auto core = buildings_.core();
    int capacity = 0;
    if (core) {
        capacity = coreResourceCapacity(storageType, core->level);
    } else if (storageType == BuildingType::WoodStorage) {
        capacity = 60;
    } else if (storageType == BuildingType::StoneStorage) {
        capacity = 30;
    } else if (storageType == BuildingType::CrystalStorage) {
        capacity = 10;
    }
    return static_cast<int>(std::min(
        static_cast<double>(std::numeric_limits<int>::max()),
        static_cast<double>(std::lround(
            static_cast<double>(capacity) * std::max(
                0.05, 1.0 + skillTree_.effectValue(
                    "storage.capacity"))))));
}

void Simulation::clampResourcesToCapacity() {
    if (unlimitedResources_) {
        return;
    }
    wood_ = std::clamp(
        wood_, 0,
        resourceCapacity(BuildingType::WoodStorage));
    stone_ = std::clamp(
        stone_, 0,
        resourceCapacity(BuildingType::StoneStorage));
    crystals_ = std::clamp(
        crystals_, 0,
        resourceCapacity(BuildingType::CrystalStorage));
}

void Simulation::addWood(int amount) {
    if (unlimitedResources_) {
        wood_ = saturatingAdd(wood_, std::max(0, amount));
        return;
    }
    wood_ = std::min(
        resourceCapacity(BuildingType::WoodStorage),
        saturatingAdd(wood_, std::max(0, amount)));
}

void Simulation::addStone(int amount) {
    if (unlimitedResources_) {
        stone_ = saturatingAdd(stone_, std::max(0, amount));
        return;
    }
    stone_ = std::min(
        resourceCapacity(BuildingType::StoneStorage),
        saturatingAdd(stone_, std::max(0, amount)));
}

void Simulation::addCrystals(int amount) {
    if (unlimitedResources_) {
        crystals_ = saturatingAdd(crystals_, std::max(0, amount));
        return;
    }
    crystals_ = std::min(
        resourceCapacity(BuildingType::CrystalStorage),
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
            resourceCapacity(BuildingType::WoodStorage);
    }
    if (resource == ResourceType::Stone) {
        return saturatingAdd(stone_, pending) <
            resourceCapacity(BuildingType::StoneStorage);
    }
    return saturatingAdd(crystals_, pending) <
        resourceCapacity(BuildingType::CrystalStorage);
}

double Simulation::resourceToolEfficiency(
    PlayerWeapon tool, ResourceType resource) const {
    if (isDestructibleProp(resource)) {
        return 1.0;
    }
    if (resource == ResourceType::Crystal) {
        return tool == PlayerWeapon::Pickaxe ? 1.0 : 0.0;
    }
    if (tool == PlayerWeapon::BareHands) {
        // Bare Hands already receives its 25% multiplier in the common melee
        // damage calculation.
        return 1.0;
    }
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

void Simulation::updatePlayerActions(
    double deltaSeconds, const PlayerCommand& command) {
    double productionDeltaSeconds = deltaSeconds;
    if (challengeActive()) {
        productionDeltaSeconds = 0.0;
    }
    if (state_ == RunState::Wave) {
        productionDeltaSeconds *= unlimitedResources_
            ? 1.0
            : std::clamp(
                  skillTree_.effectValue("production.night_speed"),
                  0.0, 1.0);
    }
    const auto production = crystalMines_.tick(
        productionDeltaSeconds);
    if (crystals_ < resourceCapacity(BuildingType::CrystalStorage)) {
        crystalStorageFullNotified_ = false;
    }
    for (const auto& produced : production) {
        const auto building = std::find_if(
            buildings_.buildings().begin(),
            buildings_.buildings().end(),
            [&produced](const BuildingInstance& candidate) {
                return candidate.id == produced.mineId;
            });
        const Vec3 productionPosition =
            building != buildings_.buildings().end()
                ? buildingWorldPosition(*building)
                : Vec3{};
        if (produced.buildingType == BuildingType::CrystalMine) {
            const int before = crystals_;
            addCrystals(produced.amount);
            const int granted = crystals_ - before;
            events_.push_back({
                .type = GameEventType::CrystalProduced,
                .entityId = produced.mineId,
                .buildingType = produced.buildingType,
                .position = productionPosition,
                .amount = granted,
                .night = state_ == RunState::Sunset || state_ == RunState::Wave,
            });
            if (granted < produced.amount &&
                !crystalStorageFullNotified_) {
                crystalStorageFullNotified_ = true;
                events_.push_back({
                    .type = GameEventType::CrystalStorageFull,
                    .entityId = produced.mineId,
                    .buildingType = produced.buildingType,
                    .position = productionPosition,
                    .amount = produced.amount - granted,
                });
            }
        } else {
            const ResourceType resourceType =
                produced.buildingType ==
                        BuildingType::LumberMill
                    ? ResourceType::Wood
                    : ResourceType::Stone;
            if (resourceType == ResourceType::Wood) {
                addWood(produced.amount);
            } else {
                addStone(produced.amount);
            }
            events_.push_back({
                .type = GameEventType::ResourceGranted,
                .entityId = produced.mineId,
                .resourceType = resourceType,
                .buildingType = produced.buildingType,
                .position = productionPosition,
                .amount = produced.amount,
            });
        }
    }

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
    const bool canGather = heldTool == PlayerWeapon::BareHands ||
                           heldTool == PlayerWeapon::Axe ||
                           heldTool == PlayerWeapon::Pickaxe ||
                           heldTool == PlayerWeapon::Hammer;
    aimedResource_ = canGather
        ? resources_.raycast(playerPosition_, direction, gameplay_.resourceGatherRange)
        : std::nullopt;
    if (command.overrideAimedResource && canGather) {
        aimedResource_ = command.aimedResourceOverride;
    }
    constexpr bool automaticToolSwitch = true;
    // Smart Tools chooses the matching unlocked tool. When the player holds
    // another tool but has not unlocked the match yet, hands are the safe
    // universal fallback. Explicit Bare Hands stays intentional.
    if (automaticToolSwitch && !selectedBuilding_ && aimedResource_ &&
        heldTool != PlayerWeapon::BareHands) {
        const auto node = std::ranges::find(
            resources_.nodes(), *aimedResource_,
            &ResourceNode::id);
        if (node != resources_.nodes().end() &&
            isHarvestableResource(node->type)) {
            const PlayerWeapon desiredTool =
                node->type == ResourceType::Wood
                    ? PlayerWeapon::Axe
                    : PlayerWeapon::Pickaxe;
            const bool desiredToolUnlocked = unlimitedResources_ ||
                skillTree_.hasEffect(
                    desiredTool == PlayerWeapon::Axe
                        ? "unlock.axe" : "unlock.pickaxe");
            const PlayerWeapon smartTool = desiredToolUnlocked
                ? desiredTool : PlayerWeapon::BareHands;
            if (heldTool != smartTool) {
                playerWeapons_.selectWeapon(smartTool);
                heldTool = smartTool;
                selectedBuilding_.reset();
                buildingPreview_.reset();
            }
        }
    }
    const double enemyAimRange =
        playerWeapons_.selectedWeapon() == PlayerWeapon::Rifle
            ? playerWeapons_.rifleRange()
            : playerWeapons_.selectedWeapon() == PlayerWeapon::IceWand
                ? iceWand_.maximumRange()
            : playerWeapons_.selectedWeapon() == PlayerWeapon::FireWand
                ? fireWand_.maximumRange()
                : gameplay_.pickaxeRange;
    aimedEnemy_ = enemies_.raycast(
        playerPosition_, direction, enemyAimRange, &terrain_);
    if (!aimedEnemy_ && isMeleeAttackWeapon(heldTool)) {
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
        bombs_.throwBomb(
            playerPosition_, direction, !unlimitedResources_)) {
        events_.push_back({
            .type = GameEventType::ConsumableUsed,
            .position = playerPosition_,
        });
    }
    if (command.fireRifle && !selectedBuilding_) {
        const auto fire = playerWeapons_.fireRifle(
            playerPosition_, direction, enemies_,
            playerDamageMultiplier_ * runPlayerDamageMultiplier_ *
                playerClassDamageMultiplier() * std::max(
                0.05, 1.0 + skillTree_.effectValue(
                    "player.damage")));
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
                registerNailHit(
                    *fire->targetId, fire->hitPosition,
                    fire->damage);
                if (fire->killed) {
                    events_.push_back({
                        .type = GameEventType::EnemyKilled,
                        .entityId = fire->targetId,
                        .position = fire->hitPosition,
                    });
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
        pickaxeCooldownRemaining_ = gameplay_.pickaxeCooldown /
            std::max(
                0.05, playerAttackSpeedMultiplier_ *
                    temporaryAttackSpeedMultiplier());
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
        double toolMultiplier = 1.0;
        if (heldTool == PlayerWeapon::BareHands) toolMultiplier = 0.25;
        else if (clubAttack) toolMultiplier = club_.damageMultiplier;
        else if (heldTool == PlayerWeapon::Hammer) toolMultiplier = 0.75;
        const double skillDamageMultiplier = std::max(
            0.05, 1.0 + skillTree_.effectValue("player.damage"));
        if (clubAttack) {
            toolMultiplier *= std::max(
                0.05, 1.0 + skillTree_.effectValue("club.damage"));
        }
        const double damage = playerDamageMultiplier_ *
            runPlayerDamageMultiplier_ *
            playerClassDamageMultiplier() *
            skillDamageMultiplier * toolMultiplier *
            gameplay_.pickaxeDamage * (1.0 + variation) *
            (critical ? 2.0 : 1.0);
        if (clubAttack) {
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
                impactPosition, club_.areaRadius * std::max(
                    0.05, 1.0 + skillTree_.effectValue("club.area")),
                damage, (club_.knockbackStrength +
                    skillTree_.effectValue("club.knockback_strength")) *
                    std::max(0.05, 1.0 +
                        skillTree_.effectValue("club.knockback")),
                playerPosition_,
                club_.maxDamagePerAttack);
            bool nailHitRegistered = false;
            for (const auto& result : results) {
                events_.push_back({
                    .type = GameEventType::PickaxeHit,
                    .entityId = result.id,
                    .position = result.position,
                    .damage = result.damage,
                    .critical = critical,
                });
                if (!nailHitRegistered) {
                    registerNailHit(
                        result.id, result.position,
                        result.damage);
                    nailHitRegistered = true;
                }
                if (result.killed) {
                    events_.push_back({
                        .type = GameEventType::EnemyKilled,
                        .entityId = result.id,
                        .position = result.position,
                    });
                }
                if (aimedEnemy_ && result.id == *aimedEnemy_ &&
                    result.killed) {
                    aimedEnemy_.reset();
                }
            }
        } else if (aimedEnemy_) {
            const auto result = enemies_.damage(*aimedEnemy_, damage);
            if (result) {
                events_.push_back({
                    .type = GameEventType::PickaxeHit,
                    .entityId = result->id,
                    .position = result->position,
                    .damage = result->damage,
                    .critical = critical,
                });
                registerNailHit(
                    result->id, result->position,
                    result->damage);
            }
            if (result && result->killed) {
                events_.push_back({
                    .type = GameEventType::EnemyKilled,
                    .entityId = result->id,
                    .position = result->position,
                });
                aimedEnemy_.reset();
            }
        } else if (aimedResource_) {
            const EntityId primaryTarget = *aimedResource_;
            std::vector<EntityId> resourceTargets{
                primaryTarget};
            const bool powerSwingUnlocked =
                skillTree_.hasEffect("gather.power_swing");
            const bool powerSwing = powerSwingUnlocked &&
                (++powerSwingResourceHits_ % 3U == 0U);
            if (powerSwing) {
                const double powerSwingRadius = 3.0 * std::max(
                    0.05, 1.0 + skillTree_.effectValue(
                        "gather.power_swing_radius"));
                const auto primary = std::ranges::find(
                    resources_.nodes(), primaryTarget,
                    &ResourceNode::id);
                if (primary != resources_.nodes().end()) {
                    for (const ResourceNode& node :
                         resources_.nodes()) {
                        if (!node.active ||
                            node.id == primaryTarget) {
                            continue;
                        }
                        const double deltaX =
                            node.position.x -
                            primary->position.x;
                        const double deltaZ =
                            node.position.z -
                            primary->position.z;
                        if (deltaX * deltaX +
                                deltaZ * deltaZ <=
                            powerSwingRadius * powerSwingRadius) {
                            resourceTargets.push_back(
                                node.id);
                        }
                    }
                }
            }
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
                Vec3 impactPosition = resourceImpactPosition(
                    resources_.nodes(), targetId,
                    playerPosition_, direction);
                const double resourceDamage =
                    targetBeforeHit != resources_.nodes().end()
                    ? damage * std::max(
                          0.05, 1.0 + skillTree_.effectValue(
                              "gather.damage")) *
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
                    .amount = hit->amount,
                    .damage = resourceDamage,
                    .critical = critical,
                    .bareHands = heldTool == PlayerWeapon::BareHands,
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
                    if (heldTool == PlayerWeapon::BareHands &&
                        !introSkillObjectiveCompleted_) {
                        if (hit->type == ResourceType::Wood)
                            bareHandsWoodGathered_ += hit->amount;
                        else if (hit->type == ResourceType::Stone)
                            bareHandsStoneGathered_ += hit->amount;
                        if (bareHandsWoodGathered_ >= 15 && bareHandsStoneGathered_ >= 10) {
                            introSkillObjectiveCompleted_ = true;
                            events_.push_back({.type = GameEventType::IntroSkillObjectiveCompleted});
                        }
                    }
                }
                if (hit->collected) {
                    if (hit->type == ResourceType::Wood) {
                        launchSawSplinters(
                            hit->nodeId, hit->position);
                    }
                    if (isDestructibleProp(hit->type)) {
                        const std::uint64_t rewardSeed = mixBits64(
                            attackSeed ^
                            (static_cast<std::uint64_t>(targetId.index)
                             << 17U));
                        const double rewardRoll = unitRandom(rewardSeed);
                        const int baseCoins = hit->type == ResourceType::Barrel
                            ? 3 + static_cast<int>(rewardSeed % 5ULL)
                            : hit->type == ResourceType::Crate
                                ? 5 + static_cast<int>(rewardSeed % 7ULL)
                                : 7 + static_cast<int>(rewardSeed % 9ULL);
                        const int coins = static_cast<int>(std::lround(
                            static_cast<double>(baseCoins) * std::max(
                                0.0, 1.0 + skillTree_.effectValue(
                                    "prop.coins"))));
                        bool droppedItem = false;
                        LootRarity rarity = LootRarity::Common;
                        if (hit->type == ResourceType::Crate) {
                            droppedItem = rewardRoll < 0.16;
                            rarity = rewardRoll < 0.025
                                ? LootRarity::Rare : LootRarity::Common;
                        } else if (hit->type == ResourceType::ItemCrate) {
                            droppedItem = rewardRoll < 0.62;
                            rarity = rewardRoll < 0.025
                                ? LootRarity::Legendary
                                : rewardRoll < 0.16
                                    ? LootRarity::Rare
                                    : LootRarity::Common;
                        }
                        if (droppedItem) {
                            lootChests_.spawnLooseLoot(
                                impactPosition, rarity, rewardSeed);
                        } else {
                            coinPickups_.spawnValue(
                                impactPosition, coins, rewardSeed, terrain_,
                                0.5);
                        }
                        const double medkitChance =
                            hit->type == ResourceType::Barrel ? 0.05 : 0.08;
                        const std::uint64_t medkitSeed = rewardSeed ^
                            0xe7037ed1a0b428dbULL;
                        if (unitRandom(medkitSeed) < medkitChance) {
                            if (droppedItem) {
                                coinPickups_.spawnValue(
                                    impactPosition, coins, rewardSeed,
                                    terrain_, 0.5);
                            }
                            coinPickups_.spawnHeart(
                                impactPosition, medkitSeed,
                                terrain_, 0.5);
                        }
                        if (hit->type == ResourceType::Barrel) {
                            grantConfiguredInsight(
                                1.0 + unitRandom(rewardSeed ^ 0x94d049bbULL),
                                InsightSource::Other,
                                InsightCategory::Exploration, {});
                        }
                    }
                    if (targetId == primaryTarget) {
                        aimedResource_.reset();
                    }
                }
            }
        } else if (canGather) {
            events_.push_back({
                .type = GameEventType::ResourceGatherMissed,
                .position = playerPosition_,
                .bareHands = heldTool == PlayerWeapon::BareHands,
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
        registerNailHit(hit.enemyId, hit.position, hit.damage);
        if (hit.killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = hit.enemyId,
                .sourceId = hit.projectileId,
                .position = hit.position,
            });
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
            registerNailHit(hit.enemyId, hit.position, hit.damage);
        }
        if (hit.killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = hit.enemyId,
                .sourceId = hit.projectileId,
                .position = hit.position,
            });
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
            const bool explorationChest =
                chest != lootChests_.chests().end() &&
                chest->purpose == LootChestPurpose::Exploration;
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
            if (result == ChestOpenResult::Opened && explorationChest &&
                lootStacks_[lootUpgradeIndex(
                    LootUpgradeEffect::Compass)] > 0) {
                freeChestRerollsRemaining_ = saturatingAdd(
                    freeChestRerollsRemaining_, 1);
            }
            events_.push_back({
                .type = result == ChestOpenResult::Opened
                    ? GameEventType::ChestOpened
                    : GameEventType::ChestOpenRejected,
                .entityId = aimedChest_,
                .critical = result == ChestOpenResult::Opened && useFreeKey,
            });
            if (result == ChestOpenResult::Opened)
                aimedChest_.reset();
        }
    }
    if (command.rerollChest) {
        const auto rerollChest = std::ranges::find(
            lootChests_.chests(), command.rerollChest->chestId,
            [](const LootChestInstance& value) { return value.loot.id; });
        const std::size_t rerollIndex = rerollChest == lootChests_.chests().end()
            ? 0U
            : std::min<std::size_t>(
                  rerollChest->rerollCount,
                  economy_.chestRerollCoinCosts.size() - 1U);
        const bool useFreeReroll =
            !unlimitedResources_ && freeChestRerollsRemaining_ > 0;
        const int rerollCost = useFreeReroll
            ? 0
            : economy_.chestRerollCoinCosts[rerollIndex];
        int availableCoins = unlimitedResources_
            ? std::numeric_limits<int>::max()
            : coins_;
        const ChestRerollResult result = lootChests_.reroll(
            command.rerollChest->chestId, availableCoins,
            rerollCost);
        if (result == ChestRerollResult::Rerolled &&
            useFreeReroll) {
            --freeChestRerollsRemaining_;
        }
        if (!unlimitedResources_) coins_ = availableCoins;
        const GameEventType eventType =
            result == ChestRerollResult::Rerolled
                ? GameEventType::ChestRerolled
                : result == ChestRerollResult::AlreadyRerolled
                    ? GameEventType::ChestRerollAlreadyUsed
                    : result == ChestRerollResult::InsufficientCoins
                        ? GameEventType::EconomyPurchaseRejected
                        : GameEventType::ChestRerollUnavailable;
        events_.push_back({
            .type = eventType,
            .entityId = command.rerollChest->chestId,
            .amount = rerollCost,
        });
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
